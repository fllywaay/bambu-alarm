#include "audio.h"
#include "pins.h"
#include "crumb.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <math.h>

// Plays 16-bit PCM WAV (mono or stereo, any sample rate) through I2S.
// Runs in its own task so the main loop / web server never block audio.
//
// The sound files are small, so they are read into PSRAM while the device is idle and played from RAM. Playing an
// alarm then needs no flash / LittleFS access at all: an open() that fails because the internal heap is low (a TLS
// reconnect), or a read that stalls behind a web-page directory scan or a config save, can no longer cut the sound.
// A file that is not cached yet (or is too big / no PSRAM) is streamed from LittleFS like before.
// If a file cannot be played at all the player stays silent and keeps retrying for a few seconds, and only then beeps
// so a real alarm is never completely silent.

static volatile float s_gain = 0.f;
static volatile bool s_play = false;
static volatile bool s_loop = true;
static volatile uint32_t s_gen = 0;
static char s_file[56] = "";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_fallbacks = 0;          // how many times the real sound could not be played and the beep stood in
static const char* volatile s_why = "";            // why the last time: open / header / format / read
static char s_detail[96] = "";                     // which file and the numbers behind it, for the status JSON

static int16_t s_in[512];
static int16_t s_out[512 * 2];

static inline int16_t scaleS(int16_t s) { return (int16_t)((float)s * s_gain); }

static void i2sWrite(const int16_t* stereo, size_t frames) {
  size_t w = 0;
  i2s_write(I2S_NUM_0, stereo, frames * 4, &w, pdMS_TO_TICKS(200));
}

// ------------------------------------------------------------- sound cache ----
#define CACHE_SLOTS      8
#define CACHE_MAX_FILE   (1024u * 1024u)       // bigger files are streamed
#define CACHE_MAX_TOTAL  (3u * 1024u * 1024u)

struct Cached {
  char name[56];
  int16_t* pcm;          // samples as stored in the file (mono or interleaved stereo), in PSRAM
  uint32_t frames;
  uint16_t ch;
  uint32_t rate;
  uint32_t used;         // last-use tick, for eviction
};
static Cached s_cache[CACHE_SLOTS];
static uint32_t s_cacheTotal = 0;
static uint32_t s_useTick = 0;
static volatile bool s_inval = false;      // set by audioInvalidate() (file uploaded / deleted); handled by the audio task
static uint32_t s_nextPreload = 0;

static uint32_t cacheBytes(const Cached& c) { return c.frames * c.ch * 2u; }

static void cacheFree(int i) {
  if (!s_cache[i].pcm) return;
  s_cacheTotal -= cacheBytes(s_cache[i]);
  heap_caps_free(s_cache[i].pcm);
  memset(&s_cache[i], 0, sizeof(Cached));
}

static void cacheClear() {
  for (int i = 0; i < CACHE_SLOTS; i++) cacheFree(i);
}

static bool cacheEvictLru() {
  int lru = -1;
  for (int i = 0; i < CACHE_SLOTS; i++)
    if (s_cache[i].pcm && (lru < 0 || (int32_t)(s_cache[i].used - s_cache[lru].used) < 0)) lru = i;
  if (lru < 0) return false;
  cacheFree(lru);
  return true;
}

static Cached* cacheFind(const char* file) {
  for (int i = 0; i < CACHE_SLOTS; i++)
    if (s_cache[i].pcm && !strcmp(s_cache[i].name, file)) {
      s_cache[i].used = ++s_useTick;
      return &s_cache[i];
    }
  return nullptr;
}

// ---------------------------------------------------------------- WAV ----
struct WavInfo {
  uint16_t ch;
  uint16_t bits;
  uint32_t rate;
  uint32_t dataLen;
  uint32_t declared;
  size_t dataStart;
  uint32_t fileSize;
};

