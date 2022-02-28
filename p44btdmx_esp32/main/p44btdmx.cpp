//
//  Copyright (c) 2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 7

#include "p44btdmx.hpp"

#ifdef CONFIG_P44BTDMX_SYSTEM_KEY
  #define DEFAULT_P44BTDMX_SYSTEM_KEY_INPUT CONFIG_P44BTDMX_SYSTEM_KEY
#else
  #define DEFAULT_P44BTDMX_SYSTEM_KEY_INPUT ""
#endif


using namespace p44;


// MARK: - base class for sender and receiver



P44BTDMXbase::P44BTDMXbase()
{
  setSystemKey(DEFAULT_P44BTDMX_SYSTEM_KEY_INPUT); // use default
}


//                                   01234567090123456709012345670901
#define DEFAULT_P44BTDMX_SYSTEM_KEY "NothingGreatButBetterThanNothing"

void P44BTDMXbase::setSystemKey(const string aSystemKeyUserInput)
{
  if (aSystemKeyUserInput.empty()) {
    mSystemKey = DEFAULT_P44BTDMX_SYSTEM_KEY;
  }
  else if (aSystemKeyUserInput.size()>=64){
    // read as hex string
    mSystemKey = hexToBinaryString(aSystemKeyUserInput.c_str(), true, 32);
  }
  else {
    // just use literally
    mSystemKey = aSystemKeyUserInput;
  }
}


uint8_t P44BTDMXbase::systemKeyByte(int aIndex)
{
  if (aIndex>=mSystemKey.size()) return 0x42;
  return mSystemKey[aIndex];
}


// CCITT 16 bit CRC
uint16_t P44BTDMXbase::crc16(uint16_t aCRC16, uint8_t aByteToAdd)
{
  uint16_t s;

  s = (aByteToAdd ^ aCRC16) & 0xff;
  s = s ^ (s << 4);
  s = (aCRC16 >> 8) ^ (s << 8) ^ (s << 3) ^ (s >> 4);
  return s & 0xffff;
}


// MARK: - plan44 DMX over Bluetooth receiver

P44BTDMXreceiver::P44BTDMXreceiver() :
  mFirstLightNumber(0),
  mlastNativeData(Never),
  mIsLogger(false)
{
}

P44BTDMXreceiver::~P44BTDMXreceiver()
{
}


// Note: Apple iBeacons use a "manufacturer specific data" structure, too:
// - they start with a "flags" AD structure (3 bytes) with "LE General Discoverable Mode” and “BR/EDR Not Supported“ set = 0x06
// - then a length=0x1A==26 + type=0xFF "manufactures specific data" packet -> payload length = 25
// - with 2 bytes ci=0x004C==Apple
// - with 1 byte specifying subtype==0x02 (eBeacon)
// - with 1 byte specifying subtype lenngth==0x15==21 -> 21 byte payload
// - 16 bytes proximity UUID
// - then 2 bytes major, 2 bytes minor (differentiating multiple beacons in one installation with same UUID)
// - then 1 byte measured Tx power
// - total AdvData = 30 bytes

// For p44DMX we define two "carriers"
// 1) to be able to use an iPhone for controlling w/o a DMX box:
//    - use the iBeacons payload = 21 bytes p44DMX data
// 2) for max data and BT specs conformant use on stage (with the DMX box)
//    - use a Bluekitchen (later: plan44) manufacturer specific data packet, no flags = 27 bytes payload
//    - use first byte as a subtype (of Bluekitchen/plan44 manufacturer specific packets)
//      - 0x44 = subtype p44DMX
//    - rest of payload == 26 bytes p44DMX data

// 4C 00 02 15 B1 6F C6 BB D1 D1 42 8A 8C 03 55 BA D7 F7 04 81 00 FE FE 00 C5

#define BT_COMPANY_ID_APPLE 0x004C
#define BT_COMPANY_ID_PLAN44 0x4444
#define BT_COMPANY_ID_BLUEKITCHEN 0x048F

#define PLAN44_SUBTYPE_P44BTDMX 0x44
#define APPLE_SUBTYPE_IBEACON 0x02

