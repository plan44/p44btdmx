//
//  Based on ESP32-DMX-RX library (c) 2019,2020 by Lukas Salomon, licensed GPLv3
//  (https://github.com/luksal/ESP32-DMX-RX)
//
//  Adapted for use with p44utils 2020 by Lukas Zeller <luz@plan44.ch>
//
//  This is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This code is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License v3
//  along with this. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__espdmxrx__
#define __p44utils__espdmxrx__

#include "p44utils_common.hpp"

#ifdef ESP_PLATFORM

#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"

using namespace std;

namespace p44 {

  typedef uint8_t DMXData[513];
  typedef boost::function<void (const uint8_t* aDMXData)> DMXDataCB;

  class DMXReceiver : public P44LoggingObj
  {
    typedef enum {
      dmxrx_idle,
      dmxrx_break,
      dmxrx_data
    } DMXRxState;

    uart_port_t mUartNum; ///< the UART number
    QueueHandle_t mDmxRxQueue; ///< queue for uart rx events
    SemaphoreHandle_t mDmxMutex; ///< semaphore for syncronising access to dmx array
    DMXRxState mDmxState; ///< status, in which recevied state we are
    uint16_t mCurrentRxAddr; ///< last received dmx channel
    MLMicroSeconds mLastDmxPacket; ///< timestamp for the last received packet
    DMXData mDmxData; ///< stores the received dmx data
    DMXDataCB mDmxDataCB; ///< callback to call when new DMX packet is complete

    DMXReceiver();
    ~DMXReceiver();

  public:

    /// access to singleton
    static DMXReceiver& sharedReceiver();

    /// initialize
    /// @param aUartNumber the EPS UART to use
    /// @param aRxPin the Rx pin number
    /// @param aTxPin the Tx pin number (not actually used, we don't send anything)
    /// @param aDataCB the callback to call whenever a complete DMX packet is ready
    ErrorPtr start(int aUartNumber, int aRxPin, int aTxPin, DMXDataCB aDataCB = NULL);

    /// stop receiving DMX data
    void stop();

    uint8_t read(uint16_t channel); /// returns the dmx value for the givven address (values from 1 to 512)
    bool isHealthy(); // returns true, when a valid DMX signal was received within the last 500ms

  private:

    static void uart_event_task(void *pvParameters);
    void uart_event_loop(void *pvParameters);
    
    void deliverDmxData(const uint8_t* aDMXData);
    static void deliveryCallback(DMXDataCB aCallback, const uint8_t* aDMXData);

  };

} // namespace p44

#endif // ESP_PLATFORM

#endif // __p44utils__espdmxrx__
