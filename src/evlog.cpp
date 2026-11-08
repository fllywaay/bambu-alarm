#include "evlog.h"
#include <LittleFS.h>
#include <time.h>
#include "crumb.h"

#define LOG_FILE  "/evlog.bin"
#define LOG_MAGIC 0x4C4F4731UL

static LogEntry s_e[LOG_MAX];   // chronological, oldest first
static uint8_t s_n = 0;
// The log lives in RAM; the flash write is deferred to evlogTick() so that no flash erase / write happens while an
// alarm is starting (audio is reading the same flash, the notification task is opening a TLS link, LEDs are strobing).
static bool s_dirty = false;
static uint32_t s_dirtyAt = 0;

static void save() {
  crumb(0, 10);
  File f = LittleFS.open(LOG_FILE, "w");
  if (!f) return;
  uint32_t magic = LOG_MAGIC;
  f.write((const uint8_t*)&magic, sizeof(magic));
  f.write(&s_n, 1);
  f.write((const uint8_t*)s_e, s_n * sizeof(LogEntry));
  f.close();
  crumb(0, 11);
}

void evlogBegin() {
  s_n = 0;
  File f = LittleFS.open(LOG_FILE, "r");
  if (!f) return;
  uint32_t magic = 0;
  uint8_t n = 0;
  if (f.read((uint8_t*)&magic, sizeof(magic)) == sizeof(magic) && magic == LOG_MAGIC &&
      f.read(&n, 1) == 1 && n <= LOG_MAX &&
      f.read((uint8_t*)s_e, n * sizeof(LogEntry)) == (int)(n * sizeof(LogEntry))) {
    s_n = n;
  }
  f.close();
}

void evlogAdd(EventType ev, int printer, uint8_t flags, const char* detail) {
  if (s_n >= LOG_MAX) {
    memmove(s_e, s_e + 1, (LOG_MAX - 1) * sizeof(LogEntry));
    s_n = LOG_MAX - 1;
  }
  LogEntry& e = s_e[s_n++];
  memset(&e, 0, sizeof(e));
  time_t t = time(nullptr);
  e.ts = t > 1700000000 ? (uint32_t)t : 0;
  e.ev = (uint8_t)ev;
  e.printer = (uint8_t)(printer < 0 ? 0 : printer);
  e.flags = flags;
  if (detail) strlcpy(e.detail, detail, sizeof(e.detail));
  s_dirty = true;
  s_dirtyAt = millis();
}

void evlogTick(bool quiet) {
  if (!s_dirty || !quiet) return;                         // never write while something is ringing
  if (millis() - s_dirtyAt < 3000UL) return;             // let the alarm path settle first
  s_dirty = false;
  save();
}

void evlogFlushNow() {
  if (!s_dirty) return;
  s_dirty = false;
  save();
}

int evlogCount() { return s_n; }

const LogEntry& evlogGet(int i) {
  if (i < 0) i = 0;
  if (i >= s_n) i = s_n ? s_n - 1 : 0;
  return s_e[s_n ? s_n - 1 - i : 0];
}

void evlogClear() {
  s_n = 0;
  s_dirty = false;
  LittleFS.remove(LOG_FILE);
}