bool P44BTDMXreceiver::processBTAdvMfgData(const string aAdvMfgData)
{
  FOCUSLOG("Got advMfgData: %s", binaryToHexString(aAdvMfgData,' ').c_str());
  // check if its one of our recognized formats
  if (aAdvMfgData.size()<4) return false;
  uint16_t companyBTId = aAdvMfgData[0]+(aAdvMfgData[1]<<8);
  if (companyBTId==BT_COMPANY_ID_PLAN44 || companyBTId==BT_COMPANY_ID_BLUEKITCHEN) {
    // raw p44BTDMX
    if (aAdvMfgData[2]==PLAN44_SUBTYPE_P44BTDMX) {
      return processP44BTDMXpayload(aAdvMfgData.substr(3), true);
    }
  }
  if (companyBTId==BT_COMPANY_ID_APPLE) {
    // check for p44BTDMX disguised as Apple iBeacon
    if (aAdvMfgData[2]==APPLE_SUBTYPE_IBEACON) {
      return processP44BTDMXpayload(aAdvMfgData.substr(4,(size_t)aAdvMfgData[3]), false);
    }
  }
  return false;
}


// p44DMX data format (21..27 bytes)
// - pairing based on a "system key" = 32 bytes random key
// - key is xored with the payload to obfuscate it
// - last two bytes of the payload are a CRC16 of the bytes preceeding them
// - this leaves 21-4..27-4 = 17..23 effective p44DMX data bytes
// - p44DMX data consists of delta update commands

#define NOT_NATIVE_LOCKOUT_PERIOD (10*Second)

bool P44BTDMXreceiver::processP44BTDMXpayload(const string aP44BTDMXData, bool aNative)
{
  FOCUSLOG("Got p44BTDMX payload: %s", binaryToHexString(aP44BTDMXData,' ').c_str());
  // FIXME: for the iOS app, we don't want MainLoop pulled in, so only checking iBeacon lockout on ESP32 for now
  #if ESP_PLATFORM
  MLMicroSeconds now = MainLoop::now();
  if (aNative || now-mlastNativeData>NOT_NATIVE_LOCKOUT_PERIOD)
  #else
  MLMicroSeconds now = 0;
  if (true)
  #endif
  {
    if (aNative) mlastNativeData = now;
    // decode from system key and verify CRC
    uint16_t crc = 0;
    string decoded;
    int i;
    for (i=0; i<aP44BTDMXData.size()-2; i++) {
      uint8_t b = aP44BTDMXData[i] ^ systemKeyByte(i);
      crc = crc16(crc, b);
      decoded.append(1, b);
    }
    uint16_t recCrc =
      ((aP44BTDMXData[i] ^ systemKeyByte(i)) << 8) |
      (aP44BTDMXData[i+1] ^ systemKeyByte(i+1));
    if (recCrc==crc) {
      // valid p44BTDMX data
      return processP44DMX(decoded);
    }
    else {
      FOCUSLOG("- p44BTDMX CRC error: received = 0x%04hX, expected=0x%04hX", recCrc, crc)
    }
  }
  else {
    FOCUSLOG("- not handling non-native data arriving less than %lld seconds after native data", NOT_NATIVE_LOCKOUT_PERIOD/Second);
  }
  return false;
}


