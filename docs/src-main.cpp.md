# src/main.cpp

## Purpose

This is the runtime coordinator for the smartwatch. It initializes the device, connects to Wi‑Fi/NTP, starts BLE, and contains the main event loop for the watch.

## Current behavior

The current `setup()` function still:

- starts serial logging
- configures the A/B GPIO buttons
- initializes the display backlight
- starts the TFT and sprite buffer
- connects to Wi‑Fi and syncs time via NTP
- starts BLE services

The current `loop()` function now handles the newer screens:

- analogue clock
- digital clock
- quad-clock screen
- stopwatch
- notifications
- music
- flashlight

## Notable current logic

```cpp
if (screen == SCREEN_NOTIFS) {
  if (notifDetail) {
    if (evB == EV_SHORT) { notifPage++; needRedraw = true; }
    if (evA == EV_SHORT || evB == EV_LONG) { notifDetail = false; needRedraw = true; }
    if (evA == EV_LONG)  { notifDismissSelected(); needRedraw = true; }
  } else {
    if (evB == EV_SHORT) { notifNext(); needRedraw = true; }
    if (evB == EV_LONG) {
      screen = (Screen)((screen + 1) % SCREEN_COUNT);
      needRedraw = true;
    }
    if (evA == EV_SHORT) { notifOpenSelected(); needRedraw = true; }
    if (evA == EV_LONG)  { notifDismissSelected(); needRedraw = true; }
  }
}
```

This allows the user to browse notification list entries and then open a full-text detail view.

The newer screen logic also includes:

```cpp
else if (screen == SCREEN_FLASHLIGHT) {
  if (evA == EV_SHORT) { toggleFlashlight(); needRedraw = true; }
  if (evB == EV_SHORT) {
    flashlightOn = false;
    screen = (Screen)((screen + 1) % SCREEN_COUNT);
    needRedraw = true;
  }
}
```

and:

```cpp
if (screen == SCREEN_QUADCLOCK && evA == EV_SHORT) {
  clock24h = !clock24h;
  needRedraw = true;
}
```

These are the main repo updates that the earlier documentation did not include.
