#pragma once

// Convenience header: pulls in the modules that own shared state.
// swRunning/swStartMs/swBaseMs  -> stopwatch.h
// musicSel/musicFlash*          -> music_remote.h
// screen/screenOn/needRedraw/lastActivity -> config.h (defined in app_state.cpp)
#include "config.h"
#include "stopwatch.h"
#include "music_remote.h"
