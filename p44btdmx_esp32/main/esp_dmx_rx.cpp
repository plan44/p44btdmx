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

// File scope debugging options
// - Set ALWAYS_DEBUG to 1 to enable DBGLOG output even in non-DEBUG builds of this file
#define ALWAYS_DEBUG 0
// - set FOCUSLOGLEVEL to non-zero log level (usually, 5,6, or 7==LOG_DEBUG) to get focus (extensive logging) for this file
//   Note: must be before including "logger.hpp" (or anything that includes "logger.hpp")
#define FOCUSLOGLEVEL 0

#include "esp_dmx_rx.hpp"
#include "application.hpp"

#define HEALTHY_TIME (500*MilliSecond) // timeout in ms
#define BUF_SIZE 1024 //  buffer size for rx events

using namespace p44;


static DMXReceiver* sharedReceiverP = NULL;

DMXReceiver& DMXReceiver::sharedReceiver()
{
  if (!sharedReceiverP) {
    sharedReceiverP = new DMXReceiver();
  }
  return *sharedReceiverP;
}


DMXReceiver::DMXReceiver() :
  mUartNum(-1),
  mDmxState(dmxrx_idle),
  mCurrentRxAddr(0),
  mLastDmxPacket(Never)
{
}


DMXReceiver::~DMXReceiver()
{
  stop();
}


void DMXReceiver::stop()
{
  if (mUartNum>=0) {
    xSemaphoreTake(mDmxMutex, portMAX_DELAY);
    mDmxDataCB = NULL;
    uart_driver_delete(mUartNum);
    xSemaphoreGive(mDmxMutex);
    mUartNum = -1;
    mDmxState = dmxrx_idle;
  }
}



ErrorPtr DMXReceiver::start(int aUartNumber, int aRxPin, int aTxPin, DMXDataCB aDataCB)
{
  ErrorPtr err;
  // first stop what's currently running
  stop();
  // save params
  mDmxDataCB = aDataCB;
  mUartNum = aUartNumber;
  // configure UART for DMX
  uart_config_t uart_config = {
    .baud_rate = 250000,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_2,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(mUartNum, &uart_config);
  // Set pins for UART
  uart_set_pin(mUartNum, aTxPin, aRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  // install queue
  uart_driver_install(mUartNum, BUF_SIZE * 2, BUF_SIZE * 2, 20, &mDmxRxQueue, 0);
  // create receive task
  xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
  // create mutex for syncronisation
  mDmxMutex = xSemaphoreCreateMutex();
  return err;
}


uint8_t DMXReceiver::read(uint16_t channel)
{
  // restrict acces to dmx array to valid values
  if(channel < 1) {
    channel = 1;
  }
  else if(channel > 512) {
    channel = 512;
  }
  // take data threadsafe from array and return
  xSemaphoreTake(mDmxMutex, portMAX_DELAY);
  uint8_t tmp_dmx = mDmxData[channel];
  xSemaphoreGive(mDmxMutex);
  return tmp_dmx;
}


bool DMXReceiver::isHealthy()
{
  // get timestamp of last received packet
  xSemaphoreTake(mDmxMutex, portMAX_DELAY);
  MLMicroSeconds lastPacket = mLastDmxPacket;
  xSemaphoreGive(mDmxMutex);
  // check if elapsed time < defined timeout
  return MainLoop::now()-lastPacket<HEALTHY_TIME;
}


void DMXReceiver::uart_event_task(void *pvParameters)
{
  sharedReceiver().uart_event_loop(pvParameters);
}


void DMXReceiver::uart_event_loop(void *pvParameters)
{
  FOCUSLOG("Starting UART event loop");
  uart_event_t event;
  uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
  while(true) {
    // wait for data in the dmx_queue
    if (xQueueReceive(mDmxRxQueue, (void * )&event, (portTickType)portMAX_DELAY)) {
      bzero(dtmp, BUF_SIZE);
      switch (event.type) {
        case UART_DATA:
          // read the received data
          DBGFOCUSLOG("- UART_DATA with %d bytes", (int)event.size);
          uart_read_bytes(mUartNum, dtmp, event.size, portMAX_DELAY);
          // check if break detected
          if(mDmxState==dmxrx_break) {
            // if not 0, then RDM or custom protocol
            if(dtmp[0] == 0) {
              DBGFOCUSLOG("- start of DMX packet");
              mDmxState = dmxrx_data;
              // reset dmx adress to 0
              mCurrentRxAddr = 0;
              xSemaphoreTake(mDmxMutex, portMAX_DELAY);
              // store received timestamp
              mLastDmxPacket = MainLoop::now();
              xSemaphoreGive(mDmxMutex);
            }
          }
          // check if in data receive mode
          if (mDmxState==dmxrx_data) {
            xSemaphoreTake(mDmxMutex, portMAX_DELAY);
            DBGFOCUSLOG("- storing %d bytes of DMX data @ %d", (int)event.size, mCurrentRxAddr);
            // copy received bytes to dmx data array
            bool completedPacket = false;
            for (int i = 0; i < event.size; i++) {
              if (mCurrentRxAddr < 513) {
                mDmxData[mCurrentRxAddr++] = dtmp[i];
                if (mCurrentRxAddr==513) {
                  // completed packet now
                  completedPacket = true;
                }
              }
            }
            //FOCUSLOG("- new mCurrentRxAddr = %d", mCurrentRxAddr);
            xSemaphoreGive(mDmxMutex);
            if (completedPacket) {
              // callback might want to process the new packet
              deliverDmxData(mDmxData);
            }
          }
          break;
        case UART_BREAK:
          // break detected
          DBGFOCUSLOG("- UART_BREAK");
          // clear queue und flush received bytes
          uart_flush_input(mUartNum);
          xQueueReset(mDmxRxQueue);
          mDmxState = dmxrx_break;
          break;
        case UART_FRAME_ERR:
        case UART_PARITY_ERR:
        case UART_BUFFER_FULL:
        case UART_FIFO_OVF:
        default:
          DBGFOCUSLOG("- other UART event %d", (int)event.type);
          // error recevied, going to idle mode
          uart_flush_input(mUartNum);
          xQueueReset(mDmxRxQueue);
          mDmxState = dmxrx_idle;
          break;
      }
    }
  }
}


void DMXReceiver::deliverDmxData(const uint8_t* aDMXData)
{
  if (mDmxDataCB) {
    FOCUSLOG("posting DMX data handler execution from mainloop@%p", &MainLoop::currentMainLoop());
    // make sure this executes on the main thread
    Application::sharedApplication()->mainLoop().executeNowFromForeignTask(
      boost::bind(&DMXReceiver::deliveryCallback, mDmxDataCB, aDMXData)
    );
  }
}


void DMXReceiver::deliveryCallback(DMXDataCB aCallback, const uint8_t* aDMXData)
{
  FOCUSLOG("calling DMX data handler in mainloop@%p", &MainLoop::currentMainLoop());
  aCallback(aDMXData);
}
