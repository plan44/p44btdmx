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
#define FOCUSLOGLEVEL 0

#include "p44lrglight.hpp"


using namespace p44;

// MARK: - ledchain "feature" Lightsegment

P44lrgLight::P44lrgLight(P44ViewPtr aRootView, PixelRect aFrame)
{
  FOCUSLOG("P44lrgLight created on mainloop@%p", &MainLoop::currentMainLoop());
  mLightView = LightSpotViewPtr(new LightSpotView);
  mLightView->setZOrder(mLocalLightNumber);
  mLightView->setFrame(aFrame);
  mLightView->setFullFrameContent();
  mLightView->setForegroundColor(black);
  mLightView->setBackgroundColor(transparent);
  mLightView->setRelativeContentOrigin(0,0,true); // center
  mLightView->setRelativeExtent(1); // full frame
  mLightView->setLabel(string_format("P44lrgLight@%p",this));
  // add to root view
  aRootView->addSubView(mLightView);
  OLOG(LOG_INFO, "view hierarchy: %s", aRootView->viewStatus()->json_c_str());
}

P44lrgLight::~P44lrgLight()
{
}


// light layout: HSB + extras: 8 channels
// 0: channel hue
// 1: channel saturation
// 2: channel brightness
// 3: channel position
// 4: channel size
// 5: channel effect specific: speed?
// 6: channel effect specific: gradient?
// 7: channel mode

