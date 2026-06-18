# THIRD-PARTY NOTICES

This repository uses third-party software components.

Generated: 2026-06-18

## Scope

- This file documents licenses for components resolved by PlatformIO for environment `pico`.
- Actual linked components can vary by platform/core/library versions.
- This file is for notice purposes only and is not legal advice.

## Direct Dependency (declared in platformio.ini)

### Adafruit TinyUSB Library

- Name: Adafruit TinyUSB Library
- Version observed: 2.4.1
- Declared constraint: `adafruit/Adafruit TinyUSB Library@^2.3.1`
- License: MIT
- Upstream: https://github.com/adafruit/Adafruit_TinyUSB_Arduino
- Local evidence:
  - `.pio/libdeps/pico/Adafruit TinyUSB Library/LICENSE`
  - `.pio/libdeps/pico/Adafruit TinyUSB Library/library.properties`

## Dependencies Observed In Build Graph

The following were present in the PlatformIO dependency graph during build:

- Adafruit TinyUSB Library @ 2.4.1
- EEPROM @ 1.0 (framework-provided)
- SPI @ 1.0 (framework-provided)

Framework package in use:

### framework-arduinopico

- Name: Arduino-Pico core package (`framework-arduinopico`)
- Version observed: 1.30601.0+sha.c3a3526
- License file indicates: GNU Lesser General Public License v2.1 (LGPL-2.1)
- Upstream: https://github.com/earlephilhower/arduino-pico
- Local evidence:
  - `C:/Users/hasegawa/.platformio/packages/framework-arduinopico/LICENSE`

## Transitive Dependencies Pulled By Adafruit TinyUSB Library

These were resolved locally as dependencies of Adafruit TinyUSB Library:

### Adafruit SPIFlash

- Version observed: 4.3.4
- License: MIT
- Upstream: https://github.com/adafruit/Adafruit_SPIFlash
- Local evidence:
  - `.pio/libdeps/pico/Adafruit SPIFlash/LICENSE`

### SdFat - Adafruit Fork

- Version observed: 2.2.3
- License: MIT
- Upstream: https://github.com/adafruit/SdFat
- Local evidence:
  - `.pio/libdeps/pico/SdFat - Adafruit Fork/LICENSE.md`
  - `.pio/libdeps/pico/SdFat - Adafruit Fork/library.properties`

## Recommended Distribution Practice

- Keep this notice file with source releases.
- Include each third-party license text when redistributing binaries/source bundles.
- Re-run dependency and license checks when updating PlatformIO platform/core/library versions.
