#include "config.h"
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctype.h>
#include <strings.h>

Config cfg;

// One lock for the shared configuration: the web handler rewrites it while the network / notify tasks read it.
static SemaphoreHandle_t s_cfgMux = nullptr;
void cfgLock() { if (s_cfgMux) xSemaphoreTake(s_cfgMux, portMAX_DELAY); }
void cfgUnlock() { if (s_cfgMux) xSemaphoreGive(s_cfgMux); }
const char* EV_KEYS[EV_COUNT] = {"failed", "finish", "pause", "error", "offline", "clock"};
static const char* EV_NAMES_ZH[EV_COUNT] = {"打印失败", "打印完成", "打印暂停/断料", "打印机错误", "打印机失联", "闹钟"};
static const char* EV_NAMES_EN[EV_COUNT] = {"Print failed", "Print finished", "Print paused / filament out", "Printer error", "Printer offline", "Alarm clock"};
const char* evName(int ev, uint8_t lang) {
  if (ev < 0 || ev >= EV_COUNT) ev = 0;
  return lang == LANG_EN ? EV_NAMES_EN[ev] : EV_NAMES_ZH[ev];
}

static void rule(EventType e, bool en, uint8_t st, bool sp, bool bz, bool ovr, uint16_t autoStop, const char* snd) {
  EventRule& r = cfg.rules[e];
  r.enabled = en;
  r.startStage = st;
  r.speaker = sp;
  r.buzzer = bz;
  r.overrideQuiet = ovr;
  r.autoStopSec = autoStop;
  strlcpy(r.sound, snd, sizeof(r.sound));
}

// Built-in default error-code list (print_error codes are 8 hex digits, HMS 16).
// action 0 = ignore, 2 = label only (normal rules apply, the note is just shown in the log / notification).
// Merged into the config once per errDefVer, so entries you delete later stay deleted.
struct DefErr { const char* code; const char* note; uint8_t action; const char* oldNote; };   // oldNote: an earlier default note to replace
static const DefErr DEF_ERR[] = {
  // Only rules that change behaviour stay. Plain labels are gone: the official Bambu text (data/hms.bin) says the same.
  {"0300A70000030001", "", 0},   // ignore (the old label "换料" was wrong: the official text is about the chamber exhaust fan)
  {"0C00010000020017", "", 4, "换料"},   // nozzle camera dirty: just one short sound
};

static void addErrDefaults() {
  for (const DefErr& d : DEF_ERR) {
    int freeSlot = -1, have = -1;
    for (int i = 0; i < MAX_ERRCODES; i++) {
      if (!strcmp(cfg.errCodes[i].code, d.code)) have = i;
      if (!cfg.errCodes[i].code[0] && freeSlot < 0) freeSlot = i;
    }
    if (have >= 0) {
      // an earlier build stored the label-only codes as "ignore": fix that
      ErrCodeCfg& E = cfg.errCodes[have];
      // an earlier build stored these defaults with a different action: bring them up to date
      if (d.oldNote && !strcmp(E.note, d.oldNote)) {                       // an earlier build had this code under another name
        strlcpy(E.note, d.note, sizeof(E.note));
        E.action = d.action;
      }
      if (!strcmp(E.note, d.note) && ((d.action == 2 && E.action == 0) || (d.action == 1 && (E.action == 2 || E.action == 3)))) E.action = d.action;
      continue;
    }
    if (freeSlot < 0) continue;
    ErrCodeCfg& E = cfg.errCodes[freeSlot];
    memset(&E, 0, sizeof(E));
    strlcpy(E.code, d.code, sizeof(E.code));
    E.action = d.action;
    strlcpy(E.note, d.note, sizeof(E.note));
  }
}

