#include "notify.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "crumb.h"
#include "hmsdb.h"

// Push notifications (ntfy / Bark / Telegram / generic webhook / Feishu / WeCom / DingTalk).
// Runs in its own task so a slow HTTPS request never blocks the alarm.

struct NMsg {
  int8_t slot;          // >= 0: test message for this slot only, -1: real event
  uint8_t ev;
  bool test;
  char printer[24];
  char detail[20];
};

static QueueHandle_t s_q = nullptr;
static volatile int s_last[MAX_NOTIFY];

// A TLS handshake needs roughly 30-40 KB of internal RAM. Under pressure (several printer links + web page open) a
// failed allocation inside the TLS / HTTP stack can take the whole device down, so wait for memory instead.
#define NOTIFY_MIN_HEAP  50000
#define NOTIFY_MIN_BLOCK 16000

static bool waitForHeap() {
  for (int i = 0; i < 16; i++) {                       // up to ~8 s
    if (heapHealthy(NOTIFY_MIN_HEAP, NOTIFY_MIN_BLOCK)) return true;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  Serial.printf("Notify skipped: low internal heap (free %u, largest block %u)\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  return false;
}

static String trimSlash(String s) {
  while (s.endsWith("/")) s.remove(s.length() - 1);
  return s;
}

static int postJson(const String& url, const String& body, const char* bearer = nullptr) {
  bool https = url.startsWith("https://");
  if (!https && !url.startsWith("http://")) return -2;     // not a valid URL
  WiFiClientSecure sec;
  WiFiClient plain;
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  crumb(3, 5);
  bool ok;
  if (https) {
    sec.setInsecure();
    ok = http.begin(sec, url);
  } else {
    ok = http.begin(plain, url);
  }
  if (!ok) return -3;
  http.addHeader("Content-Type", "application/json; charset=utf-8");
  if (bearer && bearer[0]) http.addHeader("Authorization", String("Bearer ") + bearer);
  crumb(3, 6);
  int code = http.POST(body);
  crumb(3, 7);
  http.end();
  return code;
}

static int sendOne(const NotifyCfg& N, const NMsg& m, uint8_t lg) {
  char tm[16] = "";
  time_t t = time(nullptr);
  if (t > 1700000000) {
    struct tm lt;
    localtime_r(&t, &lt);
    strftime(tm, sizeof(tm), "%m-%d %H:%M", &lt);
  }

  // Everything below is written in ONE language, the one set on the device (cfg.lang, passed in as lg).
  const char* app = tr(lg, "拓竹FDM雷达", "Bambu FDM Radar");
  String title, body;
  if (m.test) {
    title = app;
    body = tr(lg, "测试通知:收到这条说明该通道配置正确", "Test notification: if you can read this, the channel is set up correctly");
  } else {
    title = String(app) + " · " + evName(m.ev, lg);
    body = String(m.printer) + " " + evName(m.ev, lg);
    if (m.detail[0]) {
      char note[48] = "";
      errCodeNote(m.detail, note, sizeof(note));
      char desc[200] = "";
      if (!note[0]) hmsDbDescribe(m.detail, desc, sizeof(desc));   // official Bambu text in the device language (hms_zh.bin / hms_en.bin), else a meaning decoded from the code; a custom note wins
      const char* label = note[0] ? note : desc;
      body += String(" (") + (label[0] ? String(label) + " " : String(tr(lg, "代码 ", "Code "))) + m.detail + ")";
    }
  }
  if (tm[0]) body += String("  ") + tm;
  String text = title + "\n" + body;

  String kw;
  if (N.extra[0]) { kw = N.extra; kw += " "; }

  JsonDocument d;
  String out;

  switch (N.type) {
    case NT_NTFY: {
      // url = https://ntfy.sh/your-topic  (or your own server + topic)
      String u = trimSlash(N.url);
      int scheme = u.indexOf("://");
      int p = u.lastIndexOf('/');
      if (scheme < 0 || p <= scheme + 2 || p >= (int)u.length() - 1) return -4;
      d["topic"] = u.substring(p + 1);
      d["title"] = title;
      d["message"] = body;
      d["priority"] = (m.test || m.ev == EV_FINISH) ? 3 : (m.ev == EV_PAUSE ? 4 : 5);
      d["tags"].to<JsonArray>().add(m.test ? "bell" : (m.ev == EV_FINISH ? "white_check_mark" : "rotating_light"));
      serializeJson(d, out);
      return postJson(u.substring(0, p), out, N.key);
    }
    case NT_BARK: {
      if (!N.key[0]) return -4;
      String server = N.url[0] ? trimSlash(N.url) : String("https://api.day.app");
      d["device_key"] = N.key;
      d["title"] = title;
      d["body"] = body;
      d["group"] = app;
      if (!m.test && m.ev != EV_FINISH && m.ev != EV_PAUSE) {
        d["level"] = "critical";      // iOS critical alert: rings even on silent / Do Not Disturb (needs permission)
        d["volume"] = 8;
      } else {
        d["level"] = "timeSensitive";
      }
      serializeJson(d, out);
      return postJson(server + "/push", out);
    }
    case NT_TELEGRAM: {
      if (!N.key[0] || !N.extra[0]) return -4;
      String base = N.url[0] ? trimSlash(N.url) : String("https://api.telegram.org");
      d["chat_id"] = N.extra;
      d["text"] = text;
      serializeJson(d, out);
      return postJson(base + "/bot" + N.key + "/sendMessage", out);
    }
    case NT_WEBHOOK: {
      if (!N.url[0]) return -4;
      d["event"] = m.test ? "test" : EV_KEYS[m.ev];
      d["event_name"] = m.test ? tr(lg, "测试", "Test") : evName(m.ev, lg);
      d["lang"] = lg == LANG_EN ? "en" : "zh";
      d["printer"] = m.printer;
      d["detail"] = m.detail;
      d["title"] = title;
      d["message"] = body;
      d["time"] = tm;
      serializeJson(d, out);
      return postJson(N.url, out);
    }
    case NT_FEISHU: {
      if (!N.url[0]) return -4;
      d["msg_type"] = "text";
      d["content"]["text"] = kw + text;
      serializeJson(d, out);
      return postJson(N.url, out);
    }
    case NT_WECOM:
    case NT_DINGTALK: {
      if (!N.url[0]) return -4;
      d["msgtype"] = "text";
      d["text"]["content"] = (N.type == NT_DINGTALK ? kw : String("")) + text;
      serializeJson(d, out);
      return postJson(N.url, out);
    }
    default:
      return -5;
  }
}

static void notifyTask(void*) {
  NMsg m;
  for (;;) {
    crumb(3, 1);
    if (xQueueReceive(s_q, &m, portMAX_DELAY) != pdTRUE) continue;
    crumb(3, 2);
    for (int i = 0; i < MAX_NOTIFY; i++) {
      NotifyCfg N;                                     // private copy: the web page may rewrite cfg while we send
      uint8_t lg;
      cfgLock();
      N = cfg.notify[i];
      lg = cfg.lang;                                   // push text follows the device language, one language only
      cfgUnlock();
      bool sel = m.test ? (i == m.slot) : (N.enabled && ((N.evMask >> m.ev) & 1));
      if (!sel) continue;
      crumb(3, 3);
      int code;
      if (WiFi.status() != WL_CONNECTED) code = -1;
      else if (!waitForHeap()) code = -6;               // -6 = skipped, not enough free RAM
      else code = sendOne(N, m, lg);
      crumb(3, 4);
      s_last[i] = code;
      Serial.printf("Notify[%d] %s -> %d\n", i, N.name, code);
    }
  }
}

void notifyBegin() {
  for (int i = 0; i < MAX_NOTIFY; i++) s_last[i] = 0;
  s_q = xQueueCreate(6, sizeof(NMsg));
  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(notifyTask, "notify", 16384, NULL, 1, &h, 0);
  crumbSetTask(3, h);
}

void notifyEvent(EventType ev, int idx, const char* detail) {
  if (!s_q) return;
  bool any = false;
  for (int i = 0; i < MAX_NOTIFY; i++)
    if (cfg.notify[i].enabled && ((cfg.notify[i].evMask >> ev) & 1)) any = true;
  if (!any) return;
  NMsg m;
  memset(&m, 0, sizeof(m));
  m.slot = -1;
  m.ev = (uint8_t)ev;
  m.test = false;
  if (idx >= 0 && idx < MAX_PRINTERS && cfg.printers[idx].name[0]) strlcpy(m.printer, cfg.printers[idx].name, sizeof(m.printer));
  else snprintf(m.printer, sizeof(m.printer), "%s%d", tr(cfg.lang, "打印机", "Printer "), idx + 1);
  if (detail) strlcpy(m.detail, detail, sizeof(m.detail));
  xQueueSend(s_q, &m, 0);
}

void notifyTest(int slot) {
  if (!s_q || slot < 0 || slot >= MAX_NOTIFY) return;
  NMsg m;
  memset(&m, 0, sizeof(m));
  m.slot = (int8_t)slot;
  m.test = true;
  xQueueSend(s_q, &m, 0);
}

int notifyLastResult(int slot) { return (slot >= 0 && slot < MAX_NOTIFY) ? s_last[slot] : 0; }
