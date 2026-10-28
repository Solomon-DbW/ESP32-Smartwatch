# src/flashlight.cpp

## Purpose

This file implements the flashlight screen mode for the watch.

## Syntax overview

```cpp
#include "flashlight.h"

bool flashlightOn = false;

void toggleFlashlight() {
  flashlightOn = !flashlightOn;
}

void drawFlashlight() {
  spr.fillSprite(flashlightOn ? TFT_WHITE : TFT_BLACK);
  spr.setTextDatum(MC_DATUM);

  if (flashlightOn) {
    spr.setTextColor(TFT_DARKGREY, TFT_WHITE);
    spr.drawString("A: off   B: next", CX, 212, 2);
  } else {
    spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString("FLASHLIGHT", CX, 90, 4);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("A: turn on", CX, 140, 2);
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString("B: next screen", CX, 204, 2);
  }

  spr.pushSprite(0, 0);
}
```

## Current behavior

- pressing `A` toggles the flashlight
- pressing `B` exits the screen and moves to the next screen in rotation
- the screen becomes blank white when the flashlight is on, so the user has a simple light source

This is one of the newer additions in the current repo state.
