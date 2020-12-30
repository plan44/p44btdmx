//
//  p44btdmx.mm
//  BeaconTest
//
//  Created by Lukas Zeller on 09.12.20.
//

#include "p44btdmx.h"
#include "p44btdmx.hpp"
#include "utils.hpp"

using namespace p44;

@interface P44BTDMX ()
{
  P44BTDMXsenderPtr dmxSender;
}

@end


@implementation P44BTDMX

- (id)init
{
  if ((self = [super init])) {
    SETLOGLEVEL(LOG_NOTICE);
    dmxSender = P44BTDMXsenderPtr(new P44BTDMXsender);
    dmxSender->setInitialRepeatCount(3); // no repetitions
  }
  return self;
}


- (void)setSystemKey:(NSString*)aSystemKey
{
  string k;
  k.assign([aSystemKey cStringUsingEncoding:NSUTF8StringEncoding]);
  dmxSender->setSystemKey(k);
}


- (void)setRefreshUniverse:(bool)aRefreshUniverse
{
  dmxSender->setRefreshUniverse(aRefreshUniverse);
}


- (void)reset
{
  dmxSender->reset();
}


- (void)setChannel:(int)aChannel toValue:(UInt8)aValue
{
  dmxSender->setChannel(aChannel, aValue);
}


- (UInt8)getChannel:(int)aChannel
{
  return dmxSender->getChannel(aChannel);
}



- (void)setChannelsHex:(NSString*)aChannelsHex
{
  string channels = hexToBinaryString([aChannelsHex cStringUsingEncoding:NSUTF8StringEncoding], false, 512);
  for (int i=0; i<P44BTDMXsender::cUniverseSize; i++) {
    if (i<channels.size()) {
      dmxSender->setChannel(i, (uint8_t)channels[i]);
    }
    else {
      dmxSender->setChannel(i, 0);
    }
  }
}


- (NSString *)getChannelsHex
{
  string channels;
  for (int i=0; i<P44BTDMXsender::cUniverseSize; i++) {
    channels.append(1, dmxSender->getChannel(i));
  }
  return [NSString stringWithCString:binaryToHexString(channels).c_str() encoding:NSUTF8StringEncoding];
}



- (NSData *)iBeaconAdvertisementData
{
  string data = dmxSender->generateP44BTDMXpayload(21);
  return [NSData dataWithBytes:data.c_str() length:data.size()];
}



@end

