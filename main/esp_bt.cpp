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

#include "esp_bt.hpp"

#include "nvs_flash.h"

using namespace p44;

#ifdef ESP_PLATFORM

// MARK: - Bluetooth advertisement receiver

static BtAdvertisementReceiver* sharedReceiverP = NULL;


BtAdvertisementReceiver& BtAdvertisementReceiver::sharedReceiver()
{
  if (!sharedReceiverP) {
    sharedReceiverP = new BtAdvertisementReceiver();
  }
  return *sharedReceiverP;
}


BtAdvertisementReceiver::BtAdvertisementReceiver() :
  mBTInitialized(false)
{
}

BtAdvertisementReceiver::~BtAdvertisementReceiver()
{
  stop();
}


static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  BtAdvertisementReceiver::sharedReceiver().gapCBHandler(event, param);
}


void BtAdvertisementReceiver::gapCBHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  ErrorPtr err;
  FOCUSLOG("esp_gap_cb: event=%d", event);
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      esp_ble_gap_start_scanning(mScanTime);
      break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      // scan start complete event to indicate scan start successfully or failed
      err = EspError::err(param->scan_start_cmpl.status, "BLE scan start failed: ");
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      // adv start complete event to indicate adv start successfully or failed
      err = EspError::err(param->adv_start_cmpl.status, "BLE advertisement start failed: ");
      break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      err = EspError::err(param->scan_stop_cmpl.status, "BLE scan stop failed: ");
      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      err = EspError::err(param->adv_stop_cmpl.status, "BLE advertisement stop failed: ");
      break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      // scan result received
      esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
      FOCUSLOG("esp_gap_cb: ESP_GAP_BLE_SCAN_RESULT_EVT: search_evt=%d", scan_result->scan_rst.search_evt);
      switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          // get data
          string advData;
          advData.assign((const char *)scan_result->scan_rst.ble_adv, (size_t)scan_result->scan_rst.adv_data_len);
          if (mAdvertisementCB) {
            mAdvertisementCB(ErrorPtr(), advData);
          }
          return; // done
        }
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  if (Error::notOK(err) && mAdvertisementCB) {
    FOCUSLOG("GAP event handler Error: %s", err->text());
    mAdvertisementCB(err, "");
  }
}


static esp_ble_scan_params_t ble_scan_params = {
  .scan_type              = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval          = 0x50, // 0x10 = 10mS (scan interval = N*0.625mS), can be 0x0004..0x4000
  .scan_window            = 0x30, // 0x10 = 10mS (must be less than interval)
  .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE // link layer reports all packets, including duplicates
};


ErrorPtr BtAdvertisementReceiver::start(BTAdvertisementCB aAdvertisementCB, uint32_t aScanTime)
{
  mScanTime = aScanTime;
  mAdvertisementCB = aAdvertisementCB;
  ErrorPtr err;
  if (!mBTInitialized) {
    err = EspError::err(nvs_flash_init(), "nvs_flash_init: ");
    if (Error::isOK(err)) {
      err = EspError::err(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT), "bt classic mem release: ");
    }
    if (Error::isOK(err)) {
      // init the controller
      esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
      esp_bt_controller_init(&bt_cfg);
      esp_bt_controller_enable(ESP_BT_MODE_BLE);
      // init the stack
      esp_bluedroid_init();
      esp_bluedroid_enable();
      // init the GAP callback
      err = EspError::err(esp_ble_gap_register_callback(esp_gap_cb), "register GAP callback: ");
    }
    mBTInitialized = Error::isOK(err);
  }
  // now set up scanning for advertisements
  if (Error::isOK(err)) {
    esp_ble_gap_set_scan_params(&ble_scan_params);
  }

  return err;
}


void BtAdvertisementReceiver::stop()
{
  mAdvertisementCB = NULL;
  esp_ble_gap_stop_scanning();
}


#else // ESP_PLATFORM
  #warning "esp_bt only works for ESP32 IDF builds"
#endif // !ESP_PLATFORM
