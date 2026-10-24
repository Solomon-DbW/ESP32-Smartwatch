#pragma once

#include "config.h"

// Local store of iPhone notifications received over ANCS (newest first).

constexpr int NOTIF_MAX = 10;

enum AncsStatus { ANCS_IDLE, ANCS_READY, ANCS_NOT_FOUND, ANCS_ERROR };

struct Notif {
  uint32_t uid;
  uint8_t  category;
  uint8_t  flags;      // ANCS EventFlags
  bool     loaded;     // text has arrived
  bool     unread;
  char     app[24];
  char     title[48];
  char     msg[400];
  time_t   at;         // when the watch received the text
};

extern volatile int ancsStatus;
extern int notifSel;
extern bool notifDetail;   // showing the full-message view
extern int notifPage;      // page within the full-message view

void notifAdd(uint32_t uid, uint8_t category, uint8_t flags);
void notifSetText(uint32_t uid, const char* app, const char* title, const char* msg);
void notifRemove(uint32_t uid);
bool notifHas(uint32_t uid);
void notifClear();
void notifNext();
void notifDismissSelected();
bool notifOpenSelected();   // switch to the full-message view; false if nothing to open
void notifMarkAllRead();
int  notifUnread();
// Copies the selected notification. Returns false if the list is empty.
bool notifSnapshot(Notif& out, int& index, int& total);
