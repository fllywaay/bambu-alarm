#include "printer.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <strings.h>
#include <esp_heap_caps.h>
#include "crumb.h"

// Internal-RAM thresholds before opening another TLS link (each one costs ~40 KB of internal heap)
#define MQTT_MIN_HEAP  45000
#define MQTT_MIN_BLOCK 16000

struct EvMsg { uint8_t ev; uint8_t idx; uint8_t ignored; char detail[20]; };
static QueueHandle_t s_q = nullptr;
static volatile bool s_reload = false;

// ---------------------------------------------------------------- SSDP ----
static WiFiUDP s_udp;
static Discovered s_disc[8];
static int s_nDisc = 0;

static const char* findCI(const char* hay, const char* needle) {
  size_t nl = strlen(needle);
  for (; *hay; hay++)
    if (strncasecmp(hay, needle, nl) == 0) return hay;
  return nullptr;
}

static bool hdrVal(const char* buf, const char* key, char* out, size_t n) {
  const char* p = findCI(buf, key);
  if (!p) return false;
  p += strlen(key);
  while (*p == ' ' || *p == '\t') p++;
  size_t i = 0;
  while (*p && *p != '\r' && *p != '\n' && i < n - 1) out[i++] = *p++;
  out[i] = 0;
  return i > 0;
}

static void ssdpPoll() {
  while (s_udp.parsePacket() > 0) {
    char buf[900];
    int len = s_udp.read(buf, sizeof(buf) - 1);
    if (len <= 0) continue;
    buf[len] = 0;
    char loc[16] = "", usn[24] = "", model[24] = "", name[32] = "";
    hdrVal(buf, "Location:", loc, sizeof(loc));
    hdrVal(buf, "USN:", usn, sizeof(usn));
    hdrVal(buf, "DevModel.bambu.com:", model, sizeof(model));
    hdrVal(buf, "DevName.bambu.com:", name, sizeof(name));
    if (!usn[0]) continue;
    if (!loc[0]) strlcpy(loc, s_udp.remoteIP().toString().c_str(), sizeof(loc));

    int idx = -1;
    for (int i = 0; i < s_nDisc; i++)
      if (!strcmp(s_disc[i].serial, usn)) { idx = i; break; }
    if (idx < 0) {
      if (s_nDisc >= 8) continue;
      idx = s_nDisc++;
      memset(&s_disc[idx], 0, sizeof(Discovered));
      strlcpy(s_disc[idx].serial, usn, sizeof(s_disc[idx].serial));
    }
    strlcpy(s_disc[idx].ip, loc, sizeof(s_disc[idx].ip));
    if (model[0]) strlcpy(s_disc[idx].model, model, sizeof(s_disc[idx].model));
    if (name[0]) strlcpy(s_disc[idx].name, name, sizeof(s_disc[idx].name));
    s_disc[idx].lastSeen = millis();
  }
}

int ssdpCount() { return s_nDisc; }
const Discovered* ssdpGet(int i) { return (i >= 0 && i < s_nDisc) ? &s_disc[i] : nullptr; }

// ---------------------------------------------------------------- MQTT ----
#define HMS_SEEN_MAX 16

struct Link {
  WiFiClientSecure net;
  PubSubClient mq;
  int idx = 0;
  uint32_t lastTry = 0, lastPush = 0, connectedAt = 0;
  char lastG[12] = "";
  uint32_t lastErr = 0;
  uint64_t hmsSeen[HMS_SEEN_MAX];
  uint8_t nSeen = 0;
  bool wasUp = false;          // has been connected since the last reload
  uint32_t downSince = 0;      // when the MQTT link dropped (0 = link is up)
  bool offlineFired = false;
  bool bufOk = false;          // the big MQTT receive buffer has been allocated (done once, not on every retry)
  uint32_t failPend = 0;       // FAILED seen at this time; event is sent after a short delay (cancel detection)
  uint32_t pausePend = 0;      // same for PAUSE (an ignored print_error, e.g. camera-clean pause, must not ring)
  uint32_t cancelAt = 0;       // when a "cancelled by hand" print_error was last seen (the code may be cleared again quickly)
  uint32_t cancelCode = 0;
  PrinterState st;
  Link() : mq(net) { memset(&st, 0, sizeof(st)); memset(hmsSeen, 0, sizeof(hmsSeen)); }
};
static Link s_links[MAX_PRINTERS];

