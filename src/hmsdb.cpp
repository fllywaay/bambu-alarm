
#include "hmsdb.h"
#include "config.h"
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

// File layout (little-endian), written by tools/build_hms.py:
//   header 7 x u32: magic 'HMS1', nHms, nErr, hmsOff, errOff, blobOff, blobLen
//   hms index  nHms x {u64 key, u32 off}  sorted by key (key = attr<<32 | code)
//   err index  nErr x {u32 key, u32 off}  sorted by key (key = print_error)
//   blob       NUL-terminated UTF-8 strings; off is relative to blobOff
// One file per language: /hms_zh.bin (index 0) and /hms_en.bin (index 1). /hms.bin from before the bilingual
// firmware is read as the Chinese file until a new Chinese file is installed (which removes it).
#define HMSDB_PATH_ZH  "/hms_zh.bin"
#define HMSDB_PATH_EN  "/hms_en.bin"
#define HMSDB_PATH_OLD "/hms.bin"
#define HDR_SIZE   28
#define TXT_MAX    224        // longest official text is ~216 bytes
#define CACHE_N    6          // the web log asks for the same few codes again and again

struct DbHdr { uint32_t magic, nHms, nErr, hmsOff, errOff, blobOff, blobLen; };

struct CacheEnt {
  bool used;
  uint8_t digits;             // 8 or 16
  uint8_t lang;               // which file the text came from (0 / 1)
  uint64_t key;
  char txt[TXT_MAX];          // "" = looked up, not found
};

static SemaphoreHandle_t s_mx = nullptr;
static DbHdr s_h[2];
static bool s_ok[2] = {false, false};
static const char* s_path[2] = {HMSDB_PATH_ZH, HMSDB_PATH_EN};   // file actually in use for each language
static CacheEnt s_cache[CACHE_N];
static uint8_t s_cacheNext = 0;

static const char* dbPath(int lang) { return lang == LANG_EN ? HMSDB_PATH_EN : HMSDB_PATH_ZH; }

static bool readHeader(File& f, DbHdr* h) {
  if (!f || f.size() < HDR_SIZE) return false;
  f.seek(0);
  uint8_t b[HDR_SIZE];
  if (f.read(b, HDR_SIZE) != HDR_SIZE) return false;
  memcpy(h, b, HDR_SIZE);
  if (h->magic != 0x31534D48u) return false;                        // 'H','M','S','1'
  if (h->hmsOff != HDR_SIZE) return false;
  if (h->errOff != h->hmsOff + 12u * h->nHms) return false;
  if (h->blobOff != h->errOff + 8u * h->nErr) return false;
  if ((uint64_t)h->blobOff + h->blobLen != (uint64_t)f.size()) return false;
  return true;
}

static void clearCache() {
  memset(s_cache, 0, sizeof(s_cache));
  s_cacheNext = 0;
}

// Read the header of one language's file. Caller holds s_mx (or is still single-threaded in setup()).
static void loadOne(int lang) {
  s_ok[lang] = false;
  const char* cand[2] = {dbPath(lang), lang == LANG_ZH ? HMSDB_PATH_OLD : nullptr};
  for (const char* p : cand) {
    if (!p) continue;
    File f = LittleFS.open(p, "r");
    if (!f) continue;
    DbHdr h;
    bool ok = readHeader(f, &h);
    f.close();
    if (ok) {
      s_h[lang] = h;
      s_path[lang] = p;
      s_ok[lang] = true;
      return;
    }
  }
}

void hmsDbBegin() {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  clearCache();
  loadOne(LANG_ZH);
  loadOne(LANG_EN);
  Serial.printf("HMS texts: zh %s (%d codes), en %s (%d codes)\n",
                s_ok[LANG_ZH] ? "loaded" : "not installed", hmsDbCount(LANG_ZH),
                s_ok[LANG_EN] ? "loaded" : "not installed", hmsDbCount(LANG_EN));
}

int hmsDbCount(int lang) {
  if (lang < 0) lang = cfg.lang == LANG_EN ? LANG_EN : LANG_ZH;
  if (lang > 1) return 0;
  return s_ok[lang] ? (int)(s_h[lang].nHms + s_h[lang].nErr) : 0;
}

// Binary search of one index. entSize 12 (u64 key) or 8 (u32 key). Returns the blob offset in *off.
static bool findKey(File& f, uint32_t base, uint32_t count, uint32_t entSize, uint64_t key, uint32_t* off) {
  int32_t lo = 0, hi = (int32_t)count - 1;
  uint8_t e[12];
  while (lo <= hi) {
    int32_t mid = lo + (hi - lo) / 2;
    f.seek(base + (uint32_t)mid * entSize);
    if (f.read(e, entSize) != entSize) return false;
    uint64_t k;
    uint32_t o;
    if (entSize == 12) {
      memcpy(&k, e, 8);
      memcpy(&o, e + 8, 4);
    } else {
      uint32_t k32;
      memcpy(&k32, e, 4);
      memcpy(&o, e + 4, 4);
      k = k32;
    }
    if (k == key) { *off = o; return true; }
    if (k < key) lo = mid + 1; else hi = mid - 1;
  }
  return false;
}

// Copy src into dst (size n). If it does not fit: cut on a UTF-8 character boundary and add an ellipsis.
static void copyTrunc(char* dst, size_t n, const char* src) {
  size_t len = strlen(src);
  if (len < n) { memcpy(dst, src, len + 1); return; }
  if (n < 5) { dst[0] = 0; return; }
  size_t end = n - 4;                                   // room for 3 ellipsis bytes + NUL
  while (end > 0 && ((uint8_t)src[end] & 0xC0) == 0x80) end--;
  memcpy(dst, src, end);
  dst[end] = (char)0xE2; dst[end + 1] = (char)0x80; dst[end + 2] = (char)0xA6;   // U+2026
  dst[end + 3] = 0;
}

