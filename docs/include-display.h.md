# include/display.h

## Purpose

This header declares the drawing functions used by the watch UI. It includes both the original screen functions and the newer flashlight/quad-clock helpers.

## Syntax overview

```c
#pragma once

#include "config.h"
#include "stopwatch.h"
#include "music_remote.h"
#include "notifications.h"
#include "app_state.h"
#include "flashlight.h"
#include "quadclock.h"

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

## Current repo behavior

The draw API is now used by:

- analog clock
- digital clock
- quad-clock screen
- stopwatch
- notifications
- music control panel
- flashlight screen

The display header now references both the newer feature modules and the original UI functions.
