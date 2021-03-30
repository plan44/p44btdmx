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

#include "pwmlight.hpp"
#include "colorutils.hpp"

using namespace p44;

// MARK: - PWM RGB light

PWMLight::PWMLight(AnalogIoPtr aRedOut, AnalogIoPtr aGreenOut, AnalogIoPtr aBlueOut) :
  colorOutput(aRedOut, aGreenOut, aBlueOut, NULL, NULL)
{
  colorOutput.isMemberVariable();
}

PWMLight::~PWMLight()
{
}


void PWMLight::setPowerLimit(int aMilliWatts)
{
  colorOutput.setPowerLimit(aMilliWatts);
}


int PWMLight::getPowerLimit()
{
  return colorOutput.getPowerLimit();
}


int PWMLight::getCurrentPower()
{
  return colorOutput.getCurrentPower();
}


int PWMLight::getNeededPower()
{
  return colorOutput.getNeededPower();
}


void PWMLight::setChannelPowers(int aRedMilliWatts, int aGreenMilliWatts, int aBlueMilliWatts)
{
  colorOutput.mOutputMilliWatts[0] = aRedMilliWatts;
  colorOutput.mOutputMilliWatts[1] = aGreenMilliWatts;
  colorOutput.mOutputMilliWatts[2] = aBlueMilliWatts;
}



// light layout: HSB
// 0: channel hue
// 1: channel saturation
// 2: channel brightness
// 3,4: unused
// 5: speed
// 6: pulse amplitude
// 7: mode (4=pulsing, all others steady)


bool PWMLight::applyChannels()
{
  uint8_t mode = channels[7].pending;
  bool animationChanged = false;
  if (
    (channels[0].pending!=channels[0].current) ||
    (channels[1].pending!=channels[1].current) ||
    (channels[2].pending!=channels[2].current)
  ) {
    // need updating RGB outputs
    // - convert to RGB
    FOCUSLOG("Setting PWM light to H=%d, S=%d, V=%d", channels[0].pending, channels[1].pending, channels[2].pending);
    Row3 HSV;
    HSV[0] = (double)channels[0].pending/255*360;
    HSV[1] = (double)channels[1].pending/255;
    HSV[2] = (double)channels[2].pending/255;
    colorOutput.setHSV(HSV);
    animationChanged = true; // base color change also changes animator
  }
  // speed
  if (channels[5].pending!=channels[5].current) {
    animationChanged = true;
  }
  // amplitude
  if (channels[6].pending!=channels[6].current) {
    animationChanged = true;
  }
  // mode
  if (mode!=channels[7].current) {
    if (mAnimator) {
      mAnimator->stop(false);
      mAnimator.reset();
    }
    switch(mode) {
      case 4: {
        // pulse animation
        double v;
        mAnimator = ValueAnimatorPtr(new ValueAnimator(
          colorOutput.getColorComponentSetter("brightness", v),
          true // self timed
        ));
        mAnimator->function("easeinout");
        animationChanged = true;
        break;
      }
      case 7: {
        // hue animation
        double v;
        mAnimator = ValueAnimatorPtr(new ValueAnimator(
          colorOutput.getColorComponentSetter("hue", v),
          true // self timed
        ));
        mAnimator->function("linear");
        animationChanged = true;
        break;
      }
      default:
        break;
    }
  }
  // (re)start animation
  if (mAnimator && animationChanged) {
    switch (mode) {
      case 4: {
        // brightness changing from current value to currentvalue +/- gradient channel value
        double current = (double)channels[2].pending/255; // 0..1
        mAnimator->repeat(true, 0)->from(current)->animate(current+(double)channels[6].pending/128-1, (MLMicroSeconds)(255-channels[5].pending)*4900*MilliSecond/255 + 100*MilliSecond); // 5..0.1 seconds
        break;
      }
      case 7: {
        // hue changing from current value to currentvalue +/- gradient channel value
        double current = (double)channels[0].pending/255*360; // 0..360
        mAnimator->repeat(true, 0)->from(current)->animate(current+(double)channels[6].pending/128*360-360, (MLMicroSeconds)(255-channels[5].pending)*4900*MilliSecond/255 + 100*MilliSecond); // 5..0.1 seconds
        break;
      }
      default:
        break;
    }
  }
  // confirm apply
  return inherited::applyChannels();
}

