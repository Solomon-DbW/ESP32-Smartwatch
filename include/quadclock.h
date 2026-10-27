#pragma once

#include "config.h"

// Digital clock where each digit of HH:MM sits in its own quadrant:
//
//        hour tens | hour units
//       -----------+-----------
//       minute tens| minute units
//
// A short press of A (on this screen) switches between 12-hour and 24-hour time.

extern bool clock24h;

void drawQuadClock(const struct tm& t);