// Opens the file and reads the header; f is left open (the caller closes it). On failure sets s_why / s_detail.
static bool openWav(const char* file, File& f, WavInfo& w) {
  f = LittleFS.open(file, "r");
  crumb(2, 3);
  if (!f) { snprintf(s_detail, sizeof(s_detail), "%s", file); s_why = "open"; return false; }
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
    f.close();
    snprintf(s_detail, sizeof(s_detail), "%s", file);
    s_why = "header";
    return false;
  }

  w.ch = 1;
  w.bits = 16;
  w.rate = 16000;
  w.dataLen = 0;
  bool found = false;
  while (f.available() >= 8) {
    char id[4];
    uint32_t sz = 0;
    f.read((uint8_t*)id, 4);
    f.read((uint8_t*)&sz, 4);
    if (!memcmp(id, "fmt ", 4)) {
      uint8_t fm[16];
      f.read(fm, 16);
      w.ch = fm[2] | (fm[3] << 8);
      w.rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((uint32_t)fm[7] << 24);
      w.bits = fm[14] | (fm[15] << 8);
      if (sz > 16) f.seek(f.position() + sz - 16);
    } else if (!memcmp(id, "data", 4)) {
      w.dataLen = sz;
      found = true;
      break;
    } else {
      f.seek(f.position() + sz + (sz & 1));
    }
  }
  if (!found || w.bits != 16 || w.ch < 1 || w.ch > 2 || w.rate < 8000 || w.rate > 48000) {
    f.close();
    snprintf(s_detail, sizeof(s_detail), "%s rate=%u ch=%u bits=%u", file, (unsigned)w.rate, (unsigned)w.ch, (unsigned)w.bits);
    s_why = "format";
    return false;
  }

  w.dataStart = f.position();
  w.fileSize = (uint32_t)f.size();
  // Streamed / converted WAVs often carry a data length of 0 (or one bigger than the file): play what is really there.
  uint32_t fileLeft = (uint32_t)(w.fileSize > w.dataStart ? w.fileSize - w.dataStart : 0);
  w.declared = w.dataLen;
  if (w.dataLen == 0 || w.dataLen > fileLeft) w.dataLen = fileLeft;
  return true;
}

// Reads a whole sound into PSRAM. Returns nullptr when it cannot be cached (error, too big, no PSRAM): stream it then.
static Cached* cacheLoad(const char* file) {
  File f;
  WavInfo w;
  if (!openWav(file, f, w)) return nullptr;
  uint32_t frameBytes = (uint32_t)w.ch * 2u;
  uint32_t bytes = w.dataLen - (w.dataLen % frameBytes);
  if (bytes == 0) {
    f.close();
    snprintf(s_detail, sizeof(s_detail), "%s declared=%u size=%u empty", file, (unsigned)w.declared, (unsigned)w.fileSize);
    s_why = "read";
    return nullptr;
  }
  if (bytes > CACHE_MAX_FILE || strlen(file) >= sizeof(s_cache[0].name)) { f.close(); return nullptr; }

  while (s_cacheTotal + bytes > CACHE_MAX_TOTAL)
    if (!cacheEvictLru()) break;
  int slot = -1;
  for (int i = 0; i < CACHE_SLOTS; i++)
    if (!s_cache[i].pcm) { slot = i; break; }
  if (slot < 0 && cacheEvictLru())
    for (int i = 0; i < CACHE_SLOTS; i++)
      if (!s_cache[i].pcm) { slot = i; break; }
  if (slot < 0) { f.close(); return nullptr; }

  int16_t* buf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!buf) { f.close(); return nullptr; }

  uint32_t got = 0;
  while (got < bytes) {
    uint32_t want = bytes - got;
    if (want > 4096) want = 4096;
    int r = f.read((uint8_t*)buf + got, want);
    if (r <= 0) break;
    got += (uint32_t)r;
  }
  f.close();
  if (got != bytes) {
    heap_caps_free(buf);
    snprintf(s_detail, sizeof(s_detail), "%s cache read %u/%u", file, (unsigned)got, (unsigned)bytes);
    s_why = "read";
    return nullptr;
  }

  Cached& c = s_cache[slot];
  memset(&c, 0, sizeof(c));
  strlcpy(c.name, file, sizeof(c.name));
  c.pcm = buf;
  c.ch = w.ch;
  c.rate = w.rate;
  c.frames = bytes / frameBytes;
  c.used = ++s_useTick;
  s_cacheTotal += bytes;
  return &c;
}

