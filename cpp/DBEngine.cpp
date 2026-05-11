#include "DBEngine.h"
#include "BinarySerializer.h"
#include "TurboDBError.h"
#include "SyncMetadata.h"
#include "LazyRecordProxy.h"
#include "HMACProvider.h"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

#ifdef __ANDROID__
#include <jni.h>
extern JavaVM* g_jvm;
#include <android/log.h>
#define LOG_TAG "TurboDB_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) do {} while(0)
#define LOGE(...) do {} while(0)
#endif

#ifdef __APPLE__
#include "../ios/KeyManagerIOS.h"
#endif

namespace turbo_db {

// ─────────────────────────────────────────────────────────────────────────────
// DBEngine Core
// ─────────────────────────────────────────────────────────────────────────────

DBEngine::DBEngine(
    std::shared_ptr<facebook::react::CallInvoker> js_invoker,
    std::unique_ptr<SecureCryptoContext> crypto)
    : crypto_(std::move(crypto)),
      next_free_offset_(1024 * 1024),
      arena_(65536),
      js_invoker_(js_invoker),
      is_secure_mode_(true),
      sync_enabled_(false)
{
    scheduler_ = std::make_unique<DBScheduler>();
}

DBEngine::~DBEngine() {
    if (mmap_) mmap_->close();
}

facebook::jsi::Value DBEngine::get(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::PropNameID& name)
{
    auto propName = name.utf8(runtime);

    if (propName == "initStorage") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 3,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                std::string path = args[0].asString(rt).utf8(rt);
                size_t size = static_cast<size_t>(args[1].asNumber());
                bool sync = args[2].isObject() ? args[2].asObject(rt).getProperty(rt, "syncEnabled").asBool() : false;
                return facebook::jsi::Value(initStorage(path, size, sync));
            });
    }

    if (propName == "insertRec") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 2,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return insertRec(rt, args[0].asString(rt).utf8(rt), args[1]);
            });
    }

    if (propName == "findRec") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return findRec(rt, args[0].asString(rt).utf8(rt));
            });
    }

    if (propName == "clearStorage" || propName == "deleteAll") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(deleteAll());
            });
    }

    if (propName == "setMulti") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setMulti(rt, args[0]);
            });
    }

    if (propName == "getMultiple") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return getMultiple(rt, args[0]);
            });
    }

    if (propName == "remove") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(remove(args[0].asString(rt).utf8(rt)));
            });
    }

    if (propName == "rangeQuery") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 2,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                std::string start = args[0].asString(rt).utf8(rt);
                std::string end = args[1].asString(rt).utf8(rt);
                auto pairs = rangeQuery(rt, start, end);
                
                facebook::jsi::Array result(rt, pairs.size());
                for (size_t i = 0; i < pairs.size(); i++) {
                    facebook::jsi::Object obj(rt);
                    obj.setProperty(rt, "key", facebook::jsi::String::createFromUtf8(rt, pairs[i].first));
                    obj.setProperty(rt, "value", std::move(pairs[i].second));
                    result.setValueAtIndex(rt, i, obj);
                }
                return result;
            });
    }

    if (propName == "getAllKeys") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                auto keys = getAllKeys();
                facebook::jsi::Array result(rt, keys.size());
                for (size_t i = 0; i < keys.size(); i++) {
                    result.setValueAtIndex(rt, i, facebook::jsi::String::createFromUtf8(rt, keys[i]));
                }
                return result;
            });
    }

    if (propName == "getStats") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return getStats(rt);
            });
    }

    if (propName == "benchmark") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(static_cast<double>(benchmark()));
            });
    }

    // ── Asynchronous APIs ──────────────────────────────

    if (propName == "setAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setAsync(rt, args[0]);
            });
    }

    if (propName == "getAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return getAsync(rt, args[0]);
            });
    }

    if (propName == "getAllKeysAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return getAllKeysAsync(rt);
            });
    }

    if (propName == "removeAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return removeAsync(rt, args[0]);
            });
    }

    if (propName == "setMultiAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setMultiAsync(rt, args[0]);
            });
    }

    if (propName == "getMultipleAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return getMultipleAsync(rt, args[0]);
            });
    }

    if (propName == "rangeQueryAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return rangeQueryAsync(rt, args[0]);
            });
    }

    if (propName == "getLocalChangesAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return getLocalChangesAsync(rt, args[0]);
            });
    }

    if (propName == "applyRemoteChangesAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return applyRemoteChangesAsync(rt, args[0]);
            });
    }

    if (propName == "markPushedAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return markPushedAsync(rt, args[0]);
            });
    }

    if (propName == "getAllKeysPaged") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 2,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                int limit  = static_cast<int>(args[0].asNumber());
                int offset = static_cast<int>(args[1].asNumber());
                auto keys  = getAllKeysPaged(limit, offset);
                facebook::jsi::Array res(rt, keys.size());
                for (size_t i = 0; i < keys.size(); i++)
                    res.setValueAtIndex(rt, i, facebook::jsi::String::createFromUtf8(rt, keys[i]));
                return res;
            });
    }

    if (propName == "del") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(remove(args[0].asString(rt).utf8(rt)));
            });
    }

    if (propName == "flush") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                if (btree_) btree_->flush();
                if (pbtree_) pbtree_->checkpoint();
                return facebook::jsi::Value::undefined();
            });
    }

    if (propName == "verifyHealth") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(verifyHealth());
            });
    }

    if (propName == "repair") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::Value(repair());
            });
    }

    if (propName == "getDatabasePath") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::String::createFromUtf8(rt, getDatabasePath());
            });
    }

    if (propName == "getWALPath") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return facebook::jsi::String::createFromUtf8(rt, getWALPath());
            });
    }

    if (propName == "setSecureMode") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime&, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                setSecureMode(args[0].asBool());
                return facebook::jsi::Value::undefined();
            });
    }

    if (propName == "setSecureItemAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 2,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                if (!args[0].isString() || !args[1].isString()) {
                    return createPromise(rt, [](auto& r, auto, auto rej) {
                        rej->call(r, facebook::jsi::String::createFromAscii(r, "setSecureItemAsync: expected (string, string)"));
                    });
                }
                std::string k = args[0].asString(rt).utf8(rt);
                std::string v = args[1].asString(rt).utf8(rt);
                bool ok = false;
#ifdef __APPLE__
                ok = turbo_db::KeyManagerIOS::setSecureItem(k, v);
#elif defined(__ANDROID__)
                if (g_jvm) {
                    JNIEnv* env = nullptr;
                    bool attached = false;
                    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
                        g_jvm->AttachCurrentThread(&env, nullptr);
                        attached = true;
                    }
                    if (env) {
                        jclass cls = env->FindClass("com/turbodb/KeyStoreManager");
                        if (cls) {
                            jmethodID mid = env->GetStaticMethodID(cls, "setSecureItem", "(Ljava/lang/String;Ljava/lang/String;)Z");
                            if (mid) {
                                jstring jKey = env->NewStringUTF(k.c_str());
                                jstring jValue = env->NewStringUTF(v.c_str());
                                ok = env->CallStaticBooleanMethod(cls, mid, jKey, jValue);
                                env->DeleteLocalRef(jKey);
                                env->DeleteLocalRef(jValue);
                            }
                        }
                        if (attached) g_jvm->DetachCurrentThread();
                    }
                }
#endif
                return createPromise(rt, [ok](auto& r, auto resolve, auto) {
                    resolve->call(r, facebook::jsi::Value(ok));
                });
            });
    }

    if (propName == "getSecureItemAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                if (!args[0].isString()) {
                    return createPromise(rt, [](auto& r, auto, auto rej) {
                        rej->call(r, facebook::jsi::String::createFromAscii(r, "getSecureItemAsync: expected string key"));
                    });
                }
                std::string k = args[0].asString(rt).utf8(rt);
                std::string result;
#ifdef __APPLE__
                result = turbo_db::KeyManagerIOS::getSecureItem(k);
#elif defined(__ANDROID__)
                if (g_jvm) {
                    JNIEnv* env = nullptr;
                    bool attached = false;
                    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
                        g_jvm->AttachCurrentThread(&env, nullptr);
                        attached = true;
                    }
                    if (env) {
                        jclass cls = env->FindClass("com/turbodb/KeyStoreManager");
                        if (cls) {
                            jmethodID mid = env->GetStaticMethodID(cls, "getSecureItem", "(Ljava/lang/String;)Ljava/lang/String;");
                            if (mid) {
                                jstring jKey = env->NewStringUTF(k.c_str());
                                jstring jRes = (jstring)env->CallStaticObjectMethod(cls, mid, jKey);
                                if (jRes) {
                                    const char* chars = env->GetStringUTFChars(jRes, nullptr);
                                    if (chars) {
                                        result = chars;
                                        env->ReleaseStringUTFChars(jRes, chars);
                                    }
                                    env->DeleteLocalRef(jRes);
                                }
                                env->DeleteLocalRef(jKey);
                            }
                        }
                        if (attached) g_jvm->DetachCurrentThread();
                    }
                }
#endif
                return createPromise(rt, [result](auto& r, auto resolve, auto) {
                    if (result.empty()) {
                        resolve->call(r, facebook::jsi::Value::null());
                    } else {
                        resolve->call(r, facebook::jsi::String::createFromUtf8(r, result));
                    }
                });
            });
    }

    if (propName == "deleteSecureItemAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                if (!args[0].isString()) {
                    return createPromise(rt, [](auto& r, auto, auto rej) {
                        rej->call(r, facebook::jsi::String::createFromAscii(r, "deleteSecureItemAsync: expected string key"));
                    });
                }
                std::string k = args[0].asString(rt).utf8(rt);
                bool ok = false;
#ifdef __APPLE__
                ok = turbo_db::KeyManagerIOS::deleteSecureItem(k);
#elif defined(__ANDROID__)
                if (g_jvm) {
                    JNIEnv* env = nullptr;
                    bool attached = false;
                    if (g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
                        g_jvm->AttachCurrentThread(&env, nullptr);
                        attached = true;
                    }
                    if (env) {
                        jclass cls = env->FindClass("com/turbodb/KeyStoreManager");
                        if (cls) {
                            jmethodID mid = env->GetStaticMethodID(cls, "deleteSecureItem", "(Ljava/lang/String;)Z");
                            if (mid) {
                                jstring jKey = env->NewStringUTF(k.c_str());
                                ok = env->CallStaticBooleanMethod(cls, mid, jKey);
                                env->DeleteLocalRef(jKey);
                            }
                        }
                        if (attached) g_jvm->DetachCurrentThread();
                    }
                }
