# src/notifications.cpp

## Purpose

This file implements a small in-memory queue for phone notifications received via ANCS.

## Syntax overview

```cpp
static Notif notifs[NOTIF_MAX];
static int notifCount = 0;
static portMUX_TYPE notifMux = portMUX_INITIALIZER_UNLOCKED;

int notifSel = 0;
volatile int ancsStatus = ANCS_IDLE;
```

## Main functions

### `notifAdd()`

- Inserts a notification into the list.
- Keeps the newest notifications at the front.
- Prevents overflow beyond `NOTIF_MAX`.

### `notifSetText()`

- Fills in app name, title, and message.
- Marks the item as loaded.

### `notifRemove()`

- Removes a notification by UID.

### `notifNext()`

- Moves the selection to the next notification.

### `notifDismissSelected()`

- Removes the current notification and optionally requests dismissal on the phone.

### `notifUnread()`

- Counts how many notifications are currently unread.

### `notifSnapshot()`

- Returns the selected notification for display rendering.

## Why it matters

This module keeps a finite local state of notifications, which the UI can read and show without constantly calling the phone.
