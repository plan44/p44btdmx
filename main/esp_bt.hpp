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

    void gapCBHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  };



} // namespace p44


#if 0
/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/



/****************************************************************************
*
* This file is for iBeacon demo. It supports both iBeacon sender and receiver
* which is distinguished by macros IBEACON_SENDER and IBEACON_RECEIVER,
*
* iBeacon is a trademark of Apple Inc. Before building devices which use iBeacon technology,
* visit https://developer.apple.com/ibeacon/ to obtain a license.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ibeacon_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "esp_err.h"
#include "driver/ledc.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "ws2812.h"

#define DIAG 0

static const char* DEMO_TAG = "IBEACON_DEMO";
extern esp_ble_ibeacon_vendor_t vendor_config;


static uint8_t red, green, blue; // current color in PWMs
static int from, to;


static uint8_t r,g,b; // last set color
static uint8_t mode; // last set mode

#define numLeds 255
static rgbVal leds[numLeds];


static void setLEDs(uint8_t aR, uint8_t aG, uint8_t aB, int aFrom, int aTo)
{
  int i;
  for (i = 0; i<numLeds; i++) {
    if (i>=from && i<=to) {
      leds[i] = makeRGBVal(aR, aG, aB);
    }
    else {
      leds[i] = makeRGBVal(0, 0, 0);
    }
  }
  ws2812_setColors(numLeds, leds);
}



static void setupPWMs(int redGpio, int greenGpio, int blueGpio)
{
  esp_err_t err;

  // timer params
  static ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    .timer_num = LEDC_TIMER_0,            // timer index
    .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
  };
  // channel params
  static ledc_channel_config_t ledc_channels[3] = {
    {
      .channel    = LEDC_CHANNEL_0,
      .duty       = 0,
      .gpio_num   = 0,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .hpoint     = 0,
      .timer_sel  = LEDC_TIMER_0
    },
    {
      .channel    = LEDC_CHANNEL_1,
      .duty       = 0,
      .gpio_num   = 0,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .hpoint     = 0,
      .timer_sel  = LEDC_TIMER_0
    },
    {
      .channel    = LEDC_CHANNEL_2,
      .duty       = 0,
      .gpio_num   = 0,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .hpoint     = 0,
      .timer_sel  = LEDC_TIMER_0
    }
  };
  // Set configuration of timer0 for high speed channels
  if ((err = ledc_timer_config(&ledc_timer))!=ESP_OK) ESP_LOGE(DEMO_TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
  // Set up individual PWMs
  ledc_channels[0].gpio_num = redGpio;
  ledc_channels[1].gpio_num = greenGpio;
  ledc_channels[2].gpio_num = blueGpio;
  for (int i=0; i<3; i++) {
    if ((err = ledc_channel_config(&ledc_channels[i]))!=ESP_OK) ESP_LOGE(DEMO_TAG, "ledc_channel_config #%d failed: %s", i, esp_err_to_name(err));
  }
}


static void setDuty(int channel, uint32_t duty13Bit)
{
  ESP_LOGI(DEMO_TAG, "New duty cycle for channel #%d = %d", channel, duty13Bit);
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty13Bit);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}


static void setColor(uint8_t aR, uint8_t aG, uint8_t aB)
{
  int change = 0;
  if (aR!=red) {
    red = aR;
    change = 1;
    setDuty(0, (uint32_t)red<<5);
  }
  if (aG!=green) {
    green = aG;
    change = 1;
    setDuty(1, (uint32_t)green<<5);
  }
  if (aB!=blue) {
    blue = aB;
    change = 1;
    setDuty(2, (uint32_t)blue<<5);
  }
  if (change) {
    setLEDs(aR, aG, aB, from, to);
    ESP_LOGI(DEMO_TAG, "New current RGB color = %d, %d, %d", red, green, blue);
  }
}



///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

#if (IBEACON_MODE == IBEACON_RECEIVER)
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

#elif (IBEACON_MODE == IBEACON_SENDER)
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#endif

// Packet Structure Byte Map[edit]
// Byte 0-2: Standard BLE Flags
//
//  Byte 0: Length :  0x02
//  Byte 1: Type: 0x01 (Flags)
//  Byte 2: Value: 0x06 (Typical Flags)
//
// Byte 3-29: Apple Defined iBeacon Data
//
//  Byte 3: Length: 0x1a
//  Byte 4: Type: 0xff (Manufacturer specific data)
//  Byte 5-6: Manufacturer ID : 0x4c00 (Apple)
//  Byte 7: SubType: 0x02 (iBeacon)
//  Byte 8: SubType Length: 0x15
//  Byte 9-24: Proximity UUID
//  Byte 25-26: Major
//  Byte 27-28: Minor
//  Byte 29: Signal Power


// esp_ble_ibeacon_head_t ibeacon_common_head = {
//     .flags = {0x02, 0x01, 0x06},
//     .length = 0x1A,
//     .type = 0xFF,
//     .company_id = 0x004C,
//     .beacon_type = 0x1502
// };

#define ibeacon_head_len 6
static uint8_t ibeacon_head[ibeacon_head_len] = { 0x1A, 0xFF, 0x4C, 0x00, 0x02, 0x15 };
#define proximityUUID_len 16
static uint8_t proximityUUID[proximityUUID_len] = { 0xB1, 0x6F, 0xC6, 0xBB, 0xD1, 0xD1, 0x42, 0x8A, 0x8C, 0x03, 0x55, 0xBA, 0xD7, 0xF7, 0x04, 0x81 }; // B16FC6BB-D1D1-428A-8C03-55BAD7F70481


static bool is_my_ibeacon_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
      if (adv_data[0]==0x02 && adv_data[1]==0x01) {
        // flag value might not be "typical" 0x06, we've seen 0x1A, so just ignore that one
        if (
          memcmp(adv_data+3, ibeacon_head, ibeacon_head_len)==0 &&
          memcmp(adv_data+3+ibeacon_head_len, proximityUUID, proximityUUID_len)==0
        ) {
          result = true;
        }
      }
    }
    return result;
}



static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    //ESP_LOGI(DEMO_TAG, "esp_gap_cb: event=%d", event);
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
#if (IBEACON_MODE == IBEACON_SENDER)
        esp_ble_gap_start_advertising(&ble_adv_params);
#endif
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
#if (IBEACON_MODE == IBEACON_RECEIVER)
        //the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
#endif
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Adv start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        //ESP_LOGI(DEMO_TAG, "esp_gap_cb: ESP_GAP_BLE_SCAN_RESULT_EVT: search_evt=%d", scan_result->scan_rst.search_evt);
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Search for BLE iBeacon Packet */
            #if DIAG
            esp_log_buffer_hex("Scan Result: ", scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len );
            #endif
            if (is_my_ibeacon_packet(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
                uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
                #if DIAG
                ESP_LOGI(DEMO_TAG, "----------iBeacon Found----------");
                esp_log_buffer_hex("IBEACON_DEMO: Device address:", scan_result->scan_rst.bda, ESP_BD_ADDR_LEN );
                //esp_log_buffer_hex("IBEACON_DEMO: Proximity UUID:", ibeacon_data->ibeacon_vendor.proximity_uuid, ESP_UUID_LEN_128);
                ESP_LOGI(DEMO_TAG, "Major: 0x%04x (%d)", major, major);
                ESP_LOGI(DEMO_TAG, "Minor: 0x%04x (%d)", minor, minor);
                ESP_LOGI(DEMO_TAG, "Measured power (RSSI at a 1m distance):%d dbm", ibeacon_data->ibeacon_vendor.measured_power);
                ESP_LOGI(DEMO_TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);
                #else
                // Set Color
                //ESP_LOGI(DEMO_TAG, "Major, Minor: 0x%04x, 0x%04x", major, minor);
                /* if (((major>>8) & 0xFF)==0x42) */ {
                    mode = (major>>8) & 0xFF;
                    if (mode==0) {
                      r = major & 0xFF;
                      g = (minor>>8) & 0xFF;
                      b = minor & 0xFF;
                      setColor(r,g,b);
                      from = 0;
                      to = 255;
                    }
                }
                #endif
            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(DEMO_TAG, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(DEMO_TAG, "Stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(DEMO_TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(DEMO_TAG, "Stop adv successfully");
        }
        break;

    default:
        break;
    }
}


void ble_ibeacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(DEMO_TAG, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(DEMO_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }

}

void ble_ibeacon_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();
}

