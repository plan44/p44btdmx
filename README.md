# p44btdmx - LED lights controlled by DMX over Bluetooth

## Overview

_p44btdmx_ was developed out of the need for a low-cost, robust, low weight, wireless extension of a DMX512 controlled light installation, integrating mobile SmartLED color light effects powered by standard USB powerbanks into actor's and dancer's costumes, in a musical performance.

To keep the individual light controllers cost effective, these are based on very affordable ESP32 Wroom modules.

For communication, BLE advertisements using "manufacturer specific data" are used - providing a broadcast-type data transmission similar to DMX512 itself. As the available bandwith for BLE advertisements is much lower than DMX512, the main task of the p44BTDMX protocol is to detect and transmit DMX channel changes with priority, while still making sure all DMX channels are refreshed as often as possible. The protocol may not perform well with DMX master side rendered effects and smoothed transitions due to the high number of incremental changes that might saturate available bandwith - to get optimal performance and precise timing, use the light-side implemented effects instead and make sure the DMX master renders no unnecessary ramps.

The p44BTDMX protocol can drive up to 84 lights with 8 channels (HSV color + 5 channels for position, size and effect control). The p44BTDMX ESP32 application supports direct RGB(W) PWM output and/or up to 4 WS281x / SK6812 SmartLED LED chains.

## Components

_p44btdmx_ consists of a ESP32 project and an iOS App.

**The ESP32 project** can be built in different ways

- Light controller for 3-channel PWM lights and/or WS281x Smart LED chains with Bluetooth receiver
- DMX interface box which translates DMX on the serial port to the (compressed, incremental) p44BTDMX protocol which is broadcast via Bluetooth advertisements (manufacturer specific data) to the light controllers.

There ara a lot of `CONFIG_*` variables which can be set in sdkconfig via menu, or directly in `main/p44btdmx_main.cpp` from line 32ff (much quicker rebuilds than using the menu, which triggers complete idf rebuilds). To use the menu, make sure `QUICK_OVERRIDE` in line ~32 of `main/p44btdmx_main.cpp` is set to 0, otherwise the `#define`s below will be used.

**The iOS App** allows directly controlling the p44BTDMX receivers (using iBeacon type advertisements). Note that receivers only listen to the iPhone app when no DMX interface box has been detected for 10 minutes.

**The p44BTDMX broadcast protocol** is designed for robustness, not security. Still, receivers and senders must be paired with a shared secret, which must be defined in `sdkconfig` as `CONFIG_P44BTDMX_SYSTEM_KEY` as a string of 32 characters. Note that this "key" only protects from interference of two neigbouring _p44BTDMX_ systems and provides a _very_ modest tampering barrier. But in no way _p44BTDMX_ is a secure protocol; anyone reasonably capable would be able to hack it. But again: this was a design choice suitable for the task at hand when developing this. Don't use it to control nuclear submarines, please!

## Contributions

...are welcome! There's a lot that could be improved. Pull requests and comments can be posted on [github](https://github.com/plan44/p44btdmx.git)

## License

p44btDMX is GPLv3 licensed

Some parts of used submodules (in particular, p44utils) makes use of third party project's code which is under less strict licenses such as MIT or BSD. Such code is usually included in a "thirdparty" subfolder. Please see the LICENSE files or license header comments in the individual projects included in that folder.

## Copyright

(c) 2020-2022 by [plan44.ch/luz](https://plan44.ch)