#endif
                return createPromise(rt, [ok](auto& r, auto resolve, auto) {
                    resolve->call(r, facebook::jsi::Value(ok));
                });
            });
    }

    // ── R3: Data Management APIs ──────────────────────────────────────────────────

    if (propName == "setWithTTLAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 3,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setWithTTLAsync(rt, args[0]);
            });
    }

    if (propName == "cleanupExpiredAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return cleanupExpiredAsync(rt);
            });
    }

    if (propName == "prefixSearchAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return prefixSearchAsync(rt, args[0]);
            });
    }

    if (propName == "regexSearchAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return regexSearchAsync(rt, args[0]);
            });
    }

    if (propName == "exportDBAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return exportDBAsync(rt);
            });
    }

    if (propName == "importDBAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return importDBAsync(rt, args[0]);
            });
    }

    if (propName == "setBlobAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 2,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setBlobAsync(rt, args[0]);
            });
    }

    if (propName == "getBlobAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return getBlobAsync(rt, args[0]);
            });
    }

    if (propName == "compactAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 0,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value*, size_t) -> facebook::jsi::Value {
                return compactAsync(rt);
            });
    }

    if (propName == "registerEventListener") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return registerEventListener(rt, args[0]);
            });
    }

    // ── R5: Security & Enterprise ──────────────────────────────────────────────

    if (propName == "rotateEncryptionKeyAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return rotateEncryptionKeyAsync(rt, args[0]);
            });
    }

    if (propName == "setSelectiveEncryptAsync") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                return setSelectiveEncryptAsync(rt, args[0]);
            });
    }

    if (propName == "enableHMACMode") {
        return facebook::jsi::Function::createFromHostFunction(
            runtime, name, 1,
            [this](facebook::jsi::Runtime& rt, const facebook::jsi::Value&,
                   const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
                enableHMACMode(args[0].asBool());
                return facebook::jsi::Value::undefined();
            });
    }

    return facebook::jsi::Value::undefined();
}

std::vector<facebook::jsi::PropNameID> DBEngine::getPropertyNames(
    facebook::jsi::Runtime& runtime)
{
    std::vector<facebook::jsi::PropNameID> result;
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "initStorage"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "insertRec"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "findRec"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "deleteAll"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setMulti"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getMultiple"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "remove"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "rangeQuery"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getAllKeys"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getStats"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "benchmark"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getAllKeysAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "removeAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setMultiAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getMultipleAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "rangeQueryAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getLocalChangesAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "applyRemoteChangesAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "markPushedAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getAllKeysPaged"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "del"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "flush"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "verifyHealth"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "repair"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getDatabasePath"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getWALPath"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setSecureMode"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setSecureItemAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getSecureItemAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "deleteSecureItemAsync"));
    // R3: Data Management
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setWithTTLAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "cleanupExpiredAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "prefixSearchAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "regexSearchAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "exportDBAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "importDBAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setBlobAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "getBlobAsync"));
    // R4: Compaction
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "compactAsync"));
    // R4.1: Native Events
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "registerEventListener"));
    // R5: Security & Enterprise
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "rotateEncryptionKeyAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "setSelectiveEncryptAsync"));
    result.push_back(facebook::jsi::PropNameID::forUtf8(runtime, "enableHMACMode"));
    return result;
}

facebook::jsi::Value DBEngine::getStats(facebook::jsi::Runtime& runtime) {
    facebook::jsi::Object obj(runtime);
    if (pbtree_) {
        obj.setProperty(runtime, "treeHeight", static_cast<double>(pbtree_->getTreeDepth()));
        obj.setProperty(runtime, "nodeCount", static_cast<double>(pbtree_->getHeader().node_count));
        obj.setProperty(runtime, "formatVersion", static_cast<double>(pbtree_->getHeader().format_version));
    }
    if (mmap_) {
        obj.setProperty(runtime, "fileSize", static_cast<double>(mmap_->getSize()));
    }
    obj.setProperty(runtime, "isInitialized", true);
    obj.setProperty(runtime, "secureMode", is_secure_mode_);
    return obj;
}

bool DBEngine::initStorage(const std::string& path, size_t size, bool enableSync) {
    sync_enabled_ = enableSync;
    if (btree_) btree_.reset();
    if (pbtree_) pbtree_.reset();
    if (wal_) wal_.reset();
    if (mmap_) { mmap_->close(); mmap_.reset(); }

    try {
        mmap_ = std::make_unique<MMapRegion>();
        mmap_->init(path, size);
        wal_ = std::make_unique<WALManager>(path, crypto_.get());
        pbtree_ = std::make_unique<PersistentBPlusTree>(mmap_.get(), wal_.get());
        pbtree_->init();

        if (pbtree_->needsMigration()) {
            mmap_->close();
            ::unlink(path.c_str());
            ::unlink((path + ".wal").c_str());
            mmap_->init(path, size);
            wal_ = std::make_unique<WALManager>(path, crypto_.get());
            pbtree_ = std::make_unique<PersistentBPlusTree>(mmap_.get(), wal_.get());
            pbtree_->init();
        }

        next_free_offset_ = pbtree_->getHeader().next_free_offset;
        if (next_free_offset_ < 1024 * 1024) next_free_offset_ = 1024 * 1024;
        next_free_offset_ = (next_free_offset_ + 7) & ~7;

        btree_ = std::make_unique<BufferedBTree>(pbtree_.get());
        compactor_ = std::make_unique<Compactor>(this, mmap_.get(), btree_.get(), path, crypto_.get());
        return true;
    } catch (...) { return false; }
}

bool DBEngine::deleteAll() {
    if (!btree_ || !pbtree_) return false;
    std::unique_lock lock(rw_mutex_);
    btree_->clear();
    next_free_offset_ = 1024 * 1024;
    pbtree_->setNextFreeOffset(next_free_offset_);
    pbtree_->checkpoint();

    emitNativeEvent("remove", ""); // Empty key denotes "everything"

    return true;
}

// ── Transaction API ─────────────────────────────────────────────────────────────

bool DBEngine::beginTransaction() {
    std::unique_lock lock(rw_mutex_);
    if (in_transaction_) return false; // Already in transaction
    if (!wal_) return false; // WAL required for transactions

    in_transaction_ = true;
    tx_writes_.clear();
    tx_deletes_.clear();

    wal_->logBegin();
    return true;
}

bool DBEngine::commitTransaction() {
    std::unique_lock lock(rw_mutex_);
    if (!in_transaction_) return false;

    // ── Batch all writes into single WAL record ──
    if (wal_ && (!tx_writes_.empty() || !tx_deletes_.empty())) {
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>> batch;

        // Add writes
        for (const auto& write : tx_writes_) {
            (void)write; // Suppress unused warning
            // Read the data from mmap to include in batch
            // Simplified: just log the offset, let WAL replay handle it
            // For true batching, we'd copy the record data here
        }

        // For now, log COMMIT after individual writes (done in insertRecInternal)
        wal_->logCommit();
    }

    // Now persist all buffered writes to pbtree_
    for (const auto& write : tx_writes_) {
        pbtree_->insert(write.first, write.second, false);
    }
    for (const auto& key : tx_deletes_) {
        pbtree_->insert(key, 0, false);
    }

    if (pbtree_) {
        pbtree_->checkpoint();
    }

    // Emit events for all keys involved in the transaction
    for (const auto& write : tx_writes_) {
        emitNativeEvent("set", write.first);
    }
    for (const auto& key : tx_deletes_) {
        emitNativeEvent("remove", key);
    }

    // Clear transaction state
    tx_writes_.clear();
    tx_deletes_.clear();
    in_transaction_ = false;
    return true;
}

bool DBEngine::rollbackTransaction() {
    std::unique_lock lock(rw_mutex_);
    if (!in_transaction_) return false;

    // Mark WAL transaction as aborted (no COMMIT record)
    // On recovery, uncommitted transactions are discarded

    // Clear transaction state without applying to pbtree_
    tx_writes_.clear();
    tx_deletes_.clear();
    in_transaction_ = false;
    return true;
}

facebook::jsi::Value DBEngine::insertRec(
    facebook::jsi::Runtime& runtime,
    const std::string& key,
    const facebook::jsi::Value& obj)
{
    arena_.reset();
    BinarySerializer::serialize(runtime, obj, arena_);
    std::vector<uint8_t> plain(arena_.data(), arena_.data() + arena_.size());
    bool ok = false;
    try {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        ok = insertRecBytes(key, plain, true, false);
    } catch (...) {}
    
    if (ok) {
        emitNativeEvent("set", key);
    }
    
    return facebook::jsi::Value(ok);
}

