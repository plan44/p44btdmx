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
  class P44DMXLight;
  typedef boost::intrusive_ptr<P44DMXLight> P44DMXLightPtr;

  class P44BTDMXreceiver : public P44LoggingObj
  {
    friend class P44DMXLight;

    static const uint16_t cLightBytes = 5;

    typedef std::vector<P44DMXLightPtr> LightsVector;

    string mSystemKey;
    uint16_t mFirstLightNumber; ///< the first light ID we listen to (=DMX address / cLightBytes)
    LightsVector lights;


  public:

    P44BTDMXreceiver();
    virtual ~P44BTDMXreceiver();

    /// @return prefix for log messages
    virtual string logContextPrefix() P44_OVERRIDE { return "p44BTDMX Rx"; };

    /// set the system data obfuscation key
    void setSystemKey(const string aSystemKey);

    /// set the addressing info
    /// @param aFirstLightNumber the first light number handled by this receiver
    void setAddressingInfo(int aFirstLightNumber);

    /// add a light to this controller
    /// @param aLight the light to add
    void addLight(P44DMXLightPtr aLight);

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


  class P44DMXLight : public P44LoggingObj
  {
    friend class P44BTDMXreceiver;

    static const uint16_t cNumChannels = P44BTDMXreceiver::cLightBytes;

  protected:

    int mLocalLightNumber;
    int mGlobalLightOffset;

    typedef struct {
      uint8_t pending;
      uint8_t current;
    } LightChannel;

    LightChannel channels[cNumChannels];

  public:
    P44DMXLight();
    virtual ~P44DMXLight();

    /// @return prefix for log messages
    virtual string logContextPrefix() P44_OVERRIDE { return string_format("Light #%d", mGlobalLightOffset+mLocalLightNumber); };

    /// set single light channel
    void setChannel(uint8_t aChannelIndex, uint8_t aValue);

    /// apply channel values
    /// @note base class just confirms apply by updating "current" field from "pending" in internal channel data
    virtual void applyChannels();

  };



  class P44BTDMXsender : public P44LoggingObj
  {
    string mSystemKey;
    static const uint16_t cLightBytes = 5;
    static const uint16_t cNumLights = (255-2)/3; // limited not by DMX channels, but addr/command byte (3 cmds per light)
    static const int cUniverseSize = cLightBytes*cNumLights; // important to be NOT larger than actually monitored lights

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

    /// reset sender to all channels unchanged
    void reset();

    /// @return prefix for log messages
    virtual string logContextPrefix() P44_OVERRIDE { return "p44BTDMX Tx"; };

    /// set the system data obfuscation key
    void setSystemKey(const string aSystemKey);

    void setInitialRepeatCount(int aInitialRepeatCount) { mInitialRepeatCount = aInitialRepeatCount; };

    /// encode plaintext (e.g. p44DMX command) string as p44BTDMX payload
    string encodeP44BTDMXpayload(const string aPlainText);

    /// get a DMX channel value
    /// @param aDMXChannel the DMX channel index (0..511)
    /// @return the current value of the channel
    uint8_t getChannel(uint16_t aDMXChannel);

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
