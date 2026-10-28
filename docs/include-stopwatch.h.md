# include/stopwatch.h

## Purpose

This header declares the stopwatch API used by the UI and the main loop.

## Syntax overview

```c
#pragma once
#include <Arduino.h>

extern bool swRunning;
extern uint32_t swStartMs;
extern uint32_t swBaseMs;

uint32_t stopwatchElapsed();
void stopwatchToggle();
void stopwatchReset();
```

## Data flow

- `swRunning` tracks whether the stopwatch is active.
- `swStartMs` marks the start time of the current active interval.
- `swBaseMs` is the accumulated elapsed time before the current interval.

This pattern allows the watch to keep time accurately without a complex timer library.