bool DBEngine::insertRecBytes(
    const std::string& key,
    const std::vector<uint8_t>& plain,
    bool shouldCommit,
    bool is_tombstone,
    SyncMetadata* explicit_meta,
    size_t* outOffset)
{
    if (!btree_ || !mmap_) return false;
    
    const uint8_t* payload_ptr = nullptr;
    size_t payload_len = plain.size();
    std::vector<uint8_t> encrypted;

    // ── R5: Selective Encryption — bypass crypto for NOENC_PREFIX keys ──────
    bool skip_encryption = false;
    std::string noenc_pref = std::string(NOENC_PREFIX);
    if (key.compare(0, noenc_pref.size(), noenc_pref) == 0) {
        skip_encryption = true;
    }

    if (payload_len > 0) {
        payload_ptr = plain.data();
        if (crypto_ && !is_tombstone && !skip_encryption) {
            encrypted = crypto_->encrypt(plain.data(), plain.size());
            payload_ptr = encrypted.data();
            payload_len = encrypted.size();
        }
    }

    next_free_offset_ = (next_free_offset_ + 7) & ~7;
    size_t offset = next_free_offset_;
    uint32_t len32 = static_cast<uint32_t>(payload_len);
    if (sync_enabled_) len32 += sizeof(SyncMetadata);

    SyncMetadata meta;
    if (sync_enabled_) {
        if (explicit_meta) {
            std::memcpy(&meta, explicit_meta, sizeof(SyncMetadata));
        } else {
            std::memset(&meta, 0, sizeof(SyncMetadata));
            meta.flags = (is_tombstone ? SYNC_FLAG_TOMBSTONE : 0);
        }
    }

    // ── R5: Integrity tag — HMAC-SHA256 (tamper-proof) or CRC32 (corruption detection) ──
    // HMAC mode: 32-byte tag keyed by hmac_key_material_
    // CRC32 mode: 4-byte tag (legacy, accidental corruption only)
    // Neither: no tag written (non-secure mode)
    bool use_hmac = is_secure_mode_ && hmac_mode_ && !hmac_key_material_.empty();
    size_t integrity_tag_size = 0;
    HMACTag hmac_tag{};
    uint32_t crc32 = 0;

    if (is_secure_mode_) {
        // Build the buffer to be integrity-checked: [SyncMetadata?] + [payload]
        std::vector<uint8_t> full;
        if (sync_enabled_) {
            const uint8_t* m = reinterpret_cast<const uint8_t*>(&meta);
            full.insert(full.end(), m, m + sizeof(SyncMetadata));
        }
        if (payload_len > 0) full.insert(full.end(), payload_ptr, payload_ptr + payload_len);

        if (use_hmac) {
            // Derive HMAC key and compute tag
            HMACTag hk = derive_hmac_key(hmac_key_material_);
            hmac_tag = hmac_sha256(hk.data(), hk.size(), full.data(), full.size());
            integrity_tag_size = HMAC_SHA256_SIZE;
        } else {
            // Legacy CRC32
            crc32 = wal_ ? wal_->calculate_crc32(full.data(), full.size()) : 0;
            integrity_tag_size = sizeof(uint32_t);
        }
    }

    mmap_->write(offset, reinterpret_cast<const uint8_t*>(&len32), sizeof(uint32_t));
    if (sync_enabled_) {
        mmap_->write(offset + sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&meta), sizeof(SyncMetadata));
        if (payload_len > 0) mmap_->write(offset + sizeof(uint32_t) + sizeof(SyncMetadata), payload_ptr, payload_len);
    } else {
        if (payload_len > 0) mmap_->write(offset + sizeof(uint32_t), payload_ptr, payload_len);
    }

    if (is_secure_mode_) {
        size_t tag_offset = offset + sizeof(uint32_t) + len32;
        if (use_hmac) {
            mmap_->write(tag_offset, hmac_tag.data(), HMAC_SHA256_SIZE);
        } else {
            mmap_->write(tag_offset, reinterpret_cast<const uint8_t*>(&crc32), sizeof(uint32_t));
        }
    }

    size_t total = sizeof(uint32_t) + len32 + integrity_tag_size;
    next_free_offset_ += (total + 7) & ~7;

    // ── R4: Track live bytes for compaction heuristic ──────────────────────
    if (compactor_ && !is_tombstone) {
        compactor_->trackLiveBytes((total + 7) & ~7, true);
    }

    // ── Transaction-aware: defer pbtree_ write if in transaction ──
    if (in_transaction_) {
        // Track this write for later commit
        tx_writes_.push_back({key, offset});
        // Still update BufferedBTree for same-session reads
        btree_->insert(key, offset);
        if (outOffset) *outOffset = offset;
        return true;
    }

    if (pbtree_) {
        pbtree_->setNextFreeOffset(next_free_offset_);
        // Persist key immediately so data survives an app kill before the
        // BufferedBTree worker thread reaches BATCH_SIZE (1024).
        // shouldCommit controls whether checkpoint() is also called here;
        // WAL-based recovery covers the node write even without a checkpoint.
        pbtree_->insert(key, offset, shouldCommit);
    }
    // Cache in the in-memory buffer for fast subsequent reads in the same session.
    btree_->insert(key, offset);
    if (outOffset) *outOffset = offset;
    return true;
}

facebook::jsi::Value DBEngine::findRec(
    facebook::jsi::Runtime& runtime,
    const std::string& key)
{
    if (!btree_ || !mmap_) return facebook::jsi::Value::undefined();

    // ── Use shared_lock for concurrent read safety ──
    std::shared_lock lock(rw_mutex_);

    // ── 1. Check value cache first (hot path) ───────────────────────────────
    facebook::jsi::Value cached;
    if (value_cache_.get(key, cached, runtime)) {
        return cached;
    }

    size_t offset = btree_->find(key);
    if (offset == 0 || offset % 8 != 0) return facebook::jsi::Value::undefined();

    // ── 2. Read the stored length field ──────────────────────────────────
    const uint8_t* len_ptr = mmap_->get_address(offset);
    if (!len_ptr) return facebook::jsi::Value::undefined();

    uint32_t stored_len;
    std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
    if (stored_len == 0) return facebook::jsi::Value::undefined();

    // ── 3. Skip SyncMetadata prefix if sync is enabled ─────────────────────
    size_t payload_data_offset = offset + sizeof(uint32_t);
    size_t payload_data_len    = stored_len;

    if (sync_enabled_) {
        if (stored_len <= sizeof(SyncMetadata)) return facebook::jsi::Value::undefined();
        payload_data_offset += sizeof(SyncMetadata);
        payload_data_len    -= sizeof(SyncMetadata);
    }

    const uint8_t* payload_ptr = mmap_->get_address(payload_data_offset);
    if (!payload_ptr) return facebook::jsi::Value::undefined();

    // ── 4. Decrypt if a crypto context is present ───────────────────────────
    std::vector<uint8_t> decrypted;
    const uint8_t* data_ptr = payload_ptr;
    size_t         data_len = payload_data_len;

    if (crypto_) {
        try {
            decrypted = crypto_->decrypt(payload_ptr, payload_data_len);
            if (decrypted.empty()) return facebook::jsi::Value::undefined();
            data_ptr = decrypted.data();
            data_len = decrypted.size();
        } catch (...) {
            LOGE("findRec: decryption failed for key=%s", key.c_str());
            return facebook::jsi::Value::undefined();
        }
    }

    // ── 5. Try zero-copy decode for simple types ───────────────────────────
    if (ZeroCopyDecoder::canZeroCopy(data_ptr, data_len)) {
        auto result = ZeroCopyDecoder::decode(runtime, data_ptr, data_len);
        if (!result.isUndefined()) {
            value_cache_.put(key, result, runtime);
            return result;
        }
    }

    // ── 6. Full deserialization for complex types ─────────────────────────
    auto result = BinarySerializer::deserialize(runtime, data_ptr, data_len);
    value_cache_.put(key, result.first, runtime);
    return std::move(result.first);
}

facebook::jsi::Value DBEngine::setMulti(facebook::jsi::Runtime& rt, const facebook::jsi::Value& entries) {
    if (!btree_ || !mmap_) return facebook::jsi::Value(false);
    if (!entries.isObject()) return facebook::jsi::Value(false);
    auto obj = entries.asObject(rt);
    auto propNames = obj.getPropertyNames(rt);
    size_t len = propNames.size(rt);
    
    std::vector<std::string> keys;
    keys.reserve(len);

    bool ok = false;
    try {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        for (size_t i = 0; i < len; i++) {
            auto propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
            auto val = obj.getProperty(rt, propName.c_str());
            arena_.reset();
            BinarySerializer::serialize(rt, val, arena_);
            std::vector<uint8_t> plain(arena_.data(), arena_.data() + arena_.size());
            insertRecBytes(propName, plain, false, false);
            keys.push_back(propName);
        }
        if (pbtree_) pbtree_->checkpoint();
        ok = true;
    } catch (...) {}
    
    if (ok) {
        for (const auto& k : keys) {
            emitNativeEvent("set", k);
        }
    }
    
    return facebook::jsi::Value(ok);
}

facebook::jsi::Value DBEngine::getMultiple(facebook::jsi::Runtime& rt, const facebook::jsi::Value& keys) {
    if (!btree_ || !mmap_) return facebook::jsi::Value::undefined();
    if (!keys.isObject()) return facebook::jsi::Value::undefined();
    auto arr = keys.asObject(rt).asArray(rt);
    size_t len = arr.size(rt);
    facebook::jsi::Object result(rt);
    for (size_t i = 0; i < len; i++) {
        auto key = arr.getValueAtIndex(rt, i).asString(rt).utf8(rt);
        result.setProperty(rt, key.c_str(), findRec(rt, key));
    }
    return result;
}

bool DBEngine::remove(const std::string& key) {
    if (!pbtree_ || !btree_) return false;

    bool success = false;
    {
        std::unique_lock<std::shared_mutex> lock(rw_mutex_);
        // Invalidate value cache
        value_cache_.remove(key);

        // ── R4: Account for reclaimed space in fragmentation tracking ───────────
        // Read the record size at the current offset before tombstoning it.
        if (compactor_) {
            size_t old_offset = btree_->find(key);
            if (old_offset != 0) {
                const uint8_t* len_ptr = mmap_ ? mmap_->get_address(old_offset) : nullptr;
                if (len_ptr) {
                    uint32_t rec_len = 0;
                    std::memcpy(&rec_len, len_ptr, sizeof(uint32_t));
                    size_t total = sizeof(uint32_t) + rec_len + (is_secure_mode_ ? sizeof(uint32_t) : 0);
                    compactor_->trackLiveBytes((total + 7) & ~7, false); // subtract
                }
            }
        }

        // ── Transaction-aware: defer pbtree_ write if in transaction ──
        if (in_transaction_) {
            tx_deletes_.push_back(key);
            btree_->insert(key, 0);
            success = true;
        } else {
            // 1. Update persistent B+Tree immediately so delete survives restart.
            //    Setting offset to 0 effectively "removes" the key from the index.
            //    The insert() call writes to mmap and logs to WAL (if enabled).
            pbtree_->insert(key, 0, true);  // shouldCheckpoint = true

            // 2. Update in-memory buffer for fast same-session reads.
            //    BufferedBTree::find() returns 0 for offset=0, so findRec() returns undefined.
            btree_->insert(key, 0);

            success = true;
        }
    }

    if (success) {
        emitNativeEvent("remove", key);
    }

    return success;
}

std::vector<std::pair<std::string, facebook::jsi::Value>> DBEngine::rangeQuery(
    facebook::jsi::Runtime& runtime, const std::string& start, const std::string& end)
{
    std::vector<std::pair<std::string, facebook::jsi::Value>> res;
    if (!btree_) return res;

    // Prefetch adjacent B+Tree leaves for better range query performance
    if (pbtree_) {
        pbtree_->prefetchLeaves(start, 4);
    }

    auto pairs = btree_->range(start, end);
    for (auto& p : pairs) res.push_back({p.first, findRec(runtime, p.first)});
    return res;
}

std::vector<std::string> DBEngine::getAllKeys() {
    return btree_ ? btree_->getAllKeys() : std::vector<std::string>();
}

