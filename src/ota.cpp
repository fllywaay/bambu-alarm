
#include "ota.h"
#include "config.h"
#include "alarm.h"
#include "evlog.h"
#include "crumb.h"
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>

// ---- state shared with the web page (polled through otaStatus) ----
enum { OS_IDLE, OS_CHECKING, OS_UPTODATE, OS_AVAILABLE, OS_DOWNLOADING, OS_UPLOADING, OS_DONE, OS_ERROR };
static volatile uint8_t s_state = OS_IDLE;
static volatile uint32_t s_done = 0, s_total = 0;
static char s_err[24] = "";        // short key, the web page turns it into a sentence
static char s_remote[24] = "";     // version found on the server
static char s_sha[65] = "";        // sha256 of firmware.bin from version.json (lower case), "" = not given
static uint32_t s_size = 0;        // size of firmware.bin from version.json, 0 = not given
static uint8_t s_hdr[16];
static uint8_t s_hdrN = 0;

static const char* stateName(uint8_t s) {
  switch (s) {
    case OS_CHECKING: return "checking";
    case OS_UPTODATE: return "uptodate";
    case OS_AVAILABLE: return "available";
    case OS_DOWNLOADING: return "downloading";
    case OS_UPLOADING: return "uploading";
    case OS_DONE: return "done";
    case OS_ERROR: return "error";
    default: return "idle";
  }
}

static bool busy() { return s_state == OS_CHECKING || s_state == OS_DOWNLOADING || s_state == OS_UPLOADING || s_state == OS_DONE; }

static void fail(const char* key) {
  strlcpy(s_err, key, sizeof(s_err));
  s_state = OS_ERROR;
  Serial.printf("OTA failed: %s\n", key);
}

static void rebootTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(2500));          // let the web page see "done" first
  evlogFlushNow();
  ESP.restart();
}
static void rebootSoon() { xTaskCreate(rebootTask, "otaboot", 3072, nullptr, 1, nullptr); }

// "v1.2.3" / "1.2.3-dev" -> compare the numbers only. > 0: a is newer than b.
static int cmpVer(const char* a, const char* b) {
  if (*a == 'v' || *a == 'V') a++;
  if (*b == 'v' || *b == 'V') b++;
  for (int i = 0; i < 4; i++) {
    long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
    if (x != y) return x > y ? 1 : -1;
    if (*a == '.') a++;
    if (*b == '.') b++;
  }
  return 0;
}

// Check the first 16 bytes of an image: ESP image magic and that it was built for this chip (ESP32-S3 = 9).
static const char* hdrFeed(const uint8_t* b, size_t n) {
  if (s_hdrN >= 16) return nullptr;
  size_t k = 16 - s_hdrN;
  if (k > n) k = n;
  memcpy(s_hdr + s_hdrN, b, k);
  s_hdrN += (uint8_t)k;
  if (s_hdrN < 16) return nullptr;
  if (s_hdr[0] != 0xE9) return "magic";
  if ((uint16_t)(s_hdr[12] | (s_hdr[13] << 8)) != 9) return "chip";
  return nullptr;
}

// Where version.json / firmware.bin live: "owner/repo" = latest GitHub release, or a full http(s) folder URL.
static bool baseUrl(String& out) {
  String r = cfg.ghRepo;
  r.trim();
  if (!r.length()) { fail("norepo"); return false; }
  if (r.startsWith("https://") || r.startsWith("http://")) {
    out = r;
    if (!out.endsWith("/")) out += "/";
    return true;
  }
  int slash = r.indexOf('/');
  bool ok = slash > 0 && slash < (int)r.length() - 1 && r.indexOf('/', slash + 1) < 0;
  for (size_t i = 0; ok && i < r.length(); i++) {
    char c = r[i];
    ok = isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '/';
  }
  if (!ok) { fail("badrepo"); return false; }
  out = String("https://github.com/") + r + "/releases/latest/download/";
  return true;
}

static void shaHex(const uint8_t* d, char* out) {
  for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]);
}

// ---- GitHub / URL: check ----
static bool openUrl(HTTPClient& http, WiFiClientSecure& sec, WiFiClient& plain, const String& url) {
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);   // GitHub sends release downloads to another host
  http.setTimeout(15000);
  http.setConnectTimeout(10000);
  http.setUserAgent("bambu-radar");
  return url.startsWith("https://") ? http.begin(sec, url) : http.begin(plain, url);
}

