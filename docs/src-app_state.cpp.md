# src/app_state.cpp

## Purpose

This file defines the application-wide state variables that determine the watch’s current behavior.

## Syntax overview

```cpp
#include "app_state.h"

Screen screen = SCREEN_ANALOGUE;
bool screenOn = true;
bool needRedraw = true;
uint32_t lastActivity = 0;
```

## Variables

- `screen`: current active screen
- `screenOn`: whether the display is awake
- `needRedraw`: triggers a refresh of the display
- `lastActivity`: timestamp of last user interaction

These are used by `src/main.cpp` to decide when to redraw and when to sleep the screen.