bool P44BTDMXreceiver::processP44DMX(const string aP44BTDMXCmds)
{
  FOCUSLOG("Got p44BTDMX commands: %s", binaryToHexString(aP44BTDMXCmds,' ').c_str());
  // p44DMX delta update commands
  // - address byte with 3*lightnumber+cmd, 0xFF = Extended command lead-in, second byte is command, 0xFF=NOP
  // - lightnumber: 0..84 (address div 3)
  // - cmd: 0..2 (address mod 3)
  //   - 0=brightness (B channel), 1 data byte
  //   - 1=HSB, 3 data bytes
  //   - 2=other channel: channelindex/value, 2 data bytes
  int i = 0;
  int ln = (int)aP44BTDMXCmds.size();
  bool anyChanges = false;
  while (i<ln) {
    uint8_t addrCmd = aP44BTDMXCmds[i];
    if (addrCmd==0xFF) {
      // Extended command
      i++;
      if (i<ln) {
        // get extended command byte
        uint8_t extendedCmd = aP44BTDMXCmds[i++];
        switch (extendedCmd) {
          case 0xFF: break; // NOP command
          default: break;
        }
      }
      continue;
    }
    int lightIndex = addrCmd / 3;
    FOCUSLOG("Command for Global Light #%d (DMX: %d)", lightIndex, lightIndex*cLightChannels+1);
    uint8_t cmd = addrCmd - 3*lightIndex; // modulo 3
    if (!mIsLogger) {
      lightIndex -= mFirstLightNumber;
      if (lightIndex>=mLights.size()) lightIndex = -1; // not one of our lights
    }
    if (i++>=ln) return anyChanges; // error, not enough data: all commands have at least one byte
    switch (cmd) {
      case 0: {
        // set brightness (V channel)
        uint8_t b = aP44BTDMXCmds[i++];
        if (mIsLogger) {
          LOG(LOG_NOTICE, "L#%03d: V=%03d", lightIndex, b);
        }
        else if (lightIndex>=0) {
          FOCUSLOG("- local Light #%d (global #%d): Cmd%d %02X", lightIndex, lightIndex+mFirstLightNumber, cmd, b);
          P44DMXLightPtr light = mLights[lightIndex];
          light->setChannel(2,b);
          if (light->applyChannels()) anyChanges = true;
        }
        break;
      }
      case 1: {
        // set HSV at once
        if (i+3>ln) return anyChanges; // error, not enough data
        uint8_t h = aP44BTDMXCmds[i++];
        uint8_t s = aP44BTDMXCmds[i++];
        uint8_t b = aP44BTDMXCmds[i++];
        if (mIsLogger) {
          LOG(LOG_NOTICE, "L#%03d: V=%03d H=%03d S=%03d", lightIndex, b, h, s);
        }
        else if (lightIndex>=0) {
          FOCUSLOG("- local Light #%d (global #%d): Cmd%d %02X %02X %02X", lightIndex, lightIndex+mFirstLightNumber, cmd, h, s, b);
          P44DMXLightPtr light = mLights[lightIndex];
          light->setChannel(0,h);
          light->setChannel(1,s);
          light->setChannel(2,b);
          if (light->applyChannels()) anyChanges = true;
        }
        break;
      }
      case 2: {
        // set other channels by index
        if (i+2>ln) return anyChanges; // error, not enough data
        uint8_t cidx = aP44BTDMXCmds[i++];
        uint8_t value = aP44BTDMXCmds[i++];
        if (mIsLogger) {
          LOG(LOG_NOTICE, "L#%03d:     channel#%1d=%03d", lightIndex, cidx, value);
        }
        else if (lightIndex>=0) {
          FOCUSLOG("- local Light #%d (global #%d): Cmd%d %02X %02X", lightIndex, lightIndex+mFirstLightNumber, cmd, cidx, value);
          P44DMXLightPtr light = mLights[lightIndex];
          light->setChannel(cidx,value);
          if (light->applyChannels()) anyChanges = true;
        }
        break;
      }
      default:
        return anyChanges; // error
    }
  }
  return anyChanges;
}


void P44BTDMXreceiver::setAddressingInfo(int aFirstLightNumber)
{
  mFirstLightNumber = aFirstLightNumber;
}


void P44BTDMXreceiver::addLight(P44DMXLightPtr aLight)
{
  aLight->mGlobalLightOffset = mFirstLightNumber;
  aLight->mLocalLightNumber = mLights.size();
  mLights.push_back(aLight);
  aLight->applyChannels(); // set initial state
}





// MARK: - plan44 DMX Light base class

P44DMXLight::P44DMXLight() :
  mLocalLightNumber(0),
  mGlobalLightOffset(0)
{
  for (int i=0; i<cNumChannels; i++) {
    channels[i].current = 1; // to trigger an initial update
    channels[i].pending = 0;
  }
}


P44DMXLight::~P44DMXLight()
{
}


/// set single light channel
void P44DMXLight::setChannel(uint8_t aChannelIndex, uint8_t aValue)
{
  if (aChannelIndex>=cNumChannels) return;
  channels[aChannelIndex].pending = aValue;
}


bool P44DMXLight::applyChannels()
{
  // confirm all channels applied
  bool anyChanges = false;
  for (int i=0; i<cNumChannels; i++) {
    if (channels[i].current!=channels[i].pending) {
      OLOG(LOG_INFO,"Channel #%d changed from %d to %d", i, channels[i].current, channels[i].pending);
      channels[i].current = channels[i].pending;
      anyChanges = true;
    }
  }
  return anyChanges;
}



// MARK: - plan44 DMX over Bluetooth sender

