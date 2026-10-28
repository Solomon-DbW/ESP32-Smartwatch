# include/secrets.h.example

## Purpose

This file is a template for the actual Wi‑Fi credentials used by the watch when it connects to a network for NTP time sync.

## Syntax

```c
#pragma once
static const char* const WIFI_SSID = "YOUR_WIFI_NAME";
static const char* const WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

### Notes

- `#pragma once` prevents multiple inclusion in C/C++ builds.
- `static const char* const` means a constant pointer to a constant string.
- The values are set at compile time and used by `src/main.cpp` during setup.

## Real usage

The repo expects a real file named `include/secrets.h` to exist in the build environment. This file is normally git-ignored.

```c
#include "secrets.h"
```

The actual firmware does this:

```cpp
WiFi.begin(WIFI_SSID, WIFI_PASS);
```

## Why it matters

Without valid Wi‑Fi credentials, the watch cannot perform NTP time synchronization. The device can still boot and show the UI, but the clock will not get accurate time unless the user adds a valid `secrets.h` file.
