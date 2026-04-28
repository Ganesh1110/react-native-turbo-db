#import <Foundation/Foundation.h>
#import <React/RCTBridgeModule.h>
#import "TurboDBImpl.h"

#define TURBODB_LOG(fmt, ...) NSLog(@"[TurboDB] " fmt, ##__VA_ARGS__)

@interface TurboDBInstaller : NSObject <RCTBridgeModule>
@end

@implementation TurboDBInstaller

RCT_EXPORT_MODULE(TurboDBInstaller);


RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install)
{
  TURBODB_LOG("TurboDBInstaller: install called");
  return @true;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(getDocumentDirectory)
{
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  return [paths firstObject];
}

@end
