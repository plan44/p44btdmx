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

#include "p44lrglight.hpp"


using namespace p44;

// MARK: - PWM RGB light

P44lrgLight::P44lrgLight(P44ViewPtr aRootView, PixelRect aFrame)
{
  FOCUSLOG("P44lrgLight created on mainloop@%p", &MainLoop::currentMainLoop());
  lightView = LightSpotViewPtr(new LightSpotView);
  lightView->setZOrder(mLocalLightNumber);
  lightView->setFrame(aFrame);
  lightView->setFullFrameContent();
  lightView->setForegroundColor(black);
  lightView->setBackgroundColor(transparent);
  lightView->setRelativeContentOrigin(0,0,true); // center
  lightView->setRelativeExtent(1); // full frame
  lightView->setLabel(string_format("P44lrgLight@%p",this));
  // add to root view
  aRootView->addSubView(lightView);
  OLOG(LOG_INFO, "view hierarchy: %s", aRootView->viewStatus()->json_c_str());
}

P44lrgLight::~P44lrgLight()
{
}


void P44lrgLight::applyChannels()
{
  if (
    (channels[0].pending!=channels[0].current) || // H
    (channels[1].pending!=channels[1].current) || // S
    (channels[2].pending!=channels[2].current) || // V
    (channels[4].pending!=channels[4].current)    // mode
  ) {
    // need updating RGB outputs
    // - convert to Pixel
    PixelColor col = hsbToPixel(
      (double)channels[0].pending/255*360,
      (double)channels[1].pending/255,
      (double)channels[2].pending/255,
      true // brightness as alpha, full RGB value
    );
    if (channels[4].pending!=channels[4].current) {
      // mode change, including color
      uint8_t mode = (channels[4].pending>>4) & 0xF;
      uint8_t param = channels[4].pending & 0xF;
      switch(mode) {
        default:
        case 0: {
          // full size light with hard edges
          lightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent(2);
          lightView->setWrapMode(P44View::clipXY);
          break;
        }
        case 1: {
          // sizable light with hard edges
          lightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent((double)param/16);
          lightView->setWrapMode(P44View::clipXY);
          break;
        }
        case 2: {
          // sizable soft edged
          lightView->setColoringParameters(col, -1, gradient_curve_cos, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent((double)param/16);
          lightView->setWrapMode(P44View::clipXY);
          break;
        }
        case 3: {
          // tunable color gradient
          lightView->setColoringParameters(col, 0, gradient_none, (double)param/8-1, gradient_curve_lin+gradient_repeat_oscillating, 0, gradient_none, false);
          lightView->setRelativeExtent(1); // full frame
          lightView->setWrapMode(P44View::clipXY);
          break;
        }
      }
    }
    else {
      // just color
      OLOG(LOG_INFO,"Color change");
      lightView->setForegroundColor(col);
    }
  }
  if (
    (channels[3].pending!=channels[3].current) // position
  ) {
    OLOG(LOG_INFO,"Position change");
    lightView->setRelativeContentOrigin((double)channels[3].pending/128-1, 0, true);
  }
  // request update
  FOCUSLOG("will request update, mainloop@%p", &MainLoop::currentMainLoop());
  lightView->requestUpdateIfNeeded();
  FOCUSLOG("did request update");
  // confirm apply
  inherited::applyChannels();
}
