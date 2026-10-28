# include/ancs.h

## Purpose

This header defines the interface for Apple Notification Center Service (ANCS), which lets the watch receive and process notifications from an iPhone over BLE.

## Syntax overview

```c
#pragma once

#include "music_remote.h"

bool ancsSubscribe(BLEClient* client);
void ancsPump();
void ancsReset();
void ancsQueueDismiss(uint32_t uid);
```

## Functions

### `bool ancsSubscribe(BLEClient* client)`

- Locates the ANCS service on the connected iPhone.
- Subscribes to notification source and data source characteristics.
- Marks the notification subsystem as ready if successful.
- Used during the BLE connection flow managed by `music_remote.cpp`.

### `void ancsPump()`

- Sends queued ANCS requests from the BLE task loop.
- Handles both fetches and dismiss operations.
- Called from `amsTask()` in `src/music_remote.cpp`.

### `void ancsReset()`

- Clears ANCS state when the phone disconnects.
- Resets internal fetch queues and clears local notifications.

### `void ancsQueueDismiss(uint32_t uid)`

- Queues a request to dismiss a notification on the iPhone.
- Used when the watch user clears a notification from the watch UI.

## Related implementation

The real logic lives in `src/ancs.cpp`.