// errDefVer 7: the label-only defaults were dropped because the official Bambu text (data/hms.bin) says the same.
// A stored entry is removed only while it is still exactly the old default (same note and action); anything you edited stays.
struct OldDef { const char* code; const char* note; uint8_t action; bool dropRule; };   // dropRule false = keep the rule, clear only the note
static const OldDef OLD_DEF[] = {
  {"0500808F", "摄像头清理暂停", 2, true},
  {"03008003", "打印出现炒面", 2, true},
  {"0C00803F", "打印裹头需处理", 2, true},
  {"0300400C", "手动中止打印", 2, true},
  {"0500806E", "热床上有异物", 1, true},       // "always ring" is what a print_error does anyway
  {"0300A70000030001", "换料", 0, false},        // keep ignoring it, but that label was wrong
  {"0C00010000020017", "喷嘴摄像头脏了", 4, false},   // keep the one-short-sound rule
};

static void removeOldDefaults() {
  for (const OldDef& d : OLD_DEF)
    for (int i = 0; i < MAX_ERRCODES; i++) {
      ErrCodeCfg& E = cfg.errCodes[i];
      if (strcmp(E.code, d.code) || strcmp(E.note, d.note)) continue;
      if (d.dropRule) {
        if (E.action == d.action) memset(&E, 0, sizeof(E));
      } else {
        E.note[0] = 0;
      }
    }
}

void configDefaults() {
  if (!s_cfgMux) s_cfgMux = xSemaphoreCreateMutex();   // first call is in setup(), before any task exists
  memset(&cfg, 0, sizeof(cfg));
  strlcpy(cfg.host, "bambu-radar", sizeof(cfg.host));
  strlcpy(cfg.tz, "CST-8", sizeof(cfg.tz));
  cfg.volume = 70;
  cfg.stage1Sec = 30;
  cfg.stage2Sec = 90;
  cfg.snoozeMin = 5;
  cfg.offlineSec = 120;
  cfg.quietEnabled = false;
  cfg.quietStart = 23 * 60;
  cfg.quietEnd = 7 * 60;
  cfg.quietDays = 0x7F;
  cfg.setupTimeoutMin = 15;
  cfg.ledProgress = true;
  cfg.ledMode = 0;
  cfg.ledPrinter = 0;
  cfg.ledBright = 20;
  cfg.ledQuietOff = true;
  cfg.apAlways = true;
  strlcpy(cfg.apPass, "h2d-alarm", sizeof(cfg.apPass));
  cfg.lanAccess = true;
  cfg.webPass[0] = 0;
  cfg.hmsSev = 2;          // fatal + serious only; "common" and "info" notices are only logged
  cfg.ignoreAms = false;
  cfg.ignoreCancel = true;
  for (int i = 0; i < MAX_NOTIFY; i++) cfg.notify[i].evMask = 0x1F;
  cfg.clockEnabled = true;
  cfg.clockBright = 3;
  strlcpy(cfg.noticeSound, "beep.wav", sizeof(cfg.noticeSound));
  cfg.ledIdle = 50;
  cfg.ledDone = 35;
  cfg.muteClock = false;
  cfg.selfTest = true;
  cfg.selfTestSound = true;
  rule(EV_FAILED,  true, 1, true, true,  true,  0,  "alarm_urgent.wav");
  rule(EV_FINISH,  true, 0, true, false, false, 30, "chime_soft.wav");
  rule(EV_PAUSE,   true, 1, true, true,  true,  0,  "warning.wav");
  rule(EV_ERROR,   true, 1, true, true,  true,  0,  "warning.wav");
  rule(EV_OFFLINE, true, 0, true, true,  true,  0,  "beep.wav");
  rule(EV_CLOCK,   true, 0, true, false, true,  300, "chime_soft.wav");   // alarm clock: gentle start, rings through do-not-disturb
  addErrDefaults();
  cfg.errDefVer = 7;
}

