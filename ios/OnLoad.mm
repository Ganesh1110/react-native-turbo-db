#import <Foundation/Foundation.h>
#import "SecureDBImpl.h"
#import <ReactCommon/CxxTurboModuleUtils.h>

@interface SecureDBOnLoad : NSObject
@end

@implementation SecureDBOnLoad

using namespace facebook::react;

+ (void)load
{
  registerCxxModuleToGlobalModuleMap(
    std::string(NativeSecureDBCxxSpec<SecureDBImpl>::kModuleName),
    [](std::shared_ptr<CallInvoker> jsInvoker) {
      return std::make_shared<SecureDBImpl>(jsInvoker);
    }
  );
}

@end
