#include "ancs.h"
#include "notifications.h"

#define ANCS_SERVICE_UUID       "7905F431-B5CE-4E99-A40F-4B1E122D00D0"
#define ANCS_NOTIF_SOURCE_UUID  "9FBF120D-6301-42D9-8C58-25E699A21DBD"
#define ANCS_CONTROL_POINT_UUID "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9"
#define ANCS_DATA_SOURCE_UUID   "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB"

static const uint8_t EV_ADDED = 0;
static const uint8_t EV_MODIFIED = 1;
static const uint8_t EV_REMOVED = 2;

static const uint8_t CMD_GET_NOTIF_ATTRS = 0;
static const uint8_t CMD_PERFORM_ACTION = 2;
static const uint8_t ATTR_APP_ID = 0;
static const uint8_t ATTR_TITLE = 1;
static const uint8_t ATTR_MESSAGE = 3;
static const uint8_t ACTION_NEGATIVE = 1;

static BLERemoteCharacteristic* ancsControl = nullptr;
static portMUX_TYPE ancsMux = portMUX_INITIALIZER_UNLOCKED;

// Small FIFO queues, filled from BLE callbacks, drained by ancsPump() in amsTask.
static const int QLEN = 16;
static uint32_t fetchQ[QLEN];
static int fetchHead = 0, fetchCount = 0;
static uint32_t dismissQ[QLEN];
static int dismissHead = 0, dismissCount = 0;

// Data Source reassembly (responses can span several notifications).
static uint8_t dsBuf[768];
static size_t dsLen = 0;
static volatile uint32_t waitUid = 0;
static volatile uint32_t waitSince = 0;
static uint8_t waitTries = 0;
static uint32_t lastReqUid = 0;

static void push(uint32_t* q, int& head, int& count, uint32_t uid) {
  portENTER_CRITICAL(&ancsMux);
  for (int i = 0; i < count; i++)
    if (q[(head + i) % QLEN] == uid) { portEXIT_CRITICAL(&ancsMux); return; }
  // When full, drop the OLDEST entry: on connect the iPhone replays every pending
  // notification oldest-first, and the newest ones are the ones we keep.
  if (count == QLEN) { head = (head + 1) % QLEN; count--; }
  q[(head + count) % QLEN] = uid;
  count++;
  portEXIT_CRITICAL(&ancsMux);
}

static bool pop(uint32_t* q, int& head, int& count, uint32_t& uid) {
  bool ok = false;
  portENTER_CRITICAL(&ancsMux);
  if (count > 0) { uid = q[head]; head = (head + 1) % QLEN; count--; ok = true; }
  portEXIT_CRITICAL(&ancsMux);
  return ok;
}

static void friendlyApp(const char* bundle, char* out, size_t outSize) {
  char low[64];
  size_t n = 0;
  for (; bundle[n] && n < sizeof(low) - 1; n++) low[n] = tolower((unsigned char)bundle[n]);
  low[n] = 0;

  static const struct { const char* key; const char* name; } map[] = {
    {"mobilesms", "Messages"}, {"mobilephone", "Phone"}, {"facetime", "FaceTime"},
    {"mobilemail", "Mail"}, {"whatsapp", "WhatsApp"}, {"instagram", "Instagram"},
    {"discord", "Discord"}, {"telegram", "Telegram"}, {"snapchat", "Snapchat"},
    {"tiktok", "TikTok"}, {"reminders", "Reminders"}, {"mobilecal", "Calendar"},
    {"slack", "Slack"}, {"gmail", "Gmail"}, {"youtube", "YouTube"},
    {"spotify", "Spotify"}, {"outlook", "Outlook"}, {"teams", "Teams"},
    {"facebook", "Facebook"}, {"messenger", "Messenger"}, {"twitter", "X"},
  };
  for (const auto& m : map) {
    if (strstr(low, m.key)) { strlcpy(out, m.name, outSize); return; }
  }
  const char* dot = strrchr(bundle, '.');
  strlcpy(out, dot ? dot + 1 : bundle, outSize);
  if (out[0] >= 'a' && out[0] <= 'z') out[0] -= 32;
}

static void onNotifSource(BLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len < 8) return;
  uint8_t event = d[0], flags = d[1], category = d[2];
  uint32_t uid = d[4] | (d[5] << 8) | (d[6] << 16) | ((uint32_t)d[7] << 24);
  Serial.printf("ANCS: NS event=%u flags=0x%02X cat=%u uid=%u\n", event, flags, category, (unsigned)uid);

  if (event == EV_REMOVED) {
    notifRemove(uid);
  } else if (event == EV_ADDED || event == EV_MODIFIED) {
    notifAdd(uid, category, flags);
    push(fetchQ, fetchHead, fetchCount, uid);
  }
}

