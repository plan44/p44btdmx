//
//  Copyright (c) 2019 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//

#include <stdio.h>

#include "application.hpp"

#include "ledchaincomm.hpp"


using namespace p44;

class P44HelloWorld : public Application
{
  typedef Application inherited;

  MLTicket counterTicket;
  int counter;
  LEDChainCommPtr ledChain;

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

  virtual void initialize()
  {
    LOG(LOG_NOTICE,"initialize");
    ledChain = LEDChainCommPtr(new LEDChainComm(LEDChainComm::ledtype_ws281x, "gpio19", 10));
    ledChain->begin();
    counterTicket.executeOnce(boost::bind(&P44HelloWorld::count, this, _1));
  }

  void count(MLTimer &aTimer)
  {
    ledChain->clear();
    ledChain->setColor(counter, 255-counter*10, counter*10, 0);
    ledChain->show();
    MainLoop::currentMainLoop().retriggerTimer(aTimer, 1*Second);
    LOG(LOG_NOTICE, "Hello World #%d", counter++);
    if (counter>10) {
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
