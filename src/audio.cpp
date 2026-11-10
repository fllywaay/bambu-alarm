#include "audio.h"
#include "pins.h"
#include "crumb.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <math.h>

// Plays 16-bit PCM WAV (mono or stereo, any sample rate) from LittleFS through I2S.
// Runs in its own task so the main loop / web server never block audio.
// If the file is missing or unreadable, falls back to a plain beep so you still hear *something*.

static volatile float s_gain = 0.f;
static volatile bool s_play = false;
static volatile bool s_loop = true;
static volatile uint32_t s_gen = 0;
static char s_file[56] = "";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_fallbacks = 0;          // how many times the real sound could not be played and the beep stood in
static const char* volatile s_why = "";            // why the last time: open / header / format / read

static inline int16_t scaleS(int16_t s) { return (int16_t)((float)s * s_gain); }

static void i2sWrite(const int16_t* stereo, size_t frames) {
  size_t w = 0;
  i2s_write(I2S_NUM_0, stereo, frames * 4, &w, pdMS_TO_TICKS(200));
}

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

static bool playFile(const char* file, uint32_t myGen) {
  File f = LittleFS.open(file, "r");
  crumb(2, 3);
  if (!f) { s_why = "open"; return false; }
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) { f.close(); s_why = "header"; return false; }

  uint16_t ch = 1, bits = 16;
  uint32_t rate = 16000, dataLen = 0;
  bool found = false;
  while (f.available() >= 8) {
    char id[4];
    uint32_t sz = 0;
    f.read((uint8_t*)id, 4);
    f.read((uint8_t*)&sz, 4);
    if (!memcmp(id, "fmt ", 4)) {
      uint8_t fm[16];
      f.read(fm, 16);
      ch = fm[2] | (fm[3] << 8);
      rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | ((uint32_t)fm[7] << 24);
      bits = fm[14] | (fm[15] << 8);
      if (sz > 16) f.seek(f.position() + sz - 16);
    } else if (!memcmp(id, "data", 4)) {
      dataLen = sz;
      found = true;
      break;
    } else {
      f.seek(f.position() + sz + (sz & 1));
    }
  }
  if (!found || bits != 16 || ch < 1 || ch > 2 || rate < 8000 || rate > 48000) { f.close(); s_why = "format"; return false; }

  size_t dataStart = f.position();
  i2s_set_sample_rates(I2S_NUM_0, rate);
  static int16_t in[512];
  static int16_t out[512 * 2];
  do {
    f.seek(dataStart);
    uint32_t remain = dataLen;
    uint32_t got = 0;
    while (remain && s_play && s_gen == myGen) {
      crumb(2, 4);
      size_t want = remain < sizeof(in) ? remain : sizeof(in);
      size_t bytes = f.read((uint8_t*)in, want);
      if (bytes == 0) break;
      remain -= bytes;
      got += bytes;
      size_t frames;
      if (ch == 1) {
        frames = bytes / 2;
        for (size_t i = 0; i < frames; i++) {
          int16_t s = scaleS(in[i]);
          out[2 * i] = s;
          out[2 * i + 1] = s;
        }
      } else {
        frames = bytes / 4;
        for (size_t i = 0; i < frames; i++) {
          out[2 * i] = scaleS(in[2 * i]);
          out[2 * i + 1] = scaleS(in[2 * i + 1]);
        }
      }
      if (frames) i2sWrite(out, frames);
    }
    if (got == 0 && s_play && s_gen == myGen) { f.close(); s_why = "read"; return false; }   // unreadable data: let the caller retry / beep
    vTaskDelay(1);                                // never hog the CPU between repeats
  } while (s_loop && s_play && s_gen == myGen);
  f.close();
  return true;
}

static void audioTask(void*) {
  for (;;) {
    if (!s_play) { crumb(2, 1); vTaskDelay(pdMS_TO_TICKS(20)); continue; }
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
      Serial.printf("Audio: cannot play %s (%s), beeping and retrying the file\n", file, s_why);
      // Beep so the alarm is not silent, but keep trying the real sound and go back to it as soon as it works.
      while (s_play && s_gen == g) {
        playTone(g, 1500);
        if (!(s_play && s_gen == g)) break;
        if (playFile(file, g)) { Serial.println("Audio: sound file is playing again"); break; }
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
  c.dma_buf_count = 6;
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

uint32_t audioFallbackCount() { return s_fallbacks; }
const char* audioFallbackReason() { return s_why; }

void audioSetGain(float g) {
  if (g < 0) g = 0;
  if (g > 1) g = 1;
  s_gain = g;
}
