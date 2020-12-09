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


#ifndef DEFAULT_P44BTDMX_SYSTEM_KEY
  #ifdef CONFIG_P44BTDMX_SYSTEM_KEY
    #define DEFAULT_P44BTDMX_SYSTEM_KEY CONFIG_P44BTDMX_SYSTEM_KEY
  #else
    //                                   01234567090123456709012345670901
    #define DEFAULT_P44BTDMX_SYSTEM_KEY "NothingGreatButBetterThanNothing"
  #endif
#endif


using namespace p44;

// MARK: - plan44 DMX over Bluetooth receiver

P44BTDMXreceiver::P44BTDMXreceiver() :
  mFirstLightNumber(0),
  mNumLights(2)
{
  mSystemKey = DEFAULT_P44BTDMX_SYSTEM_KEY;
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
//    - use a Bluekitchen (later: plan44) manufacturer specific data packet, no flags = 28 bytes payload
//    - use first byte as a subtype (of Bluekitchen/plan44 manufacturer specific packets)
//      - 0x44 = subtype p44DMX
//    - rest of payload == 27 bytes p44DMX data

// 4C 00 02 15 B1 6F C6 BB D1 D1 42 8A 8C 03 55 BA D7 F7 04 81 00 FE FE 00 C5

#define BT_COMPANY_ID_APPLE 0x004C
#define BT_COMPANY_ID_PLAN44 0x4444
#define BT_COMPANY_ID_BLUEKITCHEN 0x048F

#define PLAN44_SUBTYPE_P44BTDMX 0x44
#define APPLE_SUBTYPE_IBEACON 0x02

void P44BTDMXreceiver::processBTAdvMfgData(const string aAdvMfgData)
{
  FOCUSLOG("Got advMfgData: %s", binaryToHexString(aAdvMfgData,' ').c_str());
  // check if its one of our recognized formats
  if (aAdvMfgData.size()<4) return;
  uint16_t companyBTId = aAdvMfgData[0]+(aAdvMfgData[1]<<8);
  if (companyBTId==BT_COMPANY_ID_PLAN44 || companyBTId==BT_COMPANY_ID_BLUEKITCHEN) {
    // raw p44BTDMX
    if (aAdvMfgData[2]==PLAN44_SUBTYPE_P44BTDMX) {
      processP44BTDMXpayload(aAdvMfgData.substr(3));
    }
  }
  if (companyBTId==BT_COMPANY_ID_APPLE) {
    // check for p44BTDMX disguised as Apple iBeacon
    if (aAdvMfgData[2]==APPLE_SUBTYPE_IBEACON) {
      processP44BTDMXpayload(aAdvMfgData.substr(4,(size_t)aAdvMfgData[3]));
    }
  }
}


// p44DMX data format (21..27 bytes)
// - pairing based on a "system key" = 32 bytes random key
// - key is xored with the payload to obfuscate it
// - last two bytes of the payload are a CRC16 of the bytes preceeding them
// - this leaves 21-4..27-4 = 17..23 effective p44DMX data bytes
// - p44DMX data consists of delta update commands


uint8_t P44BTDMXreceiver::systemKeyByte(int aIndex)
{
  if (aIndex>=mSystemKey.size()) return 0x42;
  return mSystemKey[aIndex];
}


// CCITT 16 bit CRC
uint16_t crc16(uint16_t aCRC16, uint8_t aByteToAdd)
{
  uint16_t s;

  s = (aByteToAdd ^ aCRC16) & 0xff;
  s = s ^ (s << 4);
  s = (aCRC16 >> 8) ^ (s << 8) ^ (s << 3) ^ (s >> 4);
  return s & 0xffff;
}



void P44BTDMXreceiver::processP44BTDMXpayload(const string aP44BTDMXData)
{
  FOCUSLOG("Got p44BTDMX payload: %s", binaryToHexString(aP44BTDMXData,' ').c_str());
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
    processP44DMX(decoded);
  }
  else {
    FOCUSLOG("- p44BTDMX CRC error: received = 0x%04hX, expected=0x%04hX", recCrc, crc)
  }
}


void P44BTDMXreceiver::processP44DMX(const string aP44BTDMXCmds)
{
  FOCUSLOG("Got p44BTDMX commands: %s", binaryToHexString(aP44BTDMXCmds,' ').c_str());
  // p44DMX delta update commands
  // - address byte with 3*lightnumber+cmd, 0xFF = NOP command
  // - lightnumber: 0..84 (address div 3)
  // - cmd: 0..2 (address mod 3)
  //   - 0=brightness (B channel), 1 data byte
  //   - 1=HSB, 3 data bytes
  //   - 2=pos/mode, 2 data bytes
  int i = 0;
  int ln = (int)aP44BTDMXCmds.size();
  while (i<ln) {
    uint8_t addrCmd = aP44BTDMXCmds[i];
    if (addrCmd==0xFF) {
      i++;
      continue;
    }
    int lightIndex = addrCmd / 3;
    LOG(LOG_INFO, "Command for Global Light #%d (DMX: %d)", lightIndex, lightIndex*cLightBytes);
    uint8_t cmd = addrCmd - 3*lightIndex; // modulo 3
    lightIndex -= mFirstLightNumber;
    if (lightIndex>=mNumLights) lightIndex = -1; // not one of our lights
    if (i++>=ln) return; // error, not enough data: all commands have at least one byte
    switch (cmd) {
      case 0: {
        uint8_t b = aP44BTDMXCmds[i++];
        if (lightIndex>=0) {
          LOG(LOG_INFO, "Light #%d: B = %d", lightIndex, b);
          // TODO: actually modify light params
        }
        break;
      }
      case 1: {
        if (i+3>ln) return; // error, not enough data
        uint8_t h = aP44BTDMXCmds[i++];
        uint8_t s = aP44BTDMXCmds[i++];
        uint8_t b = aP44BTDMXCmds[i++];
        if (lightIndex>=0) {
          LOG(LOG_INFO, "Light #%d: H = %d, S = %d, B = %d", lightIndex, h, s, b);
          // TODO: actually modify light params
        }
        break;
      }
      case 2: {
        if (i+2>ln) return; // error, not enough data
        uint8_t pos = aP44BTDMXCmds[i++];
        uint8_t mode = aP44BTDMXCmds[i++];
        if (lightIndex>=0) {
          LOG(LOG_INFO, "Light #%d: pos = %d, mode = %d", lightIndex, pos, mode);
          // TODO: actually modify light params
        }
        break;
      }
      default:
        return; // error
    }
  }
}


