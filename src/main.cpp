#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include "pins.h"
#include "config.h"
#include "audio.h"
#include "alarm.h"
#include "printer.h"
#include "webui.h"
#include "evlog.h"
#include "notify.h"
#include "clock.h"
#include "crumb.h"
#include "hmsdb.h"
#include "ota.h"

static Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

static bool s_setup = false;
static volatile bool s_cfgChanged = false;
static bool s_ntpDone = false;
static uint32_t s_ntpTry = 0;                       // when NTP was last (re)started
static volatile uint32_t s_holdMs = 0;               // how long the SETUP button has been held (0 = not pressed)
static uint32_t s_flashUntil = 0;                    // LED feedback after toggling mute mode
static uint8_t s_flashKind = 0;                      // 1 = mute on (violet), 2 = mute off (white)
static char s_lastSsid[33] = "", s_lastPass[65] = "", s_lastApPass[33] = "";

void onConfigSaved() { s_cfgChanged = true; }

// (Re)start NTP. The last server is an IP address so it works even when DNS is flaky.
void ntpSyncNow() {
  configTzTime(cfg.tz, "tw.pool.ntp.org", "time.cloudflare.com", "162.159.200.1");
  s_ntpTry = millis();
  Serial.println("NTP sync requested");
}

// ------------------------------------------------------------- buttons ----
enum BtnEv { B_NONE, B_SHORT, B_LONG };
struct Btn {
  uint8_t pin;
  bool last;
  uint32_t downAt;
  bool fired;
  explicit Btn(uint8_t p) : pin(p), last(false), downAt(0), fired(false) {}
};
static Btn s_ack(PIN_BTN_ACK);
static Btn s_setupBtn(PIN_BTN_SETUP);

static BtnEv pollBtn(Btn& b, uint32_t longMs) {
  bool d = digitalRead(b.pin) == LOW;
  uint32_t now = millis();
  BtnEv r = B_NONE;
  if (d && !b.last) { b.downAt = now; b.fired = false; }
  if (d && !b.fired && now - b.downAt >= longMs) { b.fired = true; r = B_LONG; }
  if (!d && b.last && !b.fired && now - b.downAt >= 30) r = B_SHORT;
  b.last = d;
  return r;
}

// SETUP button: hold 1-3 s and release = setup mode on/off; hold 3 s = mute mode on/off (fires while held).
enum SetupBtn { SB_NONE, SB_SETUP, SB_MUTE };
static int pollSetupBtn() {
  static bool last = false, fired = false;
  static uint32_t downAt = 0;
  bool d = digitalRead(PIN_BTN_SETUP) == LOW;
  uint32_t now = millis();
  int r = SB_NONE;
  if (d && !last) { downAt = now; fired = false; }
  if (d && !fired && now - downAt >= 3000) { fired = true; r = SB_MUTE; }
  if (!d && last && !fired && now - downAt >= 1000) r = SB_SETUP;
  s_holdMs = d ? now - downAt : 0;
  last = d;
  return r;
}

// ----------------------------------------------------------------- LED ----
static void fillAll(uint8_t r, uint8_t g, uint8_t b, float k) {
  uint32_t c = strip.Color((uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k));
  for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, c);
}

// 0 = nothing to show, 1 = printing, 2 = paused, 3 = failed/error, 4 = finished (kept 30 min),
// 5 = job started but nothing printed yet (preheat / bed leveling / homing): colourful marquee
static int segKind(int i, int* pct) {
  const PrinterCfg& p = cfg.printers[i];
  if (!p.enabled) return 0;
  const PrinterState& s = printerState(i);
  if (!s.connected || !s.gotReport) return 0;
  *pct = s.percent < 0 ? 0 : (s.percent > 100 ? 100 : s.percent);
  bool run = !strcmp(s.gcode, "RUNNING") || !strcmp(s.gcode, "PREPARE");
  bool pause = !strcmp(s.gcode, "PAUSE");
  if (!strcmp(s.gcode, "FAILED") || (s.err != 0 && (run || pause))) return 3;
  if (pause) return 2;
  if (run) {
    bool prep = !strcmp(s.gcode, "PREPARE") || (s.percent <= 0 && s.layer <= 0);   // no progress / layer yet
    return prep ? 5 : 1;
  }
  if (!strcmp(s.gcode, "FINISH") && millis() - s.since < 1800000UL) return 4;
  return 0;
}

