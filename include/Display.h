#pragma once
#include <Arduino.h>
#include <time.h>

enum Screen {
  SCREEN_ANALOGUE,
  SCREEN_DIGITAL,
  SCREEN_STOPWATCH,
  SCREEN_MUSIC,
  SCREEN_COUNT
};

bool displayBegin();
void displayStatus(const char* message);
void displaySetBacklight(bool enabled);

void drawClock(const struct tm& timeInfo);
void drawDigitalClock(const struct tm& timeInfo);
void drawStopwatch();
void drawMusic();
