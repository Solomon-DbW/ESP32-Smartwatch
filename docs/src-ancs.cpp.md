# src/ancs.cpp

## Purpose

This file implements the ANCS (Apple Notification Center Service) client used to receive notifications from the iPhone over BLE.

## Key concepts

ANCS operates with a service and multiple characteristics:

- Notification Source
- Data Source
- Control Point

The code subscribes to these and parses incoming notification metadata. It keeps a queue of notifications to fetch and optionally dismiss.

## Important functions

### `friendlyApp()`

- Maps bundle identifiers like `mobilesms` or `mobilemail` to friendly names like `Messages` or `Mail`.

### `onNotifSource()`

- Called when the iPhone sends a notification event.
- Handles:
  - added
  - modified
  - removed
- Calls `notifAdd()` or `notifRemove()` and queues fetches for text.

### `parseDataSource()`

- Reassembles a data-source response from one or more BLE notifications.
- Extracts app ID, title, and message.
- Calls `notifSetText()` once complete.

### `ancsSubscribe()`

- Locates the ANCS service and essential characteristics.
- Registers BLE notification handlers.
- Marks the subsystem as ready.

### `ancsPump()`

- Drains queued requests in the BLE task loop.
- Sends `GET_NOTIF_ATTRS` requests and dismiss actions.

## Syntax notes

This file uses:

- static queues for fetch and dismiss requests
- raw BLE `uint8_t` arrays
- low-level protocol parsing using offsets and lengths
- `portENTER_CRITICAL` and `portEXIT_CRITICAL` around shared state

## Related files

- `include/ancs.h`
- `src/notifications.cpp`
- `src/music_remote.cpp`