static void doCheck() {
  String base;
  if (!baseUrl(base)) return;
  if (WiFi.status() != WL_CONNECTED) { fail("wifi"); return; }
  if (!heapHealthy(50000, 16000)) { fail("memory"); return; }
  WiFiClientSecure sec;
  sec.setInsecure();                 // no certificate store on the device; integrity is checked with sha256 below
  WiFiClient plain;
  HTTPClient http;
  if (!openUrl(http, sec, plain, base + "version.json")) { fail("net"); return; }
  int code = http.GET();
  if (code != 200) {
    char k[24];
    if (code > 0) snprintf(k, sizeof(k), "http:%d", code);
    else snprintf(k, sizeof(k), "net:%d", code);
    http.end();
    fail(k);
    return;
  }
  String body = http.getString();
  http.end();
  JsonDocument d;
  if (deserializeJson(d, body)) { fail("json"); return; }
  const char* v = d["version"] | "";
  if (!v[0]) { fail("json"); return; }
  if (*v == 'v' || *v == 'V') v++;
  strlcpy(s_remote, v, sizeof(s_remote));
  const char* sha = d["sha256"] | "";
  s_sha[0] = 0;
  if (strlen(sha) == 64) {
    for (int i = 0; i < 64; i++) s_sha[i] = (char)tolower((unsigned char)sha[i]);
    s_sha[64] = 0;
  }
  s_size = d["size"] | 0;
  s_err[0] = 0;
  s_state = cmpVer(s_remote, FW_VERSION) > 0 ? OS_AVAILABLE : OS_UPTODATE;
  Serial.printf("OTA check: local %s, server %s\n", FW_VERSION, s_remote);
}

static void checkTask(void*) {
  doCheck();
  vTaskDelete(nullptr);
}

bool otaCheckStart() {
  if (busy()) return false;
  s_err[0] = 0;
  s_remote[0] = 0;
  String base;
  if (!baseUrl(base)) return false;           // sets s_err
  s_state = OS_CHECKING;
  if (xTaskCreate(checkTask, "otacheck", 12288, nullptr, 1, nullptr) != pdPASS) {
    fail("memory");
    return false;
  }
  return true;
}

