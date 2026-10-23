#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>

#include "secrets.h"

// UK time with automatic BST/GMT switching
// const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
extern const char* TZ_INFO;

// Set to 1 to print loop stalls / slow draws to Serial (115200)
#define LOOP_DIAG 0

// Set to 1 to blank the screen after SCREEN_TIMEOUT_MS of no button presses.
// Leave at 0 if the display backlight is not wired to TFT_BL (the screen would
// otherwise just stop updating while still lit).
#define SCREEN_SLEEP_ENABLED 0

// hardware constants
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
  SCREEN_MUSIC,
  SCREEN_COUNT
};

extern Screen screen;
extern bool screenOn;
extern bool needRedraw;
extern uint32_t lastActivity;
