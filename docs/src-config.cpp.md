# src/config.cpp

## Purpose

This file defines the timezone string used by the watch’s NTP time synchronization logic.

## Syntax overview

```cpp
#include "config.h"

const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
```

## Function

This string is passed to:

```cpp
configTzTime(TZ_INFO, "time.cloudflare.com", ...);
```

It configures the ESP32 to use British time with automatic daylight saving transitions.

## Why it exists

Without a valid timezone string, `getLocalTime()` would not return meaningful local time values for the clock display.