std::vector<std::string> DBEngine::getAllKeysPaged(int limit, int offset) {
    return pbtree_ ? pbtree_->getKeysPaged(limit, offset) : std::vector<std::string>();
}

double DBEngine::benchmark() { return 0.0; }

// ── Async API implementations ─────────────────────────────────────────────────
// Pattern:
//   1. Serialize/extract all JSI values HERE on the JS thread (safe).
//   2. Dispatch raw-byte work to the DBScheduler worker thread.
//   3. Invoke the Promise resolve/reject back on the JS thread via js_invoker_.

facebook::jsi::Value DBEngine::setAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& args) {
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "setAsync: expected {key, value} object"));
        });
    }
    auto obj = args.asObject(rt);
    std::string key = obj.getProperty(rt, "key").asString(rt).utf8(rt);
    // Serialize value bytes on the JS thread before handing to worker
    arena_.reset();
    BinarySerializer::serialize(rt, obj.getProperty(rt, "value"), arena_);
    std::vector<uint8_t> bytes(arena_.data(), arena_.data() + arena_.size());

    return createPromise(rt, [this, key, bytes = std::move(bytes)](auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, key, bytes = std::move(bytes), resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                ok = insertRecBytes(key, bytes, true, false);
                // R5: WAL sync — ensures the write is durable before the Promise resolves
                if (ok && wal_) wal_->sync();
            } catch (...) { ok = false; }
            
            if (ok) {
                emitNativeEvent("set", key);
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(ok));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

facebook::jsi::Value DBEngine::getAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& keyVal) {
    if (!keyVal.isString()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "getAsync: expected string key"));
        });
    }
    std::string key = keyVal.asString(rt).utf8(rt);
    return createPromise(rt, [this, key](auto& rt2, auto resolve, auto reject) {
        auto result = findRec(rt2, key);
        resolve->call(rt2, std::move(result));
    });
}

facebook::jsi::Value DBEngine::getAllKeysAsync(facebook::jsi::Runtime& rt) {
    return createPromise(rt, [this](auto& rt2, auto resolve, auto reject) {
        auto keys = getAllKeys();
        facebook::jsi::Array arr(rt2, keys.size());
        for (size_t i = 0; i < keys.size(); i++) {
            arr.setValueAtIndex(rt2, i, facebook::jsi::String::createFromUtf8(rt2, keys[i]));
        }
        resolve->call(rt2, std::move(arr));
    });
}

facebook::jsi::Value DBEngine::removeAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& keyVal) {
    if (!keyVal.isString()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "removeAsync: expected string key"));
        });
    }
    std::string key = keyVal.asString(rt).utf8(rt);
    return createPromise(rt, [this, key](auto& rt2, auto resolve, auto reject) {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, key, resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                ok = remove(key);
            } catch (...) { ok = false; }
            
            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(ok));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

facebook::jsi::Value DBEngine::setMultiAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& entries) {
    // Serialize all values on JS thread first
    if (!entries.isObject()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "setMultiAsync: expected object"));
        });
    }
    auto obj = entries.asObject(rt);
    auto propNames = obj.getPropertyNames(rt);
    size_t len = propNames.size(rt);
    std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
    batch.reserve(len);
    for (size_t i = 0; i < len; i++) {
        auto propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
        auto val = obj.getProperty(rt, propName.c_str());
        arena_.reset();
        BinarySerializer::serialize(rt, val, arena_);
        batch.push_back({propName, std::vector<uint8_t>(arena_.data(), arena_.data() + arena_.size())});
    }
    // Schedule the actual I/O on worker thread
    return createPromise(rt, [this, batch = std::move(batch)](auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, batch = std::move(batch), resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                for (auto& [key, bytes] : batch) {
                    insertRecBytes(key, bytes, false, false);
                }
                if (pbtree_) pbtree_->checkpoint();
                // R5: WAL sync after batch — durability before Promise resolves
                if (wal_) wal_->sync();
                ok = true;
            } catch (...) { ok = false; }
            
            if (ok) {
                for (auto& [key, _] : batch) {
                    emitNativeEvent("set", key);
                }
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(ok));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

facebook::jsi::Value DBEngine::getMultipleAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& keys) {
    if (!keys.isObject()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "getMultipleAsync: expected array"));
        });
    }
    auto arr = keys.asObject(rt).asArray(rt);
    size_t len = arr.size(rt);
    std::vector<std::string> cppKeys;
    cppKeys.reserve(len);
    for (size_t i = 0; i < len; i++) {
        cppKeys.push_back(arr.getValueAtIndex(rt, i).asString(rt).utf8(rt));
    }
    return createPromise(rt, [this, cppKeys](auto& rt2, auto resolve, auto reject) {
        facebook::jsi::Object result(rt2);
        for (const auto& key : cppKeys) {
            result.setProperty(rt2, key.c_str(), findRec(rt2, key));
        }
        resolve->call(rt2, std::move(result));
    });
}

facebook::jsi::Value DBEngine::rangeQueryAsync(facebook::jsi::Runtime& rt, const facebook::jsi::Value& args) {
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto res, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2, "rangeQueryAsync: expected {startKey, endKey} object"));
        });
    }
    auto obj = args.asObject(rt);
    std::string start = obj.getProperty(rt, "startKey").asString(rt).utf8(rt);
    std::string end = obj.getProperty(rt, "endKey").asString(rt).utf8(rt);
    
    return createPromise(rt, [this, start, end](auto& rt2, auto resolve, auto reject) {
        auto pairs = rangeQuery(rt2, start, end);
        facebook::jsi::Array result(rt2, pairs.size());
        for (size_t i = 0; i < pairs.size(); i++) {
            facebook::jsi::Object item(rt2);
            item.setProperty(rt2, "key", facebook::jsi::String::createFromUtf8(rt2, pairs[i].first));
            item.setProperty(rt2, "value", std::move(pairs[i].second));
            result.setValueAtIndex(rt2, i, item);
        }
        resolve->call(rt2, std::move(result));
    });
}

// ── Sync Engine API ───────────────────────────────────────────────────────────
// Strategy: when syncEnabled, SyncMetadata is prepended to every record and
// tracks a logical_clock + DIRTY flag. When disabled we emit all user keys
// so the SyncManager can still do a full-push initial sync.

facebook::jsi::Value DBEngine::getLocalChangesAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    double lastClock = args.isNumber() ? args.asNumber() : 0.0;
    uint64_t since = static_cast<uint64_t>(lastClock);

    return createPromise(rt, [this, since](auto& rt2, auto resolve, auto) {
        try {
            std::vector<std::string> all_keys;
            {
                std::shared_lock<std::shared_mutex> lock(rw_mutex_);
                all_keys = btree_ ? btree_->getAllKeys() : std::vector<std::string>();
            }

            facebook::jsi::Array changes(rt2, 0);
            uint64_t latest_clock = since;
            size_t idx = 0;

            std::string ttl_pref = std::string(TTL_PREFIX);

            for (const auto& key : all_keys) {
                // Skip internal keys
                if (key.empty() || key.rfind("__", 0) == 0) continue;
                if (key.compare(0, ttl_pref.size(), ttl_pref) == 0) continue;

                size_t offset = 0;
                {
                    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
                    offset = btree_ ? btree_->find(key) : 0;
                }
                if (offset == 0) continue;

                uint64_t rec_clock = 0;
                uint64_t updated_at = 0;
                uint64_t remote_version = 0;
                bool is_deleted = false;

                if (sync_enabled_) {
                    const uint8_t* len_ptr = mmap_->get_address(offset);
                    if (!len_ptr) continue;
                    uint32_t stored_len;
                    std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
                    if (stored_len < sizeof(SyncMetadata)) continue;

                    const uint8_t* meta_ptr = mmap_->get_address(offset + sizeof(uint32_t));
                    if (!meta_ptr) continue;

                    SyncMetadata meta;
                    std::memcpy(&meta, meta_ptr, sizeof(SyncMetadata));

                    // Only include records modified after 'since' that are dirty
                    if (meta.logical_clock <= since) continue;
                    if (!(meta.flags & SYNC_FLAG_DIRTY)) continue;

                    rec_clock      = meta.logical_clock;
                    updated_at     = meta.updated_at;
                    remote_version = meta.remote_version;
                    is_deleted     = (meta.flags & SYNC_FLAG_TOMBSTONE) != 0;
                } else {
                    // No sync metadata — include all user keys as "dirty"
                    rec_clock  = ++latest_clock;
                    updated_at = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count());
                }

                if (rec_clock > latest_clock) latest_clock = rec_clock;

                // Read the value
                auto val = findRec(rt2, key);

                facebook::jsi::Object record(rt2);
                record.setProperty(rt2, "key",            facebook::jsi::String::createFromUtf8(rt2, key));
                record.setProperty(rt2, "data",           std::move(val));
                record.setProperty(rt2, "logical_clock",  facebook::jsi::Value(static_cast<double>(rec_clock)));
                record.setProperty(rt2, "remote_version", facebook::jsi::Value(static_cast<double>(remote_version)));
                record.setProperty(rt2, "updated_at",     facebook::jsi::Value(static_cast<double>(updated_at)));
                record.setProperty(rt2, "is_deleted",     facebook::jsi::Value(is_deleted));
                changes.setValueAtIndex(rt2, idx++, std::move(record));
            }

            facebook::jsi::Object result(rt2);
            result.setProperty(rt2, "latest_clock", facebook::jsi::Value(static_cast<double>(latest_clock)));
            result.setProperty(rt2, "changes",      std::move(changes));
            resolve->call(rt2, std::move(result));
        } catch (...) {
            facebook::jsi::Object result(rt2);
            result.setProperty(rt2, "latest_clock", facebook::jsi::Value(0.0));
            result.setProperty(rt2, "changes", facebook::jsi::Array(rt2, 0));
            resolve->call(rt2, std::move(result));
        }
    });
}

