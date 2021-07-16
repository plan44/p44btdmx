# p44btdmx - LED lights controlled by DMX over Bluetooth

## About

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

(c) 2020-2021 by [plan44.ch/luz](https://plan44.ch)
