#pragma once
#include "config.h"
void alarmBegin();
void alarmLoop();
// Returns true if the alarm actually started ringing (false = disabled / muted by do-not-disturb / already ringing).
// sticky = ring until the button is pressed: no auto-stop timer, and the alarm is not cleared by itself when the printer
// clears the error (used for error codes set to "alarm until button").
bool alarmTrigger(EventType ev, bool force = false, int printer = -1, bool sticky = false);
// A short one-off sound that is NOT an alarm (e.g. print cancelled by hand). Does nothing while ringing or muted.
void alarmBlip(const char* sound, int ms);
void alarmStop();
void alarmSnooze();
void alarmCancelSnooze();
bool alarmRinging();
bool alarmSnoozed();
bool alarmQuietNow();
int  alarmStage();
EventType alarmEvent();      // event of the current / last alarm
int  alarmPrinter();         // printer index (0-based) of the current / last alarm, -1 = unknown
void alarmTestSpeaker(const char* sound, int seconds);
void alarmTestBuzzer(int seconds);
// Lets the alarm ask whether the reason for an alarm still exists (used after snooze, and to auto-clear
// "offline" / "paused" alarms when the printer recovers).
void alarmSetValidator(bool (*fn)(EventType));
// Mute mode: alarms still show on the LEDs / clock and are logged and pushed, but make no sound
// (the alarm clock too only if cfg.muteClock). Not saved: back to normal after a reboot.
void alarmSetMute(bool on);
bool alarmMuted();
// Call every loop: rings the alarm clocks (config.clocks) when their time comes.
void alarmClockTick();
