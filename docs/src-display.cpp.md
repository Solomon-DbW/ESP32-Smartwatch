# src/display.cpp

## Purpose

This file is the display rendering engine for the watch. It draws the status screens and all current UI elements for the latest repo state.

## Current major screens

### `drawClock()`

- Analogue clock
- hour, minute, and second hands
- date label
- notification badge

### `drawDigitalClock()`

- digital date/time display
- compact AM/PM text under the time
- unread notification badge

### `drawQuadClock()`

This is handled in `src/quadclock.cpp`, not here.

### `drawStopwatch()`

- stopwatch display and instructions

### `drawMusic()`

- track info and music controls
- progress bar
- send/select action labels

### `drawNotifications()`

- list view and detail view for message text
- paged multi-line full-message display
- unread badge integration

### `status()`

- simple boot and warning state text

## Current repo additions

The current code includes a full notification detail mode with paging logic, drawn in `drawNotifications()`, plus the flashlight screen is handled by `drawFlashlight()` in `src/flashlight.cpp`.

This makes the UI more capable than the earlier single-screen implementation.
