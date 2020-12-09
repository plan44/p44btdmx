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


#ifndef __p44utils__espbt__
#define __p44utils__espbt__

#include "p44utils_common.hpp"

#ifdef ESP_PLATFORM

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"


using namespace std;

namespace p44 {


  typedef boost::function<void (ErrorPtr aError, const string aAdvData)> BTAdvertisementCB;

  class BtAdvertisementReceiver : public P44LoggingObj
  {

    bool mBTInitialized;
    BTAdvertisementCB mAdvertisementCB;
    uint32_t mScanTime; // how long to keep scanning, 0=forever

    BtAdvertisementReceiver();
    virtual ~BtAdvertisementReceiver();

  public:
    friend void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    /// access to singleton
    static BtAdvertisementReceiver& sharedReceiver();

    /// start receiving BT advertisements
    /// @param aAdvertisementCB is called with advertisement data when some is received
    /// @param aScanTime how long to keep scanning, default=0=forever
    /// @return NULL if ok or error
    ErrorPtr start(BTAdvertisementCB aAdvertisementCB, uint32_t aScanTime = 0);

    /// stop receiving advertisements
    void stop();

    /// @return prefix for log messages
    virtual string logContextPrefix() P44_OVERRIDE { return "BT Advertisement Receiver"; };

    /// utility for dissecting BT advertisements
    /// @param aAdvData pointer to advertisement data
    /// @param aType the type of AD struct to find
    /// @param aStructData receives pointer to structure data when function returns true
    /// @param aStructLen receives the length of the structure data (w/o type) when function returns true
    /// @return true if AD struct found
    static bool findADStruct(const uint8_t* aAdvData, uint8_t aType, const uint8_t* &aStructData, uint8_t &aStructLen);

    void gapCBHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param); /// semantically privat

  };



} // namespace p44

#endif // ESP_PLATFORM

#endif /* defined(__p44utils__espbt__) */