// MARK: - plan44 DMX over Bluetooth sender

P44BTDMXsender::P44BTDMXsender() :
  mInitialRepeatCount(3)
{
  mSystemKey = DEFAULT_P44BTDMX_SYSTEM_KEY;
  for (int i=0; i<cUniverseSize; i++) {
    mUniverse[i].pending = 0;
    mUniverse[i].current = 0;
    mUniverse[i].age = 255; // force all channels to be sent once initially
  }
}


P44BTDMXsender::~P44BTDMXsender()
{
}


uint8_t P44BTDMXsender::systemKeyByte(int aIndex)
{
  if (aIndex>=mSystemKey.size()) return 0x42;
  return mSystemKey[aIndex];
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

void P44BTDMXsender::setChannel(uint16_t aDMXChannel, uint8_t aValue)
{
  if (aDMXChannel>=cUniverseSize) return;
  mUniverse[aDMXChannel].pending = aValue;
}


void P44BTDMXsender::setChannels(uint16_t aFromChannel, uint16_t aNumChannels, const uint8_t* aDMXChannelData)
{
  for (int i=0; i<aNumChannels; i++) {
    setChannel(aFromChannel+i, aDMXChannelData[i]);
  }
}


string P44BTDMXsender::generateP44DMXcmds(int aMaxBytes)
{
  string cmds;
  // detect changes
  for (int i=0; i<cUniverseSize; i++) {
    if (mUniverse[i].pending != mUniverse[i].current) {
      mUniverse[i].age = 255;
      mUniverse[i].current = mUniverse[i].pending;
    }
  }
  int room = aMaxBytes;
  while (room>=2) {
    // find highest remaining age
    uint8_t maxAge = 0;
    for (int i=0; i<cUniverseSize; i++) {
      if (mUniverse[i].age>maxAge) maxAge = mUniverse[i].age;
    }
    if (maxAge<1) break; // no more aged values, all sent
    uint8_t doneAge = maxAge>255-mInitialRepeatCount ? maxAge-2 : 0;
    // generate updates for oldest (=most urgent) lights
    // Note: check light by light
    for (int lidx=0; lidx<cUniverseSize/cLightBytes; lidx++) {
      int loffs = lidx*cLightBytes;
      // light layout: HSB: 5 bytes
      // - 0: hue
      // - 1: saturation
      // - 2: brightness
      // - 3: position
      // - 4: mode
      // p44DMX delta update commands:
      // - address byte with 3*lightnumber+cmd
      // - lightnumber: 0..84 (address div 3)
      // - cmd: 0..2 (address mod 3)
      //   - 0=brightness (B channel), 1 data byte
      //   - 1=HSB, 3 data bytes
      //   - 2=pos/mode, 2 data bytes
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
      // position might be sent in addition to brightness or HSB
      if ((mUniverse[loffs+3].age==maxAge || mUniverse[loffs+4].age==maxAge) && room>=3) {
        // mode or position needs update -> need a Pos/Mode packet
        cmds.append(1, 3*lidx + 0x02); // Pos/Mode update command
        cmds.append(1, mUniverse[loffs+3].current); // H
        cmds.append(1, mUniverse[loffs+4].current); // S
        room -= 3;
        // reset age for update sent
        mUniverse[loffs+3].age = doneAge;
        mUniverse[loffs+4].age = doneAge;
      }
      if (room<2) break; // no point in checking further
    }
  }
  // one update created, now age all
  for (int i=0; i<cUniverseSize; i++) {
    if (mUniverse[i].age<255) mUniverse[i].age++;
  }
  // return the commands
  LOG(LOG_NOTICE, "p44DMX delta cmds: %s", binaryToHexString(cmds, ' ').c_str());
  return cmds;
}


string P44BTDMXsender::generateP44BTDMXpayload(int aMaxBytes, int aMinBytes)
{
  string cmds = generateP44DMXcmds(aMaxBytes-2);
  int fill = aMaxBytes-2-cmds.size();
  if (fill>0) {
    cmds.append(fill,0xFF); // fill up with NOP commands
  }
  return encodeP44BTDMXpayload(cmds);
}
