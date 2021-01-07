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
  mRedOut(aRedOut),
  mGreenOut(aGreenOut),
  mBlueOut(aBlueOut),
  mModulator(1)
{
}

PWMLight::~PWMLight()
{
}

// light layout: HSB
// 0: channel hue
// 1: channel saturation
// 2: channel brightness
// 3,4: unused
// 5: speed
// 6: pulse amplitude
// 7: mode (4=pulsing, all others steady)


void PWMLight::applyChannels()
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
    updatePWM();
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
    mAnimator.reset();
    switch(mode) {
      case 4:
        // pulse animation
        mAnimator = ValueAnimatorPtr(new ValueAnimator(
          boost::bind(&PWMLight::modulate, this, _1),
          true // self timed
        ));
        mAnimator->function("easeinout");
        animationChanged = true;
        break;
      default:
        mModulator = 1;
        break;
    }
  }
  // (re)start animation
  if (mAnimator && animationChanged) {
    switch (mode) {
      case 4:
        // intensity 0..255, changing from current value to currentvalue +/- gradient channel value
        mAnimator->repeat(true, 0)->from(channels[2].pending)->animate(channels[2].pending+channels[6].pending*2-255, (MLMicroSeconds)(255-channels[5].pending)*4900*MilliSecond/255 + 100*MilliSecond); // 5..0.1 seconds
        break;
      default:
        break;
    }
  }
  // confirm apply
  inherited::applyChannels();
}


void PWMLight::updatePWM()
{
  Row3 HSV;
  HSV[0] = (double)channels[0].pending/255*360;
  HSV[1] = (double)channels[1].pending/255;
  HSV[2] = (double)channels[2].pending/255*mModulator;
  FOCUSLOG("Setting PWM light to H=%.2f, S=%.2f, V=%.2f", HSV[0], HSV[1], HSV[2]);
  Row3 RGB;
  HSVtoRGB(HSV, RGB);
  // - set PWM outputs
  FOCUSLOG("Setting PWM light to R=%.2f, G=%.2f, B=%.2f", RGB[0], RGB[1], RGB[2]);
  mRedOut->setValue(RGB[0]*100);
  mGreenOut->setValue(RGB[1]*100);
  mBlueOut->setValue(RGB[2]*100);
}


void PWMLight::modulate(double aValue)
{
  // input is 0..255
  mModulator = aValue/255;
  updatePWM();
}
