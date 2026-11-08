#pragma once
#include "config.h"

void notifyBegin();
// Queue a push message for every enabled channel that wants this event. Never blocks.
void notifyEvent(EventType ev, int printerIdx, const char* detail);
// Queue a test message for one channel slot.
void notifyTest(int slot);
// HTTP status of the last attempt for a slot (0 = never sent, <0 = connection / config error).
int notifyLastResult(int slot);
