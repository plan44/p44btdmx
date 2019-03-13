//
//  Copyright (c) 2019 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//

#include <stdio.h>

#include "application.hpp"

#include "ledchaincomm.hpp"
#include "socketcomm.hpp"


using namespace p44;

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
    SocketCommPtr conn = SocketCommPtr(new SocketComm(MainLoop::currentMainLoop()));
    conn->setReceiveHandler(boost::bind(&P44HelloWorld::gotData, this, conn, _1));
    return conn;
  }

  void gotData(SocketCommPtr aConn, ErrorPtr aError)
  {
    if (Error::isOK(aError)) {
      string s;
      aConn->receiveIntoString(s);
      LOG(LOG_NOTICE, "Got data: %s", s.c_str());
    }
  }


  void count(MLTimer &aTimer)
  {
    int ledidx = counter % NUM_LEDS;
    ledChain->clear();
    ledChain->setColor(counter, 255-counter, counter, 0);
    ledChain->show();
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 1*Second);
    LOG(LOG_NOTICE, "Hello World #%d", counter++);
    if (counter>100) {
      terminateApp(EXIT_SUCCESS);
    }
  }

};


int main(int argc, char **argv)
{
  // prevent debug output before application.main scans command line
  SETLOGLEVEL(LOG_EMERG);
  SETERRLEVEL(LOG_EMERG, false); // messages, if any, go to stderr
  // create app with current mainloop
  static P44HelloWorld application;
  // pass control to app object
  return application.main(0, NULL);
}