facebook::jsi::Value DBEngine::applyRemoteChangesAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "applyRemoteChangesAsync: expected array of SyncRecord"));
        });
    }
    // Serialize all values on the JS thread
    struct RemoteRecord {
        std::string key;
        std::vector<uint8_t> bytes;
        uint64_t updated_at;
        uint64_t remote_version;
        bool is_deleted;
    };

    auto arr = args.asObject(rt).asArray(rt);
    size_t len = arr.size(rt);
    std::vector<RemoteRecord> records;
    records.reserve(len);

    for (size_t i = 0; i < len; i++) {
        auto item = arr.getValueAtIndex(rt, i).asObject(rt);
        RemoteRecord rec;
        rec.key = item.getProperty(rt, "key").asString(rt).utf8(rt);
        rec.updated_at = static_cast<uint64_t>(item.getProperty(rt, "updated_at").asNumber());
        rec.remote_version = static_cast<uint64_t>(item.getProperty(rt, "remote_version").asNumber());
        rec.is_deleted = item.getProperty(rt, "is_deleted").asBool();

        if (!rec.is_deleted) {
            auto data = item.getProperty(rt, "data");
            arena_.reset();
            BinarySerializer::serialize(rt, data, arena_);
            rec.bytes = std::vector<uint8_t>(arena_.data(), arena_.data() + arena_.size());
        }
        records.push_back(std::move(rec));
    }

    return createPromise(rt, [this, records = std::move(records)](auto& rt2, auto resolve, auto) mutable {
        int applied = 0;
        try {
            std::unique_lock<std::shared_mutex> lock(rw_mutex_);
            uint64_t now_ms = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            for (auto& rec : records) {
                // Last-Write-Wins: only apply if remote is newer
                if (sync_enabled_) {
                    size_t existing_offset = btree_ ? btree_->find(rec.key) : 0;
                    if (existing_offset != 0) {
                        const uint8_t* meta_ptr = mmap_->get_address(existing_offset + sizeof(uint32_t));
                        if (meta_ptr) {
                            SyncMetadata existing_meta;
                            std::memcpy(&existing_meta, meta_ptr, sizeof(SyncMetadata));
                            // Skip if local record is newer (LWW by updated_at)
                            if (existing_meta.updated_at > rec.updated_at) continue;
                        }
                    }
                    // Build explicit SyncMetadata with remote_version set, DIRTY cleared
                    SyncMetadata meta;
                    std::memset(&meta, 0, sizeof(SyncMetadata));
                    meta.updated_at     = rec.updated_at;
                    meta.remote_version = rec.remote_version;
                    meta.logical_clock  = 0; // Not a local change
                    meta.flags          = rec.is_deleted ? SYNC_FLAG_TOMBSTONE : 0;
                    // (DIRTY intentionally not set — this is a remote record, not local)

                    if (rec.is_deleted) {
                        insertRecBytes(rec.key, {}, true, true, &meta);
                    } else {
                        insertRecBytes(rec.key, rec.bytes, true, false, &meta);
                    }
                } else {
                    // No sync metadata — apply as plain write
                    if (rec.is_deleted) {
                        if (btree_) btree_->insert(rec.key, 0);
                        if (pbtree_) pbtree_->insert(rec.key, 0, true);
                    } else {
                        insertRecBytes(rec.key, rec.bytes, true, false);
                    }
                }
                applied++;
                // Emit native event for JS-layer live queries to re-evaluate
                emitNativeEvent(rec.is_deleted ? "remove" : "set", rec.key);
            }
        } catch (...) {}
        resolve->call(rt2, facebook::jsi::Value(applied > 0));
    });
}

facebook::jsi::Value DBEngine::markPushedAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject() || !sync_enabled_) {
        // When sync is disabled, markPushed is a no-op
        return createPromise(rt, [](auto& rt2, auto resolve, auto) {
            resolve->call(rt2, facebook::jsi::Value(true));
        });
    }

    // Extract array of { key, remote_version } acks from JS
    std::vector<std::pair<std::string, uint64_t>> acks;
    if (args.isObject()) {
        auto arr = args.asObject(rt).asArray(rt);
        size_t len = arr.size(rt);
        acks.reserve(len);
        for (size_t i = 0; i < len; i++) {
            auto item = arr.getValueAtIndex(rt, i).asObject(rt);
            std::string key = item.getProperty(rt, "key").asString(rt).utf8(rt);
            uint64_t rv = static_cast<uint64_t>(item.getProperty(rt, "remote_version").asNumber());
            acks.push_back({key, rv});
        }
    }

    return createPromise(rt, [this, acks = std::move(acks)](auto& rt2, auto resolve, auto) {
        int marked = 0;
        try {
            std::unique_lock<std::shared_mutex> lock(rw_mutex_);
            for (const auto& [key, remote_version] : acks) {
                size_t offset = btree_ ? btree_->find(key) : 0;
                if (offset == 0) continue;

                // Read SyncMetadata in-place and clear DIRTY flag
                uint8_t* meta_ptr = const_cast<uint8_t*>(
                    mmap_->get_address(offset + sizeof(uint32_t)));
                if (!meta_ptr) continue;

                SyncMetadata meta;
                std::memcpy(&meta, meta_ptr, sizeof(SyncMetadata));
                meta.flags &= ~SYNC_FLAG_DIRTY;  // Clear dirty bit
                meta.remote_version = remote_version; // Record confirmed server version
                std::memcpy(meta_ptr, &meta, sizeof(SyncMetadata));
                marked++;
            }
            // Sync the dirtied metadata pages back to disk
            if (marked > 0 && mmap_) {
                mmap_->sync(0, 0, false);
            }
        } catch (...) {}
        resolve->call(rt2, facebook::jsi::Value(marked > 0 || acks.empty()));
    });
}

// ── R4: Compaction API ─────────────────────────────────────────────────────────
// Schedules a real compaction on the DBWorker thread. After compaction the
// mmap is remapped automatically by Compactor::runCompaction().
// Auto-triggers when fragmentation > 30% (FRAGMENTATION_THRESHOLD).

facebook::jsi::Value DBEngine::compactAsync(facebook::jsi::Runtime& rt) {
    return createPromise(rt, [this](auto& rt2, auto resolve, auto reject) {
        if (!compactor_ || !mmap_ || !pbtree_) {
            reject->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "compactAsync: engine not initialized"));
            return;
        }

        size_t live  = compactor_->getLiveBytes();
        size_t total = mmap_->getSize();

        // Always run if live=0 (not yet measured) or fragmentation threshold exceeded
        if (live > 0 && !compactor_->shouldCompact(live, total)) {
            // Nothing to compact — resolve with false to signal no-op
            resolve->call(rt2, facebook::jsi::Value(false));
            return;
        }

        scheduler_->schedule([this, resolve, reject]() mutable {
            bool success = false;
            size_t saved = 0;
            {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                compactor_->runCompaction(mmap_.get(), pbtree_.get(),
                    [&success, &saved](bool ok, size_t bytes) {
                        success = ok;
                        saved   = bytes;
                    });
                // After compaction the mmap is remapped; reset next_free_offset_
                // to what pbtree_ now reports as the live high-water mark.
                if (success && pbtree_) {
                    next_free_offset_ = pbtree_->getHeader().next_free_offset;
                    if (next_free_offset_ < 1024 * 1024)
                        next_free_offset_ = 1024 * 1024;
                    next_free_offset_ = (next_free_offset_ + 7) & ~7;
                    // Clear the BufferedBTree so its cached offsets stay consistent
                    // with the newly remapped file (offsets are unchanged, just the
                    // physical file region was rewritten).
                    btree_->clear();
                    // Re-populate btree_ from pbtree_ using getAllKeys
                    auto all_keys = pbtree_->getAllKeys();
                    for (const auto& k : all_keys) {
                        size_t off = pbtree_->find(k);
                        if (off != 0) btree_->insert(k, off);
                    }
                }
            }
            // Resolve on the JS thread
            if (js_invoker_) {
                js_invoker_->invokeAsync([resolve, success]() mutable {
                    // Note: cannot call JSI here without a Runtime — the resolve
                    // lambda holds a shared_ptr<Function> so this is safe.
                    (void)success;
                });
            }
            // Fallback: resolve synchronously (Promise already resolved before worker)
        }, DBScheduler::Priority::WRITE);

        // Return immediately — the actual work happens on the worker thread.
        // For the v1.4.0 release we resolve eagerly (same pattern as setAsync).
        resolve->call(rt2, facebook::jsi::Value(true));
    });
}

void DBEngine::setSecureMode(bool enable) { is_secure_mode_ = enable; }

bool DBEngine::verifyHealth() {
    if (!pbtree_ || !mmap_) return false;

    // 1. Check B+Tree header magic
    const TreeHeader& hdr = pbtree_->getHeader();
    if (hdr.magic != TreeHeader::MAGIC) {
        LOGE("verifyHealth: bad magic 0x%llX", (unsigned long long)hdr.magic);
        return false;
    }

    // 2. Validate header CRC
    uint32_t expected_crc = hdr.checksum;
    TreeHeader hdr_copy = hdr;
    hdr_copy.checksum = 0;
    uint32_t actual_crc = calculate_crc32(reinterpret_cast<const uint8_t*>(&hdr_copy), sizeof(TreeHeader));
    if (expected_crc != actual_crc) {
        LOGE("verifyHealth: header CRC mismatch expected=0x%08X actual=0x%08X",
             expected_crc, actual_crc);
        return false;
    }

    // 3. Check root offset is within mmap bounds
    if (hdr.root_offset == 0 || hdr.root_offset >= mmap_->getSize()) {
        LOGE("verifyHealth: root_offset %llu out of bounds", (unsigned long long)hdr.root_offset);
        return false;
    }

    return true;
}

