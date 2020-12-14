//
//  AppDelegate.m
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import "AppDelegate.h"

// MARK: - P44BTDMXManager

@interface P44BTDMXManager()
{
  CBCentralManager* mCentralManager;
  CBPeripheralManager* mPeripheralManager;
  P44BTDMX* mP44BTDMX;
}
@end

@implementation P44BTDMXManager

- (id)init
{
  if ((self = [super init])) {
    // init defaults
    [[NSUserDefaults standardUserDefaults] registerDefaults: @{
      @"p44BtDMXsystemKey": @""
    }];
    // init BT
    mCentralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    mPeripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:nil];
    // init P44BTDMX
    mP44BTDMX = [[P44BTDMX alloc] init];
  }
  return self;
}

- (P44BTDMX *)p44BTDMX
{
  return mP44BTDMX;
}


- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
  NSString* logMsg;

  switch (central.state) {
    case CBManagerStatePoweredOff:
      logMsg = @"powered off";
      break;
    case CBManagerStatePoweredOn:
      logMsg = @"powered on";
      break;
    case CBManagerStateResetting:
      logMsg = @"resetting";
      break;
    case CBManagerStateUnauthorized:
      logMsg = @"unauthorized";
      break;
    case CBManagerStateUnknown:
      logMsg = @"in unknown state";
      break;
    default:
      logMsg = @"unknown";
      break;
  }
  NSLog(@"BLE state is: %@", logMsg);
}


- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheral
{
  NSLog(@"peripheralManagerDidUpdateState");
}


- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral error:(NSError *)error
{
  if (error) {
    NSLog(@"peripheralManagerDidStartAdvertising error: %@", [error description]);
  }
}


- (void)advertiseIBeaconData:(NSData *)aIBeaconData
{
  [mPeripheralManager stopAdvertising];
  [mPeripheralManager startAdvertising:@{
    @"kCBAdvDataAppleBeaconKey": aIBeaconData
  }];
}


- (void)nextBeacon
{
  NSData* advData = [mP44BTDMX iBeaconAdvertisementData];
  if ([advData length]>0) {
    [self advertiseIBeaconData:advData];
  }
  else {
    [mPeripheralManager stopAdvertising];
  }
  [self performSelector:@selector(nextBeacon) withObject:nil afterDelay:0.1];
}



- (void)stopBroadcast
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [mPeripheralManager stopAdvertising];
  [mP44BTDMX reset];
}


- (void)startBroadcast
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [self performSelector:@selector(nextBeacon) withObject:nil afterDelay:0.1];
}

@end



// MARK: - AppDelegate

@interface AppDelegate ()
{
  P44BTDMXManager* mP44BTDMXManager;
}

@end

@implementation AppDelegate

+ (AppDelegate *)sharedAppDelegate
{
  return (AppDelegate *)[UIApplication sharedApplication].delegate;
}


- (P44BTDMXManager *)p44BTDMXManager
{
  return mP44BTDMXManager;
}


- (void)readSystemKey
{
  [mP44BTDMXManager.p44BTDMX setSystemKey:[[NSUserDefaults standardUserDefaults] objectForKey:@"p44BtDMXsystemKey"]];
}



- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  mP44BTDMXManager = [[P44BTDMXManager alloc] init];
  [self readSystemKey];
  // set system key from userdefaults
  return YES;
}


#pragma mark - UISceneSession lifecycle


- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession options:(UISceneConnectionOptions *)options
{
  // Called when a new scene session is being created.
  // Use this method to select a configuration to create the new scene with.
  return [[UISceneConfiguration alloc] initWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
}


- (void)application:(UIApplication *)application didDiscardSceneSessions:(NSSet<UISceneSession *> *)sceneSessions
{
  // Called when the user discards a scene session.
  // If any sessions were discarded while the application was not running, this will be called shortly after application:didFinishLaunchingWithOptions.
  // Use this method to release any resources that were specific to the discarded scenes, as they will not return.
}


@end
