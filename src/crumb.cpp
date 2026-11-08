#include "crumb.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// New magic value: the RTC layout changed (telemetry added), so data left by an older build is ignored once.
#define CRUMB_MAGIC 0xC0DE1235UL

static RTC_NOINIT_ATTR uint32_t s_magic;
static RTC_NOINIT_ATTR uint8_t s_c[4];
static RTC_NOINIT_ATTR CrumbTelemetry s_tel;
static uint32_t s_prev = 0;
static CrumbTelemetry s_last;
static TaskHandle_t s_task[4] = {nullptr, nullptr, nullptr, nullptr};
static uint32_t s_minBlk = 0xFFFFFFFFUL;
static uint32_t s_stk[4] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};

void crumbBegin() {
  memset(&s_last, 0, sizeof(s_last));
  if (s_magic == CRUMB_MAGIC) {
    s_prev = (uint32_t)s_c[0] | ((uint32_t)s_c[1] << 8) | ((uint32_t)s_c[2] << 16) | ((uint32_t)s_c[3] << 24);
    s_last = s_tel;
  }
  s_magic = CRUMB_MAGIC;
  for (int i = 0; i < 4; i++) s_c[i] = 0;
  memset(&s_tel, 0, sizeof(s_tel));
}

void crumb(int slot, uint8_t id) {
  if (slot >= 0 && slot < 4) s_c[slot] = id;
}

uint32_t crumbPrev() { return s_prev; }

void crumbSetTask(int slot, void* taskHandle) {
  if (slot >= 1 && slot < 4) s_task[slot] = (TaskHandle_t)taskHandle;
}

void crumbSample(bool ringing) {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < (ringing ? 250UL : 2000UL)) return;
  last = now;

  uint32_t fh = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  uint32_t mb = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  if (mb < s_minBlk) s_minBlk = mb;
  for (int i = 0; i < 4; i++) {
    TaskHandle_t h = (i == 0) ? nullptr : s_task[i];      // nullptr = the calling task = the Arduino loop task
    if (i != 0 && !h) continue;
    uint32_t w = (uint32_t)uxTaskGetStackHighWaterMark(h) * (uint32_t)sizeof(StackType_t);
    if (w < s_stk[i]) s_stk[i] = w;
  }
  s_tel.freeHeap = fh;
  s_tel.maxBlock = mb;
  s_tel.minHeap = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  s_tel.minBlock = s_minBlk;
  for (int i = 0; i < 4; i++) s_tel.stackFree[i] = (s_stk[i] == 0xFFFFFFFFUL) ? 0 : s_stk[i];
  s_tel.ringing = ringing ? 1 : 0;
  s_tel.upSec = now / 1000;
}

CrumbTelemetry crumbNow() { return s_tel; }
CrumbTelemetry crumbLast() { return s_last; }

bool heapHealthy(uint32_t minFree, uint32_t minBlock) {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= minFree &&
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= minBlock;
}
