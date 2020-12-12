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


using namespace std;

namespace p44 {

  class PWMLight : public P44DMXLight
  {
    typedef P44DMXLight inherited;

    AnalogIoPtr mRedOut;
    AnalogIoPtr mGreenOut;
    AnalogIoPtr mBlueOut;

  public:
    PWMLight(AnalogIoPtr aRedOut, AnalogIoPtr aGreenOut, AnalogIoPtr aBlueOut);
    virtual ~PWMLight();

    /// apply channel values
    /// @note base class just confirms apply by updating "current" field from "pending" in internal channel data
    virtual void applyChannels() P44_OVERRIDE;

  };

} // namespace p44


#endif /* defined(__p44utils__pwmlight__) */