// ----------------------------------------------------------- error codes ----
void errNormalize(const char* in, char* out, size_t n) {
  size_t k = 0;
  const char* p = in ? in : "";
  while (*p == ' ') p++;
  if (strncasecmp(p, "HMS", 3) == 0) p += 3;
  for (; *p && k + 1 < n; p++)
    if (isxdigit((unsigned char)*p)) out[k++] = (char)toupper((unsigned char)*p);
  out[k] = 0;
}

static int errCodeActionU(const char* normCode) {
  for (int i = 0; i < MAX_ERRCODES; i++)
    if (cfg.errCodes[i].code[0] && !strcmp(cfg.errCodes[i].code, normCode))
      return cfg.errCodes[i].action == 3 ? 1 : (cfg.errCodes[i].action == 4 ? 0 : (cfg.errCodes[i].action <= 1 ? cfg.errCodes[i].action : -1));   // 3 = ring until button; 4 = one short sound (no alarm); 2 = label only
  return -1;
}

static bool errCodeNoticeU(const char* normCode) {
  for (int i = 0; i < MAX_ERRCODES; i++)
    if (cfg.errCodes[i].code[0] && !strcmp(cfg.errCodes[i].code, normCode)) return cfg.errCodes[i].action == 4;
  return false;
}

static bool errCodeStickyU(const char* normCode) {
  for (int i = 0; i < MAX_ERRCODES; i++)
    if (cfg.errCodes[i].code[0] && !strcmp(cfg.errCodes[i].code, normCode)) return cfg.errCodes[i].action == 3;
  return false;
}

// Locked wrappers: these are called from the main loop, the network task and the web handlers.
int errCodeAction(const char* c) { cfgLock(); int r = errCodeActionU(c); cfgUnlock(); return r; }
bool errCodeNotice(const char* c) { cfgLock(); bool r = errCodeNoticeU(c); cfgUnlock(); return r; }
bool errCodeSticky(const char* c) { cfgLock(); bool r = errCodeStickyU(c); cfgUnlock(); return r; }
bool errCodeNote(const char* c, char* out, size_t n) {
  bool found = false;
  if (n) out[0] = 0;
  cfgLock();
  for (int i = 0; i < MAX_ERRCODES && n; i++)
    if (cfg.errCodes[i].code[0] && !strcmp(cfg.errCodes[i].code, c)) { strlcpy(out, cfg.errCodes[i].note, n); found = true; break; }
  cfgUnlock();
  return found;
}

bool errIsCancel(uint32_t pe) {
  return pe == 0x0300400CUL || pe == 0x0500400EUL;
}

// ---------------------------------------------------------------- json ----
static void cps(char* dst, size_t n, JsonVariantConst v) {
  const char* s = v.as<const char*>();
  if (s) strlcpy(dst, s, n);
}
template <typename T>
static void getNum(T& dst, JsonVariantConst v, long lo, long hi) {
  if (v.is<long>()) dst = (T)constrain(v.as<long>(), lo, hi);
}
static void getBool(bool& dst, JsonVariantConst v) {
  if (v.is<bool>()) dst = v.as<bool>();
}

static void parseHM(uint16_t& dst, JsonVariantConst v) {
  const char* s = v.as<const char*>();
  int h, m;
  if (s && sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) dst = h * 60 + m;
}