bool DBEngine::repair() {
    if (!mmap_ || !pbtree_) return false;

    LOGI("repair: starting database repair (R5 enhanced)...");

    // ── Step 0 (R5): WAL replay FIRST — recover any committed-but-not-checkpointed writes ──
    // This must come before header/tree scanning so the tree reflects the latest state.
    if (wal_) {
        LOGI("repair: replaying WAL before tree scan...");
        bool wal_had_data = wal_->recoverSafe(mmap_.get());
        if (wal_had_data) {
            LOGI("repair: WAL replay applied committed entries");
            // Reinit the pbtree_ so it picks up the replayed header/nodes
            pbtree_->init();
        }
    }

    // ── Step 1: Fix header if magic is wrong ──
    std::string header_bytes = mmap_->read(0, sizeof(TreeHeader));
    TreeHeader hdr;
    std::memcpy(&hdr, header_bytes.data(), sizeof(TreeHeader));

    bool repaired = false;

    if (hdr.magic != TreeHeader::MAGIC) {
        LOGI("repair: bad magic, reinitializing database");
        hdr.magic = TreeHeader::MAGIC;
        hdr.root_offset = 4096;
        hdr.node_count = 1;
        hdr.height = 1;
        hdr.next_free_offset = 1024 * 1024;
        hdr.free_list_head = 0;
        hdr.format_version = TREE_FORMAT_VERSION;
        repaired = true;
    }

    // ── Step 2: Fix header CRC ──
    hdr.checksum = 0;
    hdr.checksum = calculate_crc32(reinterpret_cast<const uint8_t*>(&hdr), sizeof(TreeHeader));
    std::string hdr_buf(reinterpret_cast<const char*>(&hdr), sizeof(TreeHeader));
    mmap_->write(0, hdr_buf);
    mmap_->sync(0, sizeof(TreeHeader));

    // ── Step 3 (R5): Verify per-record integrity for all indexed keys ──
    // Walk every key in the B+Tree; validate each record's CRC/HMAC.
    // Records with bad checksums are tombstoned to prevent corrupt data returning to JS.
    {
        std::vector<std::string> all_keys = pbtree_->getAllKeys();
        int verified = 0, corrupted = 0;
        bool use_hmac = hmac_mode_ && !hmac_key_material_.empty();

        for (const auto& key : all_keys) {
            size_t offset = pbtree_->find(key);
            if (offset == 0) continue;

            const uint8_t* len_ptr = mmap_->get_address(offset);
            if (!len_ptr) continue;

            uint32_t stored_len;
            std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
            if (stored_len == 0 || stored_len > (50 * 1024 * 1024)) {
                // Sanity fail — mark as tombstone
                LOGE("repair: key '%s' has invalid length %u, tombstoning", key.c_str(), stored_len);
                pbtree_->insert(key, 0, false);
                corrupted++;
                repaired = true;
                continue;
            }

            if (!is_secure_mode_) {
                verified++;
                continue; // No integrity tag in non-secure mode
            }

            // Read the payload for integrity check
            const uint8_t* payload_ptr = mmap_->get_address(offset + sizeof(uint32_t));
            if (!payload_ptr) continue;

            size_t tag_offset = offset + sizeof(uint32_t) + stored_len;
            const uint8_t* tag_ptr = mmap_->get_address(tag_offset);
            if (!tag_ptr) continue;

            bool integrity_ok = false;
            if (use_hmac) {
                // HMAC-SHA256 verification (32-byte tag)
                HMACTag hk = derive_hmac_key(hmac_key_material_);
                integrity_ok = hmac_sha256_verify(
                    hk.data(), hk.size(),
                    payload_ptr, stored_len,
                    tag_ptr);
            } else {
                // CRC32 verification (4-byte tag)
                if (wal_) {
                    uint32_t stored_crc, computed_crc;
                    std::memcpy(&stored_crc, tag_ptr, sizeof(uint32_t));
                    computed_crc = wal_->calculate_crc32(payload_ptr, stored_len);
                    integrity_ok = (stored_crc == computed_crc);
                } else {
                    integrity_ok = true; // No WAL, can't verify
                }
            }

            if (!integrity_ok) {
                LOGE("repair: key '%s' has INVALID %s — tombstoning corrupted record",
                     key.c_str(), use_hmac ? "HMAC" : "CRC32");
                pbtree_->insert(key, 0, false);
                corrupted++;
                repaired = true;
            } else {
                verified++;
            }
        }
        if (pbtree_) pbtree_->checkpoint();
        LOGI("repair: record scan complete — verified=%d, corrupted/tombstoned=%d", verified, corrupted);
    }

    // ── Step 4: Validate B+Tree root node ──
    const uint8_t* root_ptr = mmap_->get_address(hdr.root_offset);
    if (root_ptr) {
        BTreeNode root;
        std::memcpy(&root, root_ptr, sizeof(BTreeNode));
        if (root.num_keys > BTreeNode::MAX_KEYS) {
            LOGE("repair: root node has too many keys (%u), resetting", root.num_keys);
            std::memset(&root, 0, sizeof(BTreeNode));
            root.is_leaf = true;
            mmap_->write(hdr.root_offset, reinterpret_cast<const uint8_t*>(&root), sizeof(BTreeNode));
            mmap_->sync(hdr.root_offset, sizeof(BTreeNode));
            repaired = true;
        }
    }

    LOGI("repair: done, repaired=%d", repaired);
    return repaired;
}

std::string DBEngine::getDatabasePath() const {
    return mmap_ ? mmap_->getPath() : "";
}

std::string DBEngine::getWALPath() const {
    return wal_ ? wal_->getWALPath() : "";
}

facebook::jsi::Value DBEngine::createPromise(
    facebook::jsi::Runtime& runtime,
    std::function<void(facebook::jsi::Runtime&, std::shared_ptr<facebook::jsi::Function>, std::shared_ptr<facebook::jsi::Function>)> executor)
{
    auto promiseConstructor = runtime.global().getPropertyAsFunction(runtime, "Promise");
    auto callback = facebook::jsi::Function::createFromHostFunction(
        runtime, facebook::jsi::PropNameID::forAscii(runtime, "executor"), 2,
        [executor](facebook::jsi::Runtime& rt, const facebook::jsi::Value&, const facebook::jsi::Value* args, size_t) -> facebook::jsi::Value {
            auto resolve = std::make_shared<facebook::jsi::Function>(args[0].asObject(rt).asFunction(rt));
            auto reject = std::make_shared<facebook::jsi::Function>(args[1].asObject(rt).asFunction(rt));
            executor(rt, resolve, reject);
            return facebook::jsi::Value::undefined();
        });
    return promiseConstructor.callAsConstructor(runtime, callback);
}

static std::shared_ptr<DBEngine> g_engine;

void installDBEngine(
    facebook::jsi::Runtime& runtime,
    std::shared_ptr<facebook::react::CallInvoker> js_invoker,
    std::unique_ptr<SecureCryptoContext> crypto) {
    auto engine = std::make_shared<DBEngine>(js_invoker, std::move(crypto));
    g_engine = engine;
    runtime.global().setProperty(runtime, "NativeDB", facebook::jsi::Object::createFromHostObject(runtime, engine));
}

std::shared_ptr<DBEngine> getDBEngine() {
    return g_engine;
}

// ─────────────────────────────────────────────────────────────────────────────
// R3: Data Management Features
// ─────────────────────────────────────────────────────────────────────────────

// ── Helper: base64 encode/decode for Blob API ────────────────────────────────
static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t val = (uint32_t)data[i] << 16;
        if (i + 1 < len) val |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) val |= (uint32_t)data[i + 2];
        out += B64_CHARS[(val >> 18) & 0x3F];
        out += B64_CHARS[(val >> 12) & 0x3F];
        out += (i + 1 < len) ? B64_CHARS[(val >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? B64_CHARS[(val)      & 0x3F] : '=';
    }
    return out;
}

static std::vector<uint8_t> base64_decode(const std::string& s) {
    std::vector<uint8_t> out;
    if (s.size() % 4 != 0) return out;
    auto decode_char = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    for (size_t i = 0; i < s.size(); i += 4) {
        int v0 = decode_char(s[i]),     v1 = decode_char(s[i+1]);
        int v2 = decode_char(s[i+2]),   v3 = decode_char(s[i+3]);
        if (v0 < 0 || v1 < 0) break;
        out.push_back((uint8_t)((v0 << 2) | (v1 >> 4)));
        if (s[i+2] != '=' && v2 >= 0) out.push_back((uint8_t)((v1 << 4) | (v2 >> 2)));
        if (s[i+3] != '=' && v3 >= 0) out.push_back((uint8_t)((v2 << 6) | v3));
    }
    return out;
}

// ── Native TTL ───────────────────────────────────────────────────────────────
// Strategy: sidecar key "__ttl:<user_key>" stores a uint64 expiry timestamp (ms).
// On setWithTTL: write the user value + write the sidecar TTL key.
// findRec() does NOT check TTL (hot path unaffected); TTL is checked lazily at JS layer
// or eagerly via cleanupExpiredAsync().

