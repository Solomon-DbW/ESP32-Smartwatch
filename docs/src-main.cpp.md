# src/main.cpp

## Purpose

This is the central control file for the project. It initializes the device and contains the main runtime loop that manages the watch behavior.

## Startup flow (`setup()`)

```cpp
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(0);
  status("Starting...");

  spr.setColorDepth(16);
  spr.setAttribute(PSRAM_ENABLE, 1);
  if (!spr.createSprite(240, 240)) {
    status("Sprite alloc failed");
    while (true) delay(1000);
  }

  // Wi‑Fi + time sync
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ...

  startBle();
}
```

## Runtime loop (`loop()`) 

The main loop does the following:

- reads A/B button events
- wakes the screen on input if needed
- handles screen-specific button behavior
- updates screen sleep timeout
- redraws the current screen when required

Example logic:

```cpp
BtnEvent evA = updateButton(btnA);
BtnEvent evB = updateButton(btnB);

if (screenOn && !wokeNow) {
  if (screen == SCREEN_MUSIC) {
    if (evB == EV_SHORT) {
      musicSel = (musicSel + 1) % MUSIC_COUNT;
      needRedraw = true;
    }
  }
}
```

## Screen logic

- `SCREEN_ANALOGUE` and `SCREEN_DIGITAL` show the clock
- `SCREEN_STOPWATCH` shows the timer
- `SCREEN_NOTIFS` shows notifications
- `SCREEN_MUSIC` shows music controls

## Why it matters

This file orchestrates the entire watch firmware. All modules ultimately feed into or are controlled by this loop.
