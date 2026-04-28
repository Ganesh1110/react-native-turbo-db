#include "TurboDBImpl.h"
#include "DBEngine.h"
#include "SodiumCryptoContext.h"
#include <vector>

#if __has_include(<TurboDBSpecJSI.h>)
#include <TurboDBSpecJSI.h>
#elif __has_include("TurboDBSpecJSI.h")
#include "TurboDBSpecJSI.h"
#endif

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#include "KeyManagerIOS.h"
#include "PlatformUtilsIOS.h"
#endif

#ifdef DEBUG
#define TURBODB_LOG(fmt, ...) NSLog(@"[TurboDB] " fmt, ##__VA_ARGS__)
#else
#define TURBODB_LOG(fmt, ...) do {} while(0)
#endif

namespace facebook::react {

TurboDBImpl::TurboDBImpl(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeTurboDBCxxSpec<TurboDBImpl>(std::move(jsInvoker)) {
    TURBODB_LOG("TurboDBImpl: CONSTRUCTOR CALLED");
}

bool TurboDBImpl::install(jsi::Runtime& rt) {
    TURBODB_LOG("install: START");
    TURBODB_LOG("install: runtime=%p", &rt);
    
    if (&rt == nullptr) {
        TURBODB_LOG("install: ERROR - runtime is null!");
        return false;
    }
    
    TURBODB_LOG("install: runtime is valid");
    auto js_invoker = this->jsInvoker_;
    
#ifdef __APPLE__
    @try {
        TURBODB_LOG("install: getting docs dir");
        std::string docsDir = getDocumentsDirectory(rt);
        TURBODB_LOG("install: docsDir=%s", docsDir.c_str());
        
#if TARGET_IPHONE_SIMULATOR
        TURBODB_LOG("install: using fallback key");
        std::vector<uint8_t> masterKey(32, 0);
        for (int i = 0; i < 32; i++) masterKey[i] = (uint8_t)(i * 7 + 13);
#else
        TURBODB_LOG("install: getting key");
        auto masterKey = turbo_db::KeyManagerIOS::getOrGenerateMasterKey("TurboDBMasterKey");
#endif
        
        TURBODB_LOG("install: creating crypto");
        auto crypto = std::make_unique<turbo_db::SodiumCryptoContext>();
        crypto->setMasterKey(masterKey);
        
        TURBODB_LOG("install: calling installDBEngine");
        turbo_db::installDBEngine(rt, js_invoker, std::move(crypto));
        
        if (rt.global().hasProperty(rt, "NativeDB")) {
            TURBODB_LOG("install: SUCCESS");
            return true;
        } else {
            TURBODB_LOG("install: WARNING - NativeDB not found");
            return true;
        }
    } @catch (NSException *exception) {
        TURBODB_LOG("install: NSException - %@", exception.reason);
        return false;
    } @catch (id exception) {
        TURBODB_LOG("install: ObjC exception - %@", exception);
        return false;
    }
#else
    TURBODB_LOG("install: not Apple");
#endif
    return true;
}

std::string TurboDBImpl::getDocumentsDirectory(jsi::Runtime& rt) {
#ifdef __APPLE__
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *basePath = [paths firstObject];
    if (basePath == nil) {
        return "";
    }
    return [basePath UTF8String];
#else
    return "";
#endif
}

bool TurboDBImpl::isInitialized(jsi::Runtime& rt) {
    return rt.global().hasProperty(rt, "NativeDB");
}

std::string TurboDBImpl::getVersion(jsi::Runtime& rt) {
    return "0.1.0";
}

}