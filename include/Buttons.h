#pragma once
#include <Arduino.h>

enum BtnEvent { EV_NONE, EV_DOWN, EV_SHORT, EV_LONG };

struct Button {
  uint8_t pin;
  bool lastRaw;
  bool stable;
  uint32_t lastChange;
  uint32_t pressedAt;
  bool longFired;
  bool consume;
};

extern Button btnA;
extern Button btnB;

void buttonsBegin();
BtnEvent updateButton(Button& button);