P44BTDMXsender::P44BTDMXsender() :
  mInitialRepeatCount(3),
  mRefreshUniverse(false)
{
  for (int i=0; i<cUniverseSize; i++) {
    mUniverse[i].pending = 0;
    mUniverse[i].current = 0;
    mUniverse[i].age = 0; // assume channels all sent out at start
  }
}


P44BTDMXsender::~P44BTDMXsender()
{
}


void P44BTDMXsender::reset()
{
  for (int i=0; i<cUniverseSize; i++) {
    mUniverse[i].current = mUniverse[i].pending; // treat as if updated
    mUniverse[i].age = 128; // force all channels to be sent once initially, but with less priority than new changes
  }
}



string P44BTDMXsender::encodeP44BTDMXpayload(const string aPlainText)
{
  uint16_t crc = 0;
  string encoded;
  int i;
  for (i=0; i<aPlainText.size(); i++) {
    uint8_t b = aPlainText[i];
    crc = crc16(crc, b);
    encoded.append(1, b ^ systemKeyByte(i));
  }
  encoded.append(1, ((crc>>8)^systemKeyByte(i)) & 0xFF);
  encoded.append(1, (crc^systemKeyByte(i+1)) & 0xFF);
  return encoded;
}


uint8_t P44BTDMXsender::getChannel(uint16_t aDMXChannel)
{
  if (aDMXChannel>=cUniverseSize) return 0;
  return mUniverse[aDMXChannel].pending;
}


void P44BTDMXsender::setChannel(uint16_t aDMXChannel, uint8_t aValue)
{
  if (aDMXChannel>=cUniverseSize) return;
  if (FOCUSLOGGING) {
    if (mUniverse[aDMXChannel].pending!=aValue) {
      FOCUSLOG("DMX #%u pending value changes from %u to %u", aDMXChannel+1, mUniverse[aDMXChannel].pending, aValue);
    }
  }
  mUniverse[aDMXChannel].pending = aValue;
}


void P44BTDMXsender::setChannels(uint16_t aFromChannel, uint16_t aNumChannels, const uint8_t* aDMXChannelData)
{
  for (int i=0; i<aNumChannels; i++) {
    setChannel(aFromChannel+i, aDMXChannelData[i]);
  }
}


// DMX-to-BT update strategy
// - for each DMX channel in the universe (512 channels), we have
//   - pending value (as recently received from DMX)
//   - current value to detect changes
//   - age: number of cycles not sent, 0=just sent
// - Global param: initialrepeatcount, how many times a change is repeated quickly
// - for every (BT Advertisement sending) cycle:
//   - compare all current with previous values, set age to 255 if changed
//   - find max age
//   - generate p44DMX delta updates for entries with found max age
//   - if age>255-initialrepeatcount set age:=age-1
//   - otherwise, set age to 0 (for all bytes actually sent in p44DMX update packet
//   - if p44DMX data packet still has room, repeat with finding remaining max age
//   - increment all ages <255.
//   - send the p44DMX packet

// Flaws:
// - too many recent changes will prevent initial repeating -> badly lagging change in case of packet loss
// - small changes generated by light desk smoothing -> too many recent changes
// -> handle larger changes before smaller ones


