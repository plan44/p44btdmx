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


#ifndef __p44utils__p44btdmx__
#define __p44utils__p44btdmx__

#include "p44utils_common.hpp"

using namespace std;

namespace p44 {

  class P44BTDMXreceiver;
  typedef boost::intrusive_ptr<P44BTDMXreceiver> P44BTDMXreceiverPtr;
  class P44BTDMXsender;
  typedef boost::intrusive_ptr<P44BTDMXsender> P44BTDMXsenderPtr;

  class P44BTDMXreceiver : public P44LoggingObj
  {
    static const uint16_t cLightBytes = 5;

    string mSystemKey;
    uint16_t mFirstLightNumber; ///< the first light ID we listen to (=DMX address / cLightBytes)
    uint16_t mNumLights; ///< number of lights in this controllr (number of cLightBytes blocks)

  public:

    P44BTDMXreceiver();
    virtual ~P44BTDMXreceiver();

    /// set the system data obfuscation key
    void setSystemKey(const string aSystemKey);

    /// set the addressing info
    /// @param aLightNumber the first light number handled by this receiver
    /// @param aNumLights number of lights in this receiver
    void setAddressingInfo(int aLightNumber, int aNumLights);

    /// process manufacturer specific advertisement data (which might contain p44BTDMX data
    /// @param aAdvMfgData data bytes from a AD Struct of type "manufacturer specific data"
    /// @note p44BTDMX recognizes Apple iBeacons as well as native plan44 and bluekitchen manufacturer data as carriers
    void processBTAdvMfgData(const string aAdvMfgData);

    /// process p44BTDMX payload data, coming from one of the possible carriers, encrypted/obfuscated by the system key
    /// @param aP44BTDMXData raw p44BTDMX data
    void processP44BTDMXpayload(const string aP44BTDMXData);

    /// process p44DMX decrypted delta update commands
    /// @param aP44DMXCmds plain text p44DMX delta commands
    void processP44DMX(const string aP44DMXCmds);

  private:

    uint8_t systemKeyByte(int aIndex);

  };
  

  class P44BTDMXsender : public P44LoggingObj
  {
    string mSystemKey;
    static const int cUniverseSize = 512;
    static const uint16_t cLightBytes = 5;

    typedef struct {
      uint8_t pending;
      uint8_t current;
      uint8_t age;
    } DMXChannel;

    DMXChannel mUniverse[cUniverseSize];
    int mInitialRepeatCount;

  public:

    P44BTDMXsender();
    virtual ~P44BTDMXsender();

    /// set the system data obfuscation key
    void setSystemKey(const string aSystemKey);

    void setInitialRepeatCount(int aInitialRepeatCount) { mInitialRepeatCount = aInitialRepeatCount; };

    /// encode plaintext (e.g. p44DMX command) string as p44BTDMX payload
    string encodeP44BTDMXpayload(const string aPlainText);

    /// set a DMX channel value
    /// @param aDMXChannel the DMX channel index (0..511)
    /// @param aValue the new value
    void setChannel(uint16_t aDMXChannel, uint8_t aValue);

    /// set a DMX channel value
    /// @param aFromChannel the first channel to update
    /// @param aNumChannels number of channels to update
    /// @param aDMXChannelData array of DMX channel values
    void setChannels(uint16_t aFromChannel, uint16_t aNumChannels, const uint8_t* aDMXChannelData);

    /// generate next round of p44DMX delta commands to send out
    /// @param aMaxBytes maximum size of p44DMX command bytes
    /// @return string of p44DMX commands to send
    string generateP44DMXcmds(int aMaxBytes);

    /// generate p44BTDMX payload (with CRC and encrypted/obfuscated by the system key)
    /// @param aMaxBytes maximum size of payload
    /// @param aMinBytes minimum size of payload
    string generateP44BTDMXpayload(int aMaxBytes, int aMinBytes = 0);

  private:

    uint8_t systemKeyByte(int aIndex);

  };





} // namespace p44


#endif /* defined(__p44utils__p44btdmx__) */