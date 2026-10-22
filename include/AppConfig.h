#pragma once
#include <Arduino.h>

constexpr uint8_t PIN_BTN_A = 4;
constexpr uint8_t PIN_BTN_B = 5;
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t LONG_PRESS_MS = 800;
constexpr uint32_t SCREEN_TIMEOUT_MS = 5000;
constexpr uint32_t STOPWATCH_TIMEOUT_MS = 15000;

constexpr int DISPLAY_WIDTH = 240;
constexpr int DISPLAY_HEIGHT = 240;
constexpr int DISPLAY_CX = DISPLAY_WIDTH / 2;
constexpr int DISPLAY_CY = DISPLAY_HEIGHT / 2;

constexpr const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
#define BLE_DEVICE_NAME "Solomon's ESP32 Watch"
