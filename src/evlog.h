#pragma once
#include "config.h"

#define LOG_MAX      30
#define LOGF_IGNORED 1   // filtered out by severity / custom error-code rules (no alarm, no notification)
#define LOGF_SILENT  2   // did not ring (event disabled, do-not-disturb, or another alarm already ringing)
#define LOGF_CLEARED 4   // an alarm stopped by itself because the printer cleared the problem
#define LOGF_TIMEOUT 8   // an alarm stopped by itself after its auto-stop time
#define LOGF_BUTTON  16  // an alarm stopped / snoozed with the ACK button or from the web page
#define LOGF_BLIP    32  // not an alarm: the error code is set to "just one short sound" and that sound was played
#define LOGF_TEST    64  // a manual test alarm started from the web page (not a real printer event)

struct LogEntry {
  uint32_t ts;           // unix time, 0 = clock was not synced
  uint8_t ev;            // EventType
  uint8_t printer;       // 0-based
  uint8_t flags;
  char detail[20];       // error code (hex) or empty
};

void evlogBegin();
void evlogAdd(EventType ev, int printer, uint8_t flags, const char* detail);
int  evlogCount();
const LogEntry& evlogGet(int i);   // 0 = newest
void evlogClear();
void evlogTick(bool quiet);   // call from loop(): writes the log to flash when idle (never while quiet == false)
void evlogFlushNow();        // write it now (before a deliberate reboot)