static uint8_t sc(int v, float k) { int x = (int)(v * k + 0.5f); return x > 255 ? 255 : (uint8_t)x; }

// Rainbow colour for hue h (0..360), value k (0..1).
static void rainbow(float h, float k, uint8_t& r, uint8_t& g, uint8_t& b) {
  float x = k * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float rr = 0, gg = 0, bb = 0;
  switch ((int)(h / 60.0f) % 6) {
    case 0: rr = k; gg = x; break;
    case 1: rr = x; gg = k; break;
    case 2: gg = k; bb = x; break;
    case 3: gg = x; bb = k; break;
    case 4: rr = x; bb = k; break;
    default: rr = k; bb = x; break;
  }
  r = (uint8_t)(rr * 255); g = (uint8_t)(gg * 255); b = (uint8_t)(bb * 255);
}

// Returns false if there is nothing to show (caller then draws the idle status pixel).
static bool drawProgress(uint32_t now) {
  if (!cfg.ledProgress) return false;
  int kind[MAX_PRINTERS], pct[MAX_PRINTERS], n = 0;
  if (cfg.ledMode == 1) {
    int i = cfg.ledPrinter < MAX_PRINTERS ? cfg.ledPrinter : 0, p = 0;
    int k = segKind(i, &p);
    if (k) { kind[0] = k; pct[0] = p; n = 1; }
  } else {
    for (int i = 0; i < MAX_PRINTERS; i++) {
      int p = 0, k = segKind(i, &p);
      if (k) { kind[n] = k; pct[n] = p; n++; }
    }
  }
  if (n == 0) return false;

  float bright = cfg.ledBright / 100.0f;
  int base = LED_COUNT / n, extra = LED_COUNT % n, pos = 0;
  for (int s = 0; s < n; s++) {
    int len = base + (s < extra ? 1 : 0);
    if (kind[s] == 5) {                                     // preparing: rainbow with a bright head running round
      float head = fmodf(now / 90.0f, (float)len);
      for (int j = 0; j < len; j++) {
        float dist = fabsf(j - head);
        if (dist > len - dist) dist = len - dist;
        float level = 0.22f + 0.78f * expf(-dist * 0.45f);
        uint8_t cr, cg, cb;
        rainbow(fmodf(now * 0.18f + j * (360.0f / len), 360.0f), level * bright, cr, cg, cb);
        strip.setPixelColor(pos + j, strip.Color(cr, cg, cb));
      }
      pos += len;
      continue;
    }
    int r = 0, g = 0, b = 0;
    switch (kind[s]) {
      case 1: r = 0;   g = 120; b = 255; break;   // printing: blue
      case 2: r = 255; g = 110; b = 0;   break;   // paused: amber
      case 3: r = 255; g = 0;   b = 0;   break;   // failed / error: red
      default: r = 0;  g = 255; b = 60;  break;   // finished: green
    }
    float lit = pct[s] / 100.0f * len;
    for (int j = 0; j < len; j++) {
      float level;
      if (kind[s] == 1 || kind[s] == 2) {
        float frac = lit - j;
        frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
        level = 0.06f * (cfg.ledIdle / 50.0f) + 0.94f * frac;   // unlit LEDs keep a faint track (ledIdle 0 = none)
        if (kind[s] == 2) level *= 0.65f + 0.35f * sinf(now / 600.0f);
      } else if (kind[s] == 3) {
        level = ((now / 400) % 2) ? 1.0f : 0.25f;
      } else {
        level = cfg.ledDone / 100.0f;                       // finished bar has its own brightness (0 = off)
      }
      float k = level * bright;
      strip.setPixelColor(pos + j, strip.Color(sc(r, k), sc(g, k), sc(b, k)));
    }
    pos += len;
  }
  return true;
}

