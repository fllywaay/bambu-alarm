#pragma once
#include <stdint.h>

// "Breadcrumbs" for hunting resets. Each task leaves a small number saying what it is doing. The numbers live in
// RTC memory, which survives a watchdog reset, so after a reboot the status page can show what every task was doing
// right before it happened.
//   slot 0 = main loop, 1 = network task, 2 = audio task, 3 = push-notification task
void crumbBegin();                   // first thing in setup(): keeps what the previous run was doing
void crumb(int slot, uint8_t id);
uint32_t crumbPrev();                // 4 bytes, one per slot (slot 0 = lowest byte); 0 = unknown / power-on

// Heap / stack telemetry. Sampled into RTC memory (survives a crash) so that after a reboot the status page can show
// how much internal RAM / stack the previous run had left right before it died.
struct CrumbTelemetry {
  uint32_t freeHeap;       // free internal RAM at the last sample
  uint32_t maxBlock;       // largest free internal block at the last sample
  uint32_t minHeap;        // lowest free internal RAM since boot
  uint32_t minBlock;       // lowest "largest free block" seen since boot
  uint32_t stackFree[4];   // smallest stack headroom in bytes: [0] loop task, [1] net, [2] audio, [3] notify
  uint32_t ringing;        // 1 = an alarm was ringing at the last sample
  uint32_t upSec;          // uptime at the last sample
};

void crumbSetTask(int slot, void* taskHandle);   // slot 1..3: register a task so its stack headroom can be measured
void crumbSample(bool ringing);                  // call from loop(): samples every 2 s (every 250 ms while ringing)
CrumbTelemetry crumbNow();                       // this run
CrumbTelemetry crumbLast();                      // what the previous run left behind (all zero = nothing)

// True when internal RAM is not under pressure: at least minFree bytes free AND a free block of minBlock bytes.
bool heapHealthy(uint32_t minFree, uint32_t minBlock);