// The network task works on its own copy of the printer settings. PubSubClient keeps a pointer to the host string,
// and the web page rewrites cfg.printers[] at any time, so the task must never point into cfg directly.
static PrinterCfg s_pc[MAX_PRINTERS];
static void snapPrinters() {
  cfgLock();
  memcpy(s_pc, cfg.printers, sizeof(s_pc));
  cfgUnlock();
}
#define RAW_BUF 61440
static uint8_t* s_rawBuf[MAX_PRINTERS][2];
static size_t s_rawLen[MAX_PRINTERS][2];
static uint32_t s_rawAt[MAX_PRINTERS][2];

static void rawStore(int idx, int kind, const uint8_t* p, unsigned int len) {
  if (idx < 0 || idx >= MAX_PRINTERS || len == 0 || len > RAW_BUF) return;
  if (!s_rawBuf[idx][kind]) s_rawBuf[idx][kind] = (uint8_t*)heap_caps_malloc(RAW_BUF, MALLOC_CAP_SPIRAM);
  if (!s_rawBuf[idx][kind]) return;          // no PSRAM: debug view simply stays empty
  s_rawLen[idx][kind] = 0;
  memcpy(s_rawBuf[idx][kind], p, len);
  s_rawLen[idx][kind] = len;
  s_rawAt[idx][kind] = millis();
}

const uint8_t* printersRawGet(int idx, int kind, size_t* len, uint32_t* ageSec) {
  if (idx < 0 || idx >= MAX_PRINTERS || kind < 0 || kind > 1 || !s_rawBuf[idx][kind] || !s_rawLen[idx][kind]) {
    *len = 0;
    *ageSec = 0;
    return nullptr;
  }
  *len = s_rawLen[idx][kind];
  *ageSec = (millis() - s_rawAt[idx][kind]) / 1000;
  return s_rawBuf[idx][kind];
}

static void fire(EventType ev, int idx, bool ignored = false, const char* detail = "") {
  EvMsg m;
  memset(&m, 0, sizeof(m));
  m.ev = (uint8_t)ev;
  m.idx = (uint8_t)idx;
  m.ignored = ignored ? 1 : 0;
  if (detail) strlcpy(m.detail, detail, sizeof(m.detail));
  if (s_q) xQueueSend(s_q, &m, 0);
}

static JsonDocument& filterDoc() {
  static JsonDocument f;
  static bool init = false;
  if (!init) {
    f["print"]["gcode_state"] = true;
    f["print"]["mc_percent"] = true;
    f["print"]["print_error"] = true;
    f["print"]["hms"] = true;
    f["print"]["mc_remaining_time"] = true;
    f["print"]["layer_num"] = true;
    f["print"]["total_layer_num"] = true;
    f["print"]["subtask_name"] = true;
    f["print"]["nozzle_temper"] = true;
    f["print"]["bed_temper"] = true;
    f["print"]["chamber_temper"] = true;
    f["print"]["device"]["extruder"] = true;   // dual nozzle (H2D): per-nozzle temps + active nozzle
    f["print"]["device"]["nozzle"] = true;
    f["print"]["device"]["ctc"] = true;        // chamber
    f["print"]["device"]["bed_temp"] = true;
    // AMS filament overview: only the fields we show
    f["print"]["ams"]["tray_now"] = true;
    f["print"]["ams"]["tray_exist_bits"] = true;
    f["print"]["ams"]["ams_exist_bits"] = true;
    f["print"]["ams"]["ams"][0]["id"] = true;
    f["print"]["ams"]["ams"][0]["info"] = true;
    f["print"]["ams"]["ams"][0]["humidity_raw"] = true;
    f["print"]["ams"]["ams"][0]["temp"] = true;
    f["print"]["ams"]["ams"][0]["tray"][0]["id"] = true;
    f["print"]["ams"]["ams"][0]["tray"][0]["tray_color"] = true;
    f["print"]["ams"]["ams"][0]["tray"][0]["tray_type"] = true;
    f["print"]["ams"]["ams"][0]["tray"][0]["remain"] = true;
    f["print"]["vir_slot"][0]["id"] = true;
    f["print"]["vir_slot"][0]["tray_color"] = true;
    f["print"]["vir_slot"][0]["tray_type"] = true;
    f["print"]["vir_slot"][0]["remain"] = true;
    init = true;
  }
  return f;
}