// Loads every sound the settings use while nothing is ringing, so an alarm never has to touch flash.
static void preloadAll() {
  char names[12][48];
  int n = 0;
  cfgLock();
  for (int i = 0; i < EV_COUNT && n < 12; i++)
    if (cfg.rules[i].sound[0]) snprintf(names[n++], sizeof(names[0]), "/sounds/%s", cfg.rules[i].sound);
  if (cfg.noticeSound[0] && n < 12) snprintf(names[n++], sizeof(names[0]), "/sounds/%s", cfg.noticeSound);
  if (cfg.cancelSound[0] && n < 12) snprintf(names[n++], sizeof(names[0]), "/sounds/%s", cfg.cancelSound);
  if (n < 12) snprintf(names[n++], sizeof(names[0]), "/sounds/chime_soft.wav");   // power-on self-test
  cfgUnlock();
  for (int i = 0; i < n; i++) {
    if (s_play) return;                     // an alarm is starting: leave the rest for later
    if (!cacheFind(names[i])) cacheLoad(names[i]);
  }
}

// ----------------------------------------------------------- playback ----
// Plain beep (880 Hz, half a second period). maxMs 0 = until stopped.
static void playTone(uint32_t myGen, uint32_t maxMs) {
  crumb(2, 5);
  static int16_t buf[256 * 2];
  i2s_set_sample_rates(I2S_NUM_0, 16000);
  uint32_t n = 0;
  uint32_t maxN = maxMs * 16;                      // 16 samples per ms at 16 kHz
  while (s_play && s_gen == myGen && (maxN == 0 || n < maxN)) {
    for (int i = 0; i < 256; i++) {
      uint32_t pos = (n + i) % 8000;  // 0.5 s period
      int16_t v = 0;
      if (pos < 4000) v = (int16_t)(12000.0f * sinf(2.0f * (float)M_PI * 880.0f * (float)pos / 16000.0f));
      int16_t o = scaleS(v);
      buf[2 * i] = o;
      buf[2 * i + 1] = o;
    }
    n += 256;
    i2sWrite(buf, 256);
  }
}

static bool playCached(const Cached* c, uint32_t myGen) {
  i2s_set_sample_rates(I2S_NUM_0, c->rate);
  do {
    uint32_t pos = 0;
    while (pos < c->frames && s_play && s_gen == myGen) {
      crumb(2, 4);
      uint32_t n = c->frames - pos;
      if (n > 512) n = 512;
      if (c->ch == 1) {
        for (uint32_t i = 0; i < n; i++) {
          int16_t s = scaleS(c->pcm[pos + i]);
          s_out[2 * i] = s;
          s_out[2 * i + 1] = s;
        }
      } else {
        for (uint32_t i = 0; i < n; i++) {
          s_out[2 * i] = scaleS(c->pcm[2 * (pos + i)]);
          s_out[2 * i + 1] = scaleS(c->pcm[2 * (pos + i) + 1]);
        }
      }
      i2sWrite(s_out, n);
      pos += n;
    }
    vTaskDelay(1);                                // never hog the CPU between repeats
  } while (s_loop && s_play && s_gen == myGen);
  return true;
}

// Streams straight from LittleFS (fallback when the sound is not in the cache).
static bool playStream(const char* file, uint32_t myGen) {
  File f;
  WavInfo w;
  if (!openWav(file, f, w)) return false;
  i2s_set_sample_rates(I2S_NUM_0, w.rate);
  do {
    f.seek(w.dataStart);
    uint32_t remain = w.dataLen;
    uint32_t got = 0;
    while (remain && s_play && s_gen == myGen) {
      crumb(2, 4);
      size_t want = remain < sizeof(s_in) ? remain : sizeof(s_in);
      size_t bytes = f.read((uint8_t*)s_in, want);
      if (bytes == 0) break;
      remain -= bytes;
      got += bytes;
      size_t frames;
      if (w.ch == 1) {
        frames = bytes / 2;
        for (size_t i = 0; i < frames; i++) {
          int16_t s = scaleS(s_in[i]);
          s_out[2 * i] = s;
          s_out[2 * i + 1] = s;
        }
      } else {
        frames = bytes / 4;
        for (size_t i = 0; i < frames; i++) {
          s_out[2 * i] = scaleS(s_in[2 * i]);
          s_out[2 * i + 1] = scaleS(s_in[2 * i + 1]);
        }
      }
      if (frames) i2sWrite(s_out, frames);
    }
    if (got == 0 && s_play && s_gen == myGen) {
      snprintf(s_detail, sizeof(s_detail), "%s declared=%u size=%u start=%u rate=%u ch=%u", file, (unsigned)w.declared,
               (unsigned)w.fileSize, (unsigned)w.dataStart, (unsigned)w.rate, (unsigned)w.ch);
      f.close();
      s_why = "read";
      return false;
    }   // unreadable data: let the caller retry / beep
    vTaskDelay(1);                                // never hog the CPU between repeats
  } while (s_loop && s_play && s_gen == myGen);
  f.close();
  return true;
}

