# include/display.h

## Purpose

This header declares the drawing functions used by the smartwatch UI.

## Syntax overview

```c
#pragma once

#include "config.h"
#include "stopwatch.h"
#include "music_remote.h"
#include "notifications.h"
#include "app_state.h"

void drawHand(float angleDeg, int length, int width, uint16_t color);
void drawFace();
void drawClock(const struct tm& t);
void drawDigitalClock(const struct tm& t);
void drawStopwatch();
void drawMusicIcon(int idx, int x, int y, bool selected, bool flashing, int playState);
void fitText(const char* src, int font, int maxW, char* out, size_t outSize);
void drawMusic();
void drawNotifications();
void drawNotifBadge(int y);
void status(const char* msg);
```

## Usage pattern

Most drawing functions are called from `src/main.cpp` when the watch is in a screen mode. For example:

```cpp
if (screen == SCREEN_ANALOGUE) drawClock(t);
else if (screen == SCREEN_STOPWATCH) drawStopwatch();
```

## Why it matters

This is the interface between the app logic and the actual display rendering engine in `src/display.cpp`.
