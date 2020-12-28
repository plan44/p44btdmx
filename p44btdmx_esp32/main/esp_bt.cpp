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
#include "application.hpp"

#include "nvs_flash.h"

using namespace p44;

#ifdef ESP_PLATFORM

// MARK: - Bluetooth advertisement receiver

static BtAdvertisements* sharedAdvertisementsInstanceP = NULL;


BtAdvertisements& BtAdvertisements::sharedInstance()
{
  if (!sharedAdvertisementsInstanceP) {
    sharedAdvertisementsInstanceP = new BtAdvertisements();
  }
  return *sharedAdvertisementsInstanceP;
}


BtAdvertisements::BtAdvertisements() :
  mBTInitialized(false)
{
}

BtAdvertisements::~BtAdvertisements()
{
  stopAdvertising();
  stopScanning();
}


static esp_ble_scan_params_t ble_scan_params = {
  .scan_type              = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval          = 0x20, // 0x10 = 10mS (scan interval = N*0.625mS), can be 0x0004..0x4000
  .scan_window            = 0x18, // 0x10 = 10mS (must be less than interval)
  .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE // link layer reports all packets, including duplicates
};


static esp_ble_adv_params_t ble_adv_params = {
  .adv_int_min        = 0x20,
  .adv_int_max        = 0x40,
  .adv_type           = ADV_TYPE_NONCONN_IND,
  .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
  .channel_map        = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  BtAdvertisements::sharedInstance().gapCBHandler(event, param);
}


void BtAdvertisements::gapCBHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  ErrorPtr err;
  FOCUSLOG("esp_gap_cb: event=%d", event);
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      esp_ble_gap_start_advertising(&ble_adv_params);
      break;
    }
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
      if (mAdvertisingStartedCB) {
        Application::sharedApplication()->mainLoop().executeNowFromForeignTask(
          boost::bind(&BtAdvertisements::startedCallback, mAdvertisingStartedCB, err)
        );
      }
      return;
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
          deliverAdvertisement(ErrorPtr(), advData);
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
  if (Error::notOK(err)) {
    FOCUSLOG("GAP event handler Error: %s", err->text());
    deliverAdvertisement(err, "");
  }
}


void BtAdvertisements::deliverAdvertisement(ErrorPtr aError, const string aAdvData)
{
  if (mAdvertisementCB) {
    FOCUSLOG("posting Advertisement handler execution from mainloop@%p", &MainLoop::currentMainLoop());
    // make sure this executes on the main thread
    Application::sharedApplication()->mainLoop().executeNowFromForeignTask(
      boost::bind(&BtAdvertisements::deliveryCallback, mAdvertisementCB, aError, aAdvData)
    );
  }
}


void BtAdvertisements::deliveryCallback(BTAdvertisementCB aCallback, ErrorPtr aError, const string aAdvData)
{
  FOCUSLOG("calling Advertisement handler in mainloop@%p", &MainLoop::currentMainLoop());
  aCallback(aError, aAdvData);
}


void BtAdvertisements::startedCallback(StatusCB aCallback, ErrorPtr aError)
{
  FOCUSLOG("calling advertisement started handler in mainloop@%p", &MainLoop::currentMainLoop());
  aCallback(aError);
}



ErrorPtr BtAdvertisements::initBLE()
{
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
      // set highest tx power for advertisements
      err = EspError::err(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9), "setting tx power: ");
    }
    if (Error::isOK(err)) {
      // init the GAP callback
      err = EspError::err(esp_ble_gap_register_callback(esp_gap_cb), "register GAP callback: ");
    }
    mBTInitialized = Error::isOK(err);
  }
  return err;
}


ErrorPtr BtAdvertisements::startScanning(BTAdvertisementCB aAdvertisementCB, uint32_t aScanTime)
{
  mScanTime = aScanTime;
  mAdvertisementCB = aAdvertisementCB;
  ErrorPtr err = initBLE();
  // now set up scanning for advertisements
  if (Error::isOK(err)) {
    esp_ble_gap_set_scan_params(&ble_scan_params);
  }
  return err;
}


void BtAdvertisements::stopScanning()
{
  mAdvertisementCB = NULL;
  esp_ble_gap_stop_scanning();
}



ErrorPtr BtAdvertisements::startAdvertising(StatusCB aAdvertisingCB, const string aAdvData)
{
  ErrorPtr err = initBLE();
  // now set up advertising
  if (Error::isOK(err)) {
    stopAdvertising();
    mAdvertisingStartedCB = aAdvertisingCB;
    ErrorPtr err = EspError::err(esp_ble_gap_config_adv_data_raw((uint8_t*)aAdvData.c_str(), aAdvData.size()), "setting advertisement raw data: ");
  }
  return err;
}


void BtAdvertisements::stopAdvertising()
{
  mAdvertisingStartedCB = NULL;
  esp_ble_gap_stop_advertising();
}


// MARK: - Advertisement decoding utilities

// - BT Advertisement data (AdvData) consists of 0..31 bytes (plus header containing randomized BT address)
//   See BT core specs 2.3.1 "Advertisement PDUs"

// - AdvData (0..31 bytes) data format consists of one or multiple "AD Structures" (BT core specs, 11, figure 11.1)

// - AD Structures consist of a length byte, followed by a type byte, followed by type-specific data

// - The AD Type byte numbers are specified in https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
// - The AD types are described in the BT core spec supplement, chapter 1.


bool BtAdvertisements::findADStruct(const uint8_t* aAdvData, uint8_t aType, const uint8_t* &aStructData, uint8_t &aStructLen)
{
  const uint8_t maxAdvDataLen = 31;
  uint8_t idx = 0;
  while (idx<maxAdvDataLen) {
    // length byte
    uint8_t ln = aAdvData[idx];
    if (ln<1 || idx+ln>maxAdvDataLen) break; // invalid
    idx++;
    // type byte
    if (aAdvData[idx]==aType) {
      // found requested structure type
      idx++;
      aStructLen = ln-1;
      aStructData = aAdvData+idx;
      return true;
    }
  }
  return false; // not found
}


#else // ESP_PLATFORM
  #warning "esp_bt only works for ESP32 IDF builds"
#endif // !ESP_PLATFORM
