# ESP32-Smartwatch Documentation Index

This folder contains per-file documentation for the ESP32-Smartwatch firmware. Each document explains the file's purpose, key symbols, expected syntax, and how it interacts with the rest of the project.

## Project configuration

- [platformio.ini](./platformio.ini.md)
- [include/secrets.h.example](./include-secrets.h.example.md)

## Header files

- [include/ancs.h](./include-ancs.h.md)
- [include/app_state.h](./include-app_state.h.md)
- [include/buttons.h](./include-buttons.h.md)
- [include/config.h](./include-config.h.md)
- [include/display.h](./include-display.h.md)
- [include/music_remote.h](./include-music_remote.h.md)
- [include/notifications.h](./include-notifications.h.md)
- [include/stopwatch.h](./include-stopwatch.h.md)

## Source files

- [src/ancs.cpp](./src-ancs.cpp.md)
- [src/app_state.cpp](./src-app_state.cpp.md)
- [src/buttons.cpp](./src-buttons.cpp.md)
- [src/config.cpp](./src-config.cpp.md)
- [src/display.cpp](./src-display.cpp.md)
- [src/main.cpp](./src-main.cpp.md)
- [src/music_remote.cpp](./src-music_remote.cpp.md)
- [src/notifications.cpp](./src-notifications.cpp.md)
- [src/stopwatch.cpp](./src-stopwatch.cpp.md)

## Legacy reference

- [old_src/main.cpp](./old_src-main.cpp.md)

## Overall project model

This repository is an ESP32-S3 smartwatch firmware built in Arduino/C++ with PlatformIO. The design is intentionally modular:

- `include/*.h` defines the interfaces and shared state
- `src/*.cpp` implements the behavior
- `platformio.ini` configures the board and display hardware
- `Wiring/` files document the hardware wiring
- `old_src/` contains a legacy prototype

The watch firmware does the following:

- boots the display
- reads two button inputs
- connects to Wi‑Fi briefly for NTP time sync
- starts BLE services
- displays clock, stopwatch, music, and notification UIs
- handles Apple Notification Center Service and media commands over BLE
