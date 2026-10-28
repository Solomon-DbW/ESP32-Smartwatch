# include/notifications.h

## Purpose

This header defines the local notification list used to display iPhone notifications on the watch.

## Syntax overview

```c
#pragma once

#include "config.h"

constexpr int NOTIF_MAX = 10;

enum AncsStatus { ANCS_IDLE, ANCS_READY, ANCS_NOT_FOUND, ANCS_ERROR };

struct Notif {
  uint32_t uid;
  uint8_t  category;
  uint8_t  flags;
  bool     loaded;
  bool     unread;
  char     app[24];
  char     title[48];
  char     msg[96];
  time_t   at;
};

extern volatile int ancsStatus;
extern int notifSel;

void notifAdd(uint32_t uid, uint8_t category, uint8_t flags);
void notifSetText(uint32_t uid, const char* app, const char* title, const char* msg);
void notifRemove(uint32_t uid);
void notifClear();
void notifNext();
void notifDismissSelected();
void notifMarkAllRead();
int  notifUnread();
bool notifSnapshot(Notif& out, int& index, int& total);
```

## Behavior model

- Notifications are stored in a small ring-like list with a maximum size of 10.
- `notifSel` tracks which notification is currently selected.
- `unread` is used for the small orange badge on the digital clock.
- `loaded` indicates whether the message text has successfully arrived.

## Why it matters

This is the watch’s notification data model. The actual receiving logic is in `src/ancs.cpp`, and the UI is handled in `src/display.cpp`.