void configToJson(JsonObject o) {
  o["ssid"] = cfg.ssid;
  o["pass"] = cfg.pass;
  o["host"] = cfg.host;
  o["tz"] = cfg.tz;
  o["volume"] = cfg.volume;
  o["stage1Sec"] = cfg.stage1Sec;
  o["stage2Sec"] = cfg.stage2Sec;
  o["snoozeMin"] = cfg.snoozeMin;
  o["offlineSec"] = cfg.offlineSec;
  o["quietEnabled"] = cfg.quietEnabled;
  char tb[8];
  snprintf(tb, sizeof(tb), "%02d:%02d", cfg.quietStart / 60, cfg.quietStart % 60);
  o["quietStart"] = String(tb);
  snprintf(tb, sizeof(tb), "%02d:%02d", cfg.quietEnd / 60, cfg.quietEnd % 60);
  o["quietEnd"] = String(tb);
  o["quietDays"] = cfg.quietDays;
  o["setupTimeoutMin"] = cfg.setupTimeoutMin;
  o["ledProgress"] = cfg.ledProgress;
  o["ledMode"] = cfg.ledMode;
  o["ledPrinter"] = cfg.ledPrinter;
  o["ledBright"] = cfg.ledBright;
  o["ledQuietOff"] = cfg.ledQuietOff;
  o["apAlways"] = cfg.apAlways;
  o["apPass"] = cfg.apPass;
  o["lanAccess"] = cfg.lanAccess;
  o["webPass"] = cfg.webPass;
  o["hmsSev"] = cfg.hmsSev;
  o["ignoreAms"] = cfg.ignoreAms;
  o["ignoreCancel"] = cfg.ignoreCancel;
  o["errDefVer"] = cfg.errDefVer;
  o["clockEnabled"] = cfg.clockEnabled;
  o["clockBright"] = cfg.clockBright;
  o["ledIdle"] = cfg.ledIdle;
  o["ledDone"] = cfg.ledDone;
  o["muteClock"] = cfg.muteClock;
  o["cancelSound"] = cfg.cancelSound;
  o["noticeSound"] = cfg.noticeSound;
  o["selfTest"] = cfg.selfTest;
  o["selfTestSound"] = cfg.selfTestSound;
  o["lang"] = cfg.lang;
  o["ghRepo"] = cfg.ghRepo;
  JsonArray ps = o["printers"].to<JsonArray>();
  for (int i = 0; i < MAX_PRINTERS; i++) {
    JsonObject p = ps.add<JsonObject>();
    p["enabled"] = cfg.printers[i].enabled;
    p["name"] = cfg.printers[i].name;
    p["ip"] = cfg.printers[i].ip;
    p["serial"] = cfg.printers[i].serial;
    p["code"] = cfg.printers[i].code;
  }
  JsonObject rs = o["rules"].to<JsonObject>();
  for (int i = 0; i < EV_COUNT; i++) {
    JsonObject r = rs[EV_KEYS[i]].to<JsonObject>();
    r["enabled"] = cfg.rules[i].enabled;
    r["startStage"] = cfg.rules[i].startStage;
    r["speaker"] = cfg.rules[i].speaker;
    r["buzzer"] = cfg.rules[i].buzzer;
    r["overrideQuiet"] = cfg.rules[i].overrideQuiet;
    r["autoStopSec"] = cfg.rules[i].autoStopSec;
    r["sound"] = cfg.rules[i].sound;
  }
  JsonArray ec = o["errCodes"].to<JsonArray>();
  for (int i = 0; i < MAX_ERRCODES; i++) {
    if (!cfg.errCodes[i].code[0]) continue;
    JsonObject e = ec.add<JsonObject>();
    e["code"] = cfg.errCodes[i].code;
    e["action"] = cfg.errCodes[i].action;
    e["note"] = cfg.errCodes[i].note;
  }
  JsonArray cl = o["clocks"].to<JsonArray>();
  for (int i = 0; i < MAX_CLOCKS; i++) {
    if (!cfg.clocks[i].used) continue;
    JsonObject c = cl.add<JsonObject>();
    c["enabled"] = cfg.clocks[i].enabled;
    snprintf(tb, sizeof(tb), "%02d:%02d", cfg.clocks[i].time / 60, cfg.clocks[i].time % 60);
    c["time"] = String(tb);
    c["days"] = cfg.clocks[i].days;
    c["label"] = cfg.clocks[i].label;
  }
  JsonArray nf = o["notify"].to<JsonArray>();
  for (int i = 0; i < MAX_NOTIFY; i++) {
    JsonObject n = nf.add<JsonObject>();
    n["enabled"] = cfg.notify[i].enabled;
    n["type"] = cfg.notify[i].type;
    n["evMask"] = cfg.notify[i].evMask;
    n["name"] = cfg.notify[i].name;
    n["url"] = cfg.notify[i].url;
    n["key"] = cfg.notify[i].key;
    n["extra"] = cfg.notify[i].extra;
  }
}

