# p44BTDMX for EPS32

## Summary

This ESP32 IDF project can build as either

- a light controller receiving p44BTDMX DMX512-like bluetooth protocol and controlling a 3-channel RGB PWM plus up to 4 WS281x or SK68912 SmartLED strips.
- a DMX-to-p44BTDMX sender, receiving DMX512 via UART serial port and sending out p44BTDMX via bluetooth to control up to 84 p44BTDMX receivers.

## Hardware

For both receiver and sender a ESP32 Wroom module can be used, with the following connections:

- GPIO 26,27,32,34,35,18 can be connected to a DIP switch towards GND. GPIO 34 and 35 need a external 100k pullup to 3.3V each as these GPIOs don't have internal pullups. The DIP switch determines the light number (DMX channel*8) of the controller within the p44BTDMX universe.
- If `CONFIG_P44BTDMX_PWMLIGHT` is enabled, GPIO 14,12,33 will be R,G,B PWM outputs and can be used to directly drive the gate of a suitable Power FET (e.g. IRLR2905PBF) switching 12-24V LED voltages
- If `CONFIG_P44BTDMX_PWMLIGHT` is enabled, GPIO23 will output a WS2813 data signal to drive a SmartLED chain. With `CONFIG_P44BTDMX_PWMLIGHT` disabled, GPIO22 and GPIO23 will each output a SmartLED chain data signal.
- For the DMX to p44BTDMX sender, connect the DMX512 signal to RX=GPIO13, TX=GPIO4 via a suitable differential RS485 driver chip (e.g. ADM3485).

All the GPIO assignments, SmartLED chip types (WS281x, SKxx) and layouts (linear, areas, combinations) are configurable via `CONFIG_*` definitions in `skdconfig` or directly in `p44btdmx_main.cpp`. For details for the features of *p44lrgraphics*, *p44utils* and in particular the configuration strings for SmartLED chains see [plan44 techdocs](https://plan44.ch/p44-techdocs/en/lrgraphics/#ledchainspec).

## Building
### how to add submodules for ESP32
The project depends on two submodules, which must be added before building:

```bash
git submodule add -b esp32 ssh://plan44@plan44.ch/home/plan44/sharedgit/p44utils.git components/p44utils

git submodule add -b master ssh://plan44@plan44.ch/home/plan44/sharedgit/p44lrgraphics.git components/p44lrgraphics
```

### Configuration
As described above, there are a lot of `CONFIG_*` symbols that can be defined in sdkconfig and/or directly in `p44btdmx_main.cpp`. All have defaults, and the most important ones are exposed via menuconfig (i.e. included with explanatory texts in `Kconfig.projbuild`).

To build a default setup, make sure `QUICK_OVERRIDE` on line 32 of `
p44btdmx_main.cpp` is set to 0, then use menuconfig to set the basic options.

The definitions inside `#if QUICK_OVERRIDE` are predefined variants of the receiver (different light configurations and DMX sender), and might serve as examples for your own configuration. Unlike changes in sdkconfig/menuconfig, changing these local definitions does not trigger a complete IDF+App rebuild, which helps during testing of different configurations. 

To prevent other users to accidentally control your p44BTDMX, set `CONFIG_P44BTDMX_SYSTEMKEY` to a unique secret value, either 64 hex characters representing 32 bytes (best), or a text string of at least 32 characters. Note that the protection the secret key provides is not a secure encryption at all, it is only to prevent interference between neighbouring p44BTDMX systems and prevent users having the iOS app on their phone to control the system without the key - but any modestly skilled attacker could hack it.

## Using
- Either build the DMX512 to p44BTDMX ESP32 application on a separate Wroom to connect to a DMX512 master. Config settings for this are:

    ```C
    #define CONFIG_P44_DMX_RX 1
    #define CONFIG_P44_BTDMX_SENDER 1
    #define CONFIG_P44_BTDMX_RECEIVER 0
    #define CONFIG_P44_BTDMX_LIGHTS 0
    ```
    
- Or use the iOS app to directly control the system (useful for individual prop or costume tests etc.). The app is [available on AppStore](https://apps.apple.com/ch/app/p44btdmx/id1545043495), if you don't want to build it from sources (in this repo). Note that when a DMX512 to p44BTDMX sender box is active, the iOS app is blocked from interfering.

## Light channel layout

The p44BTDMX universe's 512 channels contains up to 64 lights with 8 channels each. An actual ESP32-based light controller can host 2 or 4 lights (depending on the `CONFIG_P44_ENABLE_FOURLIGHT_CONTROLLERS` setting and the DMX address set via Dip Switches: Setting light address to 48..63 makes the controller hosting 4 lights, setting address to 0..47 switches to only 2 lights ).

Each light has the following channels:

| Channel | Function | Details | Modes |
| --- | --- | --- | --- |
| 1 | Hue | 0=red, 85=green, 170=blue, 255=red (color circle) | all |
| 2 | Saturation | 0=white, 255=full color saturation | all |
| 3 | Value/Brightness | 0=off, 255=full brightness | all |
| 4 | Position | Positioning of the light on the LED strip, 0=center at the beginning, 255=center at the end. For ticker: selects the text to be displayed (see separate table, 1..14)" | all |
| 5 | Size | Size of the light effect on the LED strip, 0=small, 255=whole strip" | all except 0 |
| 6 | Speed | Speed of animation 0=off, 255=max, for text: scroll speed |  4..8 |
| 7 | Gradient/Extent | Intensity or extent of the effect, 0 = fully negative, 128 = zero, 255 = fully positive | 2..8 |
| 8 | mode | Operating mode | all |

The operating mode selects different behaviours:

| mode | Description | Size | Speed | Gradient/Extent |
| --- | --- | --- | --- | --- |
| 0 | Light of fixed size (half stripe) | - | - | - |
| 1 | Light with adjustable size and sharp edges | Size | - | - |
| 2 | Light with adjustable size and soft edges | Size | - | Brightness decreases/increases towards the edge |
| 3 | Adjustable size light with hue gradient | Size | - | Increase/decrease of hue towards the edge |
| 4 | Pulsating light with adjustable size | Size | Pulse frequency | Increase/decrease of brightness during pulsation |
| 5 | Oscillating light (pendulum style) with adjustable size soft edge and adjustable deflection | Size | Speed | Amount of deflection of "pendulum" movement |
| 6 | Oscillating light with adjustable size, hard edge and adjustable deflection | Size | Speed | Amount of deflection of "pendulum" movement |
| 7 | **For SmartLED lights:** Light accelerating in one direction with adjustable size, soft edge and adjustable deflection | Size | Speed | Length of movement |
| 7 | **For PWM lights:** hue animation | n/a | Speed | Amount of hue change |
| 8 | Light accelerating in one direction with adjustable size, hard edge and adjustable deflection | Size | Speed | Length of movement |
| 9 | Light moving uniformly in one direction with adjustable size, soft edge and adjustable deflection | Size | Speed | Length of movement |
| 10 | Light moving uniformly in one direction with adjustable size, hard edge and adjustable deflection | Size | Speed | Length of movement |
| +128 | Light always appears on the first LED strip (otherwise the first light appears on the first strip, the second on the second strip, etc.)" |
| +64 | Light is 4 strips wide, so it appears on several strips |

Note that for the PWM light, all modes involving movement of the light do not make any sense. 

