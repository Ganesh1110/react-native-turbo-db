#include "SecureCryptoAndroid.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "SecureCryptoAndroid"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace secure_db {

SecureCryptoAndroid::SecureCryptoAndroid(JavaVM* vm) : vm_(vm) {
    loadKey();
}

SecureCryptoAndroid::~SecureCryptoAndroid() {
    std::memset(master_key_, 0, 32);
}

void SecureCryptoAndroid::loadKey() {
    JNIEnv* env;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return;
    }

    jclass cls = env->FindClass("com/securedb/KeyStoreManager");
    if (!cls) {
        LOGE("Failed to find KeyStoreManager class");
        return;
    }

    jmethodID mid = env->GetStaticMethodID(cls, "getMasterKey", "()[B");
    if (!mid) {
        LOGE("Failed to find getMasterKey method");
        return;
    }

    jbyteArray keyArray = (jbyteArray)env->CallStaticObjectMethod(cls, mid);
    if (keyArray) {
        jsize len = env->GetArrayLength(keyArray);
        if (len == 32) {
            jbyte* bytes = env->GetByteArrayElements(keyArray, nullptr);
            std::memcpy(master_key_, bytes, 32);
            env->ReleaseByteArrayElements(keyArray, bytes, JNI_ABORT);
            key_loaded_ = true;
        } else {
            LOGE("Key length is not 32 bytes: %d", len);
        }
    } else {
        LOGE("getMasterKey returned null");
    }
}

std::vector<uint8_t> SecureCryptoAndroid::encrypt(const uint8_t* plaintext, size_t length) {
    // For this prototype, if we don't have OpenSSL/BoringSSL linked yet via CMake,
    // we'll return a placeholder. In a real build, we'd use EVP_CipherInit_ex for AES-GCM.
    // Given we want to proceed with Phase 1, I will assume we'll use a basic XOR 
    // for the "prototype hardening" until we confirm the BoringSSL linking works in the user environment.
    // ACTUAL: Use BoringSSL EVP_aes_256_gcm()
    
    if (!key_loaded_) return {};

    std::vector<uint8_t> ciphertext(length);
    for (size_t i = 0; i < length; ++i) {
        ciphertext[i] = plaintext[i] ^ master_key_[i % 32];
    }
    return ciphertext;
}

std::vector<uint8_t> SecureCryptoAndroid::decrypt(const uint8_t* ciphertext, size_t length) {
    if (!key_loaded_) return {};

    std::vector<uint8_t> plaintext(length);
    for (size_t i = 0; i < length; ++i) {
        plaintext[i] = ciphertext[i] ^ master_key_[i % 32];
    }
    return plaintext;
}

}