static void pushAll(Link& L) {
  char t[48];
  snprintf(t, sizeof(t), "device/%s/request", s_pc[L.idx].serial);
  L.mq.publish(t, "{\"pushing\":{\"sequence_id\":\"1\",\"command\":\"pushall\"}}");
  L.lastPush = millis();
}

// JSON values may be numbers or numeric strings ("29"); hex strings use jHex.
static long jInt(JsonVariantConst v, long def) {
  if (v.is<long>()) return v.as<long>();
  if (v.is<const char*>()) return atol(v.as<const char*>());
  return def;
}
static uint32_t jHex(JsonVariantConst v, uint32_t def) {
  if (v.is<const char*>()) return (uint32_t)strtoul(v.as<const char*>(), nullptr, 16);
  return def;
}
static void readTray(AmsTrayState& T, JsonVariantConst tr) {
  const char* col = tr["tray_color"];
  if (col) {
    uint32_t v = (uint32_t)strtoul(col, nullptr, 16);
    T.rgb = (strlen(col) >= 8) ? (v >> 8) : v;      // RRGGBBAA -> RRGGBB
  }
  const char* ty = tr["tray_type"];
  if (ty) strlcpy(T.type, ty, sizeof(T.type));
  if (!tr["remain"].isNull()) T.remain = (int8_t)jInt(tr["remain"], -1);
}

