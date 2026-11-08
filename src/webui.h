#pragma once
#include <Arduino.h>
void webBegin();            // start the HTTP server (+ mDNS). Does not touch the hotspot.
void webEnd();              // stop the server and the hotspot
void webApSet(bool on);     // hotspot "H2D-Alarm-Setup" on/off
void webLoop();
bool webExitRequested();
uint32_t webLastActivity();
bool webActive();
void webSetLanOpen(bool open);   // true while in setup mode: LAN clients may use the page without password
void onConfigSaved();   // implemented in main.cpp
void ntpSyncNow();      // implemented in main.cpp: (re)start NTP right now
void selfTestNow();     // implemented in main.cpp: run the LED / digits / sound self-test (about 2 s)
