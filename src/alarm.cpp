#include "alarm.h"
#include "audio.h"
#include "evlog.h"
#include "printer.h"
#include "pins.h"
#include <Arduino.h>
#include <time.h>

static bool s_ring = false;
static EventType s_ev = EV_FAILED;
static int s_printer = -1;
static bool s_autoClear = false;     // clear "offline"/"paused" alarms by themselves when the reason is gone
static bool s_sticky = false;        // ring until the button is pressed (no auto-stop, no auto-clear)
static bool s_snoozeSticky = false;
static uint32_t s_goneSince = 0;     // since when the reason for the alarm has been gone (0 = still there)
static uint32_t s_ringStart = 0;
static uint32_t s_stageBase = 0;
static int s_stage = 0;
static bool s_snoozed = false;
static uint32_t s_snoozeAt = 0;
static EventType s_snoozeEv = EV_FAILED;
static int s_snoozePrinter = -1;
static uint32_t s_testSpk = 0, s_testBz = 0;
static int s_bzFreq = 0;
static bool (*s_valid)(EventType) = nullptr;
static bool s_mute = false;

static void setBuzz(int freq) {
  if (freq == s_bzFreq) return;
  s_bzFreq = freq;
#if BUZZER_ACTIVE
  digitalWrite(PIN_BUZZER, freq > 0 ? HIGH : LOW);
#else
  ledcWriteTone(0, freq > 0 ? freq : 0);
#endif
}

static bool inQuiet() {
  if (!cfg.quietEnabled) return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // clock not synced yet
  struct tm t;
  localtime_r(&now, &t);
  int mins = t.tm_hour * 60 + t.tm_min, s = cfg.quietStart, e = cfg.quietEnd, wd = t.tm_wday;
  if (s == e) return false;
  if (s < e) return mins >= s && mins < e && ((cfg.quietDays >> wd) & 1);
  // overnight window: evening part belongs to today's day bit, morning part to yesterday's
  if (mins >= s) return (cfg.quietDays >> wd) & 1;
  if (mins < e) return (cfg.quietDays >> ((wd + 6) % 7)) & 1;
  return false;
}

bool alarmQuietNow() { return inQuiet(); }

static bool silent() { return s_mute && (s_ev != EV_CLOCK || cfg.muteClock); }
bool alarmMuted() { return s_mute; }
void alarmSetMute(bool on) {
  if (on == s_mute) return;
  s_mute = on;
  Serial.printf("Mute mode %s\n", on ? "ON" : "OFF");
  if (!s_ring) return;
  if (silent()) {
    audioStop();
    setBuzz(0);
  } else if (cfg.rules[s_ev].speaker) {          // un-muted while ringing: start the sound again
    char p[64];
    snprintf(p, sizeof(p), "/sounds/%s", cfg.rules[s_ev].sound);
    audioSetGain(0.f);
    audioPlay(p, true);
  }
}

void alarmSetValidator(bool (*fn)(EventType)) { s_valid = fn; }

void alarmBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#if !BUZZER_ACTIVE
  ledcSetup(0, 4000, 8);
  ledcAttachPin(PIN_BUZZER, 0);
  ledcWriteTone(0, 0);
#endif
}

bool alarmTrigger(EventType ev, bool force, int printer, bool sticky) {
  const EventRule& r = cfg.rules[ev];
  if (!force) {
    if (!r.enabled) return false;
    if (s_ring) return false;
    if (inQuiet() && !r.overrideQuiet) {
      Serial.printf("Event %s muted by do-not-disturb\n", EV_KEYS[ev]);
      return false;
    }
  } else if (s_ring) {
    alarmStop();
  }
  s_snoozed = false;
  s_ring = true;
  s_ev = ev;
  s_printer = printer;
  s_autoClear = !force && !sticky;
  s_goneSince = 0;
  s_sticky = sticky;
  s_ringStart = millis();
  uint16_t off = r.startStage == 0 ? 0 : (r.startStage == 1 ? cfg.stage1Sec : cfg.stage2Sec);
  s_stageBase = millis() - (uint32_t)off * 1000UL;
  if (r.speaker && !(s_mute && (ev != EV_CLOCK || cfg.muteClock))) {
    char p[64];
    snprintf(p, sizeof(p), "/sounds/%s", r.sound);
    audioSetGain(0.f);
    audioPlay(p, true);
  }
  return true;
}

