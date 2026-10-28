# include/notifications.h

## Purpose

This header defines the notification store and the UI state used for reading full notifications on the watch.

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
  char     msg[400];
  time_t   at;
};

extern volatile int ancsStatus;
extern int notifSel;
extern bool notifDetail;
extern int notifPage;

void notifAdd(uint32_t uid, uint8_t category, uint8_t flags);
void notifSetText(uint32_t uid, const char* app, const char* title, const char* msg);
void notifRemove(uint32_t uid);
bool notifHas(uint32_t uid);
void notifClear();
void notifNext();
void notifDismissSelected();
bool notifOpenSelected();
void notifMarkAllRead();
int  notifUnread();
bool notifSnapshot(Notif& out, int& index, int& total);
```

## Current repo additions

The current version supports:

- `notifDetail`: full-message view mode
- `notifPage`: page index for multi-page notification text
- `notifOpenSelected()`: open a selected notification into expanded detail view

These additions are used by the updated notification screen in `src/display.cpp` and by the updated button handling in `src/main.cpp`.