static void onMsg(Link& L, uint8_t* payload, unsigned int len) {
  crumb(1, 4);
  rawStore(L.idx, 0, payload, len);            // latest message
  if (len > 3000) rawStore(L.idx, 1, payload, len);   // big message = full snapshot
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, payload, len, DeserializationOption::Filter(filterDoc()));
  if (e) return;
  JsonObjectConst pr = doc["print"];
  if (pr.isNull()) return;

  L.st.gotReport = true;
  L.st.lastReport = millis();
  bool grace = (millis() - L.connectedAt) < 4000;  // first snapshot after connect = baseline only

  // 1) print_error first, so a FAILED in the same message can tell "cancelled by user" from a real failure
  if (pr["print_error"].is<uint32_t>()) {
    uint32_t pe = pr["print_error"].as<uint32_t>();
    uint32_t prevErr = L.lastErr;
    L.lastErr = pe;
    L.st.err = (int)pe;          // state first, then the event (the alarm checks the state)
    if (pe != 0 && errIsCancel(pe)) { L.cancelAt = millis() ? millis() : 1; L.cancelCode = pe; }
    if (!grace && pe != 0 && pe != prevErr) {
      char norm[12];
      snprintf(norm, sizeof(norm), "%08lX", (unsigned long)pe);
      int act = errCodeAction(norm);
      bool ign = (act == 0) || (act != 1 && cfg.ignoreCancel && errIsCancel(pe));
      fire(EV_ERROR, L.idx, ign, norm);
    }
  }

  // 2) print state
  const char* g = pr["gcode_state"];
  if (g && strcmp(g, L.lastG) != 0) {
    char prev[12];
    strlcpy(prev, L.lastG, sizeof(prev));
    strlcpy(L.lastG, g, sizeof(L.lastG));
    strlcpy(L.st.gcode, g, sizeof(L.st.gcode));   // state first, then the event (the alarm checks the state)
    L.st.since = millis();
    if (strcmp(g, "FAILED") != 0) L.failPend = 0;
    if (strcmp(g, "PAUSE") != 0) L.pausePend = 0;
    if (!grace) {
      if (!strcmp(g, "FAILED")) L.failPend = millis() ? millis() : 1;
      else if (!strcmp(g, "FINISH") && !strcmp(prev, "RUNNING")) fire(EV_FINISH, L.idx);
      else if (!strcmp(g, "PAUSE")) L.pausePend = millis() ? millis() : 1;
    }
  }
  if (pr["mc_percent"].is<int>()) L.st.percent = pr["mc_percent"].as<int>();
  if (pr["mc_remaining_time"].is<int>()) { L.st.remainMin = pr["mc_remaining_time"].as<int>(); L.st.has |= 8; }
  if (pr["layer_num"].is<int>()) { L.st.layer = pr["layer_num"].as<int>(); L.st.has |= 16; }
  if (pr["total_layer_num"].is<int>()) { L.st.totalLayer = pr["total_layer_num"].as<int>(); L.st.has |= 16; }
  if (pr["nozzle_temper"].is<float>() && !L.st.nozMask) { L.st.nozzle = pr["nozzle_temper"].as<float>(); L.st.has |= 1; }
  if (pr["bed_temper"].is<float>() && !(L.st.has & 64)) { L.st.bed = pr["bed_temper"].as<float>(); L.st.has |= 2; }
  if (pr["chamber_temper"].is<float>() && !(L.st.has & 128)) { L.st.chamber = pr["chamber_temper"].as<float>(); L.st.has |= 4; }

  // Newer printers (H2D) report temps in print.device: encoded as current | (target << 16)
  JsonObjectConst dev = pr["device"];
  if (!dev.isNull()) {
    if (dev["bed_temp"].is<uint32_t>()) {
      uint32_t v = dev["bed_temp"].as<uint32_t>();
      L.st.bed = (float)(v & 0xFFFF); L.st.bedT = (int)(v >> 16); L.st.has |= 2 | 64;
    }
    if (dev["ctc"]["info"]["temp"].is<uint32_t>()) {
      uint32_t v = dev["ctc"]["info"]["temp"].as<uint32_t>();
      L.st.chamber = (float)(v & 0xFFFF); L.st.chamberT = (int)(v >> 16); L.st.has |= 4 | 128;
    }
    JsonArrayConst ex = dev["extruder"]["info"];
    if (!ex.isNull()) {
      int k = 0;
      for (JsonVariantConst e : ex) {
        if (k >= 2) break;
        int id = e["id"].is<int>() ? e["id"].as<int>() : k;
        if (id < 0 || id > 1) id = k;
        if (e["temp"].is<uint32_t>()) {
          uint32_t v = e["temp"].as<uint32_t>();
          L.st.nozCur[id] = (int)(v & 0xFFFF); L.st.nozTgt[id] = (int)(v >> 16); L.st.nozMask |= (uint8_t)(1 << id);
        }
        k++;
      }
    }
    if (dev["extruder"]["state"].is<uint32_t>()) {
      L.st.active = (int8_t)((dev["extruder"]["state"].as<uint32_t>() >> 4) & 0xF);
      L.st.has |= 32;
    }
  }
  const char* jn = pr["subtask_name"];
  if (jn) strlcpy(L.st.job, jn, sizeof(L.st.job));

  // AMS filament overview (merged: incremental reports may carry only some units / trays)
  JsonObjectConst am = pr["ams"];
  if (!am.isNull()) {
    if (!am["tray_now"].isNull()) L.st.trayNow = (int)jInt(am["tray_now"], 255);
    if (!am["tray_exist_bits"].isNull()) L.st.trayBits = jHex(am["tray_exist_bits"], L.st.trayBits);
    JsonArrayConst units = am["ams"];
    for (JsonVariantConst u : units) {
      int id = (int)jInt(u["id"], -1);
      if (id < 0 || id >= MAX_AMS) continue;
      AmsUnitState& U = L.st.ams[id];
      if (!U.present) { U.present = true; U.extruder = -1; U.humidity = -1; }
      if (!u["humidity_raw"].isNull()) U.humidity = (int8_t)jInt(u["humidity_raw"], -1);
      if (u["temp"].is<const char*>()) U.temp = (float)atof(u["temp"].as<const char*>());
      if (u["info"].is<const char*>()) U.extruder = (int8_t)((jHex(u["info"], 0) >> 8) & 0xF);   // dual-nozzle assignment (from the info bits; unverified)
      JsonArrayConst trays = u["tray"];
      for (JsonVariantConst tr : trays) {
        int tid = (int)jInt(tr["id"], -1);
        if (tid >= 0 && tid < 4) readTray(U.tray[tid], tr);
      }
    }
    if (!am["ams_exist_bits"].isNull()) {            // units that are gone
      uint32_t eb = jHex(am["ams_exist_bits"], 0xF);
      for (int i = 0; i < MAX_AMS; i++) if (!((eb >> i) & 1)) L.st.ams[i].present = false;
    }
  }
  JsonArrayConst vs = pr["vir_slot"];
  if (!vs.isNull()) {
    int k = 0;
    for (JsonVariantConst v : vs) {
      if (k >= 2) break;
      L.st.extId[k] = (int)jInt(v["id"], 255 - k);
      readTray(L.st.ext[k], v);
      L.st.extMask |= (uint8_t)(1 << k);
      k++;
    }
  }

  // 3) HMS notices: only NEW entries trigger, filtered by severity / AMS / custom code rules
  JsonArrayConst h = pr["hms"];
  if (!h.isNull()) {
    uint64_t cur[HMS_SEEN_MAX];
    uint8_t nc = 0, nAlarm = 0;   // nAlarm = listed HMS entries that would ring (not filtered out)
    char pendNorm[4][20];
    bool pendIgn[4];
    uint8_t np = 0;
    for (JsonVariantConst v : h) {
      if (nc >= HMS_SEEN_MAX) break;
      uint32_t attr = v["attr"].as<uint32_t>();
      uint32_t code = v["code"].as<uint32_t>();
      uint64_t key = ((uint64_t)attr << 32) | code;
      cur[nc++] = key;
      bool seen = false;
      for (uint8_t k = 0; k < L.nSeen; k++)
        if (L.hmsSeen[k] == key) { seen = true; break; }

      uint32_t sev = (code >> 16) & 0xFFFF;   // 1 fatal, 2 serious, 3 common, 4 info
      uint32_t mod = (attr >> 24) & 0xFF;     // 0x07 = AMS
      char norm[20];
      snprintf(norm, sizeof(norm), "%08lX%08lX", (unsigned long)attr, (unsigned long)code);
      int act = errCodeAction(norm);
      bool alarmIt;
      if (act == 0) alarmIt = false;
      else if (act == 1) alarmIt = true;
      else {
        alarmIt = (sev < 1 || sev > 4 || sev <= cfg.hmsSev);   // unknown severity: better safe than sorry
        if (cfg.ignoreAms && mod == 0x07 && sev != 1) alarmIt = false;
      }
      if (alarmIt) nAlarm++;
      if (!seen && !grace && np < 4) {
        strlcpy(pendNorm[np], norm, sizeof(pendNorm[np]));
        pendIgn[np] = !alarmIt;
        np++;
      }
    }
    memcpy(L.hmsSeen, cur, nc * sizeof(uint64_t));
    L.nSeen = nc;
    L.st.hms = nAlarm;
    for (uint8_t k = 0; k < np; k++) fire(EV_ERROR, L.idx, pendIgn[k], pendNorm[k]);   // after the state is updated
  }
}

