//
//  Copyright (c) 2020 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//

#include <stdio.h>

#include "application.hpp"

#include "ledchaincomm.hpp"
#include "socketcomm.hpp"
#include "jsoncomm.hpp"
#include "digitalio.hpp"

#include "esp_bt.hpp"
#include "esp_dmx_rx.hpp"
#include "p44btdmx.hpp"
#include "pwmlight.hpp"
#include "p44lrglight.hpp"
#include "p44lrgtextlight.hpp"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#endif


// MARK: - Basic application configuration

// quick overrides for development (because changing in actual config triggers full rebuild of everything)
#define QUICK_OVERRIDE 1
#if QUICK_OVERRIDE
  // device types
  #define AUTO 1
  #define MASK 2
  #define TEXT 3
  #define DMX 4
  // current device type
  #define DEVICE MASK
  // common settings
  #define CONFIG_DEFAULT_LOG_LEVEL 5
  #define CONFIG_P44_WIFI_SUPPORT 0
  // settings for different device types
  #if DEVICE==MASK
    //#define CONFIG_DEFAULT_LOG_LEVEL 6 // FIXME: remove
    #define CONFIG_P44_DMX_RX 0
    #define CONFIG_P44_BTDMX_SENDER 0
    #define CONFIG_P44_BTDMX_RECEIVER 1
    #define CONFIG_P44_BTDMX_LIGHTS 1
    #define CONFIG_P44BTDMX_PWMLIGHT 1
  #elif DEVICE==AUTO
    #define CONFIG_P44_DMX_RX 0
    #define CONFIG_P44_BTDMX_SENDER 0
    #define CONFIG_P44_BTDMX_RECEIVER 1
    #define CONFIG_P44_BTDMX_LIGHTS 1
    #define CONFIG_P44BTDMX_PWMLIGHT 0
  #elif DEVICE==TEXT
    //#define CONFIG_DEFAULT_LOG_LEVEL 6 // FIXME: remove
    #define CONFIG_P44_DMX_RX 0
    #define CONFIG_P44_BTDMX_SENDER 0
    #define CONFIG_P44_BTDMX_RECEIVER 1
    #define CONFIG_P44_BTDMX_LIGHTS 1
    #define CONFIG_P44BTDMX_PWMLIGHT 1
    #define CONFIG_P44BTDMX_SINGLECHAIN 1 // only one, so RMT can use all buffers for text
//    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0 "WS2813:gpio22:791:0:113:0:7:A"
//    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1 "SK6812:gpio22:791:0:113:0:7:A"
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0 "WS2813:gpio23:791:0:113:0:7:A"
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1 "SK6812:gpio23:791:0:113:0:7:A"
  #elif DEVICE==DMX
    //#define CONFIG_DEFAULT_LOG_LEVEL 6 // FIXME: remove
    #define CONFIG_P44BTDMX_REFRESH_UNIVERSE true
    #define CONFIG_P44_DMX_RX 1
    #define CONFIG_P44_BTDMX_SENDER 1
    #define CONFIG_P44_BTDMX_RECEIVER 0
    #define CONFIG_P44_BTDMX_LIGHTS 0
  #else
    #error "Unknown device"
  #endif
#endif

#ifndef CONFIG_DEFAULT_LOG_LEVEL
  #define CONFIG_DEFAULT_LOG_LEVEL 5
#endif
#ifndef CONFIG_P44BTDMX_REFRESH_UNIVERSE
  #define CONFIG_P44BTDMX_REFRESH_UNIVERSE true
#endif

#if CONFIG_P44_WIFI_SUPPORT
  #define JSONAPI 1 // JSON socket API
#else
  #define JSONAPI 0
#endif

#ifndef CONFIG_P44_DMX_RX
  #define CONFIG_P44_DMX_RX 0
#endif
#ifndef CONFIG_P44_BTDMX_SENDER
  #define CONFIG_P44_BTDMX_SENDER 0
#endif
#ifndef CONFIG_P44_BTDMX_RECEIVER
  #define CONFIG_P44_BTDMX_RECEIVER 1
#endif
#ifndef CONFIG_P44_BTDMX_LIGHTS
  #define CONFIG_P44_BTDMX_LIGHTS 1
#endif


// MARK: - light hardware configuration

