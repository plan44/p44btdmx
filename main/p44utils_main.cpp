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

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

using namespace p44;

static size_t memAtStart = 0;

class P44HelloWorld : public Application
{
  typedef Application inherited;

  MLTicket counterTicket;
  int counter;
  LEDChainCommPtr ledChain;
  SocketCommPtr apiServer;

public:

  P44HelloWorld() :
    counter(0)
  {
  }

  virtual int main(int argc, char **argv)
  {
    if (!isTerminated()) {
      SETLOGLEVEL(LOG_INFO);
      SETERRLEVEL(LOG_ERR, false);
      SETDELTATIME(false);
    } // if !terminated
    // app now ready to run (or cleanup when already terminated)
    return run();
  }

  #define NUM_LEDS 10

  virtual void initialize()
  {
    LOG(LOG_NOTICE,"initialize");
    // socket
    apiServer = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
    apiServer->setConnectionParams(NULL, "8842", SOCK_STREAM, PF_UNSPEC);
    apiServer->setAllowNonlocalConnections(true);
    ErrorPtr err = apiServer->startServer(boost::bind(&P44HelloWorld::apiConnectionHandler, this, _1), 10);
    // ledchain
    ledChain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, "gpio19", NUM_LEDS));
    ledChain->begin();
    // second counter
    counterTicket.executeOnce(boost::bind(&P44HelloWorld::count, this, _1));
  }


  SocketCommPtr apiConnectionHandler(SocketCommPtr aServerSocketCommP)
  {
//    SocketCommPtr conn = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
//    conn->setReceiveHandler(boost::bind(&P44HelloWorld::gotData, this, conn, _1));
    JsonCommPtr conn = JsonCommPtr(new JsonComm(MainLoop::currentMainLoop()));
    conn->setMessageHandler(boost::bind(&P44HelloWorld::gotMessage, this, _1, _2));
    return conn;
  }

  void gotData(SocketCommPtr aConn, ErrorPtr aError)
  {
    if (!Error::isOK(aError)) {
      LOG(LOG_ERR, "Error: %s", Error::text(aError).c_str());
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
      LOG(LOG_ERR, "Error: %s", Error::text(aError).c_str());
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
