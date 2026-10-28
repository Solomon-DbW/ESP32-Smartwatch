# include/buttons.h

## Purpose

This header exposes the button interface used by the watch UI and control loop.

## Syntax overview

```c
#pragma once
#include "config.h"

extern Button btnA;
extern Button btnB;

BtnEvent updateButton(Button& b);
```

## Key types

`Button` is defined in `config.h`.

The API exposes:

- `btnA` and `btnB`: button state objects
- `updateButton(Button& b)`: debounced event reader for one button

## Event flow

`src/main.cpp` calls this function each loop iteration. It translates raw GPIO state into a high-level event such as:

- `EV_NONE`
- `EV_DOWN`
- `EV_SHORT`
- `EV_LONG`

This is how the watch understands taps and long presses.
