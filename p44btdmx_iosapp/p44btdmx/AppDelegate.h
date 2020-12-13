//
//  AppDelegate.h
//  p44btdmx
//
//  Created by Lukas Zeller on 12.12.20.
//

#import <UIKit/UIKit.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import "p44btdmx.h"

@interface P44BTDMXManager : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate, CBPeripheralManagerDelegate>
@property (readonly, nonatomic) P44BTDMX* p44BTDMX;
- (void)advertiseIBeaconData:(NSData *)aIBeaconData;
- (void)stopBroadcast;
- (void)startBroadcast;
@end


@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (readonly, nonatomic) P44BTDMXManager* p44BTDMXManager;

// convenience method to grab the typed AppDelegate
+ (AppDelegate *)sharedAppDelegate;

@end