#ifndef CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0
  #if CONFIG_P44BTDMX_PWMLIGHT
    // - single ledchain on DI0 (gpio23)
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0 "WS2813:gpio23:150:0:150:0:1"
  #else
    // - dual ledchains on DI1 and DI2 (gpio22 and gpio21)
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0 "WS2813:gpio22:150:0:150:0:1"
  #endif
#endif
#ifndef CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1
  #if CONFIG_P44BTDMX_PWMLIGHT
    // - single ledchain on DI0 (gpio23)
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1 "SK6812:gpio23:150:0:150:0:1"
  #else
    // - dual ledchains on DI1 and DI2 (gpio22 and gpio21)
    #define CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1 "SK6812:gpio22:150:0:150:0:1"
  #endif
#endif
#ifndef CONFIG_P44BTDMX_SECONDCHAIN_CFG
  // - second ledchain on DI2 (gpio21)
  #define CONFIG_P44BTDMX_SECONDCHAIN_CFG "WS2813:gpio21:150:0:150:1:1"
#endif


using namespace p44;

static size_t memAtStart = 0;

class P44BTDMXController : public Application
{
  typedef Application inherited;

  #if JSONAPI
  SocketCommPtr apiServer;
  #endif

  #if CONFIG_P44_BTDMX_LIGHTS
  LEDChainArrangementPtr ledChainArrangement;
  DigitalIoPtr ledChainEnable; ///< output in P44-BTLC for enabling the 5V WS281x drivers (active low)
  P44BTDMXreceiverPtr dmxReceiver; ///< p44 BT DMX receiver
  #endif

  #if CONFIG_P44_BTDMX_SENDER
  P44BTDMXsenderPtr dmxSender; ///< p44 BT DMX sender
  MLTicket advertisingTicket;
  #endif

public:

  P44BTDMXController()
  {
  }

