#include "stopwatch.h"

bool swRunning = false;
uint32_t swStartMs = 0;
uint32_t swBaseMs = 0;

uint32_t stopwatchElapsed() {
  return swRunning ? swBaseMs + (millis() - swStartMs) : swBaseMs;
}

void stopwatchToggle() {
  if (swRunning) {
    swBaseMs += millis() - swStartMs;
    swRunning = false;
  } else {
    swStartMs = millis();
    swRunning = true;
  }
}

void stopwatchReset() {
  swRunning = false;
  swBaseMs = 0;
}
