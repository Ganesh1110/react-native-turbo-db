#pragma once

#include <SecureDBSpecJSI.h>

#include <memory>

namespace facebook::react {

class SecureDBImpl
  : public NativeSecureDBCxxSpec<SecureDBImpl> {
public:
  SecureDBImpl(std::shared_ptr<CallInvoker> jsInvoker);

  void install(jsi::Runtime& rt);
  std::string getVersion(jsi::Runtime& rt);
};

}
