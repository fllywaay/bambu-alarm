#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

enum EventType { EV_FAILED = 0, EV_FINISH, EV_PAUSE, EV_ERROR, EV_OFFLINE, EV_CLOCK, EV_COUNT };
extern const char* EV_KEYS[EV_COUNT];

// Device language (cfg.lang): one setting for everything the device writes in words - push notifications,
// the web page, the official error texts. 0 = Chinese, 1 = English.
#define LANG_ZH 0
#define LANG_EN 1
const char* evName(int ev, uint8_t lang);   // display name of an event in the given language
static inline const char* tr(uint8_t lang, const char* zh, const char* en) { return lang == LANG_EN ? en : zh; }

struct EventRule {
  bool enabled;
  uint8_t startStage;      // 0 gentle, 1 loud, 2 max
  bool speaker;
  bool buzzer;
  bool overrideQuiet;      // still ring during quiet hours
  uint16_t autoStopSec;    // 0 = ring until button
  char sound[32];
};

#define MAX_PRINTERS 4   // each extra printer costs ~40-50KB of internal heap (TLS)
#define MAX_ERRCODES 40
#define MAX_NOTIFY   4
#define MAX_CLOCKS   6

// Wake-up / reminder alarm clock. days: bit0 = Sunday ... bit6 = Saturday; 0 = once (disables itself after ringing).
struct ClockAlarm {
  bool used;
  bool enabled;
  uint16_t time;           // minutes since midnight
  uint8_t days;
  char label[24];
};

struct PrinterCfg {
  bool enabled;
  char name[24];
  char ip[16];
  char serial[24];
  char code[12];
};

// Custom error-code rule. code = hex digits only, upper-case:
//   print_error  -> 8 digits  (e.g. 0300400C)
//   HMS entry    -> 16 digits (attr+code, e.g. 0700200000020001)
struct ErrCodeCfg {
  char code[17];
  uint8_t action;          // 0 = ignore, 1 = always alarm, 2 = label only, 3 = alarm until the button is pressed, 4 = just one short sound
  char note[48];
};

// Notification channel types
enum NotifyType { NT_NTFY = 0, NT_BARK, NT_TELEGRAM, NT_WEBHOOK, NT_FEISHU, NT_WECOM, NT_DINGTALK };

struct NotifyCfg {
  bool enabled;
  uint8_t type;            // NotifyType
  uint8_t evMask;          // bit per EventType
  char name[16];
  char url[160];           // ntfy: https://ntfy.sh/topic | bark: server | telegram: optional API base | webhooks: full URL
  char key[100];           // ntfy: optional token | bark: device key | telegram: bot token
  char extra[48];          // telegram: chat id | feishu/dingtalk: optional keyword prefix
};

struct Config {
  char ssid[33];
  char pass[65];
  char host[24];
  PrinterCfg printers[MAX_PRINTERS];
  EventRule rules[EV_COUNT];
  uint8_t volume;          // 0-100
  uint16_t stage1Sec;
  uint16_t stage2Sec;
  uint16_t snoozeMin;
  uint16_t offlineSec;
  bool quietEnabled;
  uint16_t quietStart;     // minutes since midnight
  uint16_t quietEnd;
  uint8_t quietDays;       // bit0=Sun ... bit6=Sat (day the window starts on)
  char tz[40];
  uint16_t setupTimeoutMin;
  bool ledProgress;        // show print progress on the LED ring
  uint8_t ledMode;         // 0 = auto segments (all active printers), 1 = one chosen printer
  uint8_t ledPrinter;      // printer index for mode 1
  uint8_t ledBright;       // 1-100 %
  bool ledQuietOff;        // LEDs / clock off during do-not-disturb window
  bool apAlways;           // keep the setup hotspot on all the time
  char apPass[33];         // hotspot password (>= 8 chars)
  // --- added in v2 ---
  bool lanAccess;          // settings page reachable from the home LAN (PC) without pressing the SETUP button
  char webPass[33];        // optional password for LAN access (user "admin"); empty = no password
  uint8_t hmsSev;          // alarm on HMS with severity <= this (1 fatal, 2 serious, 3 common, 4 info)
  bool ignoreAms;          // ignore AMS-module HMS notices (unless fatal)
  bool ignoreCancel;       // a user-cancelled print is not a failure / error
  ErrCodeCfg errCodes[MAX_ERRCODES];
  uint8_t errDefVer;       // built-in default ignore list already merged (so deleted entries stay deleted)
  NotifyCfg notify[MAX_NOTIFY];
  bool clockEnabled;       // TM1637 clock display
  uint8_t clockBright;     // 0-7
  uint8_t ledIdle;         // 0-100 %: idle / waiting light and the unlit track of the progress bar (0 = off, 50 = original level)
  uint8_t ledDone;         // 0-100 %: brightness of the "finished" bar (0 = off)
  bool muteClock;          // mute mode (SETUP button held 3 s) also silences the alarm clock
  bool selfTest;           // power-on self-test: LED sweep, digits, a short sound
  bool selfTestSound;      // ... including the sound
  char cancelSound[32];    // short sound when a print is cancelled by hand (file name in /sounds, empty = silent)
  char noticeSound[32];    // short sound for error codes set to "just one short sound"
  ClockAlarm clocks[MAX_CLOCKS];   // alarm clocks (ring with the sound / strength of the "clock" event rule)
  uint8_t lang;            // LANG_ZH (0) or LANG_EN (1): push messages, web page and official error texts
  char ghRepo[96];         // firmware update source: "owner/repo" (latest GitHub release) or a full http(s):// folder URL; empty = none
};

extern Config cfg;
void configDefaults();
bool configLoad();
bool configSave();
void configToJson(JsonObject o);
void configFromJson(JsonVariantConst d);

// error-code helpers
void errNormalize(const char* in, char* out, size_t n);   // keep hex digits, upper-case, drop "HMS" prefix
int  errCodeAction(const char* normCode);                 // -1 = no rule, 0 = ignore, 1 = always alarm
bool errCodeSticky(const char* normCode);                 // rule says: ring until the button is pressed
bool errCodeNotice(const char* normCode);                 // rule says: just one short sound, no alarm
bool errIsCancel(uint32_t printError);                    // "task cancelled by user" codes
bool errCodeNote(const char* normCode, char* out, size_t n);   // copy the note of a custom code rule (false = none)

// Lock for the shared cfg: hold it while rewriting cfg (web save) or while copying fields out in another task.
// Not recursive. The errCode* helpers above take it themselves, so never call them while holding it.
void cfgLock();
void cfgUnlock();