void P44lrgLight::applyChannels()
{
  uint8_t mode = channels[7].pending;
  bool animationChanged = false;
  // - convert HSV to Pixel
  PixelColor col = hsbToPixel(
    (double)channels[0].pending/255*360,
    (double)channels[1].pending/255,
    (double)channels[2].pending/255,
    true // brightness as alpha, full RGB value
  );
  // check what to update
  if (
    (channels[0].pending!=channels[0].current) || // H
    (channels[1].pending!=channels[1].current) || // S
    (channels[2].pending!=channels[2].current) || // V
    (channels[7].pending!=channels[7].current)    // mode
  ) {
    // need updating RGB outputs
    if (channels[7].pending!=channels[7].current) {
      // mode change, including color
      // - stop animation, reset alpha
      mLightView->stopAnimations();
      mAnimation.reset();
      mLightView->setAlpha(255);
      // - switch mode
      switch(mode) {
        default:
        case 0: {
          // full size light with hard edges
          mLightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          mLightView->setRelativeExtent(2);
          mLightView->setWrapMode(P44View::clipXY);
          break;
        }
        case 1: {
          // sizable light with hard edges
          mLightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
          mLightView->setWrapMode(P44View::clipXY);
          mLightView->setRelativeExtent((double)channels[4].pending/255);
          break;
        }
        case 2: {
          // sizable with tunable soft edge
          mLightView->setColoringParameters(col, (double)channels[6].pending/64-2, gradient_curve_lin, 0, gradient_none, 0, gradient_none, false);
          mLightView->setWrapMode(P44View::clipXY);
          mLightView->setRelativeExtent((double)channels[4].pending/255);
          break;
        }
        case 3: {
          // sizable with tunable color gradient
          mLightView->setColoringParameters(col, 0, gradient_none, (double)channels[6].pending/64-2, gradient_curve_lin+gradient_repeat_oscillating, 0, gradient_none, false);
          mLightView->setWrapMode(P44View::clipXY);
          mLightView->setRelativeExtent((double)channels[4].pending/255);
          break;
        }
        case 4: {
          // pulsing with fixed soft edge, gradient and speed determine amplitude and interval
          mLightView->setColoringParameters(col, -0.9, gradient_curve_lin, 0, gradient_none, 0, gradient_none, false);
          mLightView->setWrapMode(P44View::clipXY);
          mLightView->setRelativeExtent((double)channels[4].pending/255);
          // install new animation
          mAnimation = mLightView->animatorFor("alpha");
          mAnimation->function("easeinout");
          animationChanged = true;
          break;
        }
        case 5:
          // fixed soft edge, moving in x direction, gradient and speed determine amplitude and interval
          mLightView->setColoringParameters(col, -0.9, gradient_curve_lin, 0, gradient_none, 0, gradient_none, false);
          goto mover;
        case 6:
          // hard edge, moving in x direction, gradient and speed determine amplitude and interval
          mLightView->setColoringParameters(col, 0, gradient_none, 0, gradient_none, 0, gradient_none, false);
        mover: {
          mLightView->setWrapMode(P44View::clipXY);
          mLightView->setRelativeExtent((double)channels[4].pending/255);
          // install new animation
          mAnimation = mLightView->animatorFor("content_x");
          mAnimation->function("easeinout");
          animationChanged = true;
          break;
        }
      }
      OLOG(LOG_INFO,"Mode and/or color change");
      OLOG(LOG_INFO,"- new view hierarchy: %s", mLightView->getParent()->viewStatus()->json_c_str());
    }
    else {
      // just color
      OLOG(LOG_INFO,"Color only change");
      mLightView->setForegroundColor(col);
    }
  }
  // Position
  if (
    (channels[3].pending!=channels[3].current)
  ) {
    OLOG(LOG_INFO,"Position change");
    mLightView->setRelativeContentOrigin((double)channels[3].pending/128-1, 0, true);
    if (mode==5 || mode==6) {
      // change of position needs restart of (positional) animation
      animationChanged = true;
    }
  }
  // Size
  if (
    (channels[4].pending!=channels[4].current)
  ) {
    // to make sure light works out of the box, mode 0 actively suppresses size changes!
    if (mode!=0) {
      OLOG(LOG_INFO,"Size change");
      mLightView->setRelativeExtent((double)channels[4].pending/255);
    }
  }
  // Speed
  if (
    (channels[5].pending!=channels[5].current) // (animation) speed change
  ) {
    animationChanged = true;
  }
  // Gradient/Effect param
  if (
    (channels[6].pending!=channels[6].current) // mode dependent gradient
  ) {
    OLOG(LOG_INFO,"Feature param / Gradient change");
    switch(mode) {
      case 2: {
        // sizable soft edged
        mLightView->setColoringParameters(col, (double)channels[6].pending/64-2, gradient_curve_lin, 0, gradient_none, 0, gradient_none, false);
        break;
      }
      case 3: {
        // sizable with tunable color gradient
        mLightView->setColoringParameters(col, 0, gradient_none, (double)channels[6].pending/64-2, gradient_curve_lin+gradient_repeat_oscillating, 0, gradient_none, false);
        break;
      }
      default:
        // assume related to animation
        animationChanged = true;
        break;
    }
  }
  // (re)start animation
  if (mAnimation && animationChanged) {
    switch (mode) {
      case 4: {
        // intensity 0..255, changing from current value to currentvalue +/- gradient channel value
        mAnimation->repeat(true, 0)->from(channels[2].pending)->animate(channels[2].pending+channels[6].pending*2-255, (MLMicroSeconds)(255-channels[5].pending)*4900*MilliSecond/255 + 100*MilliSecond); // 5..0.1 seconds
        break;
      }
      case 5:
      case 6: {
        // position 0..255, changing from current x to x +/- frame.x size scaled by gradient channel value
        // - set start poition
        mLightView->setRelativeContentOrigin((double)channels[3].pending/128-1, 0, true);
        PixelRect content = mLightView->getContent();
        PixelRect frame = mLightView->getFrame();
        OLOG(LOG_INFO, "mLightView: %s", mLightView->viewStatus()->json_c_str());
        // - animate from there +/- the frame size
        mAnimation->repeat(true, 0)->from(content.x)->animate(content.x+channels[6].pending*frame.dx*2/255-frame.dx, (MLMicroSeconds)(255-channels[5].pending)*4900*MilliSecond/255 + 100*MilliSecond); // 5..0.1 seconds
        break;
      }
      default:
        break;
    }
  }
  // request LED update
  FOCUSLOG("will request update, mainloop@%p", &MainLoop::currentMainLoop());
  mLightView->requestUpdateIfNeeded();
  FOCUSLOG("did request update");
  // confirm apply
  inherited::applyChannels();
}
