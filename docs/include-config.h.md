# include/config.h

## Purpose

This is the core global configuration header for the project. It contains hardware constants, UI state declarations, button enum definitions, and the active screen set for the current firmware.

## Syntax overview

```c
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>

#include "secrets.h"

extern const char* TZ_INFO;

#define LOOP_DIAG 0
#define SCREEN_SLEEP_ENABLED 0

const uint8_t PIN_BTN_A = 4;
const uint8_t PIN_BTN_B = 5;
const uint32_t DEBOUNCE_MS = 30;
const uint32_t LONG_PRESS_MS = 800;
const uint32_t SCREEN_TIMEOUT_MS = 5000;
const uint32_t STOPWATCH_TIMEOUT_MS = 15000;

extern TFT_eSPI tft;
extern TFT_eSprite spr;
extern const int CX;
extern const int CY;

enum BtnEvent { EV_NONE, EV_DOWN, EV_SHORT, EV_LONG };

struct Button {
  uint8_t  pin;
  bool     lastRaw;
  bool     stable;
  uint32_t lastChange;
  uint32_t pressedAt;
  bool     longFired;
  bool     consume;
};

enum Screen {
  SCREEN_ANALOGUE,
  SCREEN_DIGITAL,
  SCREEN_QUADCLOCK,
  SCREEN_STOPWATCH,
  SCREEN_NOTIFS,
  SCREEN_MUSIC,
  SCREEN_FLASHLIGHT,
  SCREEN_COUNT
};

extern Screen screen;
extern bool screenOn;
extern bool needRedraw;
extern uint32_t lastActivity;
```

## Important additions in the current repo

The current version adds:

- `SCREEN_QUADCLOCK` for a four-quadrant digital time layout
- `SCREEN_FLASHLIGHT` for a white-screen flashlight mode

The project also includes a newer screen cycle and a dedicated button flow for those screens.