void alarmStop() {
  s_ring = false;
  s_snoozed = false;
  s_testSpk = s_testBz = 0;
  audioStop();
  setBuzz(0);
}

void alarmSnooze() {
  if (!s_ring) return;
  EventType ev = s_ev;
  int pr = s_printer;
  bool st = s_sticky;
  alarmStop();
  s_snoozed = true;
  s_snoozeEv = ev;
  s_snoozePrinter = pr;
  s_snoozeSticky = st;
  s_snoozeAt = millis() + (uint32_t)cfg.snoozeMin * 60000UL;
}

void alarmCancelSnooze() { s_snoozed = false; }
bool alarmRinging() { return s_ring; }
bool alarmSnoozed() { return s_snoozed; }
int alarmStage() { return s_stage; }
EventType alarmEvent() { return s_ev; }
int alarmPrinter() { return s_printer; }

void alarmBlip(const char* sound, int ms) {
  if (!sound || !sound[0] || s_ring || s_mute) return;
  Serial.printf("Blip: %s\n", sound);
  char p[64];
  snprintf(p, sizeof(p), "/sounds/%s", sound);
  audioSetGain(cfg.volume / 100.0f);
  audioPlay(p, false);                       // plays once; the timer below stops it if the file is long
  s_testSpk = millis() + (uint32_t)ms;
}

void alarmTestSpeaker(const char* sound, int seconds) {
  if (s_ring) alarmStop();   // a test always wins over a running alarm (used to silently do nothing)
  Serial.printf("Test speaker: %s for %ds\n", sound, seconds);
  char p[64];
  snprintf(p, sizeof(p), "/sounds/%s", sound);
  audioSetGain(cfg.volume / 100.0f);
  audioPlay(p, true);
  s_testSpk = millis() + (uint32_t)seconds * 1000UL;
}

void alarmTestBuzzer(int seconds) {
  if (s_ring) alarmStop();
  Serial.printf("Test buzzer: %d Hz on GPIO%d for %ds (BUZZER_ACTIVE=%d)\n", 3800, PIN_BUZZER, seconds, BUZZER_ACTIVE);
  setBuzz(3800);
  s_testBz = millis() + (uint32_t)seconds * 1000UL;
}

void alarmClockTick() {
  static uint32_t last = 0;
  static int firedKey = -1;                 // yday*1440 + minute of the last ring: one ring per minute
  uint32_t now = millis();
  if (now - last < 1000) return;
  last = now;
  time_t t = time(nullptr);
  if (t < 1700000000) return;               // clock not synced yet
  struct tm lt;
  localtime_r(&t, &lt);
  int mins = lt.tm_hour * 60 + lt.tm_min;
  int key = lt.tm_yday * 1440 + mins;
  if (key == firedKey) return;
  for (int i = 0; i < MAX_CLOCKS; i++) {
    ClockAlarm& a = cfg.clocks[i];
    if (!a.used || !a.enabled || a.time != mins) continue;
    bool once = (a.days == 0);
    if (!once && !((a.days >> lt.tm_wday) & 1)) continue;
    if (s_ring) return;                     // another alarm is ringing right now: try again next second
    firedKey = key;
    Serial.printf("Alarm clock %d (%s) at %02d:%02d\n", i + 1, a.label, mins / 60, mins % 60);
    bool rang = alarmTrigger(EV_CLOCK, false, -1);
    char det[20];
    strlcpy(det, a.label, sizeof(det));
    size_t n = strlen(det);
    if (strlen(a.label) >= sizeof(det)) {   // label was cut: do not leave half a UTF-8 character at the end
      while (n && ((uint8_t)det[n - 1] & 0xC0) == 0x80) n--;
      if (n && (uint8_t)det[n - 1] >= 0xC0) n--;
    }
    det[n] = 0;
    evlogAdd(EV_CLOCK, 0, rang ? 0 : LOGF_SILENT, det);
    if (once) {                             // a one-time alarm switches itself off
      a.enabled = false;
      configSave();
    }
    return;
  }
}

