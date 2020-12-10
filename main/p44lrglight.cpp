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
  lightView = LightSpotViewPtr(new LightSpotView);
  lightView->setZOrder(mLocalLightNumber);
  lightView->setFrame(aFrame);
  lightView->setFullFrameContent();
  lightView->setForegroundColor(black);
  lightView->setBackgroundColor(transparent);
  lightView->setRelativeContentOrigin(0,0,true); // center
  lightView->setRelativeExtent(1); // full frame
  // add to root view
  aRootView->addSubView(lightView);
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
      (double)channels[0].pending/255,
      (double)channels[0].pending/255,
      true // brightness as alpha, full RGB value
    );
    if (channels[4].pending!=channels[2].current) {
      // mode change, including color
      switch(channels[4].pending) {
        default:
        case 0: {
          // just fixed light with hard edges
          lightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent(1); // full frame
          break;
        }
        case 1: {
          // soft edged small
          lightView->setColoringParameters(col, -1, gradient_curve_cos, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent(0.5); // half frame
          break;
        }
        case 2: {
          // soft edged large
          lightView->setColoringParameters(col, -1, gradient_curve_cos, 0, gradient_none, 0, gradient_none, false);
          lightView->setRelativeExtent(1); // full frame
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
    lightView->setRelativeContentOrigin((double)channels[3].pending/255-0.5 ,0,true);
  }
  // request update
  OLOG(LOG_INFO,"will request update");
  lightView->requestUpdateIfNeeded();
  OLOG(LOG_INFO,"did request update");
  // confirm apply
  inherited::applyChannels();
}
