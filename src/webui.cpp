#include "webui.h"
#include "ui_html.h"
#include "config.h"
#include "alarm.h"
#include "printer.h"
#include "evlog.h"
#include "notify.h"
#include "hmsdb.h"
#include "ota.h"
#include "audio.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "crumb.h"

static WebServer s_srv(80);
static DNSServer s_dns;
static bool s_active = false;   // HTTP server running
static bool s_apOn = false;     // hotspot running
static bool s_exitReq = false;
static uint32_t s_last = 0;
static File s_up;
static bool s_lanOpen = false;
static bool s_routes = false;

static void touch() { s_last = millis(); }

// ---- access control -------------------------------------------------------------------------------
// * Phones on the device's own hotspot: always allowed.
// * Setup mode (SETUP button held 1 s): everybody on the LAN is allowed, no password.
// * Otherwise LAN clients (your PC) are allowed when "LAN access" is on; if a web password is set they must
//   log in (user name: admin).
static bool isApClient() {
  if (!s_apOn) return false;
  IPAddress r = s_srv.client().remoteIP(), ap = WiFi.softAPIP();
  return r[0] == ap[0] && r[1] == ap[1] && r[2] == ap[2];
}

static bool authOk() {
  if (s_lanOpen || isApClient()) return true;
  if (!cfg.lanAccess) return false;
  if (!cfg.webPass[0]) return true;
  return s_srv.authenticate("admin", cfg.webPass);
}

static bool guard() {
  if (!authOk()) {
    if (cfg.lanAccess && cfg.webPass[0] && !s_lanOpen && !isApClient())
      s_srv.requestAuthentication(BASIC_AUTH, "H2D Alarm", "Password required (user name: admin)");
    else
      s_srv.send(403, "text/plain", "Forbidden. Enable LAN access in the settings, or long-press the SETUP button on the device.");
    return false;
  }
  touch();
  return true;
}

static void sendJson(JsonDocument& d, int code = 200) {
  String s;
  serializeJson(d, s);
  s_srv.send(code, "application/json", s);
}
static void sendOk() { s_srv.send(200, "application/json", "{\"ok\":true}"); }

// Stream a JSON document to the client in 1 KB chunks instead of building one big String first: the String grew by
// repeated realloc and left holes in the internal heap every time the status page polled.
class ChunkPrint : public Print {
 public:
  using Print::write;
  size_t write(uint8_t c) override {
    buf_[n_++] = c;
    if (n_ == sizeof(buf_)) flush();
    return 1;
  }
  size_t write(const uint8_t* p, size_t len) override {
    size_t total = len;
    while (len) {
      size_t room = sizeof(buf_) - n_;
      size_t k = len < room ? len : room;
      memcpy(buf_ + n_, p, k);
      n_ += k;
      p += k;
      len -= k;
      if (n_ == sizeof(buf_)) flush();
    }
    return total;
  }
  void flush() {
    if (n_) { s_srv.sendContent((const char*)buf_, n_); n_ = 0; }
  }
 private:
  uint8_t buf_[1024];
  size_t n_ = 0;
};

static void sendJsonStream(JsonDocument& d) {
  s_srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  s_srv.send(200, "application/json", "");
  ChunkPrint cp;
  serializeJson(d, cp);
  cp.flush();
  s_srv.sendContent("");          // end of the chunked body
}