// ---- GitHub / URL: download and flash ----
static void doInstall() {
  String base;
  if (!baseUrl(base)) return;
  if (WiFi.status() != WL_CONNECTED) { fail("wifi"); return; }
  if (!heapHealthy(50000, 16000)) { fail("memory"); return; }
  WiFiClientSecure sec;
  sec.setInsecure();
  WiFiClient plain;
  HTTPClient http;
  if (!openUrl(http, sec, plain, base + "firmware.bin")) { fail("net"); return; }
  int code = http.GET();
  if (code != 200) {
    char k[24];
    if (code > 0) snprintf(k, sizeof(k), "http:%d", code);
    else snprintf(k, sizeof(k), "net:%d", code);
    http.end();
    fail(k);
    return;
  }
  int len = http.getSize();
  uint32_t total = len > 0 ? (uint32_t)len : s_size;
  if (total == 0 || (s_size && len > 0 && (uint32_t)len != s_size)) { http.end(); fail("size"); return; }
  if (!Update.begin(total, U_FLASH)) { http.end(); fail("size"); return; }

  s_total = total;
  s_done = 0;
  s_hdrN = 0;
  WiFiClient* st = http.getStreamPtr();
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  uint8_t* buf = (uint8_t*)malloc(2048);
  if (!buf) { Update.abort(); http.end(); mbedtls_sha256_free(&ctx); fail("memory"); return; }

  uint32_t got = 0, lastData = millis();
  const char* bad = nullptr;
  while (got < total && !bad) {
    size_t avail = st->available();
    if (!avail) {
      if (!st->connected()) break;
      if (millis() - lastData > 20000) break;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    size_t want = total - got;
    if (want > 2048) want = 2048;
    if (want > avail) want = avail;
    size_t n = st->readBytes(buf, want);
    if (!n) continue;
    lastData = millis();
    bad = hdrFeed(buf, n);
    if (bad) break;
    mbedtls_sha256_update_ret(&ctx, buf, n);
    if (Update.write(buf, n) != n) { bad = "write"; break; }
    got += n;
    s_done = got;
  }
  free(buf);
  http.end();
  uint8_t dg[32];
  mbedtls_sha256_finish_ret(&ctx, dg);
  mbedtls_sha256_free(&ctx);

  if (!bad && got != total) bad = "net";
  if (!bad && s_sha[0]) {
    char hex[65];
    shaHex(dg, hex);
    if (strcmp(hex, s_sha) != 0) bad = "hash";
  }
  if (bad) { Update.abort(); fail(bad); return; }
  if (!Update.end(true)) { fail("end"); return; }   // true = also checks the image and sets it as the next boot slot
  s_err[0] = 0;
  s_state = OS_DONE;
  Serial.printf("OTA from server done: %s -> %s, rebooting\n", FW_VERSION, s_remote);
  rebootSoon();
}

static void installTask(void*) {
  doInstall();
  vTaskDelete(nullptr);
}

bool otaInstallStart() {
  if (busy() || s_state != OS_AVAILABLE) return false;
  if (alarmRinging()) { fail("ringing"); return false; }
  s_err[0] = 0;
  s_state = OS_DOWNLOADING;
  s_done = 0;
  s_total = s_size;
  if (xTaskCreate(installTask, "otainst", 12288, nullptr, 1, nullptr) != pdPASS) {
    fail("memory");
    return false;
  }
  return true;
}

// ---- web upload ----
bool otaUploadBegin() {
  if (busy()) { strlcpy(s_err, "busy", sizeof(s_err)); return false; }
  if (alarmRinging()) { fail("ringing"); return false; }
  s_err[0] = 0;
  s_hdrN = 0;
  s_done = 0;
  s_total = 0;
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { fail("write"); return false; }
  s_state = OS_UPLOADING;
  return true;
}

bool otaUploadWrite(const uint8_t* buf, size_t len) {
  if (s_state != OS_UPLOADING) return false;
  const char* bad = hdrFeed(buf, len);
  if (bad) { Update.abort(); fail(bad); return false; }
  if (Update.write((uint8_t*)buf, len) != len) { Update.abort(); fail("write"); return false; }
  s_done += (uint32_t)len;
  return true;
}

bool otaUploadEnd() {
  if (s_state != OS_UPLOADING) return false;
  if (s_hdrN < 16) { Update.abort(); fail("magic"); return false; }
  if (!Update.end(true)) { fail("end"); return false; }
  s_err[0] = 0;
  s_state = OS_DONE;
  Serial.printf("OTA upload done (%u bytes), rebooting\n", (unsigned)s_done);
  rebootSoon();
  return true;
}

void otaUploadAbort() {
  if (s_state == OS_UPLOADING) {
    Update.abort();
    fail("net");
  }
}

// ---- automatic rollback ----
// The Arduino core normally marks a newly installed image valid right at startup, before setup() runs, which means
// the bootloader's rollback never gets a chance. Returning true here keeps the image "pending verify" until
// otaLoop() confirms it; any reset before that (crash, watchdog, power cycle) boots the previous image again.
extern "C" bool verifyRollbackLater() { return true; }

static bool pendingVerify() {
  const esp_partition_t* p = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  return p && esp_ota_get_state_partition(p, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
}

#define OTA_CONFIRM_MS 60000UL
void otaLoop() {
  static int pend = -1;                       // -1 not looked at yet, 1 waiting for confirmation, 0 nothing to do
  if (pend == 0) return;
  if (pend < 0) {
    pend = pendingVerify() ? 1 : 0;           // a USB-flashed image is never pending
    if (pend) Serial.println("OTA: new firmware is on trial, it is confirmed after 60 s without a reset");
    return;
  }
  if (millis() < OTA_CONFIRM_MS) return;
  esp_ota_mark_app_valid_cancel_rollback();
  pend = 0;
  Serial.println("OTA: new firmware confirmed");
}

void otaStatus(JsonObject o) {
  o["ver"] = FW_VERSION;
  o["state"] = stateName(s_state);
  o["remote"] = s_remote;
  o["new"] = s_state == OS_AVAILABLE;
  o["done"] = (uint32_t)s_done;
  o["total"] = (uint32_t)s_total;
  o["err"] = s_err;
  o["hasRepo"] = cfg.ghRepo[0] != 0;
  const esp_partition_t* p = esp_ota_get_running_partition();
  o["slot"] = p ? p->label : "";
  o["pending"] = pendingVerify();     // new firmware still on trial (a reset now would roll back)
}