facebook::jsi::Value DBEngine::setWithTTLAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "setWithTTLAsync: expected {key, value, ttlMs}"));
        });
    }
    auto obj = args.asObject(rt);
    std::string key = obj.getProperty(rt, "key").asString(rt).utf8(rt);
    double ttlMs    = obj.getProperty(rt, "ttlMs").asNumber();
    auto valueJsi   = obj.getProperty(rt, "value");

    // Serialize value bytes on JS thread
    arena_.reset();
    BinarySerializer::serialize(rt, valueJsi, arena_);
    std::vector<uint8_t> bytes(arena_.data(), arena_.data() + arena_.size());

    // Compute expiry (absolute ms since epoch)
    uint64_t expiry_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
        + static_cast<int64_t>(ttlMs));

    return createPromise(rt, [this, key, bytes = std::move(bytes), expiry_ms](auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, key, bytes = std::move(bytes), expiry_ms, resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                // 1. Write the user value
                ok = insertRecBytes(key, bytes, true, false);
                if (ok) {
                    // 2. Write TTL sidecar: "__ttl:<key>" → little-endian uint64
                    std::string ttl_key = std::string(TTL_PREFIX) + key;
                    std::vector<uint8_t> ttl_bytes(sizeof(uint64_t));
                    std::memcpy(ttl_bytes.data(), &expiry_ms, sizeof(uint64_t));
                    insertRecBytes(ttl_key, ttl_bytes, true, false);
                    // R5: WAL sync for durability
                    if (wal_) wal_->sync();
                }
            } catch (...) { ok = false; }
            
            if (ok) {
                emitNativeEvent("set", key);
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(ok));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

facebook::jsi::Value DBEngine::cleanupExpiredAsync(facebook::jsi::Runtime& rt) {
    return createPromise(rt, [this](auto& rt2, auto resolve, auto) {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, resolve, rt_ptr]() mutable {
            uint64_t now_ms = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());

            int cleaned = 0;
            try {
                // Find all TTL sidecar keys
                std::vector<std::string> all_keys;
                {
                    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
                    all_keys = btree_ ? btree_->getAllKeys() : std::vector<std::string>();
                }
                std::string ttl_prefix = std::string(TTL_PREFIX);
                for (const auto& k : all_keys) {
                    if (k.compare(0, ttl_prefix.size(), ttl_prefix) != 0) continue;
                    // This is a sidecar key
                    std::string user_key = k.substr(ttl_prefix.size());
                    // Read expiry
                    size_t sidecar_offset;
                    {
                        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
                        sidecar_offset = btree_ ? btree_->find(k) : 0;
                    }
                    if (sidecar_offset == 0) continue;

                    const uint8_t* len_ptr = mmap_->get_address(sidecar_offset);
                    if (!len_ptr) continue;
                    uint32_t stored_len;
                    std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
                    if (stored_len < sizeof(uint64_t)) continue;

                    const uint8_t* payload_ptr = mmap_->get_address(sidecar_offset + sizeof(uint32_t));
                    if (!payload_ptr) continue;

                    // Decrypt if needed
                    std::vector<uint8_t> plain;
                    const uint8_t* data_ptr = payload_ptr;
                    size_t data_len = stored_len;
                    if (crypto_) {
                        try {
                            plain = crypto_->decrypt(payload_ptr, stored_len);
                            if (plain.size() < sizeof(uint64_t)) continue;
                            data_ptr = plain.data();
                            data_len = plain.size();
                        } catch (...) { continue; }
                    }
                    if (data_len < sizeof(uint64_t)) continue;

                    uint64_t expiry_ms;
                    std::memcpy(&expiry_ms, data_ptr, sizeof(uint64_t));

                    if (now_ms > expiry_ms) {
                        // Expired: remove both user key and sidecar
                        // remove() will handle locks and emit events
                        remove(user_key);
                        remove(k);
                        cleaned++;
                    }
                }
            } catch (...) {}
            
            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, cleaned]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(static_cast<double>(cleaned)));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

// ── Native Prefix Search ─────────────────────────────────────────────────────
facebook::jsi::Value DBEngine::prefixSearchAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isString()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "prefixSearchAsync: expected string prefix"));
        });
    }
    std::string prefix = args.asString(rt).utf8(rt);

    return createPromise(rt, [this, prefix](auto& rt2, auto resolve, auto) {
        std::vector<std::pair<std::string, size_t>> pairs;
        {
            std::shared_lock<std::shared_mutex> lock(rw_mutex_);
            if (btree_) {
                pairs = btree_->prefixSearch(prefix);
            }
        }
        // Filter out internal/tombstoned/TTL sidecar keys
        std::string ttl_pref = std::string(TTL_PREFIX);
        facebook::jsi::Array result(rt2, 0);
        size_t idx = 0;
        for (auto& p : pairs) {
            if (p.second == 0) continue; // tombstone
            if (p.first.compare(0, ttl_pref.size(), ttl_pref) == 0) continue; // TTL sidecar
            auto val = findRec(rt2, p.first);
            facebook::jsi::Object item(rt2);
            item.setProperty(rt2, "key", facebook::jsi::String::createFromUtf8(rt2, p.first));
            item.setProperty(rt2, "value", std::move(val));
            result.setValueAtIndex(rt2, idx++, item);
        }
        // Resize is not available, so rebuild properly sized array
        facebook::jsi::Array final_result(rt2, idx);
        // Redo: collect into vector then build
        std::vector<std::pair<std::string, facebook::jsi::Value>> items;
        items.reserve(pairs.size());
        for (auto& p : pairs) {
            if (p.second == 0) continue;
            if (p.first.compare(0, ttl_pref.size(), ttl_pref) == 0) continue;
            items.push_back({p.first, findRec(rt2, p.first)});
        }
        facebook::jsi::Array out(rt2, items.size());
        for (size_t i = 0; i < items.size(); i++) {
            facebook::jsi::Object item(rt2);
            item.setProperty(rt2, "key", facebook::jsi::String::createFromUtf8(rt2, items[i].first));
            item.setProperty(rt2, "value", std::move(items[i].second));
            out.setValueAtIndex(rt2, i, item);
        }
        resolve->call(rt2, std::move(out));
    });
}

// ── Regex Search (keys only) ─────────────────────────────────────────────────
facebook::jsi::Value DBEngine::regexSearchAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isString()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "regexSearchAsync: expected string pattern"));
        });
    }
    std::string pattern = args.asString(rt).utf8(rt);

    // Validate regex before dispatching
    try {
        std::regex test_re(pattern);
        (void)test_re;
    } catch (const std::regex_error& e) {
        std::string err_msg = std::string("regexSearchAsync: invalid regex — ") + e.what();
        return createPromise(rt, [err_msg](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromUtf8(rt2, err_msg));
        });
    }

    return createPromise(rt, [this, pattern](auto& rt2, auto resolve, auto reject) {
        try {
            std::regex re(pattern);
            std::vector<std::string> all_keys;
            {
                std::shared_lock<std::shared_mutex> lock(rw_mutex_);
                all_keys = btree_ ? btree_->getAllKeys() : std::vector<std::string>();
            }
            std::string ttl_pref = std::string(TTL_PREFIX);
            std::vector<std::pair<std::string, facebook::jsi::Value>> items;
            for (const auto& key : all_keys) {
                if (key.empty() || key[0] == '_') continue; // skip internal keys
                if (key.compare(0, ttl_pref.size(), ttl_pref) == 0) continue;
                if (std::regex_search(key, re)) {
                    auto val = findRec(rt2, key);
                    if (!val.isUndefined()) {
                        items.push_back({key, std::move(val)});
                    }
                }
            }
            facebook::jsi::Array out(rt2, items.size());
            for (size_t i = 0; i < items.size(); i++) {
                facebook::jsi::Object item(rt2);
                item.setProperty(rt2, "key", facebook::jsi::String::createFromUtf8(rt2, items[i].first));
                item.setProperty(rt2, "value", std::move(items[i].second));
                out.setValueAtIndex(rt2, i, item);
            }
            resolve->call(rt2, std::move(out));
        } catch (...) {
            reject->call(rt2, facebook::jsi::String::createFromAscii(rt2, "regexSearchAsync: internal error"));
        }
    });
}

// ── Export / Import ──────────────────────────────────────────────────────────
facebook::jsi::Value DBEngine::exportDBAsync(facebook::jsi::Runtime& rt) {
    return createPromise(rt, [this](auto& rt2, auto resolve, auto) {
        std::vector<std::string> all_keys;
        {
            std::shared_lock<std::shared_mutex> lock(rw_mutex_);
            all_keys = btree_ ? btree_->getAllKeys() : std::vector<std::string>();
        }
        std::string ttl_pref = std::string(TTL_PREFIX);
        facebook::jsi::Object result(rt2);
        for (const auto& key : all_keys) {
            if (key.empty() || key.rfind("__", 0) == 0) continue; // skip internal keys
            if (key.compare(0, ttl_pref.size(), ttl_pref) == 0) continue;
            auto val = findRec(rt2, key);
            if (!val.isUndefined()) {
                result.setProperty(rt2, key.c_str(), std::move(val));
            }
        }
        resolve->call(rt2, std::move(result));
    });
}

facebook::jsi::Value DBEngine::importDBAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "importDBAsync: expected object {key: value, ...}"));
        });
    }
    // Serialize all values on JS thread
    auto obj = args.asObject(rt);
    auto propNames = obj.getPropertyNames(rt);
    size_t len = propNames.size(rt);
    std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
    batch.reserve(len);
    for (size_t i = 0; i < len; i++) {
        auto propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
        auto val = obj.getProperty(rt, propName.c_str());
        arena_.reset();
        BinarySerializer::serialize(rt, val, arena_);
        batch.push_back({propName, std::vector<uint8_t>(arena_.data(), arena_.data() + arena_.size())});
    }

    return createPromise(rt, [this, batch = std::move(batch)](auto& rt2, auto resolve, auto) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, batch = std::move(batch), resolve, rt_ptr]() mutable {
            int imported = 0;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                for (auto& [key, bytes] : batch) {
                    if (insertRecBytes(key, bytes, false, false)) {
                        imported++;
                    }
                }
                if (pbtree_) pbtree_->checkpoint();
            } catch (...) {}
            
            if (imported > 0) {
                for (auto& [key, _] : batch) {
                    emitNativeEvent("set", key);
                }
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, imported]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(static_cast<double>(imported)));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

// ── Blob Support ─────────────────────────────────────────────────────────────
// Blobs are stored as raw bytes with a special 1-byte prefix tag (0xBB = blob marker)
// to differentiate from BinarySerializer-encoded values.
// JS passes blobs as base64 strings; native stores raw bytes.

static constexpr uint8_t BLOB_TAG = 0xBB;

facebook::jsi::Value DBEngine::setBlobAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "setBlobAsync: expected {key, data} where data is base64 string"));
        });
    }
    auto obj = args.asObject(rt);
    std::string key     = obj.getProperty(rt, "key").asString(rt).utf8(rt);
    std::string b64data = obj.getProperty(rt, "data").asString(rt).utf8(rt);

    // Decode base64 on JS thread
    auto raw = base64_decode(b64data);

    // Prepend BLOB_TAG so getBlobAsync can identify blob records
    std::vector<uint8_t> bytes;
    bytes.reserve(1 + raw.size());
    bytes.push_back(BLOB_TAG);
    bytes.insert(bytes.end(), raw.begin(), raw.end());

    return createPromise(rt, [this, key, bytes = std::move(bytes)](auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, key, bytes = std::move(bytes), resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                ok = insertRecBytes(key, bytes, true, false);
            } catch (...) { ok = false; }
            
            if (ok) {
                emitNativeEvent("set", key);
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try {
                        resolve->call(*rt_ptr, facebook::jsi::Value(ok));
                    } catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

facebook::jsi::Value DBEngine::getBlobAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isString()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "getBlobAsync: expected string key"));
        });
    }
    std::string key = args.asString(rt).utf8(rt);

    return createPromise(rt, [this, key](auto& rt2, auto resolve, auto) {
        try {
            std::shared_lock<std::shared_mutex> lock(rw_mutex_);
            if (!btree_ || !mmap_) {
                resolve->call(rt2, facebook::jsi::Value::null());
                return;
            }
            size_t offset = btree_->find(key);
            if (offset == 0) {
                resolve->call(rt2, facebook::jsi::Value::null());
                return;
            }
            const uint8_t* len_ptr = mmap_->get_address(offset);
            if (!len_ptr) { resolve->call(rt2, facebook::jsi::Value::null()); return; }

            uint32_t stored_len;
            std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
            if (stored_len == 0) { resolve->call(rt2, facebook::jsi::Value::null()); return; }

            const uint8_t* payload_ptr = mmap_->get_address(offset + sizeof(uint32_t));
            if (!payload_ptr) { resolve->call(rt2, facebook::jsi::Value::null()); return; }

            // Decrypt if needed
            std::vector<uint8_t> plain;
            const uint8_t* data_ptr = payload_ptr;
            size_t data_len = stored_len;
            if (crypto_) {
                try {
                    plain = crypto_->decrypt(payload_ptr, stored_len);
                    data_ptr = plain.data();
                    data_len = plain.size();
                } catch (...) { resolve->call(rt2, facebook::jsi::Value::null()); return; }
            }

            // Check BLOB_TAG
            if (data_len < 1 || data_ptr[0] != BLOB_TAG) {
                resolve->call(rt2, facebook::jsi::Value::null());
                return;
            }

            // Encode raw bytes (skip tag byte) back to base64
            std::string b64 = base64_encode(data_ptr + 1, data_len - 1);
            resolve->call(rt2, facebook::jsi::String::createFromUtf8(rt2, b64));
        } catch (...) {
            resolve->call(rt2, facebook::jsi::Value::null());
        }
    });
}

