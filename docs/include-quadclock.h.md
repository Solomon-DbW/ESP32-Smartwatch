# include/quadclock.h

## Purpose

This header declares the quad-clock digital time screen, which splits the time into four quadrants.

## Syntax overview

```c
#pragma once

#include "config.h"

extern bool clock24h;

void drawQuadClock(const struct tm& t);
```

## Behavior

- `clock24h` toggles between 12-hour and 24-hour display mode.
- `drawQuadClock()` draws each digit in its own quadrant, with a central blinking separator.
- A short press of `A` on this screen toggles the time mode in `src/main.cpp`.

This screen is a newer feature compared with the original analog/digital clock only design.
