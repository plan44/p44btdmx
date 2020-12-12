//
//  p44btdmx.h
//  BeaconTest
//
//  Created by Lukas Zeller on 09.12.20.
//

#ifndef p44btdmx_h
#define p44btdmx_h

#import <Foundation/Foundation.h>

@interface P44BTDMX : NSObject
{
}

- (id)init;
- (void)setChannel:(int)aChannel toValue:(UInt8)aValue;
- (UInt8)getChannel:(int)aChannel;
- (NSData *)advertisementData;

@end // NSObject


#endif /* p44btdmx_h */