bool hmsDbLookup(const char* normCode, char* out, size_t n) {
  if (!out || n < 2) return false;
  out[0] = 0;
  if (!normCode || !s_mx) return false;
  size_t digits = strlen(normCode);
  if (digits != 8 && digits != 16) return false;
  uint64_t key = 0;
  for (size_t i = 0; i < digits; i++) {
    char c = normCode[i];
    int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
    if (v < 0) return false;
    key = (key << 4) | (uint64_t)v;
  }

  if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(300)) != pdTRUE) return false;

  // the language the device is set to; if that file is not installed, fall back to the other one
  int want = cfg.lang == LANG_EN ? LANG_EN : LANG_ZH;
  int use = s_ok[want] ? want : (s_ok[1 - want] ? 1 - want : -1);
  if (use < 0) { xSemaphoreGive(s_mx); return false; }

  bool found = false;
  for (int i = 0; i < CACHE_N; i++) {
    const CacheEnt& c = s_cache[i];
    if (c.used && c.digits == digits && c.lang == use && c.key == key) {
      if (c.txt[0]) { copyTrunc(out, n, c.txt); found = out[0] != 0; }
      xSemaphoreGive(s_mx);
      return found;
    }
  }

  char tmp[TXT_MAX] = {0};
  File f = LittleFS.open(s_path[use], "r");
  if (f) {
    uint32_t off = 0;
    bool hit = (digits == 16) ? findKey(f, s_h[use].hmsOff, s_h[use].nHms, 12, key, &off)
                              : findKey(f, s_h[use].errOff, s_h[use].nErr, 8, key, &off);
    if (hit && off < s_h[use].blobLen) {
      f.seek(s_h[use].blobOff + off);
      size_t got = f.read((uint8_t*)tmp, TXT_MAX - 1);
      tmp[got] = 0;
    }
    f.close();
  }

  CacheEnt& c = s_cache[s_cacheNext];
  s_cacheNext = (s_cacheNext + 1) % CACHE_N;
  c.used = true;
  c.digits = (uint8_t)digits;
  c.lang = (uint8_t)use;
  c.key = key;
  memcpy(c.txt, tmp, sizeof(c.txt));
  if (tmp[0]) { copyTrunc(out, n, tmp); found = out[0] != 0; }
  xSemaphoreGive(s_mx);
  return found;
}

// Module and alert level as documented on the Bambu wiki (HMS intro): an HMS code is
// module(2) module no.(2) part(2) part no.(2) | level(4) id(4), 16 hex digits in all.
static const char* moduleName(unsigned m, bool en) {
  switch (m) {
    case 0x03: return en ? "Motion Controller (MC)" : "运动控制器(MC)";
    case 0x05: return en ? "Main Board (AP)" : "主板(AP)";
    case 0x07: return "AMS";
    case 0x08: return en ? "Tool Head (TH)" : "工具头(TH)";
    case 0x0C: return en ? "Micro Lidar (XCAM)" : "激光雷达(XCAM)";
    case 0x10: return "Studio";
    case 0x11: return "APP";
    case 0x12: return "AMS Lite";
    default: return nullptr;
  }
}

static const char* levelText(unsigned lv, bool en) {
  switch (lv) {
    case 1: return en ? "Error: print stopped" : "错误:任务停止";
    case 2: return en ? "Warning: print pauses" : "警告:打印暂停";
    case 3: return en ? "Info" : "提示";
    default: return nullptr;
  }
}

bool hmsDbDescribe(const char* normCode, char* out, size_t n) {
  if (hmsDbLookup(normCode, out, n)) return true;                  // official text (or empty out + false)
  if (!out || n < 2) return false;
  out[0] = 0;
  if (!normCode || strlen(normCode) != 16) return false;           // 8-digit print_error codes have no module layout to decode
  for (int i = 0; i < 16; i++)
    if (!isxdigit((unsigned char)normCode[i])) return false;
  char a[9], c[9];
  memcpy(a, normCode, 8);
  a[8] = 0;
  memcpy(c, normCode + 8, 8);
  c[8] = 0;
  unsigned module = (unsigned)(strtoul(a, nullptr, 16) >> 24);
  unsigned level = (unsigned)(strtoul(c, nullptr, 16) >> 16);
  bool en = cfg.lang == LANG_EN;
  const char* mod = moduleName(module, en);
  const char* lv = levelText(level, en);
  snprintf(out, n, "%s%s%s%s%s", en ? "Not in the official list" : "官方表未收录", mod ? " · " : "", mod ? mod : "", lv ? " · " : "", lv ? lv : "");
  return true;
}

bool hmsDbInstall(const char* tmpPath, int lang) {
  if (!s_mx) s_mx = xSemaphoreCreateMutex();
  int l = lang == LANG_EN ? LANG_EN : LANG_ZH;
  bool valid = false;
  File f = LittleFS.open(tmpPath, "r");
  if (f) {
    DbHdr h;
    valid = readHeader(f, &h);
    f.close();
  }
  if (!valid) {
    LittleFS.remove(tmpPath);
    return false;
  }
  if (xSemaphoreTake(s_mx, pdMS_TO_TICKS(2000)) != pdTRUE) return false;
  LittleFS.remove(dbPath(l));
  bool ok = LittleFS.rename(tmpPath, dbPath(l));
  if (ok && l == LANG_ZH) LittleFS.remove(HMSDB_PATH_OLD);   // the new Chinese file replaces the pre-bilingual /hms.bin
  loadOne(l);
  clearCache();
  xSemaphoreGive(s_mx);
  return ok && s_ok[l];
}
