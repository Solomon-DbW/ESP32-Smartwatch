#pragma once

#include "music_remote.h"

// Apple Notification Center Service client. Uses the same back-connection to the
// iPhone that the Apple Media Service client uses (see music_remote.cpp).

bool ancsSubscribe(BLEClient* client);   // find ANCS and subscribe (idempotent)
void ancsPump();                         // send queued requests; call from the BLE task
void ancsReset();                        // phone disconnected
void ancsQueueDismiss(uint32_t uid);     // ask the iPhone to clear a notification
