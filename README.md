# Redboard Artemis Artemia Test

This repo contains code to test a few components of the whole Artemia system.

- Redboard Artemis (and onboard microphone)
- RTC ([datasheet](https://ambiq.com/artasie-am1815/))
- Flash chip ([datasheet](https://www.macronix.com/Lists/Datasheet/Attachments/8879/MX25V16066,%202.5V,%2016Mb,%20v1.4.pdf))
- Photoresistor
- Temperature/pressure sensor ([datasheet](https://cdn-shop.adafruit.com/datasheets/BST-BMP280-DS001-11.pdf))

The RTC, flash, and temperature/pressure sensor use SPI to communicate.
The photoresistor uses the redboard's ADC. The microphone uses PDM.

For the SPI CS lines:
 *   SPI_CS_0 - pin 11 (Flash)
 *   SPI_CS_1 - pin 17 (Temp/pressure sensor)
 *   SPI_CS_2 - pin 14
 *   SPI_CS_3 - pin 15 (RTC)

 Flash Pins:
- 1 - CS
- 2 - SDO
- 3 -
- 4 - GND
- 5 - SDI
- 6 - SCK
- 7 - 
- 8 - VCC

## Dependencies
 - https://github.com/gemarcano/AmbiqSuiteSDK
 - https://github.com/gemarcano/asimple
 - https://github.com/mborgerding/kissfft/

In order for the libraries to be found, `pkgconf` must know where they are. The
special meson cross-file property `sys_root` is used for this, and the
`artemis` cross-file already has a shortcut for it-- it just needs a
variable to be overriden. To override a cross-file constant, you only need to
provide a second cross-file with that variable overriden. For example:

Contents of `my_cross`:
```
[constants]
prefix = '/home/gabriel/.local/redboard'
```

# Compiling and installing
```
mkdir build
cd build
# The `artemis` cross-file is assumed to be installed per recommendations from
# the `asimple` repository
meson setup --prefix [prefix-where-sdk-installed] --cross-file artemis --cross-file ../my_cross --buildtype release
meson install
```

# License

See the license file for details. In summary, this project is licensed
Apache-2.0, except for the bits copied from the Ambiq SDK, which is BSD
3-clause licensed.

