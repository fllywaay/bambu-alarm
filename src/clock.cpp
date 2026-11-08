#include "clock.h"
#include "pins.h"
#include "config.h"
#include "alarm.h"
#include "printer.h"
#include <Arduino.h>
#include <time.h>

// 4-digit TM1637 module with colon (colon = dot of the 2nd digit, the usual clock-module wiring).
// Normal: shows HH:MM (colon blinks). While an alarm rings: shows e.g. "F-1" where the event letter and the
// printer number are fixed/flashing: F = failed, d = done, P = paused, E = error, o = offline.
// Wiring: VCC -> 3V3 (matches the ESP32 logic level), GND, CLK -> GPIO8, DIO -> GPIO9.

static const uint8_t DIG[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
static const uint8_t SEG_DASH = 0x40;

static inline void tmDelay() { delayMicroseconds(8); }
static inline void clkSet(bool h) { digitalWrite(PIN_TM_CLK, h ? HIGH : LOW); }
static inline void dioHigh() { pinMode(PIN_TM_DIO, INPUT_PULLUP); }               // released (module pull-up)
static inline void dioLow() { digitalWrite(PIN_TM_DIO, LOW); pinMode(PIN_TM_DIO, OUTPUT); }

static void tmStart() { dioHigh(); clkSet(true); tmDelay(); dioLow(); tmDelay(); clkSet(false); }
static void tmStop() { clkSet(false); dioLow(); tmDelay(); clkSet(true); tmDelay(); dioHigh(); tmDelay(); }

static void tmWrite(uint8_t b) {
  for (int i = 0; i < 8; i++) {
    clkSet(false);
    if (b & 1) dioHigh(); else dioLow();
    tmDelay();
    clkSet(true);
    tmDelay();
    b >>= 1;
  }
  clkSet(false);
  dioHigh();          // release DIO: the chip pulls it low for the ACK
  tmDelay();
  clkSet(true);       // 9th clock (ACK is not checked)
  tmDelay();
  clkSet(false);
  tmDelay();
}

static uint8_t s_seg[4] = {0, 0, 0, 0};
static bool s_on = false;
static uint8_t s_br = 255;
static bool s_valid = false;
static uint32_t s_lastWrite = 0;

static void tmShow(const uint8_t seg[4], bool on, uint8_t br) {
  uint32_t now = millis();
  if (s_valid && !memcmp(seg, s_seg, 4) && on == s_on && br == s_br && now - s_lastWrite < 5000) return;
  tmStart(); tmWrite(0x40); tmStop();                              // auto-increment address mode
  tmStart(); tmWrite(0xC0);
  for (int i = 0; i < 4; i++) tmWrite(seg[i]);
  tmStop();
  tmStart(); tmWrite((on ? 0x88 : 0x80) | (br & 7)); tmStop();     // display on/off + brightness
  memcpy(s_seg, seg, 4);
  s_on = on;
  s_br = br;
  s_valid = true;
  s_lastWrite = now;
}

static uint8_t evSeg(EventType e) {
  switch (e) {
    case EV_FAILED: return 0x71;   // F
    case EV_FINISH: return 0x5E;   // d
    case EV_PAUSE:  return 0x73;   // P
    case EV_ERROR:  return 0x79;   // E
    case EV_CLOCK:  return 0x39;   // C
    default:        return 0x5C;   // o
  }
}

// Alarm clock ringing: the whole time display flashes (colon stays on).
static void fillTime(uint8_t seg[4]) {
  time_t t = time(nullptr);
  if (t < 1700000000) return;
  struct tm lt;
  localtime_r(&t, &lt);
  seg[0] = DIG[lt.tm_hour / 10];
  seg[1] = DIG[lt.tm_hour % 10] | 0x80;
  seg[2] = DIG[lt.tm_min / 10];
  seg[3] = DIG[lt.tm_min % 10];
}

static uint8_t hexSeg(uint32_t v) {
  static const uint8_t H[16] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
  return H[v & 15];
}

// A printer is paused because of an error: the time, "E-n" (n = printer) and the error code (last 4 hex digits)
// take turns (3 s / 1.5 s / 2 s). Returns false during the "time" part so the caller shows the time.
static bool errFrame(uint32_t now, uint8_t seg[4]) {
  int ep[MAX_PRINTERS], ne = 0;
  for (int i = 0; i < MAX_PRINTERS; i++) {
    if (!cfg.printers[i].enabled) continue;
    const PrinterState& s = printerState(i);
    if (!s.connected || s.err == 0 || strcmp(s.gcode, "PAUSE") != 0) continue;
    char n[12];
    snprintf(n, sizeof(n), "%08lX", (unsigned long)s.err);
    if (errCodeAction(n) == 0) continue;               // an ignored code is not shown
    ep[ne++] = i;
  }
  if (!ne) return false;
  uint32_t ph = now % (6500UL * ne);
  int k = (int)(ph / 6500UL);
  uint32_t t = ph % 6500UL;
  if (t < 3000) return false;
  const PrinterState& s = printerState(ep[k]);
  if (t < 4500) {
    seg[0] = 0x79;                                      // E
    seg[1] = SEG_DASH;
    seg[2] = DIG[ep[k] + 1 > 9 ? 9 : ep[k] + 1];
    seg[3] = 0;
  } else {
    uint32_t v = (uint32_t)s.err;
    seg[0] = hexSeg(v >> 12);
    seg[1] = hexSeg(v >> 8);
    seg[2] = hexSeg(v >> 4);
    seg[3] = hexSeg(v);
  }
  return true;
}

// Print progress on the digits (ACK button held while nothing rings): "n-50" = printer n at 50 %,
// "n-dn" = finished, "n-oF" = offline, "n----" = no job. Several printers take turns (2 s each).
static uint32_t s_progUntil = 0, s_progStart = 0;

void clockShowProgress(uint32_t ms) {
  uint32_t now = millis();
  if ((int32_t)(s_progUntil - now) <= 0) s_progStart = now;   // a new display, start with the first printer
  s_progUntil = now + ms;
}

static bool progFrame(uint32_t now, uint8_t seg[4]) {
  if ((int32_t)(s_progUntil - now) <= 0) return false;
  int ids[MAX_PRINTERS], n = 0;
  for (int i = 0; i < MAX_PRINTERS; i++)
    if (cfg.printers[i].enabled) ids[n++] = i;
  if (!n) return false;
  int i = ids[((now - s_progStart) / 2000) % n];
  const PrinterState& s = printerState(i);
  seg[0] = DIG[i + 1 > 9 ? 9 : i + 1];
  seg[1] = SEG_DASH;
  if (!s.connected) {
    seg[2] = 0x5C; seg[3] = 0x71;                                   // oF
  } else if (!strcmp(s.gcode, "FINISH") || ((!strcmp(s.gcode, "RUNNING") || !strcmp(s.gcode, "PAUSE")) && s.percent >= 100)) {
    seg[2] = 0x5E; seg[3] = 0x54;                                   // dn
  } else if (!strcmp(s.gcode, "RUNNING") || !strcmp(s.gcode, "PAUSE")) {
    int p = s.percent < 0 ? 0 : s.percent;
    seg[2] = DIG[p / 10]; seg[3] = DIG[p % 10];
  } else {
    seg[2] = SEG_DASH; seg[3] = SEG_DASH;                           // no job
  }
  return true;
}

void clockBegin() {
  pinMode(PIN_TM_CLK, OUTPUT);
  clkSet(true);
  dioHigh();
  s_valid = false;
}

// Raw segments (power-on self-test). clockLoop() redraws the normal content afterwards.
void clockShow(const uint8_t seg[4], bool on) { tmShow(seg, on, cfg.clockBright); }

void clockLoop() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 100) return;
  last = now;

  uint8_t seg[4] = {0, 0, 0, 0};
  bool on = cfg.clockEnabled;

  if (!cfg.clockEnabled) {
    on = false;
  } else if (alarmRinging() && alarmEvent() == EV_CLOCK) {
    fillTime(seg);
    on = ((now / 400) % 2) == 0;
  } else if (alarmRinging()) {
    int p = alarmPrinter();
    seg[0] = evSeg(alarmEvent());
    seg[1] = SEG_DASH;
    bool phase = ((now / 400) % 2) == 0;                   // the printer number flashes
    if (phase) seg[2] = (p >= 0 && p < 9) ? DIG[p + 1] : SEG_DASH;
  } else if (progFrame(now, seg)) {
    // progress requested with the ACK button: shown even during do-not-disturb
  } else if (cfg.ledQuietOff && alarmQuietNow()) {
    on = false;                                            // dark during do-not-disturb
  } else if (errFrame(now, seg)) {
    // a printer is paused with an error: time, "E-n" and the error code take turns
  } else {
    time_t t = time(nullptr);
    if (t > 1700000000) {
      struct tm lt;
      localtime_r(&t, &lt);
      seg[0] = DIG[lt.tm_hour / 10];
      seg[1] = DIG[lt.tm_hour % 10];
      seg[2] = DIG[lt.tm_min / 10];
      seg[3] = DIG[lt.tm_min % 10];
      if (lt.tm_sec % 2 == 0) seg[1] |= 0x80;              // colon blinks once per second
    } else {
      seg[0] = seg[1] = seg[2] = seg[3] = SEG_DASH;        // time not synced yet
    }
  }
  tmShow(seg, on, cfg.clockBright);
}
