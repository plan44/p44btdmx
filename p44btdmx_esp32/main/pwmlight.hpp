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


#ifndef __p44utils__pwmlight__
#define __p44utils__pwmlight__

#include "p44utils_common.hpp"
#include "p44btdmx.hpp"
#include "analogio.hpp"
#include "valueanimator.hpp"


using namespace std;

namespace p44 {

  class PWMLight : public P44DMXLight
  {
    typedef P44DMXLight inherited;

    AnalogColorOutput colorOutput;

    ValueAnimatorPtr mAnimator;

  public:
    PWMLight(AnalogIoPtr aRedOut, AnalogIoPtr aGreenOut, AnalogIoPtr aBlueOut);
    virtual ~PWMLight();

    /// get current power
    /// @return power in milliwatts
    int getCurrentPower();

    /// Return the power it *would* need to display the current state (altough power limiting might actually reducing it)
    /// @return how many milliwatts (approximatively) the color light would use if not limited
    int getNeededPower();

    /// set power limit
    /// @param aMilliWatts max power in milliwatts
    void setPowerLimit(int aMilliWatts);

    /// get current power limit
    /// @return currently set power limit in milliwatts, 0=no limit
    int getPowerLimit();

    /// set powers of the channels (when set to 100% intensity)
    void setChannelPowers(int aRedMilliWatts, int aGreenMilliWatts, int aBlueMilliWatts);

    /// apply channel values
    /// @note base class just confirms apply by updating "current" field from "pending" in internal channel data
    virtual bool applyChannels() P44_OVERRIDE;

  };
  typedef boost::intrusive_ptr<PWMLight> PWMLightPtr;

} // namespace p44


#endif /* defined(__p44utils__pwmlight__) */
