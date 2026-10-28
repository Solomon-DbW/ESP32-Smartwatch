# ESP32-Smartwatch Documentation Index

This folder contains the current documentation for the ESP32-Smartwatch firmware. It reflects the latest repository state, including the newer flashlight and quad-clock screens.

## Project configuration

- [platformio.ini](./platformio.ini.md)
- [include/secrets.h.example](./include-secrets.h.example.md)

## Header files

- [include/ancs.h](./include-ancs.h.md)
- [include/app_state.h](./include-app_state.h.md)
- [include/buttons.h](./include-buttons.h.md)
- [include/config.h](./include-config.h.md)
- [include/display.h](./include-display.h.md)
- [include/flashlight.h](./include-flashlight.h.md)
- [include/music_remote.h](./include-music_remote.h.md)
- [include/notifications.h](./include-notifications.h.md)
- [include/quadclock.h](./include-quadclock.h.md)
- [include/stopwatch.h](./include-stopwatch.h.md)

## Source files

- [src/ancs.cpp](./src-ancs.cpp.md)
- [src/app_state.cpp](./src-app_state.cpp.md)
- [src/buttons.cpp](./src-buttons.cpp.md)
- [src/config.cpp](./src-config.cpp.md)
- [src/display.cpp](./src-display.cpp.md)
- [src/flashlight.cpp](./src-flashlight.cpp.md)
- [src/main.cpp](./src-main.cpp.md)
- [src/music_remote.cpp](./src-music_remote.cpp.md)
- [src/notifications.cpp](./src-notifications.cpp.md)
- [src/quadclock.cpp](./src-quadclock.cpp.md)
- [src/stopwatch.cpp](./src-stopwatch.cpp.md)

## Legacy reference

- [old_src/main.cpp](./old_src-main.cpp.md)

## Current project model

This repository is an ESP32-S3 smartwatch firmware built in Arduino/C++ with PlatformIO. The current design is modular and includes:

- `include/*.h` for interfaces and global state
- `src/*.cpp` for implementations
- `platformio.ini` for board and display configuration
- `Wiring/` for connector and hardware reference docs
- `old_src/` for earlier prototype code

The current watch firmware includes these features:

- digital and analog clock screens
- a quad-clock screen with 12h/24h switching
- stopwatch
- iPhone notification mirroring over ANCS
- music playback control and track display via BLE
- flashlight mode
- screen wake/sleep logic
- low-level rendering with a sprite buffer and TFT_eSPI
