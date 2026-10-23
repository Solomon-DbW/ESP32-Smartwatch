#include "buttons.h"

Button btnA = {PIN_BTN_A, false, false, 0, 0, false, false};
Button btnB = {PIN_BTN_B, false, false, 0, 0, false, false};

BtnEvent updateButton(Button& b) {
  uint32_t now = millis();
  bool raw = (digitalRead(b.pin) == LOW);

  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastChange = now;
  }

  BtnEvent ev = EV_NONE;
  if (now - b.lastChange >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable) {
      b.pressedAt = now;
      b.longFired = false;
      ev = EV_DOWN;
    } else {
      if (!b.longFired && !b.consume) ev = EV_SHORT;
      b.consume = false;
    }
  }

  if (ev == EV_NONE && b.stable && !b.longFired && !b.consume &&
      now - b.pressedAt >= LONG_PRESS_MS) {
    b.longFired = true;
    ev = EV_LONG;
  }
  return ev;
}
