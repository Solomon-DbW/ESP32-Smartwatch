# src/display.cpp

## Purpose

This is the UI rendering engine for the smartwatch. It draws the analogue clock, digital clock, stopwatch, music UI, and notification UI.

## Core setup

```cpp
const int CX = 120;
const int CY = 120;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
```

These define the center of the circular screen and an off-screen sprite for drawing.

## Major functions

### `drawHand()`

- Draws a clock hand with a line from the center to a point on the circumference.

### `drawFace()`

- Draws the watch face and hour markers.

### `drawClock()`

- Draws analogue time and date.
- Uses `strftime` for formatted date and time strings.

### `drawDigitalClock()`

- Draws a digital display with current time.

### `drawStopwatch()`

- Renders the stopwatch timer and instruction labels.

### `drawMusic()`

- Displays music connectivity status and track metadata.
- Draws a progress bar and multiple action icons.

### `drawNotifications()`

- Displays the currently selected notification and a message body.
- Wraps long text and shows app name and time.

### `fitText()`

- Truncates text so it fits inside the display width.

### `status()`

- Draws a simple center status message to the screen.

## Why it matters

This is the visual layer of the watch. Most of the device’s value is in how clearly the UI communicates time, music, and notifications.
