# src/stopwatch.cpp

## Purpose

This file implements the stopwatch logic for the watch.

## Syntax overview

```cpp
#include "stopwatch.h"

bool swRunning = false;
uint32_t swStartMs = 0;
uint32_t swBaseMs = 0;

uint32_t stopwatchElapsed() {
  return swRunning ? swBaseMs + (millis() - swStartMs) : swBaseMs;
}

void stopwatchToggle() {
  if (swRunning) {
    swBaseMs += millis() - swStartMs;
    swRunning = false;
  } else {
    swStartMs = millis();
    swRunning = true;
  }
}

void stopwatchReset() {
  swRunning = false;
  swBaseMs = 0;
}
```

## Behavior

- The stopwatch uses milliseconds from `millis()`.
- When running, it accumulates elapsed time from `swStartMs`.
- When stopped, it freezes the total in `swBaseMs`.
- `stopwatchReset()` clears the timer without needing to reset the MCU.

## Why it matters

This is a simple but important feature: it lets the watch serve as a basic timer in the stopwatch screen.
