# src/quadclock.cpp

## Purpose

This file implements the quad-clock screen, a segmented four-quadrant digital clock designed for the circular 240x240 display.

## Key concepts

The display is laid out as a 7-segment clock across four quadrants:

- top left: hour tens
- top right: hour ones
- bottom left: minute tens
- bottom right: minute ones

The code uses a 7-segment font and draws each digit individually.

## Important logic

```cpp
bool clock24h = false;

if (!clock24h) {
  hour %= 12;
  if (hour == 0) hour = 12;
}
```

This switches between 12-hour and 24-hour time.

The central separator is a blinking red dot:

```cpp
spr.fillCircle(CX, CY, 4, (t.tm_sec % 2 == 0) ? TFT_RED : COL_OFF);
```

The screen also displays the weekday/day and AM/PM or 24H label at the bottom/top edges.

This is a newer feature in the current repo state and is toggled from the main loop in `src/main.cpp`.
