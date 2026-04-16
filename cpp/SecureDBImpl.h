#pragma once

#if __has_include(<SecureDBSpecJSI.h>)
#include <SecureDBSpecJSI.h>
#elif __has_include("SecureDBSpecJSI.h")
#include "SecureDBSpecJSI.h"
#endif

#include <memory>

namespace facebook::react {

class SecureDBImpl
  : public NativeSecureDBCxxSpec<SecureDBImpl> {
public:
  SecureDBImpl(std::shared_ptr<CallInvoker> jsInvoker);

  void install(jsi::Runtime& rt);
  std::string getDocumentsDirectory(jsi::Runtime& rt);
  std::string getVersion(jsi::Runtime& rt);
};

}