static void drawIdle() {
  uint8_t r = 0, g = 0;
  if (WiFi.status() != WL_CONNECTED) {
    r = LED_IDLE_LEVEL;
  } else {
    int en = 0, ok = 0;
    for (int i = 0; i < MAX_PRINTERS; i++)
      if (cfg.printers[i].enabled) { en++; if (printerState(i).connected) ok++; }
    if (en == 0) { r = g = LED_IDLE_LEVEL / 2; }
    else if (ok == en) { g = LED_IDLE_LEVEL; }
    else { r = LED_IDLE_LEVEL; g = LED_IDLE_LEVEL / 2; }
  }
  float ik = cfg.ledIdle / 50.0f;                           // idle / waiting light: 0 = off, 50 = original level
  strip.setPixelColor(0, strip.Color(sc(r, ik), sc(g, ik), 0));
}

// Self-test (power-on, or the "run self-test" button): digit display all lit, the LED ring sweeps red / green /
// blue, and a short chime plays (unless disabled, muted or volume 0). Takes about 2 seconds.
static void runSelfTest() {
  if (alarmRinging()) return;
  Serial.println("Self-test: digits, LED ring, sound");
  if (cfg.clockEnabled) {
    static const uint8_t all[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    clockShow(all, true);
  }
  bool sound = cfg.selfTestSound && cfg.volume > 0 && !alarmMuted();
  if (sound) {
    audioSetGain((cfg.volume > 60 ? 60 : cfg.volume) / 100.0f);   // not louder than 60 % at power-on
    audioPlay("/sounds/chime_soft.wav", true);                     // falls back to a beep if the file is missing
    alarmTestBuzzer(1);                                            // short blip, only matters if a buzzer is fitted
  }
  static const uint8_t col[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 80, 255}};
  for (int c = 0; c < 3; c++) {
    for (int i = 0; i < LED_COUNT; i++) {
      strip.clear();
      if (cfg.ledBright > 0) strip.setPixelColor(i, strip.Color(sc(col[c][0], 0.35f), sc(col[c][1], 0.35f), sc(col[c][2], 0.35f)));
      strip.show();
      delay(50);
    }
  }
  strip.clear();
  strip.show();
  if (sound) audioStop();
}

void selfTestNow() { runSelfTest(); }

static void ledUpdate() {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 30) return;
  last = now;
  strip.clear();
  bool content = false;                                     // progress / idle picture (gets the breathing effect in mute mode)
  if (s_holdMs >= 3000 || now < s_flashUntil) {
    if (s_flashKind == 1) fillAll(180, 0, 255, 0.45f);      // mute on
    else fillAll(220, 220, 220, 0.35f);                     // mute off
  } else if (s_holdMs >= 1000) {
    fillAll(0, 80, 255, 0.35f);                             // release now = setup mode; keep holding = mute mode
  } else if (alarmRinging()) {
    int st = alarmStage();
    if (st == 0) fillAll(255, 120, 0, 0.25f + 0.25f * sinf(now / 400.0f));
    else if (st == 1) { if ((now / 500) % 2) fillAll(255, 0, 0, 0.8f); }
    else { if ((now / 80) % 2) fillAll(255, 0, 0, 1.0f); }
  } else if (s_setup) {
    fillAll(0, 80, 255, 0.15f + 0.15f * (1.0f + sinf(now / 500.0f)));
  } else if (cfg.ledQuietOff && alarmQuietNow()) {
    // dark during do-not-disturb
  } else {
    content = true;
    if (!drawProgress(now)) drawIdle();
  }
  if (content && alarmMuted()) {                            // mute mode: the picture breathes
    float br = 0.15f + 0.85f * (0.5f + 0.5f * sinf(now / 650.0f));
    for (int i = 0; i < LED_COUNT; i++) {
      uint32_t c = strip.getPixelColor(i);
      strip.setPixelColor(i, strip.Color(sc((c >> 16) & 255, br), sc((c >> 8) & 255, br), sc(c & 255, br)));
    }
  }
  strip.show();
}