static void linkLoop(Link& L) {
  const PrinterCfg& p = s_pc[L.idx];
  uint32_t now = millis();
  bool usable = p.enabled && p.ip[0] && p.serial[0] && p.code[0];
  if (!usable) {
    if (L.mq.connected()) L.mq.disconnect();
    L.st.connected = false;
    L.wasUp = false;
    L.downSince = 0;
    L.offlineFired = false;
    return;
  }
  if (!L.mq.connected()) {
    L.st.connected = false;
    if (L.wasUp && L.downSince == 0) L.downSince = now ? now : 1;
    if ((L.lastTry == 0 || now - L.lastTry > 6000) && heapHealthy(MQTT_MIN_HEAP, MQTT_MIN_BLOCK)) {
      L.lastTry = now;
      L.net.setInsecure();  // printer uses a self-signed certificate
      L.mq.setServer(p.ip, 8883);
      if (!L.bufOk) {
        L.bufOk = L.mq.setBufferSize(60000);        // PubSubClient max is 65535; lands in PSRAM
        if (!L.bufOk) {
          L.bufOk = L.mq.setBufferSize(20000);
          Serial.printf("Printer %d: 60 KB MQTT buffer failed, using 20 KB (big snapshots will be skipped)\n", L.idx + 1);
        }
      }
      L.mq.setKeepAlive(30);
      L.mq.setSocketTimeout(5);
      char cid[24];
      snprintf(cid, sizeof(cid), "h2d-%04x", (unsigned)(esp_random() & 0xffff));
      crumb(1, 6);
      if (L.mq.connect(cid, "bblp", p.code)) {
        char t[48];
        snprintf(t, sizeof(t), "device/%s/report", p.serial);
        L.mq.subscribe(t);
        L.st.connected = true;
        L.st.err = 0;
        L.st.hms = 0;
        L.connectedAt = millis();
        L.lastG[0] = 0;
        L.lastErr = 0;
        L.nSeen = 0;
        L.wasUp = true;
        L.downSince = 0;
        L.offlineFired = false;
        pushAll(L);
      }
    }
  } else {
    L.st.connected = true;
    L.downSince = 0;
    L.offlineFired = false;
    crumb(1, 3);
    L.mq.loop();
    crumb(1, 5);
    if (now - L.lastPush > 30000) pushAll(L);  // also acts as a heartbeat
  }

  // FAILED waits a moment so a user-cancel (print_error 0300400C etc.) can be told apart from a real failure
  if (L.failPend && now - L.failPend > 1500) {
    L.failPend = 0;
    char det[12] = "";
    bool recentCancel = cfg.ignoreCancel && L.cancelAt && now - L.cancelAt < 15000;   // cancel code may already be cleared
    if (L.st.err) snprintf(det, sizeof(det), "%08lX", (unsigned long)L.st.err);
    else if (recentCancel) snprintf(det, sizeof(det), "%08lX", (unsigned long)L.cancelCode);
    bool cancelled = (cfg.ignoreCancel && (errIsCancel((uint32_t)L.st.err) || recentCancel)) || (det[0] && errCodeAction(det) == 0);
    fire(EV_FAILED, L.idx, cancelled, det);
  }
  if (L.pausePend && now - L.pausePend > 1500) {
    L.pausePend = 0;
    char det[12] = "";
    bool recentCancel = cfg.ignoreCancel && L.cancelAt && now - L.cancelAt < 15000;
    if (L.st.err) snprintf(det, sizeof(det), "%08lX", (unsigned long)L.st.err);
    else if (recentCancel) snprintf(det, sizeof(det), "%08lX", (unsigned long)L.cancelCode);
    bool ign = (det[0] && errCodeAction(det) == 0) || (cfg.ignoreCancel && (errIsCancel((uint32_t)L.st.err) || recentCancel));
    fire(EV_PAUSE, L.idx, ign, det);
  }

  // Offline = the MQTT link has been DOWN for offlineSec. (Not "no message received": an idle H2D sends
  // nothing for minutes, which used to raise a false offline alarm while the printer was fine.)
  if (L.wasUp && L.downSince && !L.offlineFired && now - L.downSince > (uint32_t)cfg.offlineSec * 1000UL) {
    L.offlineFired = true;
    fire(EV_OFFLINE, L.idx);
  }
}