  virtual int main(int argc, char **argv)
  {
    if (!isTerminated()) {
      SETLOGLEVEL(CONFIG_DEFAULT_LOG_LEVEL);
      SETERRLEVEL(LOG_ERR, false);
      SETDELTATIME(true);
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }

  virtual void initialize()
  {
    LOG(LOG_NOTICE,"initialize");
    // get DIP switch state
    // A0..A5 = GPIO26,27,32,34,35,18 (18 must be patched, is connected to 7 which disturbs SPI flash)
    const int numSwitchBits = 6;
    int switchpins[numSwitchBits] = { 26, 27, 32, 34, 35, 18 };
    int dispswitch = 0;
    for (int i=0; i<numSwitchBits; i++) {
      DigitalIoPtr bit = DigitalIoPtr(new DigitalIo(string_format("+/gpio.%d",switchpins[i]).c_str(), false));
      MainLoop::sleep(5*MilliSecond); // give pullup chance to actually pull pin up
      dispswitch |= bit->isSet() ? (1<<i) : 0;
    }
    LOG(LOG_NOTICE, "DIP Switch = 0x%02X", dispswitch);
    #if JSONAPI
    // socket
    apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
    apiServer->setConnectionParams(NULL, "8842", SOCK_STREAM, PF_UNSPEC);
    apiServer->setAllowNonlocalConnections(true);
    ErrorPtr err = apiServer->startServer(boost::bind(&P44BTDMXController::apiConnectionHandler, this, _1), 10);
    #endif // JSONAPI
    #if CONFIG_P44_BTDMX_SENDER
    // P44BTDMX sender object
    dmxSender = P44BTDMXsenderPtr(new P44BTDMXsender);
    dmxSender->setRefreshUniverse(CONFIG_P44BTDMX_REFRESH_UNIVERSE);
    dmxSender->setInitialRepeatCount(3);
    #ifdef CONFIG_P44BTDMX_SYSTEMKEY
    string systemkey = CONFIG_P44BTDMX_SYSTEMKEY;
    dmxSender->setSystemKey(systemkey);
    #endif
    #endif // CONFIG_P44_BTDMX_SENDER
    #if CONFIG_P44_BTDMX_LIGHTS
    // enable LED chain outputs in P44-BTLC
    ledChainEnable = DigitalIoPtr(new DigitalIo("gpio.25", true, 0)); // IO25 is LED_DATA_EN0
    // P44BTDMX receiver object
    dmxReceiver = P44BTDMXreceiverPtr(new P44BTDMXreceiver);
    dmxReceiver->setAddressingInfo((dispswitch & 0x1F)*2); // 64 lights max, in steps of 2
    #ifdef CONFIG_P44BTDMX_SYSTEMKEY
    string systemkey = CONFIG_P44BTDMX_SYSTEMKEY;
    dmxReceiver->setSystemKey(systemkey);
    #endif
    // Real lights initialisation
    const char *firstChainConfig = (dispswitch&0x20) ? CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT1 : CONFIG_P44BTDMX_FIRSTCHAIN_CFG_VARIANT0;
    #if CONFIG_P44BTDMX_PWMLIGHT
    P44DMXLightPtr light;
    // - PWM
    light = P44DMXLightPtr(new PWMLight(
      new AnalogIo("pwmchip14.0", true, 0),
      new AnalogIo("pwmchip12.1", true, 0),
      new AnalogIo("pwmchip33.2", true, 0)
    ));
    dmxReceiver->addLight(light);
    #endif
    #if CONFIG_P44BTDMX_PWMLIGHT
    // - single ledchain
    LEDChainArrangement::addLEDChain(ledChainArrangement, firstChainConfig);
    #else
    // - dual ledchains
    const char *secondChainConfig = CONFIG_P44BTDMX_SECONDCHAIN_CFG;
    LEDChainArrangement::addLEDChain(ledChainArrangement, firstChainConfig);
    #if !CONFIG_P44BTDMX_SINGLECHAIN
    LEDChainArrangement::addLEDChain(ledChainArrangement, secondChainConfig);
    #endif
    #endif
    if (ledChainArrangement) {
      PixelRect r = ledChainArrangement->totalCover();
      ViewStackPtr rootView = ViewStackPtr(new ViewStack);
      rootView->setFrame(r);
      rootView->setBackgroundColor(black); // stack with black background is more efficient (and there's nothing below, anyway)
      ledChainArrangement->setRootView(rootView);
      ledChainArrangement->begin(true);
      #if DEVICE==TEXT
      dmxReceiver->addLight(new P44lrgTextLight(ledChainArrangement->getRootView(), r));
      #else
      r.dy = 1;
      r.y = 0;
      dmxReceiver->addLight(new P44lrgLight(ledChainArrangement->getRootView(), r));
      #if !CONFIG_P44BTDMX_PWMLIGHT && !CONFIG_P44BTDMX_SINGLECHAIN
      r.y = 1;
      dmxReceiver->addLight(new P44lrgLight(ledChainArrangement->getRootView(), r));
      #endif // !PWMLIGHT
      #endif // TEXT
      LOG(LOG_INFO, "lrg status: %s", rootView->viewStatus()->json_c_str());
    }
    else {
      LOG(LOG_ERR,"cannot create LED chain arrangement");
    }
    #endif // CONFIG_P44_BTDMX_LIGHTS
    #if CONFIG_P44_BTDMX_RECEIVER
    // start scanning BLE advertisements
    BtAdvertisements::sharedInstance().startScanning(boost::bind(&P44BTDMXController::gotAdvertisement, this, _1, _2));
    #endif // CONFIG_P44_BTDMX_RECEIVER
    #if CONFIG_P44_DMX_RX
    // start receiving DMX packets
    // - disable Tx
    gpio_reset_pin(GPIO_NUM_16);
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 0); // Tx disabled
    // - start receiver
    DMXReceiver::sharedReceiver().start(UART_NUM_2, GPIO_NUM_13, GPIO_NUM_4, boost::bind(&P44BTDMXController::gotDMXPacket, this, _1));
    #endif // CONFIG_P44_BTDMX_RECEIVER
    #if CONFIG_P44_BTDMX_SENDER
    // start sending P44DMX advertisements
    sendNextP44BTDMXAdvertisement();
    #endif // CONFIG_P44_BTDMX_SENDER
  }


  #if CONFIG_P44_DMX_RX

  void gotDMXPacket(const uint8_t* aDMXData)
  {
    #if CONFIG_P44_BTDMX_LIGHTS
    // TODO: directly let lights process the data (plain DMX light mode, not BTDMX involved)
    #endif
    #if CONFIG_P44_BTDMX_SENDER
    // update DMX channels in sender
    dmxSender->setChannels(0, 512, aDMXData+1); // byte 1 is first actual channel (byte 0 is 0x00)
    #endif
  }