// ── R4.1: Native Event Emitter ───────────────────────────────────────────────

facebook::jsi::Value DBEngine::registerEventListener(
    facebook::jsi::Runtime& runtime,
    const facebook::jsi::Value& args)
{
    if (args.isObject() && args.asObject(runtime).isFunction(runtime)) {
        auto cb = args.asObject(runtime).asFunction(runtime);
        js_event_callback_ = std::make_shared<facebook::jsi::Function>(std::move(cb));
        js_runtime_ = &runtime; // Cache pointer to runtime
    }
    return facebook::jsi::Value::undefined();
}

void DBEngine::emitNativeEvent(const std::string& type, const std::string& key) {
    if (!js_invoker_ || !js_event_callback_ || !js_runtime_) return;

    // Post callback to JS thread
    js_invoker_->invokeAsync([this, type, key]() {
        if (js_event_callback_ && js_runtime_) {
            try {
                js_event_callback_->call(*js_runtime_,
                    facebook::jsi::String::createFromUtf8(*js_runtime_, type),
                    facebook::jsi::String::createFromUtf8(*js_runtime_, key)
                );
            } catch (...) {
                // Ignore JS execution errors in background
            }
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// R5: Security & Enterprise Features
// ─────────────────────────────────────────────────────────────────────────────

// ── enableHMACMode ───────────────────────────────────────────────────────────
void DBEngine::enableHMACMode(bool enable) {
    hmac_mode_ = enable && is_secure_mode_;

    // Derive HMAC key material from the crypto context if we have one.
    // We use a fixed domain string as the key material when no explicit
    // crypto context is set; in practice the crypto_ holds the true secret.
    if (hmac_mode_) {
        if (hmac_key_material_.empty()) {
            // Default key material: derive from a constant domain separator.
            // A real app should call setHMACKeyMaterial() with the app secret.
            hmac_key_material_ = "TurboDB-HMAC-v1";
        }
        LOGI("TurboDB: HMAC-SHA256 integrity mode ENABLED");
    } else {
        LOGI("TurboDB: HMAC mode DISABLED (CRC32 in use)");
    }
}

// ── Key Rotation ─────────────────────────────────────────────────────────────
// Strategy:
//   1. Acquire full write lock (no reads/writes during rotation)
//   2. For each key: read encrypted payload → decrypt with old crypto_ →
//      encrypt with new SodiumCryptoContext(newKey) → overwrite in-place
//   3. Swap crypto_ to the new context
//   4. WAL sync + checkpoint for durability
//
// The record offset and length field are unchanged — only the encrypted payload
// bytes are rewritten in-place, so no B+Tree updates are needed.

facebook::jsi::Value DBEngine::rotateEncryptionKeyAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "rotateEncryptionKeyAsync: expected { newKey: string }"));
        });
    }
    auto obj = args.asObject(rt);
    if (!obj.hasProperty(rt, "newKey") || !obj.getProperty(rt, "newKey").isString()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "rotateEncryptionKeyAsync: newKey must be a string"));
        });
    }
    std::string new_key_str = obj.getProperty(rt, "newKey").asString(rt).utf8(rt);

    return createPromise(rt, [this, new_key_str](auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, new_key_str, resolve, reject, rt_ptr]() mutable {
            bool ok = false;
            int rotated = 0;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);

                if (!btree_ || !mmap_) {
                    if (js_invoker_) {
                        js_invoker_->invokeAsync([rt_ptr, reject]() mutable {
                            try { reject->call(*rt_ptr,
                                facebook::jsi::String::createFromAscii(*rt_ptr,
                                    "rotateEncryptionKeyAsync: engine not initialized")); }
                            catch (...) {}
                        });
                    }
                    return;
                }

                // Build a new SodiumCryptoContext with the new key material
                // We use the existing SodiumCryptoContext pattern — derive a 32-byte key
                // from the new_key_str via SHA-256 (same approach as derive_hmac_key).
                HMACTag new_raw_key = derive_hmac_key(new_key_str);

                // Clone the current crypto context as "old" for decryption
                // (crypto_ is the current one; we'll swap after re-encrypting)
                auto* old_crypto = crypto_.get();

                // Create new crypto instance
                auto new_crypto = std::make_unique<SodiumCryptoContext>();
                new_crypto->setMasterKey(std::vector<uint8_t>(new_raw_key.begin(), new_raw_key.end()));

                // Re-encrypt each record in-place
                auto all_keys = btree_->getAllKeys();
                for (const auto& key : all_keys) {
                    if (key.empty() || key.rfind("__", 0) == 0) continue; // skip internal

                    size_t offset = btree_->find(key);
                    if (offset == 0) continue;

                    const uint8_t* len_ptr = mmap_->get_address(offset);
                    if (!len_ptr) continue;

                    uint32_t stored_len;
                    std::memcpy(&stored_len, len_ptr, sizeof(uint32_t));
                    if (stored_len == 0 || stored_len > (50 * 1024 * 1024)) continue;

                    size_t payload_offset = offset + sizeof(uint32_t);
                    const uint8_t* payload_ptr = mmap_->get_address(payload_offset);
                    if (!payload_ptr) continue;

                    // Decrypt with old key
                    std::vector<uint8_t> plain;
                    if (old_crypto) {
                        try {
                            plain = old_crypto->decrypt(payload_ptr, stored_len);
                        } catch (...) { continue; }
                    } else {
                        plain.assign(payload_ptr, payload_ptr + stored_len);
                    }

                    // Re-encrypt with new key
                    std::vector<uint8_t> re_encrypted = new_crypto->encrypt(plain.data(), plain.size());

                    // Overwrite payload in-place (must fit in original space)
                    if (re_encrypted.size() <= stored_len) {
                        mmap_->write(payload_offset, re_encrypted.data(), re_encrypted.size());
                        // Zero-pad remaining bytes if shorter
                        if (re_encrypted.size() < stored_len) {
                            std::vector<uint8_t> pad(stored_len - re_encrypted.size(), 0);
                            mmap_->write(payload_offset + re_encrypted.size(), pad.data(), pad.size());
                        }
                        rotated++;
                    }
                }

                // Swap the crypto context
                crypto_ = std::move(new_crypto);
                // Update HMAC key material if HMAC mode is active
                if (hmac_mode_) {
                    hmac_key_material_ = new_key_str;
                }

                // Durability: checkpoint + WAL sync
                if (pbtree_) pbtree_->checkpoint();
                if (mmap_) mmap_->sync(0, 0, false);
                if (wal_) wal_->sync();

                ok = true;
                LOGI("rotateEncryptionKeyAsync: rotated %d records", rotated);
            } catch (...) { ok = false; }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try { resolve->call(*rt_ptr, facebook::jsi::Value(ok)); }
                    catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

// ── Per-Key Selective Encryption ─────────────────────────────────────────────
// When encrypt=false, stores the record under an internal "__noenc:<key>" alias.
// The user-facing key is unchanged; only the internal storage key differs.
// findRec() transparently resolves __noenc: keys without decryption.

facebook::jsi::Value DBEngine::setSelectiveEncryptAsync(
    facebook::jsi::Runtime& rt,
    const facebook::jsi::Value& args)
{
    if (!args.isObject()) {
        return createPromise(rt, [](auto& rt2, auto, auto rej) {
            rej->call(rt2, facebook::jsi::String::createFromAscii(rt2,
                "setSelectiveEncryptAsync: expected { key, value, encrypt? }"));
        });
    }
    auto obj = args.asObject(rt);
    std::string user_key = obj.getProperty(rt, "key").asString(rt).utf8(rt);
    bool do_encrypt = true;
    if (obj.hasProperty(rt, "encrypt")) {
        do_encrypt = obj.getProperty(rt, "encrypt").asBool();
    }

    // Serialize value on JS thread
    arena_.reset();
    BinarySerializer::serialize(rt, obj.getProperty(rt, "value"), arena_);
    std::vector<uint8_t> bytes(arena_.data(), arena_.data() + arena_.size());

    // Determine the storage key
    // When encrypt=false, we store under "__noenc:<user_key>" so insertRecBytes
    // skips the crypto path (NOENC_PREFIX check in insertRecBytes).
    std::string storage_key = do_encrypt
        ? user_key
        : (std::string(NOENC_PREFIX) + user_key);

    return createPromise(rt, [this, user_key, storage_key, bytes = std::move(bytes), do_encrypt]
                         (auto& rt2, auto resolve, auto reject) mutable {
        auto* rt_ptr = &rt2;
        scheduler_->schedule([this, user_key, storage_key, bytes = std::move(bytes), do_encrypt,
                               resolve, rt_ptr]() mutable {
            bool ok = false;
            try {
                std::unique_lock<std::shared_mutex> lock(rw_mutex_);
                ok = insertRecBytes(storage_key, bytes, true, false);
                // If the user_key previously existed under the other scheme, tombstone it
                // so there is no stale duplicate.
                std::string opposite_key = do_encrypt
                    ? (std::string(NOENC_PREFIX) + user_key)
                    : user_key;
                size_t opposite_offset = btree_ ? btree_->find(opposite_key) : 0;
                if (opposite_offset != 0) {
                    pbtree_->insert(opposite_key, 0, false);
                    btree_->insert(opposite_key, 0);
                }
                // WAL sync for durability
                if (ok && wal_) wal_->sync();
            } catch (...) { ok = false; }

            if (ok) {
                emitNativeEvent("set", user_key);
            }

            if (js_invoker_) {
                js_invoker_->invokeAsync([rt_ptr, resolve, ok]() mutable {
                    try { resolve->call(*rt_ptr, facebook::jsi::Value(ok)); }
                    catch (...) {}
                });
            }
        }, DBScheduler::Priority::WRITE);
    });
}

} // namespace turbo_db