static void netTask(void*) {
  bool udpUp = false;
  for (;;) {
    if (s_reload) {
      snapPrinters();
      for (int i = 0; i < MAX_PRINTERS; i++) {
        Link& L = s_links[i];
        if (L.mq.connected()) L.mq.disconnect();
        L.lastG[0] = 0;
        L.lastErr = 0;
        L.nSeen = 0;
        L.wasUp = false;
        L.downSince = 0;
        L.offlineFired = false;
        L.failPend = 0;
        L.pausePend = 0;
        L.st.gotReport = false;
        L.st.connected = false;
        L.lastTry = 0;
      }
      s_reload = false;
    }
    if (WiFi.status() == WL_CONNECTED) {
      if (!udpUp) {
        s_udp.beginMulticast(IPAddress(239, 255, 255, 250), 1990);
        udpUp = true;
      }
      crumb(1, 2);
      ssdpPoll();
      for (int i = 0; i < MAX_PRINTERS; i++) linkLoop(s_links[i]);
      crumb(1, 7);
    } else {
      // Home Wi-Fi is down: the alarm itself is offline, not the printers -> do not raise "printer offline".
      if (udpUp) { s_udp.stop(); udpUp = false; }
      for (int i = 0; i < MAX_PRINTERS; i++) {
        Link& L = s_links[i];
        if (L.mq.connected()) L.mq.disconnect();
        L.st.connected = false;
        L.downSince = 0;
        L.offlineFired = false;
        L.wasUp = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void printersBegin() {
  s_q = xQueueCreate(12, sizeof(EvMsg));
  for (int i = 0; i < MAX_PRINTERS; i++) {
    Link& L = s_links[i];
    L.idx = i;
    L.mq.setCallback([&L](char*, uint8_t* p, unsigned int n) { onMsg(L, p, n); });
  }
  snapPrinters();
  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(netTask, "net", 16384, NULL, 1, &h, 0);
  crumbSetTask(1, h);
}

void printersReload() { s_reload = true; }
const PrinterState& printerState(int idx) { return s_links[(idx < 0 || idx >= MAX_PRINTERS) ? 0 : idx].st; }

bool printersNextEvent(EventType* ev, int* idx, bool* ignored, char* detail, size_t detailLen) {
  EvMsg m;
  if (!s_q || xQueueReceive(s_q, &m, 0) != pdTRUE) return false;
  *ev = (EventType)m.ev;
  *idx = m.idx;
  *ignored = m.ignored != 0;
  if (detail && detailLen) strlcpy(detail, m.detail, detailLen);
  return true;
}

bool printersEventStillActive(EventType ev) {
  if (ev == EV_FINISH || ev == EV_CLOCK) return true;
  for (int i = 0; i < MAX_PRINTERS; i++) {
    if (!cfg.printers[i].enabled) continue;
    const Link& L = s_links[i];
    switch (ev) {
      case EV_OFFLINE: if (L.offlineFired && !L.st.connected) return true; break;
      case EV_FAILED:  if (!strcmp(L.st.gcode, "FAILED")) return true; break;
      case EV_PAUSE:   if (!strcmp(L.st.gcode, "PAUSE")) return true; break;
      case EV_ERROR: {
        if (L.st.hms > 0) return true;                 // an HMS notice that would ring is still listed
        if (L.st.err != 0) {
          char n[12];
          snprintf(n, sizeof(n), "%08lX", (unsigned long)L.st.err);
          int a = errCodeAction(n);
          bool ign = (a == 0) || (a != 1 && cfg.ignoreCancel && errIsCancel((uint32_t)L.st.err));
          if (!ign) return true;                       // print_error is still set
        }
        break;
      }
      default: break;
    }
  }
  return false;
}
