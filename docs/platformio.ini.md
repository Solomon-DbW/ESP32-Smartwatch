# platformio.ini

## Purpose

This file is the PlatformIO project definition for the smartwatch firmware. It tells the build system which board, framework, libraries, and display configuration to use.

## Key syntax

PlatformIO uses INI-style sections and key/value pairs.

```ini
[env:watch]
platform = espressif32@6.9.0
board = esp32-s3-devkitc-1
framework = arduino
```

### Meaning

- `[env:watch]` creates a build environment named `watch`.
- `platform` selects the ESP32 platform package.
- `board` selects the exact ESP32-S3 development board.
- `framework = arduino` enables the Arduino core.

## Display configuration

The project configures a GC9A01 round TFT using TFT_eSPI.

```ini
build_flags =
    -DBOARD_HAS_PSRAM
    -DUSER_SETUP_LOADED=1
    -DGC9A01_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=240
    -DTFT_MOSI=11
    -DTFT_SCLK=12
    -DTFT_CS=10
    -DTFT_DC=9
    -DTFT_RST=8
    -DTFT_BL=7
    -DTFT_BACKLIGHT_ON=HIGH
    -DSPI_FREQUENCY=40000000
    -DUSE_HSPI_PORT
```

This tells the TFT_eSPI library which display driver and pins are used.

## Memory and flash settings

```ini
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
```

These settings ensure the board uses the desired PSRAM and flash layout, which matters because the project uses a sprite buffer and needs memory headroom.

## Library dependency

```ini
lib_deps =
    bodmer/TFT_eSPI@^2.5.43
```

This adds the display library which handles drawing to the GC9A01 panel.

## Why it matters

The firmware relies on the board and display configuration being exact. If the pins or memory settings are wrong, the display will not initialize correctly, and the UI will fail.
