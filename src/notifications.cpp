#include "notifications.h"
#include "ancs.h"

static Notif notifs[NOTIF_MAX];
static int notifCount = 0;
static portMUX_TYPE notifMux = portMUX_INITIALIZER_UNLOCKED;

int notifSel = 0;
volatile int ancsStatus = ANCS_IDLE;

// Caller must hold notifMux.
static int findUid(uint32_t uid) {
  for (int i = 0; i < notifCount; i++)
    if (notifs[i].uid == uid) return i;
  return -1;
}

void notifAdd(uint32_t uid, uint8_t category, uint8_t flags) {
  portENTER_CRITICAL(&notifMux);
  int i = findUid(uid);
  if (i < 0) {
    int n = (notifCount < NOTIF_MAX) ? notifCount : NOTIF_MAX - 1;
    memmove(&notifs[1], &notifs[0], n * sizeof(Notif));
    memset(&notifs[0], 0, sizeof(Notif));
    notifs[0].uid = uid;
    notifs[0].category = category;
    notifs[0].flags = flags;
    notifs[0].unread = !(flags & 0x04);   // pre-existing ones aren't "new"
    if (notifCount < NOTIF_MAX) notifCount++;
    // keep the user's cursor on the same card when something is inserted above it
    if (notifSel > 0) notifSel = (notifSel + 1 < notifCount) ? notifSel + 1 : notifCount - 1;
  } else {
    notifs[i].category = category;
    notifs[i].flags = flags;
  }
  portEXIT_CRITICAL(&notifMux);
}

void notifSetText(uint32_t uid, const char* app, const char* title, const char* msg) {
  time_t now = time(nullptr);
  portENTER_CRITICAL(&notifMux);
  int i = findUid(uid);
  if (i >= 0) {
    strlcpy(notifs[i].app, app, sizeof(notifs[i].app));
    strlcpy(notifs[i].title, title, sizeof(notifs[i].title));
    strlcpy(notifs[i].msg, msg, sizeof(notifs[i].msg));
    if (!notifs[i].loaded) notifs[i].at = now;
    notifs[i].loaded = true;
  }
  portEXIT_CRITICAL(&notifMux);
}

// Caller must hold notifMux.
static void removeAt(int i) {
  memmove(&notifs[i], &notifs[i + 1], (notifCount - i - 1) * sizeof(Notif));
  notifCount--;
  memset(&notifs[notifCount], 0, sizeof(Notif));
  if (notifSel >= notifCount) notifSel = notifCount > 0 ? notifCount - 1 : 0;
}

void notifRemove(uint32_t uid) {
  portENTER_CRITICAL(&notifMux);
  int i = findUid(uid);
  if (i >= 0) removeAt(i);
  portEXIT_CRITICAL(&notifMux);
}

void notifClear() {
  portENTER_CRITICAL(&notifMux);
  memset(notifs, 0, sizeof(notifs));
  notifCount = 0;
  notifSel = 0;
  portEXIT_CRITICAL(&notifMux);
}

void notifNext() {
  portENTER_CRITICAL(&notifMux);
  if (notifCount > 0) notifSel = (notifSel + 1) % notifCount;
  portEXIT_CRITICAL(&notifMux);
}

void notifDismissSelected() {
  uint32_t uid = 0;
  bool canClearOnPhone = false;
  portENTER_CRITICAL(&notifMux);
  if (notifCount > 0 && notifSel < notifCount) {
    uid = notifs[notifSel].uid;
    canClearOnPhone = notifs[notifSel].flags & 0x10;   // negative action available
    removeAt(notifSel);
  }
  portEXIT_CRITICAL(&notifMux);
  if (uid && canClearOnPhone) ancsQueueDismiss(uid);
}

void notifMarkAllRead() {
  portENTER_CRITICAL(&notifMux);
  for (int i = 0; i < notifCount; i++) notifs[i].unread = false;
  portEXIT_CRITICAL(&notifMux);
}

int notifUnread() {
  int n = 0;
  portENTER_CRITICAL(&notifMux);
  for (int i = 0; i < notifCount; i++)
    if (notifs[i].unread && notifs[i].loaded) n++;
  portEXIT_CRITICAL(&notifMux);
  return n;
}

bool notifSnapshot(Notif& out, int& index, int& total) {
  bool ok = false;
  portENTER_CRITICAL(&notifMux);
  total = notifCount;
  if (notifCount > 0) {
    if (notifSel >= notifCount) notifSel = notifCount - 1;
    index = notifSel;
    out = notifs[notifSel];
    ok = true;
  }
  portEXIT_CRITICAL(&notifMux);
  return ok;
}