string P44BTDMXsender::generateP44DMXcmds(int aMaxBytes)
{
  string cmds;
  // detect changes
  for (int i=0; i<cUniverseSize; i++) {
    if (mUniverse[i].pending != mUniverse[i].current) {
      LOG(LOG_INFO, "channel #%d changes from %d to %d", i, mUniverse[i].current, mUniverse[i].pending);
      mUniverse[i].age = 255;
      mUniverse[i].current = mUniverse[i].pending;
    }
  }
  int room = aMaxBytes;
  int lastMaxAge = 9999;
  int recentChangeMinAge = 255-mInitialRepeatCount;
  uint8_t doneAge = 0;
  while (room>=2) {
    // find highest remaining age not already covered in this packet
    int maxAge = 0;
    for (int i=0; i<cUniverseSize; i++) {
      if (mUniverse[i].age>maxAge && mUniverse[i].age<lastMaxAge) {
        maxAge = mUniverse[i].age;
      }
    }
    if (maxAge==0) {
      break; // no more aged values smaller than those already seen in last iteration
    }
    else if (maxAge>recentChangeMinAge) {
      // high ages meaning we are repeating recent changes
      lastMaxAge = recentChangeMinAge; // prevent repeating recent changes in same cycle
      doneAge = maxAge-1; // repeat with one priority less than current cycle
    }
    else {
      // just repeat oldest values
      lastMaxAge = maxAge;
      doneAge = 0;
    }
    // generate updates for oldest (=most urgent) lights
    // Note: check light by light
    for (int lidx=0; lidx<cNumLights; lidx++) {
      int loffs = lidx*cLightChannels;
      // light layout: HSB + n extra channels
      // - 0: hue
      // - 1: saturation
      // - 2: brightness
      // - 3..n: other channels
      // p44DMX delta update commands:
      // - address byte with 3*lightnumber+cmd
      // - lightnumber: 0..84 (address div 3)
      // - cmd: 0..2 (address mod 3)
      //   - 0=brightness (B channel), 1 data byte
      //   - 1=HSB, 3 data bytes
      //   - 2=channelindex/value, 2 data bytes
      if ((mUniverse[loffs+0].age==maxAge || mUniverse[loffs+1].age==maxAge) && room>=4) {
        // hue or saturation needs update -> need a HSB packet
        cmds.append(1, 3*lidx + 0x01); // HSB update command
        cmds.append(1, mUniverse[loffs+0].current); // H
        cmds.append(1, mUniverse[loffs+1].current); // S
        cmds.append(1, mUniverse[loffs+2].current); // B
        room -= 4;
        // reset age for update sent
        mUniverse[loffs+0].age = doneAge;
        mUniverse[loffs+1].age = doneAge;
        mUniverse[loffs+2].age = doneAge;
      }
      else if (mUniverse[loffs+2].age==maxAge) {
        // brightness changed, has priority over position/mode
        cmds.append(1, 3*lidx + 0x00); // Brightness update command
        cmds.append(1, mUniverse[loffs+2].current); // B
        room -= 2;
        // reset age for update sent
        mUniverse[loffs+2].age = doneAge;
      }
      // other channels might be sent in addition to brightness or HSB
      for (int cidx = 3; cidx<cLightChannels; cidx++) {
        if (mUniverse[loffs+cidx].age==maxAge && room>=3) {
          // other channel needs update -> need a channelindex/value packet
          cmds.append(1, 3*lidx + 0x02); // channelindex/value update command
          cmds.append(1, cidx); // channel index
          cmds.append(1, mUniverse[loffs+cidx].current); // value
          room -= 3;
          // reset age for update sent
          mUniverse[loffs+cidx].age = doneAge;
        }
      }
      if (room<2) break; // no point in checking further
    }
  }
  // one update created, now age all
  if (mRefreshUniverse) {
    for (int i=0; i<cUniverseSize; i++) {
      if (mUniverse[i].age<recentChangeMinAge) mUniverse[i].age++;
    }
  }
  // return the commands
  if (LOGENABLED(LOG_INFO) && !cmds.empty()) {
    OLOG(LOG_INFO, "p44DMX delta cmds: %s", binaryToHexString(cmds, ' ').c_str());
  }
  return cmds;
}


string P44BTDMXsender::generateP44BTDMXpayload(int aMaxBytes, int aMinBytes)
{
  if (aMinBytes==0) aMinBytes = aMaxBytes-2;
  string cmds = generateP44DMXcmds(aMaxBytes-2);
  if (cmds.empty()) return ""; // nothing at all
  int fill = aMinBytes-(int)cmds.size();
  if (fill>0) {
    cmds.append(fill,0xFF); // fill up with extended/NOP commands
  }
  return encodeP44BTDMXpayload(cmds);
}


string P44BTDMXsender::generateBTAdvMfgData(int aMaxBytes)
{
  string payload = generateP44BTDMXpayload(aMaxBytes-5);
  if (payload.empty()) return ""; // nothing at all
  string advData;
  advData.append(1, payload.size()+4); // length = ADStruct type, 2 byte company identifier, 1 byte subytpe + payload
  advData.append(1, 0xFF); // ADStruct type: manufacturer specific data
  advData.append(1, BT_COMPANY_ID_BLUEKITCHEN & 0xFF); // LSB of company ID
  advData.append(1, (BT_COMPANY_ID_BLUEKITCHEN>>8) & 0xFF); // MSB of company ID
  advData.append(1, PLAN44_SUBTYPE_P44BTDMX); // subtype
  advData.append(payload);
  return advData;
}