void app_main(void)
{
    TickType_t xDelay = 200 / portTICK_PERIOD_MS;
    uint8_t temp_r, temp_g, temp_b;

    int progress = 0;
    mode = 0;
    int i;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_ibeacon_init();

    /* set scan parameters */
#if (IBEACON_MODE == IBEACON_RECEIVER)
    esp_ble_gap_set_scan_params(&ble_scan_params);

#elif (IBEACON_MODE == IBEACON_SENDER)
    esp_ble_ibeacon_t ibeacon_adv_data;
    esp_err_t status = esp_ble_config_ibeacon_data (&vendor_config, &ibeacon_adv_data);
    if (status == ESP_OK){
        esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
    }
    else {
        ESP_LOGE(DEMO_TAG, "Config iBeacon data failed: %s\n", esp_err_to_name(status));
    }
#endif

    ws2812_init(GPIO_NUM_23); // IO23 is LED_DATA0
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT); // IO25 is LED_DATA_EN0
    gpio_set_level(GPIO_NUM_25, 0); // enable
    from = 0;
    to = 255;

    setLEDs(0,0,0,0,numLeds);

    setupPWMs(14,12,33);
    //setColor(128,128,128);

    while (true) {
      if (mode==1) {
        progress++;
        from = progress & 0x3F;
        to = from+10;
        if (progress & 0x1) setColor(r,g,b);
        else setColor(0,0,0);
        xDelay = 200 / portTICK_PERIOD_MS;
      }
      if (mode==2) {
        uint32_t rand = esp_random();
        from = rand & 0x3F;
        to = from + ((rand>>8) & 0x0F);
        rand = esp_random();
        setColor(rand & 0xFF, (rand>>8) & 0xFF, (rand>>16) & 0xFF);
        xDelay = 50+((rand>>24)&0x7F) / portTICK_PERIOD_MS;

      }
      vTaskDelay(xDelay);
    }


}


#endif // 0


#endif // ESP_PLATFORM

#endif /* defined(__p44utils__espbt__) */
