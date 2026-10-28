# include/config.h

## Purpose

This is the core global configuration header for the project. It contains hardware constants, UI state declarations, button enum definitions, and watch screen definitions.

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
  SCREEN_STOPWATCH,
  SCREEN_NOTIFS,
  SCREEN_MUSIC,
  SCREEN_COUNT
};

extern Screen screen;
extern bool screenOn;
extern bool needRedraw;
extern uint32_t lastActivity;
```

## Important notes

- The file defines the watch’s button map and screen model.
- `SCREEN_COUNT` is used to cycle through UI screens.
- `screenOn` and `lastActivity` are used for screen sleep logic.
- `CX` and `CY` define the center point of the circular display.

## Why it matters

This file centralizes the watch’s global behavior. Most modules depend on it directly or indirectly.