void configFromJson(JsonVariantConst d) {
  cps(cfg.ssid, sizeof(cfg.ssid), d["ssid"]);
  cps(cfg.pass, sizeof(cfg.pass), d["pass"]);
  cps(cfg.host, sizeof(cfg.host), d["host"]);
  if (!cfg.host[0] || !strcmp(cfg.host, "h2d-alarm")) strlcpy(cfg.host, "bambu-radar", sizeof(cfg.host));   // old default name -> new
  cps(cfg.tz, sizeof(cfg.tz), d["tz"]);
  if (!cfg.tz[0]) strlcpy(cfg.tz, "CST-8", sizeof(cfg.tz));
  getNum(cfg.volume, d["volume"], 0, 100);
  getNum(cfg.stage1Sec, d["stage1Sec"], 1, 600);
  getNum(cfg.stage2Sec, d["stage2Sec"], 2, 1800);
  getNum(cfg.snoozeMin, d["snoozeMin"], 1, 60);
  getNum(cfg.offlineSec, d["offlineSec"], 30, 3600);
  getBool(cfg.quietEnabled, d["quietEnabled"]);
  parseHM(cfg.quietStart, d["quietStart"]);
  parseHM(cfg.quietEnd, d["quietEnd"]);
  getNum(cfg.quietDays, d["quietDays"], 0, 127);
  getNum(cfg.setupTimeoutMin, d["setupTimeoutMin"], 1, 120);
  getBool(cfg.ledProgress, d["ledProgress"]);
  getNum(cfg.ledMode, d["ledMode"], 0, 1);
  getNum(cfg.ledPrinter, d["ledPrinter"], 0, MAX_PRINTERS - 1);
  getNum(cfg.ledBright, d["ledBright"], 0, 100);
  getBool(cfg.ledQuietOff, d["ledQuietOff"]);
  getBool(cfg.apAlways, d["apAlways"]);
  cps(cfg.apPass, sizeof(cfg.apPass), d["apPass"]);
  if (strlen(cfg.apPass) < 8) strlcpy(cfg.apPass, "h2d-alarm", sizeof(cfg.apPass));
  if (cfg.stage2Sec <= cfg.stage1Sec) cfg.stage2Sec = cfg.stage1Sec + 30;
  getBool(cfg.lanAccess, d["lanAccess"]);
  cps(cfg.webPass, sizeof(cfg.webPass), d["webPass"]);
  getNum(cfg.hmsSev, d["hmsSev"], 1, 4);
  getBool(cfg.ignoreAms, d["ignoreAms"]);
  getBool(cfg.ignoreCancel, d["ignoreCancel"]);
  getNum(cfg.errDefVer, d["errDefVer"], 0, 255);
  getBool(cfg.clockEnabled, d["clockEnabled"]);
  getNum(cfg.clockBright, d["clockBright"], 0, 7);
  getNum(cfg.ledIdle, d["ledIdle"], 0, 100);
  getNum(cfg.ledDone, d["ledDone"], 0, 100);
  getBool(cfg.muteClock, d["muteClock"]);
  cps(cfg.cancelSound, sizeof(cfg.cancelSound), d["cancelSound"]);
  cps(cfg.noticeSound, sizeof(cfg.noticeSound), d["noticeSound"]);
  getBool(cfg.selfTest, d["selfTest"]);
  getBool(cfg.selfTestSound, d["selfTestSound"]);
  getNum(cfg.lang, d["lang"], 0, 1);
  cps(cfg.ghRepo, sizeof(cfg.ghRepo), d["ghRepo"]);

  JsonArrayConst ps = d["printers"];
  for (int i = 0; i < MAX_PRINTERS && i < (int)ps.size(); i++) {
    JsonVariantConst p = ps[i];
    getBool(cfg.printers[i].enabled, p["enabled"]);
    cps(cfg.printers[i].name, sizeof(cfg.printers[i].name), p["name"]);
    cps(cfg.printers[i].ip, sizeof(cfg.printers[i].ip), p["ip"]);
    cps(cfg.printers[i].serial, sizeof(cfg.printers[i].serial), p["serial"]);
    cps(cfg.printers[i].code, sizeof(cfg.printers[i].code), p["code"]);
  }
  for (int i = 0; i < EV_COUNT; i++) {
    JsonVariantConst r = d["rules"][EV_KEYS[i]];
    if (r.isNull()) continue;
    EventRule& R = cfg.rules[i];
    getBool(R.enabled, r["enabled"]);
    getNum(R.startStage, r["startStage"], 0, 2);
    getBool(R.speaker, r["speaker"]);
    getBool(R.buzzer, r["buzzer"]);
    getBool(R.overrideQuiet, r["overrideQuiet"]);
    getNum(R.autoStopSec, r["autoStopSec"], 0, 3600);
    cps(R.sound, sizeof(R.sound), r["sound"]);
  }

  JsonArrayConst ec = d["errCodes"];
  if (!ec.isNull()) {
    for (int i = 0; i < MAX_ERRCODES; i++) {
      ErrCodeCfg& E = cfg.errCodes[i];
      if (i < (int)ec.size()) {
        JsonVariantConst e = ec[i];
        char raw[40] = "";
        cps(raw, sizeof(raw), e["code"]);
        errNormalize(raw, E.code, sizeof(E.code));
        getNum(E.action, e["action"], 0, 4);
        E.note[0] = 0;
        cps(E.note, sizeof(E.note), e["note"]);
      } else {
        memset(&E, 0, sizeof(E));
      }
    }
  }

  JsonArrayConst cl = d["clocks"];
  if (!cl.isNull()) {
    for (int i = 0; i < MAX_CLOCKS; i++) {
      ClockAlarm& A = cfg.clocks[i];
      if (i < (int)cl.size()) {
        JsonVariantConst c = cl[i];
        A.used = true;
        getBool(A.enabled, c["enabled"]);
        parseHM(A.time, c["time"]);
        getNum(A.days, c["days"], 0, 127);
        A.label[0] = 0;
        cps(A.label, sizeof(A.label), c["label"]);
      } else {
        memset(&A, 0, sizeof(A));
      }
    }
  }

  JsonArrayConst nf = d["notify"];
  for (int i = 0; i < MAX_NOTIFY && i < (int)nf.size(); i++) {
    JsonVariantConst n = nf[i];
    NotifyCfg& N = cfg.notify[i];
    getBool(N.enabled, n["enabled"]);
    getNum(N.type, n["type"], 0, NT_DINGTALK);
    getNum(N.evMask, n["evMask"], 0, 63);
    cps(N.name, sizeof(N.name), n["name"]);
    cps(N.url, sizeof(N.url), n["url"]);
    cps(N.key, sizeof(N.key), n["key"]);
    cps(N.extra, sizeof(N.extra), n["extra"]);
  }
}

bool configLoad() {
  configDefaults();
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  JsonDocument d;
  DeserializationError e = deserializeJson(d, f);
  f.close();
  if (e) return false;
  cfg.errDefVer = 0;          // an old config.json without this key gets the default ignore list merged once
  configFromJson(d);
  if (cfg.errDefVer < 7) {
    addErrDefaults();
    removeOldDefaults();
    cfg.errDefVer = 7;
    configSave();
  }
  return true;
}

bool configSave() {
  JsonDocument d;
  cfgLock();
  configToJson(d.to<JsonObject>());
  cfgUnlock();
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}