  #endif // CONFIG_P44_BTDMX_RECEIVER


  #if CONFIG_P44_BTDMX_RECEIVER

  void gotAdvertisement(ErrorPtr aError, const string aAdvData)
  {
    if (!Error::isOK(aError)) {
      LOG(LOG_ERR, "Error: %s", Error::text(aError));
    }
    else {
      FOCUSLOG("Got advData: %s", binaryToHexString(aAdvData,' ').c_str());
      // fetch possible manufacturer specific data from advertisement
      const uint8_t* adMfgData;
      uint8_t adMfgDataSz;
      if (BtAdvertisements::findADStruct((uint8_t *)aAdvData.c_str(), 0xFF, adMfgData, adMfgDataSz)) {
        // let dmxreceiver handle it
        dmxReceiver->processBTAdvMfgData(string((const char*)adMfgData, adMfgDataSz));
      }
    }
  }

  #endif // CONFIG_P44_BTDMX_RECEIVER


  #if CONFIG_P44_BTDMX_SENDER

  #define ADVERTISING_START_INTERVAL (100*MilliSecond)
  #define ADVERTISING_START_TO_UPDATE (5*MilliSecond)
  #define ADVERTISING_ERROR_TO_RESTART (5*Second)

  void sendNextP44BTDMXAdvertisement()
  {
    advertisingTicket.cancel();
    string advData = dmxSender->generateBTAdvMfgData();
    if (advData.empty()) {
      // try again shortly
      advertisingTicket.executeOnce(boost::bind(&P44BTDMXController::sendNextP44BTDMXAdvertisement, this), ADVERTISING_START_INTERVAL);
      return;
    }
    // advertise the new data
    LOG(LOG_DEBUG, "Sending advertisement: (%d bytes) %s", advData.size(), binaryToHexString(advData, ' ').c_str());
    BtAdvertisements::sharedInstance().startAdvertising(boost::bind(&P44BTDMXController::advertisementStarted, this, _1), advData);
  }


  void advertisementStarted(ErrorPtr aError)
  {
    LOG(LOG_DEBUG, "advertisementStarted");
    if (Error::notOK(aError)) {
      LOG(LOG_ERR, "error starting adverisement: %s", aError->text());
      advertisingTicket.executeOnce(boost::bind(&P44BTDMXController::sendNextP44BTDMXAdvertisement, this), ADVERTISING_ERROR_TO_RESTART);
    }
    else {
      advertisingTicket.executeOnce(boost::bind(&P44BTDMXController::sendNextP44BTDMXAdvertisement, this), ADVERTISING_START_TO_UPDATE);
    }
  }


  #endif // CONFIG_P44_BTDMX_SENDER


  #if JSONAPI

  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketCommP)
  {
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&P44BTDMXController::gotMessage, this, _1, _2));
    return conn;
  }

  void gotData(SocketCommPtr aConn, ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      LOG(LOG_ERR, "Error: %s", Error::text(aError));
    }
    else {
      string s;
      aConn->receiveIntoString(s);
      LOG(LOG_NOTICE, "Got data: %s", s.c_str());
    }
  }


  void gotMessage(ErrorPtr aError, JsonObjectPtr aJsonObject)
  {
    if (!Error::isOK(aError)) {
      LOG(LOG_ERR, "Error: %s", Error::text(aError));
    }
    else {
      LOG(LOG_NOTICE, "Got Json Object: %s", aJsonObject ? aJsonObject->c_strValue() : "<none>");
      #ifdef ESP_PLATFORM
      LOG(LOG_INFO, "- app mem usage = %zd", memAtStart-heap_caps_get_free_size(MALLOC_CAP_8BIT));
      #endif
      if (aJsonObject) {
        if (aJsonObject->stringValue()=="quit") {
          terminateAppWith(TextError::err("received quit command via JSON"));
        }
      }
    }
  }

  #endif // JSONAPI

};


int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  #ifdef ESP_PLATFORM
  memAtStart = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  LOG(LOG_EMERG, "Start of app - Free heap = %zd", memAtStart);
  #endif
  // create app with current mainloop
  static P44BTDMXController application;
  // pass control to app object
  return application.main(0, NULL);
}
