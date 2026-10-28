# include/flashlight.h

## Purpose

This header declares the flashlight mode for the watch.

## Syntax overview

```c
#pragma once

#include "config.h"

extern bool flashlightOn;

void toggleFlashlight();
void drawFlashlight();
```

## Behavior

- `flashlightOn` tracks whether the flashlight mode is active.
- `toggleFlashlight()` flips the mode on/off.
- `drawFlashlight()` renders either:
  - a bright white screen when on
  - a dark screen with instructions when off

This screen is entered via the watch screen cycle and is controlled in `src/main.cpp`.
