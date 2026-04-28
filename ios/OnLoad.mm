#import <Foundation/Foundation.h>
#import "TurboDBImpl.h"
#import <ReactCommon/CxxTurboModuleUtils.h>

#define TURBODB_LOG(fmt, ...) NSLog(@"[TurboDB] " fmt, ##__VA_ARGS__)

@interface TurboDBOnLoad : NSObject
@end

@implementation TurboDBOnLoad

using namespace facebook::react;

+ (void)load
{
  TURBODB_LOG("OnLoad: +load called - STARTING MODULE REGISTRATION");
  registerCxxModuleToGlobalModuleMap(
    std::string(NativeTurboDBCxxSpec<TurboDBImpl>::kModuleName),
    [](std::shared_ptr<CallInvoker> jsInvoker) {
      TURBODB_LOG("OnLoad: creating TurboDBImpl instance");
      return std::make_shared<TurboDBImpl>(jsInvoker);
    }
  );
  TURBODB_LOG("OnLoad: module registered successfully");
}

@end
