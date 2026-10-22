# DIY Smartwatch — Modular PlatformIO Project

ESP32-S3 N16R8 + GC9A01 240×240 display, two buttons, stopwatch, BLE HID media remote, and Apple Media Service (AMS) now-playing metadata.

## Build

1. Open this folder in VS Code with the PlatformIO extension.
2. Confirm `src/secrets.h` contains your Wi-Fi credentials. If it is missing, copy `src/secrets.example.h` to `src/secrets.h` and edit it.
3. Build the `watch` environment.
4. Upload to the ESP32-S3 and open the serial monitor at 115200 baud.

The existing `platformio.ini` is retained, including its pinned `espressif32@6.9.0` platform and TFT_eSPI configuration.

## Source layout

- `src/main.cpp` — setup, screen navigation, button actions, redraw scheduling.
- `src/Buttons.cpp` / `include/Buttons.h` — debounced button events.
- `src/Stopwatch.cpp` / `include/Stopwatch.h` — stopwatch state and elapsed-time calculation.
- `src/Clock.cpp` / `include/Clock.h` — Wi-Fi connection and NTP/timezone setup.
- `src/Display.cpp` / `include/Display.h` — TFT/sprite setup and all screen rendering.
- `src/MusicRemote.cpp` / `include/MusicRemote.h` — BLE HID, media-key sending, AMS connection and now-playing snapshot.
- `include/AppConfig.h` — shared pin and timing configuration.

## Controls

- Button B short press: next screen.
- Stopwatch: button A short press starts/stops; hold A resets.
- Music screen: button B short press selects a control, button A sends it, hold B exits to the next screen.
- Screen timeout is intentionally disabled, matching the supplied sketch.

## Notes

- I made the SVGs for the GC9A01 and the ESP32 so sorry if they look trash
- The watch advertises as `Solomon's ESP32 Watch`; pair it with the iPhone in Bluetooth settings.
- AMS metadata is best-effort. BLE media-key controls can still work if AMS track information is unavailable.
- `src/secrets.h` is ignored by Git. Do not commit Wi-Fi credentials.
