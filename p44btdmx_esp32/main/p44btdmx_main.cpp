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
#include "p44btdmx.hpp"
#include "pwmlight.hpp"
#include "p44lrglight.hpp"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif


#if CONFIG_P44_WIFI_SUPPORT
  #define JSONAPI 1 // JSON socket API
#else
  #define JSONAPI 0
#endif


using namespace p44;

static size_t memAtStart = 0;

class P44BTDMXController : public Application
{
  typedef Application inherited;

  #if JSONAPI
  SocketCommPtr apiServer;
  #endif

  LEDChainArrangementPtr ledChainArrangement;
  DigitalIoPtr ledChainEnable; ///< output in P44-BTLC for enabling the 5V WS281x drivers (active low)
  P44BTDMXreceiverPtr dmxReceiver; ///< p44 BT DMX receiver

public:

  P44BTDMXController()
  {
  }

  virtual int main(int argc, char **argv)
  {
    if (!isTerminated()) {
      SETLOGLEVEL(LOG_NOTICE);
      SETERRLEVEL(LOG_ERR, false);
      SETDELTATIME(true);
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }

  virtual void initialize()
  {
    LOG(LOG_NOTICE,"initialize");
    // enable LED chain outputs in P44-BTLC
    ledChainEnable = DigitalIoPtr(new DigitalIo("gpio.25", true, 0)); // IO25 is LED_DATA_EN0
    // P44BTDMX receiver object
    dmxReceiver = P44BTDMXreceiverPtr(new P44BTDMXreceiver);
    dmxReceiver->setAddressingInfo(0); // TODO: get from DIP switches!
    #ifdef CONFIG_P44BTDMX_SYSTEMKEY
    string systemkey = CONFIG_P44BTDMX_SYSTEMKEY;
    dmxReceiver->setSystemKey(systemkey);
    #endif
    #if JSONAPI
    // socket
    apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
    apiServer->setConnectionParams(NULL, "8842", SOCK_STREAM, PF_UNSPEC);
    apiServer->setAllowNonlocalConnections(true);
    ErrorPtr err = apiServer->startServer(boost::bind(&P44BTDMXController::apiConnectionHandler, this, _1), 10);
    #endif
    // Real lights initialisation
    #if CONFIG_P44BTDMX_PWMLIGHT
    P44DMXLightPtr light;
    // - PWM
    light = P44DMXLightPtr(new PWMLight(
      new AnalogIo("pwmchip14.0", true, 0),
      new AnalogIo("pwmchip12.1", true, 0),
      new AnalogIo("pwmchip33.2", true, 0)
    ));
    dmxReceiver->addLight(light);
    // - single Ledchain on DI0 (gpio23)
    LEDChainArrangement::addLEDChain(ledChainArrangement, "WS2813:gpio23:150:0:150:0:1");
    #else
    // - dual ledchains on DI1 and DI2 (gpio22 and gpio21)
    LEDChainArrangement::addLEDChain(ledChainArrangement, "WS2813:gpio22:150:0:150:0:1");
    LEDChainArrangement::addLEDChain(ledChainArrangement, "WS2813:gpio21:150:0:150:1:1");
    #endif
    if (ledChainArrangement) {
      PixelRect r = ledChainArrangement->totalCover();
      ViewStackPtr rootView = ViewStackPtr(new ViewStack);
      rootView->setFrame(r);
      rootView->setBackgroundColor(black); // stack with black background is more efficient (and there's nothing below, anyway)
      ledChainArrangement->setRootView(rootView);
      ledChainArrangement->begin(true);
      r.dy = 1;
      r.y = 0;
      dmxReceiver->addLight(new P44lrgLight(ledChainArrangement->getRootView(), r));
      #if !CONFIG_P44BTDMX_PWMLIGHT
      r.y = 1;
      dmxReceiver->addLight(new P44lrgLight(ledChainArrangement->getRootView(), r));
      #endif
      LOG(LOG_INFO, "lrg status: %s", rootView->viewStatus()->json_c_str());
    }
    else {
      LOG(LOG_ERR,"cannot create LED chain arrangement");
    }
    // start scanning BLE advertisements
    BtAdvertisementReceiver::sharedReceiver().start(boost::bind(&P44BTDMXController::gotAdvertisement, this, _1, _2));
  }


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
      if (BtAdvertisementReceiver::findADStruct((uint8_t *)aAdvData.c_str(), 0xFF, adMfgData, adMfgDataSz)) {
        // let dmxreceiver handle it
        dmxReceiver->processBTAdvMfgData(string((const char*)adMfgData, adMfgDataSz));
      }
    }
  }


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