// ------------------------------------------------------- setup / wifi ----
// The web server runs when the hotspot is wanted (setup mode / hotspot always on) or LAN access is enabled.
// The hotspot itself only runs in setup mode or when "hotspot always on" is set.
static void applyWeb() {
  bool wantAp = s_setup || cfg.apAlways;
  bool wantSrv = wantAp || cfg.lanAccess;
  if (wantSrv && !webActive()) webBegin();
  if (!wantSrv && webActive()) webEnd();
  if (webActive()) webApSet(wantAp);
}

static void enterSetup() {
  s_setup = true;
  webSetLanOpen(true);
  applyWeb();
  Serial.println("Setup mode ON  (AP: Bambu-FDM-Radar -> http://192.168.4.1)");
}
static void exitSetup() {
  s_setup = false;
  webSetLanOpen(false);
  applyWeb();
  Serial.println("Setup mode OFF");
}

static void wifiLoop() {
  static uint32_t lastTry = 0;
  if (s_cfgChanged) {
    s_cfgChanged = false;
    printersReload();
    bool apPassChanged = strcmp(s_lastApPass, cfg.apPass) != 0;
    strlcpy(s_lastApPass, cfg.apPass, sizeof(s_lastApPass));
    if (apPassChanged) webApSet(false);     // hotspot restarts below with the new password
    applyWeb();
    if (strcmp(s_lastSsid, cfg.ssid) || strcmp(s_lastPass, cfg.pass)) {
      strlcpy(s_lastSsid, cfg.ssid, sizeof(s_lastSsid));
      strlcpy(s_lastPass, cfg.pass, sizeof(s_lastPass));
      if (cfg.ssid[0]) { WiFi.disconnect(false); WiFi.begin(cfg.ssid, cfg.pass); }
      lastTry = millis();
    }
    s_ntpDone = false;
  }
  if (!cfg.ssid[0]) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastTry > 15000) { lastTry = millis(); WiFi.begin(cfg.ssid, cfg.pass); }
  } else if (!s_ntpDone || (time(nullptr) < 1700000000 && millis() - s_ntpTry > 20000)) {
    ntpSyncNow();            // on first connect, then again every 20 s until the clock is set
    s_ntpDone = true;
  }
}

// ---------------------------------------------------------- arduino -------
void setup() {
  crumbBegin();            // remember what the previous run was doing (shown on the status page after a reset)
  Serial.begin(115200);
  pinMode(PIN_BTN_ACK, INPUT_PULLUP);
  pinMode(PIN_BTN_SETUP, INPUT_PULLUP);
  strip.begin();
  strip.show();

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  if (!LittleFS.exists("/sounds")) LittleFS.mkdir("/sounds");

  // Factory reset: hold the SETUP button for 3 s while powering on
  if (digitalRead(PIN_BTN_SETUP) == LOW) {
    uint32_t t0 = millis();
    while (digitalRead(PIN_BTN_SETUP) == LOW && millis() - t0 < 3000) delay(10);
    if (digitalRead(PIN_BTN_SETUP) == LOW) {
      LittleFS.remove("/config.json");
      fillAll(255, 0, 0, 0.5f);
      strip.show();
      delay(800);
    }
  }

  bool have = configLoad();
  Serial.printf("Config %s\n", have ? "loaded" : "defaults");

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.host);
  WiFi.setAutoReconnect(true);
  strlcpy(s_lastSsid, cfg.ssid, sizeof(s_lastSsid));
  strlcpy(s_lastPass, cfg.pass, sizeof(s_lastPass));
  strlcpy(s_lastApPass, cfg.apPass, sizeof(s_lastApPass));
  if (cfg.ssid[0]) WiFi.begin(cfg.ssid, cfg.pass);

  audioBegin();
  alarmBegin();
  alarmSetValidator(printersEventStillActive);
  evlogBegin();
  hmsDbBegin();                    // official error texts from LittleFS /hms.bin (optional)
  printersBegin();
  notifyBegin();
  clockBegin();
  if (cfg.selfTest) runSelfTest();

  if (!cfg.ssid[0]) enterSetup();  // nothing configured yet -> go straight to setup
  else applyWeb();                 // hotspot (if always-on) and/or LAN access
}