void alarmLoop() {
  uint32_t now = millis();

  if (s_snoozed && (int32_t)(now - s_snoozeAt) >= 0) {
    s_snoozed = false;
    if (!s_snoozeSticky && s_valid && !s_valid(s_snoozeEv)) {
      Serial.printf("Snooze over: %s no longer applies, not ringing again\n", EV_KEYS[s_snoozeEv]);
    } else {
      alarmTrigger(s_snoozeEv, true, s_snoozePrinter, s_snoozeSticky);
      s_autoClear = !s_snoozeSticky;
    }
  }

  if (s_ring) {
    // A new print started on that printer (after a failure, an error or a finished print): that acknowledges the alarm.
    // The printer keeps the old error code / FAILED state until the new job is well under way, so do not wait for it.
    if (s_autoClear && (s_ev == EV_FAILED || s_ev == EV_ERROR || s_ev == EV_FINISH) && printerJobStartedSince(s_printer, s_ringStart)) {
      Serial.printf("%s: a new print started, stopping alarm\n", EV_KEYS[s_ev]);
      evlogAdd(s_ev, s_printer < 0 ? 0 : s_printer, LOGF_CLEARED, "");
      alarmStop();
      return;
    }

    // The reason is gone (printer reconnected / resumed / error cleared on the printer): stop the alarm by itself,
    // but only once it has stayed gone for 8 s, so a flickering status does not cut an alarm short.
    if (s_autoClear && (s_ev == EV_OFFLINE || s_ev == EV_PAUSE || s_ev == EV_ERROR || s_ev == EV_FAILED) && s_valid) {
      if (!s_valid(s_ev)) {
        if (!s_goneSince) s_goneSince = now ? now : 1;
        if (now - s_goneSince >= 8000) {
          Serial.printf("%s cleared by itself, stopping alarm\n", EV_KEYS[s_ev]);
          evlogAdd(s_ev, s_printer < 0 ? 0 : s_printer, LOGF_CLEARED, "");   // so the log shows why it stopped
          alarmStop();
          return;
        }
      } else {
        s_goneSince = 0;
      }
    }

    const EventRule& r = cfg.rules[s_ev];
    float t = (now - s_stageBase) / 1000.0f;
    s_stage = t < cfg.stage1Sec ? 0 : (t < cfg.stage2Sec ? 1 : 2);

    if (!s_sticky && r.autoStopSec && (now - s_ringStart) / 1000UL >= r.autoStopSec) {
      evlogAdd(s_ev, s_printer < 0 ? 0 : s_printer, LOGF_TIMEOUT, "");
      alarmStop();
      return;
    }

    float vol = cfg.volume / 100.0f;
    if (r.speaker && !silent()) {
      float ramp = 1.0f;
      if (s_stage == 0) {
        ramp = 0.12f + 0.88f * (t / (float)cfg.stage1Sec);
        if (ramp > 1.0f) ramp = 1.0f;
      }
      audioSetGain(vol * ramp);
    }

    int f = 0;
    if (r.buzzer && !silent()) {
      if (s_stage == 1) f = (now % 1000 < 400) ? 3800 : 0;
      else if (s_stage == 2) f = ((now / 150) % 2) ? 3200 : 4000;
    }
    setBuzz(f);
  } else {
    if (s_testSpk && (int32_t)(now - s_testSpk) >= 0) { s_testSpk = 0; audioStop(); }
    if (s_testBz && (int32_t)(now - s_testBz) >= 0) { s_testBz = 0; setBuzz(0); }
  }
}