static bool playFile(const char* file, uint32_t myGen) {
  Cached* c = cacheFind(file);
  if (!c) c = cacheLoad(file);
  if (c) return playCached(c, myGen);
  return playStream(file, myGen);
}

static void audioTask(void*) {
  for (;;) {
    if (s_inval && !s_play) {                   // a sound file was uploaded / deleted: forget the cached copies
      s_inval = false;
      cacheClear();
      s_nextPreload = 0;
    }
    if (!s_play) {
      uint32_t now = millis();
      if (now > 1500 && (int32_t)(now - s_nextPreload) >= 0) {   // idle: keep every sound the settings use in RAM
        crumb(2, 7);
        preloadAll();
        s_nextPreload = millis() + 30000UL;      // also retries a sound that could not be loaded
      }
      crumb(2, 1);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    char file[sizeof(s_file)];
    uint32_t g;
    portENTER_CRITICAL(&s_mux);                 // take file name and generation together: audioPlay() may change them any time
    strlcpy(file, s_file, sizeof(file));
    g = s_gen;
    portEXIT_CRITICAL(&s_mux);
    crumb(2, 2);

    bool ok = false;
    for (int t = 0; t < 3 && !ok && s_play && s_gen == g; t++) {   // a failed open / read is usually a one-off: try again
      ok = playFile(file, g);
      if (!ok && t < 2) vTaskDelay(pdMS_TO_TICKS(40));
    }
    if (!ok && s_play && s_gen == g) {
      s_fallbacks++;
      Serial.printf("Audio: cannot play %s (%s), retrying quietly, beep only if it keeps failing\n", file, s_why);
      // Stay silent for the first seconds: a failed open is usually a short low-memory moment. Beep only if the sound
      // is still not playing after that, so a real alarm is never completely silent. Keep trying the real sound.
      uint32_t t0 = millis();
      while (s_play && s_gen == g) {
        if (playFile(file, g)) { Serial.println("Audio: sound file is playing again"); break; }
        if (millis() - t0 > 4000) {
          playTone(g, 1500);
        } else {
          vTaskDelay(pdMS_TO_TICKS(250));
        }
      }
    }
    if (g == s_gen && !s_loop) s_play = false;
    i2s_zero_dma_buffer(I2S_NUM_0);
    crumb(2, 6);
  }
}

void audioBegin() {
  i2s_config_t c;
  memset(&c, 0, sizeof(c));
  c.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  c.sample_rate = 16000;
  c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  c.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  c.dma_buf_count = 8;                         // a little more headroom while another task writes flash
  c.dma_buf_len = 256;
  c.use_apll = false;
  c.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_NUM_0, &c, 0, NULL);

  i2s_pin_config_t p;
  memset(&p, 0, sizeof(p));
  p.mck_io_num = I2S_PIN_NO_CHANGE;
  p.bck_io_num = PIN_I2S_BCLK;
  p.ws_io_num = PIN_I2S_LRC;
  p.data_out_num = PIN_I2S_DIN;
  p.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_NUM_0, &p);
  i2s_zero_dma_buffer(I2S_NUM_0);

  TaskHandle_t h = nullptr;
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 3, &h, 0);   // 6144 was tight for LittleFS + VFS calls
  crumbSetTask(2, h);
}

void audioPlay(const char* path, bool loop) {
  portENTER_CRITICAL(&s_mux);
  strlcpy(s_file, path, sizeof(s_file));
  s_loop = loop;
  s_gen++;
  s_play = true;
  portEXIT_CRITICAL(&s_mux);
}

void audioStop() {
  s_play = false;
  s_gen++;
}

void audioInvalidate() { s_inval = true; }

uint32_t audioFallbackCount() { return s_fallbacks; }
const char* audioFallbackReason() { return s_why; }
const char* audioFallbackDetail() { return s_detail; }

void audioSetGain(float g) {
  if (g < 0) g = 0;
  if (g > 1) g = 1;
  s_gain = g;
}