void loop() {
  crumb(0, 1);
  // buttons
  BtnEv a = pollBtn(s_ack, 1500);
  // Nothing ringing: holding the ACK button shows the print progress on the digits (while held and 5 s after).
  bool ackHeld = digitalRead(PIN_BTN_ACK) == LOW && millis() - s_ack.downAt >= 700;
  if (ackHeld && !alarmRinging()) clockShowProgress(5000);
  if (alarmRinging()) {
    if (a == B_SHORT || a == B_LONG) {              // log who stopped it: a noisy button would look like "rings once, then silent"
      int ap = alarmPrinter();
      evlogAdd(alarmEvent(), ap < 0 ? 0 : ap, LOGF_BUTTON, "");
      Serial.printf("ACK button: %s\n", a == B_SHORT ? "stop" : "snooze");
    }
    if (a == B_SHORT) alarmStop();
    else if (a == B_LONG) alarmSnooze();
  } else if (a == B_SHORT && alarmSnoozed() && millis() - s_ack.downAt < 700) {
    alarmCancelSnooze();
  }
  int sb = pollSetupBtn();
  if (sb == SB_SETUP) { if (s_setup) exitSetup(); else enterSetup(); }
  else if (sb == SB_MUTE) {
    alarmSetMute(!alarmMuted());
    s_flashKind = alarmMuted() ? 1 : 2;
    s_flashUntil = millis() + 700;
  }

  wifiLoop();

  // printer events -> alarm
  crumb(0, 2);
  EventType ev;
  int idx;
  bool ign;
  char det[20];
  while (printersNextEvent(&ev, &idx, &ign, det, sizeof(det))) {
    Serial.printf("Event %s (printer %d: %s) %s%s\n", EV_KEYS[ev], idx + 1, cfg.printers[idx].name, det, ign ? " [filtered]" : "");
    uint8_t fl = 0;
    if (ign) {
      fl |= LOGF_IGNORED;
      // error code set to "just one short sound" (e.g. nozzle camera dirty): one blip, no alarm, no push
      if (det[0] && errCodeNotice(det)) {
        fl = LOGF_BLIP;
        static uint32_t lastNotice = 0;
        if (millis() - lastNotice > 2000) {
          lastNotice = millis();
          alarmBlip(cfg.noticeSound[0] ? cfg.noticeSound : "beep.wav", 1500);
        }
      }
      // print cancelled by hand: no alarm, but an optional short sound (Settings > Alerts)
      static uint32_t lastBlip = 0;
      if (cfg.cancelSound[0] && det[0] && errIsCancel((uint32_t)strtoul(det, nullptr, 16)) && errCodeAction(det) != 0 &&
          millis() - lastBlip > 3000) {
        lastBlip = millis();
        alarmBlip(cfg.cancelSound, 1500);
      }
    } else {
      if (!alarmTrigger(ev, false, idx, det[0] && errCodeSticky(det))) fl |= LOGF_SILENT;
      if (cfg.rules[ev].enabled) notifyEvent(ev, idx, det);
    }
    evlogAdd(ev, idx, fl, det);
  }
  crumb(0, 3);
  alarmClockTick();
  crumb(0, 4);
  alarmLoop();
  evlogTick(!alarmRinging());        // deferred event-log flash write: only when nothing rings
  crumbSample(alarmRinging());       // heap / stack telemetry into RTC memory (survives a crash)
  crumb(0, 5);
  clockLoop();
  crumb(0, 6);

  webLoop();  // no-op while the web server is not running
  otaLoop();  // confirms a freshly updated firmware once it has run for a minute (see ota.cpp)
  crumb(0, 7);
  if (s_setup) {
    bool timeout = cfg.ssid[0] && (millis() - webLastActivity() > (uint32_t)cfg.setupTimeoutMin * 60000UL);
    if (webExitRequested() || timeout) exitSetup();
  }

  ledUpdate();
  delay(2);
}
