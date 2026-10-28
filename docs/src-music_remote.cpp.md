# src/music_remote.cpp

## Purpose

This file implements the most advanced subsystem in the project: BLE media control and Apple Music / media metadata tracking.

## Key sections

### HID media report map

```cpp
static const uint8_t mediaReportMap[] = {
  0x05, 0x0C,
  0x09, 0x01,
  ...
};
```

This defines the HID consumer control report used to send media commands to a connected host.

### `startBle()`

Creates a BLE server and HID device, then starts advertising the device as `DIY Watch`.

### `sendMediaKey()`

Sends a one-byte HID media key report to the connected phone.

### `amsConnectOnce()`

Connects back to the iPhone and subscribes to the Apple Media Service.

### `onEntityUpdate()`

Parses key-value entity updates such as:

- playback state
- rate
- elapsed time
- title
- artist
- duration

This is the logic that allows the watch to display the current song.

### `musicSend()`

Triggered by user button presses, this calls the appropriate HID media key and gives visual feedback on the display.

## Why it matters

This module allows the watch to act as a Bluetooth media remote and a display for the current song, effectively turning the device into a smartwatch controller for music playback.