// Returns true once the whole response has been parsed and stored.
static bool parseDataSource() {
  if (dsLen < 5 || dsBuf[0] != CMD_GET_NOTIF_ATTRS) return false;
  uint32_t uid = dsBuf[1] | (dsBuf[2] << 8) | (dsBuf[3] << 16) | ((uint32_t)dsBuf[4] << 24);

  // static: this only ever runs in the single BLE callback task, and it keeps
  // these large buffers off that task's small stack
  static char app[64], title[64], msg[448];
  app[0] = title[0] = msg[0] = 0;
  uint8_t got = 0;
  size_t p = 5;
  while (p + 3 <= dsLen) {
    uint8_t id = dsBuf[p];
    size_t l = dsBuf[p + 1] | (dsBuf[p + 2] << 8);
    if (p + 3 + l > dsLen) return false;     // attribute not fully received yet

    char* dst = nullptr;
    size_t cap = 0;
    if (id == ATTR_APP_ID)       { dst = app;   cap = sizeof(app);   got |= 1; }
    else if (id == ATTR_TITLE)   { dst = title; cap = sizeof(title); got |= 2; }
    else if (id == ATTR_MESSAGE) { dst = msg;   cap = sizeof(msg);   got |= 4; }
    if (dst) {
      size_t n = (l < cap - 1) ? l : cap - 1;
      memcpy(dst, dsBuf + p + 3, n);
      dst[n] = 0;
    }
    p += 3 + l;
  }
  if (got != 7) return false;

  static char appName[24], cleanTitle[64], cleanMsg[448];
  friendlyApp(app, appName, sizeof(appName));
  asciiFold(title, cleanTitle, sizeof(cleanTitle));
  asciiFold(msg, cleanMsg, sizeof(cleanMsg));
  notifSetText(uid, appName, cleanTitle, cleanMsg);
  return true;
}

static void onDataSource(BLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  Serial.printf("ANCS: DS chunk len=%u (have %u)\n", (unsigned)len, (unsigned)dsLen);
  if (dsLen + len > sizeof(dsBuf)) { dsLen = 0; waitUid = 0; return; }
  memcpy(dsBuf + dsLen, d, len);
  dsLen += len;
  if (parseDataSource()) { Serial.println("ANCS: notification text stored"); dsLen = 0; waitUid = 0; }
}

bool ancsSubscribe(BLEClient* client) {
  if (ancsStatus == ANCS_READY) return true;

  BLERemoteService* svc = client->getService(BLEUUID(ANCS_SERVICE_UUID));
  if (!svc) {
    Serial.println("ANCS: service not found (enable Show Notifications for the watch on the iPhone)");
    ancsStatus = ANCS_NOT_FOUND;
    return false;
  }

  BLERemoteCharacteristic* src = svc->getCharacteristic(BLEUUID(ANCS_NOTIF_SOURCE_UUID));
  BLERemoteCharacteristic* data = svc->getCharacteristic(BLEUUID(ANCS_DATA_SOURCE_UUID));
  BLERemoteCharacteristic* ctl = svc->getCharacteristic(BLEUUID(ANCS_CONTROL_POINT_UUID));
  if (!src || !data || !ctl) {
    Serial.println("ANCS: characteristics missing");
    ancsStatus = ANCS_ERROR;
    return false;
  }

  ancsControl = ctl;
  dsLen = 0;
  waitUid = 0;
  // Apple recommends subscribing to Data Source before Notification Source.
  data->registerForNotify(onDataSource);
  src->registerForNotify(onNotifSource);

  ancsStatus = ANCS_READY;
  Serial.println("ANCS: ready");
  return true;
}

void ancsQueueDismiss(uint32_t uid) {
  push(dismissQ, dismissHead, dismissCount, uid);
}

void ancsPump() {
  if (ancsStatus != ANCS_READY || !ancsControl) return;

  uint32_t now = millis();
  if (waitUid) {
    if (now - waitSince < 3000) return;      // still waiting for a response
    Serial.printf("ANCS: no response for uid=%u (try %u)\n", (unsigned)waitUid, waitTries);
    if (waitTries < 3) push(fetchQ, fetchHead, fetchCount, waitUid);   // retry a couple of times
    waitUid = 0;
    dsLen = 0;
  }

  uint32_t uid;
  if (pop(dismissQ, dismissHead, dismissCount, uid)) {
    uint8_t cmd[6] = { CMD_PERFORM_ACTION,
                       (uint8_t)uid, (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24),
                       ACTION_NEGATIVE };
    ancsControl->writeValue(cmd, sizeof(cmd), true);
    return;
  }

  // Skip UIDs that have already fallen out of the (10-entry) store or were cleared.
  bool haveFetch = false;
  while (pop(fetchQ, fetchHead, fetchCount, uid)) {
    if (notifHas(uid)) { haveFetch = true; break; }
  }
  if (haveFetch) {
    uint8_t req[] = { CMD_GET_NOTIF_ATTRS,
                      (uint8_t)uid, (uint8_t)(uid >> 8), (uint8_t)(uid >> 16), (uint8_t)(uid >> 24),
                      ATTR_APP_ID,
                      ATTR_TITLE, 40, 0,
                      ATTR_MESSAGE, 0x90, 0x01 };   // up to 400 characters
    dsLen = 0;
    waitTries = (uid == lastReqUid) ? waitTries + 1 : 1;
    lastReqUid = uid;
    waitUid = uid;
    waitSince = now;
    Serial.printf("ANCS: requesting attributes for uid=%u\n", (unsigned)uid);
    ancsControl->writeValue(req, sizeof(req), true);
    Serial.println("ANCS: request written");
  }
}

void ancsReset() {
  ancsStatus = ANCS_IDLE;
  ancsControl = nullptr;
  portENTER_CRITICAL(&ancsMux);
  fetchHead = fetchCount = dismissHead = dismissCount = 0;
  portEXIT_CRITICAL(&ancsMux);
  dsLen = 0;
  waitUid = 0;
  lastReqUid = 0;
  notifClear();
}
