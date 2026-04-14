#include "SecureDBImpl.h"
#include "DBEngine.h"

#ifdef __APPLE__
#include "../ios/SecureCryptoIOS.h"
#endif

namespace facebook::react {

SecureDBImpl::SecureDBImpl(
  std::shared_ptr<CallInvoker> jsInvoker
)
  : NativeSecureDBCxxSpec(std::move(jsInvoker)) {
}

void SecureDBImpl::install(jsi::Runtime& rt) {
#ifdef __APPLE__
    std::unique_ptr<secure_db::SecureCryptoContext> crypto = std::make_unique<secure_db::SecureCryptoIOS>();
#else
    std::unique_ptr<secure_db::SecureCryptoContext> crypto = nullptr;
#endif

    secure_db::installDBEngine(rt, std::move(crypto));
}

std::string SecureDBImpl::getVersion(jsi::Runtime& rt) {
    return "1.0.0";
}

}