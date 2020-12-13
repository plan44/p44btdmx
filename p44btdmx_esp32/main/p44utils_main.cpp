//
//  Copyright (c) 2019 plan44.ch / Lukas Zeller, Zurich, Switzerland
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


// TODO: remove this test stuff
#define JSONAPI 0 // JSON socket API
#define TEST 0 // tests (inlcuding direct LED)
#define NUM_LEDS 10 // number of LEDs for direct test, 0=none


using namespace p44;

static size_t memAtStart = 0;

class P44HelloWorld : public Application
{
  typedef Application inherited;

  #if TEST
  MLTicket counterTicket;
  int counter;
  #if NUM_LEDS
  LEDChainCommPtr ledChain;
  #endif
  #endif
  #if JSONAPI
  SocketCommPtr apiServer;
  #endif

  LEDChainArrangementPtr ledChainArrangement;
  DigitalIoPtr ledChainEnable; ///< output in P44-BTLC for enabling the 5V WS281x drivers (active low)
  P44BTDMXreceiverPtr dmxReceiver; ///< p44 BT DMX receiver

public:

  P44HelloWorld()
    #if TEST
    : counter(0)
    #endif
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
    #if JSONAPI
    // socket
    apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
    apiServer->setConnectionParams(NULL, "8842", SOCK_STREAM, PF_UNSPEC);
    apiServer->setAllowNonlocalConnections(true);
    ErrorPtr err = apiServer->startServer(boost::bind(&P44HelloWorld::apiConnectionHandler, this, _1), 10);
    #endif
    #if TEST
    #if NUM_LEDS>0
    // ledchain direct test
    ledChain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, "gpio23", NUM_LEDS));
    ledChain->begin();
    #endif // NUM_LEDS>0
    // second counter
    counterTicket.executeOnce(boost::bind(&P44HelloWorld::count, this, _1));
    #else
    // Real lights initialisation
    P44DMXLightPtr light;
    // - PWM
    light = P44DMXLightPtr(new PWMLight(
      new AnalogIo("pwmchip14.0", true, 0),
      new AnalogIo("pwmchip12.1", true, 0),
      new AnalogIo("pwmchip33.2", true, 0)
    ));
    dmxReceiver->addLight(light);
    // - Ledchain
    LEDChainArrangement::addLEDChain(ledChainArrangement, "WS2813:gpio23:150:0:150:0:1");
    if (ledChainArrangement) {
      PixelRect r = ledChainArrangement->totalCover();
      ViewStackPtr rootView = ViewStackPtr(new ViewStack);
      rootView->setFrame(r);
      rootView->setBackgroundColor(black); // stack with black background is more efficient (and there's nothing below, anyway)
      ledChainArrangement->setRootView(rootView);
      ledChainArrangement->begin(true);
      dmxReceiver->addLight(new P44lrgLight(ledChainArrangement->getRootView(), r));
      LOG(LOG_INFO, "lrg status: %s", rootView->viewStatus()->json_c_str());
    }
    else {
      LOG(LOG_ERR,"cannot create LED chain arrangement");
    }
    #endif // !TEST
    // start scanning BLE advertisements
    BtAdvertisementReceiver::sharedReceiver().start(boost::bind(&P44HelloWorld::gotAdvertisement, this, _1, _2));
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
    conn->setMessageHandler(boost::bind(&P44HelloWorld::gotMessage, this, _1, _2));
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


  #if TEST

  void count(MLTimer &aTimer)
  {
    int ledidx = counter % NUM_LEDS;
    ledChain->clear();
    ledChain->setColor(ledidx, 255-(counter & 0xFF), counter & 0xFF, 0);
    ledChain->show();
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 1*Second);
    LOG(LOG_NOTICE, "Hello World #%d", counter);
    #ifdef ESP_PLATFORM
    LOG(LOG_INFO, "- app mem usage = %zd", memAtStart-heap_caps_get_free_size(MALLOC_CAP_8BIT));
    #endif
    counter++;
  }

  #endif // TEST

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
  static P44HelloWorld application;
  // pass control to app object
  return application.main(0, NULL);
}