static void hIndex() {
  if (!guard()) return;
  s_srv.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void hFavicon() {
  s_srv.sendHeader("Cache-Control", "max-age=86400");
  s_srv.send_P(200, "image/svg+xml", FAVICON_SVG);
}

// /sounds listing, cached: scanning the directory on every status poll meant constant flash reads while audio plays.
struct SndEnt { char name[32]; uint32_t size; };
static SndEnt s_snd[32];
static int s_nSnd = 0;
static uint32_t s_sndAt = 0;
static bool s_sndDirty = true;

static void sndRescan() {
  s_nSnd = 0;
  File dir = LittleFS.open("/sounds");
  if (dir && dir.isDirectory()) {
    File f = dir.openNextFile();
    while (f && s_nSnd < 32) {
      if (!f.isDirectory()) {
        strlcpy(s_snd[s_nSnd].name, f.name(), sizeof(s_snd[0].name));
        s_snd[s_nSnd].size = (uint32_t)f.size();
        s_nSnd++;
      }
      f = dir.openNextFile();
    }
  }
  s_sndAt = millis();
  s_sndDirty = false;
}

static void hState() {
  if (!guard()) return;
  JsonDocument d;
  cfgLock();
  configToJson(d["cfg"].to<JsonObject>());
  cfgUnlock();

  JsonObject w = d["wifi"].to<JsonObject>();
  w["connected"] = (WiFi.status() == WL_CONNECTED);
  w["ip"] = WiFi.localIP().toString();
  w["rssi"] = WiFi.RSSI();
  w["ssid"] = WiFi.SSID();
  w["ap"] = WiFi.softAPIP().toString();
  w["apOn"] = s_apOn;
  w["host"] = String(cfg.host) + ".local";

  JsonArray ds = d["discovered"].to<JsonArray>();
  for (int i = 0; i < ssdpCount(); i++) {
    const Discovered* x = ssdpGet(i);
    JsonObject o = ds.add<JsonObject>();
    o["ip"] = String(x->ip);
    o["serial"] = String(x->serial);
    o["model"] = String(x->model);
    o["name"] = String(x->name);
    o["age"] = (millis() - x->lastSeen) / 1000;
  }

  JsonArray ps = d["printers"].to<JsonArray>();
  for (int i = 0; i < MAX_PRINTERS; i++) {
    const PrinterState& s = printerState(i);
    JsonObject o = ps.add<JsonObject>();
    o["connected"] = s.connected;
    o["gcode"] = String(s.gcode);
    o["percent"] = s.percent;
    o["has"] = s.has;
    o["nozzle"] = s.nozzle;
    o["bed"] = s.bed;
    o["chamber"] = s.chamber;
    o["remain"] = s.remainMin;
    o["layer"] = s.layer;
    o["layers"] = s.totalLayer;
    o["job"] = String(s.job);
    o["since"] = s.connected ? (long)((millis() - s.since) / 1000) : -1L;
    JsonArray nz = o["noz"].to<JsonArray>();           // one entry per nozzle the printer actually reports
    for (int k = 0; k < 2; k++)
      if (s.nozMask & (1 << k)) {
        JsonObject n = nz.add<JsonObject>();
        n["i"] = k;
        n["c"] = s.nozCur[k];
        n["t"] = s.nozTgt[k];
        n["a"] = ((s.has & 32) && s.active == k);
      }
    o["bedT"] = s.bedT;
    o["chamberT"] = s.chamberT;
    size_t rl = 0;
    uint32_t ra = 0;
    printersRawGet(i, 0, &rl, &ra);
    o["rawLen"] = (uint32_t)rl;
    o["rawAge"] = ra;
    printersRawGet(i, 1, &rl, &ra);
    o["snapLen"] = (uint32_t)rl;
    o["snapAge"] = ra;
    // AMS filament overview
    JsonArray au = o["ams"].to<JsonArray>();
    for (int a = 0; a < MAX_AMS; a++) {
      const AmsUnitState& U = s.ams[a];
      if (!U.present) continue;
      JsonObject uo = au.add<JsonObject>();
      uo["i"] = a;
      uo["ex"] = U.extruder;
      uo["h"] = U.humidity;
      uo["tp"] = (int)(U.temp + 0.5f);
      JsonArray ta = uo["t"].to<JsonArray>();
      for (int k = 0; k < 4; k++) {
        JsonObject to = ta.add<JsonObject>();
        char cb[8];
        snprintf(cb, sizeof(cb), "%06X", (unsigned)(U.tray[k].rgb & 0xFFFFFF));
        to["c"] = String(cb);
        to["ty"] = String(U.tray[k].type);
        to["r"] = U.tray[k].remain;
        to["e"] = ((s.trayBits >> (a * 4 + k)) & 1) != 0;
      }
    }
    // Which tray is feeding right now. Single-nozzle printers: ams.tray_now = amsId*4 + slot (254 external, 255 none).
    // Dual-nozzle (H2D): ams.tray_now is only the SLOT number (1 = slot 2 of whichever AMS), so reading it as amsId*4+slot
    // always lands on AMS 1. Use the active nozzle's own snow = (amsId << 8) | slot instead (0xFFFF = none).
    int tn = s.trayNow;
    if ((s.has & 32) && s.active >= 0 && s.active < 2 && (s.snowMask & (1 << s.active))) {
      int sn = s.snow[s.active], a = (sn >> 8) & 0xFF, sl = sn & 0xFF;
      if (sn == 0xFFFF) tn = 255;
      else if (sl >= 254) tn = sl;                   // external spool
      else if (a < MAX_AMS && sl < 4) tn = a * 4 + sl;
      else tn = 255;                                 // a unit this page does not draw (e.g. AMS HT)
    }
    o["tn"] = tn;
    JsonArray ex = o["ext"].to<JsonArray>();
    for (int k = 0; k < 2; k++) {
      if (!(s.extMask & (1 << k))) continue;
      JsonObject eo = ex.add<JsonObject>();
      char cb[8];
      snprintf(cb, sizeof(cb), "%06X", (unsigned)(s.ext[k].rgb & 0xFFFFFF));
      eo["id"] = s.extId[k];
      eo["c"] = String(cb);
      eo["ty"] = String(s.ext[k].type);
      eo["r"] = s.ext[k].remain;
    }
    o["age"] = s.gotReport ? (long)((millis() - s.lastReport) / 1000) : -1L;
  }

  JsonArray nl = d["notifyLast"].to<JsonArray>();
  for (int i = 0; i < MAX_NOTIFY; i++) nl.add(notifyLastResult(i));

  JsonArray so = d["sounds"].to<JsonArray>();
  if (s_sndDirty || millis() - s_sndAt > 10000UL) sndRescan();
  for (int i = 0; i < s_nSnd; i++) {
    JsonObject o = so.add<JsonObject>();
    o["name"] = String(s_snd[i].name);
    o["size"] = s_snd[i].size;
  }

  time_t nowT = time(nullptr);
  if (nowT > 1700000000) {
    struct tm tt;
    localtime_r(&nowT, &tt);
    char tb[24];
    strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M", &tt);
    d["time"] = String(tb);
  } else {
    d["time"] = "";
  }
  d["ringing"] = alarmRinging();
  d["mute"] = alarmMuted();
  d["up"] = (uint32_t)(millis() / 1000);
  d["rst"] = (int)esp_reset_reason();
  d["crumb"] = crumbPrev();
  d["setup"] = s_lanOpen;
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  d["fsUsed"] = (uint32_t)LittleFS.usedBytes();
  d["fsTotal"] = (uint32_t)LittleFS.totalBytes();
  d["hmsdb"] = hmsDbCount();          // official error texts installed for the current language (0 = file missing)
  d["hmsdbZh"] = hmsDbCount(0);
  d["hmsdbEn"] = hmsDbCount(1);
  d["fw"] = FW_VERSION;
  d["audioFb"] = audioFallbackCount();       // times a sound file could not be played and the beep stood in (since boot)
  d["audioWhy"] = audioFallbackReason();
  d["audioInfo"] = audioFallbackDetail();
  {
    // heap / stack telemetry: this run, and what the previous run left behind in RTC memory before it died
    JsonObject t = d["tel"].to<JsonObject>();
    t["free"] = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    t["blk"] = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    t["min"] = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    CrumbTelemetry cn = crumbNow(), cp = crumbLast();
    t["minBlk"] = cn.minBlock;
    JsonArray sk = t["stk"].to<JsonArray>();      // stack headroom in bytes: loop, net, audio, notify
    for (int i = 0; i < 4; i++) sk.add(cn.stackFree[i]);
    JsonObject pv = d["prevRun"].to<JsonObject>();
    pv["free"] = cp.freeHeap;
    pv["blk"] = cp.maxBlock;
    pv["min"] = cp.minHeap;
    pv["minBlk"] = cp.minBlock;
    pv["ringing"] = cp.ringing;
    pv["up"] = cp.upSec;
    JsonArray pk = pv["stk"].to<JsonArray>();
    for (int i = 0; i < 4; i++) pk.add(cp.stackFree[i]);
  }
  sendJsonStream(d);
}

static void hConfig() {
  if (!guard()) return;
  JsonDocument d;
  if (deserializeJson(d, s_srv.arg("plain"))) {
    s_srv.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  cfgLock();
  configFromJson(d);
  cfgUnlock();
  bool saved = configSave();
  onConfigSaved();
  s_srv.send(200, "application/json", saved ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void hScan() {
  if (!guard()) return;
  JsonDocument d;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    d["scanning"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    d["scanning"] = true;
  } else {
    d["scanning"] = false;
    JsonArray a = d["nets"].to<JsonArray>();
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject o = a.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["enc"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  }
  sendJson(d);
}

static void hTest() {
  if (!guard()) return;
  String type = s_srv.arg("type");
  if (type == "speaker") {
    String snd = s_srv.arg("sound");
    Serial.printf("Test speaker: %s\n", snd.c_str());
    if (snd.indexOf('/') < 0) alarmTestSpeaker(snd.c_str(), 3);
  } else if (type == "buzzer") {
    Serial.println("Test buzzer");
    alarmTestBuzzer(3);
  } else if (type == "event") {
    String ev = s_srv.arg("ev");
    for (int i = 0; i < EV_COUNT; i++)
      if (ev == EV_KEYS[i]) {
        bool rang = alarmTrigger((EventType)i, true, 0);
        Serial.printf("Test alarm: %s (printer 1) %s\n", EV_KEYS[i], rang ? "ringing" : "not ringing");
        evlogAdd((EventType)i, 0, LOGF_TEST | (rang ? 0 : LOGF_SILENT), "");   // shows up in the Log tab, marked as a test
      }
  }
  sendOk();
}

// ---- clock sync / mute ----
static void hTimeSync() { if (!guard()) return; ntpSyncNow(); sendOk(); }

// Set the clock from the browser (fallback when NTP is blocked). Only accepts sane values.
static void hTimeSet() {
  if (!guard()) return;
  uint32_t ts = (uint32_t)strtoul(s_srv.arg("ts").c_str(), nullptr, 10);
  if (ts > 1700000000UL && ts < 4102444800UL) {
    setenv("TZ", cfg.tz, 1);
    tzset();
    struct timeval tv = {(time_t)ts, 0};
    settimeofday(&tv, nullptr);
    Serial.printf("Clock set from browser: %lu\n", (unsigned long)ts);
  }
  sendOk();
}

static void hMute() {
  if (!guard()) return;
  alarmSetMute(s_srv.arg("on") == "1");
  sendOk();
}

static void hSelfTest() {
  if (!guard()) return;
  sendOk();            // answer first, the test blocks this task for about 2 s
  selfTestNow();
}

static void hStop() {
  if (!guard()) return;
  if (alarmRinging()) evlogAdd(alarmEvent(), alarmPrinter() < 0 ? 0 : alarmPrinter(), LOGF_BUTTON, "");
  alarmStop();
  sendOk();
}
static void hExit() { if (!guard()) return; sendOk(); s_exitReq = true; }
static void hReboot() { if (!guard()) return; sendOk(); evlogFlushNow(); delay(300); ESP.restart(); }

static void hSoundDelete() {
  if (!guard()) return;
  String n = s_srv.arg("name");
  if (n.length() && n.indexOf('/') < 0) LittleFS.remove("/sounds/" + n);
  s_sndDirty = true;
  audioInvalidate();
  sendOk();
}

// ---- alarm log / notification test ----
static void hLog() {
  if (!guard()) return;
  JsonDocument d;
  JsonArray a = d.to<JsonArray>();
  for (int i = 0; i < evlogCount(); i++) {
    const LogEntry& e = evlogGet(i);
    JsonObject o = a.add<JsonObject>();
    char tb[20] = "";
    if (e.ts) {
      time_t t = (time_t)e.ts;
      struct tm lt;
      localtime_r(&t, &lt);
      strftime(tb, sizeof(tb), "%m-%d %H:%M:%S", &lt);
    }
    o["t"] = String(tb);
    o["ev"] = String(EV_KEYS[e.ev < EV_COUNT ? e.ev : 0]);
    o["p"] = e.printer;
    o["f"] = e.flags;
    o["code"] = String(e.detail);
    char note[200] = "";
    if (e.detail[0] && !(e.flags & LOGF_TEST)) errCodeNote(e.detail, note, sizeof(note));
    if (!note[0] && e.detail[0] && !(e.flags & LOGF_TEST)) hmsDbDescribe(e.detail, note, sizeof(note));   // no custom note: official Bambu text, or a meaning decoded from the code
    o["note"] = String(note);
  }
  sendJsonStream(d);          // the log can now carry long texts: stream it instead of building one big String
}

static void hRaw() {
  if (!guard()) return;
  size_t n = 0;
  uint32_t age = 0;
  const uint8_t* p = printersRawGet(s_srv.arg("i").toInt(), s_srv.arg("kind") == "msg" ? 0 : 1, &n, &age);
  if (!p) {
    s_srv.send(200, "application/json", "{}");
    return;
  }
  s_srv.setContentLength(n);                       // stream straight from PSRAM, no big String copy
  s_srv.send(200, "application/json; charset=utf-8", "");
  s_srv.sendContent((const char*)p, n);
}

static void hLogClear() { if (!guard()) return; evlogClear(); sendOk(); }

static void hNotifyTest() {
  if (!guard()) return;
  notifyTest(s_srv.arg("slot").toInt());
  sendOk();
}

static void hSoundDone() {
  if (!guard()) return;
  sendOk();
}

static void hUpload() {
  HTTPUpload& u = s_srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    if (!authOk()) return;
    touch();
    String n = u.filename;
    int sl = n.lastIndexOf('/');
    if (sl >= 0) n = n.substring(sl + 1);
    String clean;
    for (size_t i = 0; i < n.length(); i++) {
      char c = n[i];
      if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.') clean += c;
    }
    if (!clean.endsWith(".wav")) clean += ".wav";
    if (clean.length() > 31) clean = clean.substring(clean.length() - 31);
    if (clean.length() < 5) clean = "sound.wav";
    s_up = LittleFS.open("/sounds/" + clean, "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (s_up) s_up.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (s_up) s_up.close();
    s_sndDirty = true;
    audioInvalidate();
  }
}

// ---- official error texts (data/hms_zh.bin, data/hms_en.bin) ----
// Upload without the web page (lang=zh or lang=en, default zh):
//   curl -u admin:PASSWORD -F "file=@data/hms_zh.bin" "http://bambu-radar.local/api/hmsdb?lang=zh"
//   curl -u admin:PASSWORD -F "file=@data/hms_en.bin" "http://bambu-radar.local/api/hmsdb?lang=en"
// (leave out -u when no web password is set, or while setup mode is on). Written to /hms.tmp first, checked, then swapped in.
static File s_hmsUp;
static bool s_hmsOk = false;
static int s_hmsLang = 0;

static void hHmsDone() {
  if (!guard()) return;
  JsonDocument d;
  d["ok"] = s_hmsOk;
  d["lang"] = s_hmsLang == 1 ? "en" : "zh";
  d["count"] = hmsDbCount(s_hmsLang);
  sendJson(d);
}

static void hHmsUpload() {
  HTTPUpload& u = s_srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    s_hmsOk = false;
    if (!authOk()) return;
    touch();
    s_hmsLang = s_srv.arg("lang") == "en" ? 1 : 0;   // from the URL query: ?lang=en
    s_hmsUp = LittleFS.open("/hms.tmp", "w");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (s_hmsUp) s_hmsUp.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (s_hmsUp) {
      s_hmsUp.close();
      s_hmsOk = hmsDbInstall("/hms.tmp", s_hmsLang);
    }
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (s_hmsUp) {
      s_hmsUp.close();
      LittleFS.remove("/hms.tmp");
    }
  }
}

// ---- device language: the one setting behind push messages, the web page and the official error texts ----
static void hLang() {
  if (!guard()) return;
  String l = s_srv.arg("l");
  if (l != "zh" && l != "en") {
    s_srv.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  cfgLock();
  cfg.lang = (l == "en") ? LANG_EN : LANG_ZH;
  cfgUnlock();
  bool saved = configSave();
  s_srv.send(200, "application/json", saved ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ---- firmware update ----
static void hOtaStatus() {
  if (!guard()) return;
  JsonDocument d;
  otaStatus(d.to<JsonObject>());
  sendJson(d);
}

static void hOtaCheck() {
  if (!guard()) return;
  bool ok = otaCheckStart();
  JsonDocument d;
  d["ok"] = ok;
  sendJson(d);
}

static void hOtaInstall() {
  if (!guard()) return;
  bool ok = otaInstallStart();
  JsonDocument d;
  d["ok"] = ok;
  sendJson(d);
}

// Upload firmware.bin from the web page (multipart POST). The image goes straight into the idle OTA slot; the
// device reboots into it only after the whole file was received and checked (magic byte, chip, size).
static bool s_otaUpOk = false;

static void hOtaDone() {
  if (!guard()) return;
  JsonDocument d;
  d["ok"] = s_otaUpOk;
  otaStatus(d["st"].to<JsonObject>());
  sendJson(d);
}

static void hOtaUpload() {
  HTTPUpload& u = s_srv.upload();
  if (u.status == UPLOAD_FILE_START) {
    s_otaUpOk = false;
    if (!authOk()) return;
    touch();
    s_otaUpOk = otaUploadBegin();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    touch();
    if (s_otaUpOk) s_otaUpOk = otaUploadWrite(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (s_otaUpOk) s_otaUpOk = otaUploadEnd();
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (s_otaUpOk) otaUploadAbort();
    s_otaUpOk = false;
  }
}

static void hNotFound() {
  s_srv.sendHeader("Location", "/");
  s_srv.send(302, "text/plain", "");
}

void webApSet(bool on) {
  if (on == s_apOn) return;
  if (on) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Bambu-FDM-Radar", cfg.apPass);
    s_dns.start(53, "*", WiFi.softAPIP());
  } else {
    s_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  s_apOn = on;
}

void webBegin() {
  touch();
  if (s_active) return;
  if (MDNS.begin(cfg.host)) MDNS.addService("http", "tcp", 80);

  if (!s_routes) {
    const char* hk[] = {"Cookie"};
    s_srv.collectHeaders(hk, 1);       // also makes sure the Authorization header is kept for Basic auth
    s_srv.on("/", HTTP_GET, hIndex);
    s_srv.on("/favicon.ico", HTTP_GET, hFavicon);
    s_srv.on("/api/state", HTTP_GET, hState);
    s_srv.on("/api/config", HTTP_POST, hConfig);
    s_srv.on("/api/scan", HTTP_GET, hScan);
    s_srv.on("/api/test", HTTP_POST, hTest);
    s_srv.on("/api/stop", HTTP_POST, hStop);
    s_srv.on("/api/exit", HTTP_POST, hExit);
    s_srv.on("/api/reboot", HTTP_POST, hReboot);
    s_srv.on("/api/log", HTTP_GET, hLog);
    s_srv.on("/api/raw", HTTP_GET, hRaw);
    s_srv.on("/api/time/sync", HTTP_POST, hTimeSync);
    s_srv.on("/api/time/set", HTTP_POST, hTimeSet);
    s_srv.on("/api/mute", HTTP_POST, hMute);
    s_srv.on("/api/selftest", HTTP_POST, hSelfTest);
    s_srv.on("/api/log/clear", HTTP_POST, hLogClear);
    s_srv.on("/api/notify/test", HTTP_POST, hNotifyTest);
    s_srv.on("/api/sound/delete", HTTP_POST, hSoundDelete);
    s_srv.on("/api/sound", HTTP_POST, hSoundDone, hUpload);
    s_srv.on("/api/hmsdb", HTTP_POST, hHmsDone, hHmsUpload);
    s_srv.on("/api/lang", HTTP_POST, hLang);
    s_srv.on("/api/ota/status", HTTP_GET, hOtaStatus);
    s_srv.on("/api/ota/check", HTTP_POST, hOtaCheck);
    s_srv.on("/api/ota/install", HTTP_POST, hOtaInstall);
    s_srv.on("/api/ota", HTTP_POST, hOtaDone, hOtaUpload);
    s_srv.serveStatic("/sounds", LittleFS, "/sounds");
    s_srv.onNotFound(hNotFound);
    s_routes = true;
  }
  s_srv.begin();

  s_active = true;
  s_exitReq = false;
  touch();
}

void webEnd() {
  if (!s_active) return;
  s_srv.stop();
  MDNS.end();
  webApSet(false);
  s_active = false;
}

void webLoop() {
  if (!s_active) return;
  if (s_apOn) s_dns.processNextRequest();
  s_srv.handleClient();
}

bool webExitRequested() { bool r = s_exitReq; s_exitReq = false; return r; }
bool webActive() { return s_active; }
void webSetLanOpen(bool open) { s_lanOpen = open; }
uint32_t webLastActivity() { return s_last; }
