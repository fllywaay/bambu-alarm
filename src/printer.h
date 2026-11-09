#pragma once
#include <Arduino.h>
#include "config.h"

struct Discovered { char ip[16]; char serial[24]; char model[24]; char name[32]; uint32_t lastSeen; };
// AMS filament info (up to 4 AMS units x 4 trays, plus 2 external spools)
#define MAX_AMS 4
struct AmsTrayState { uint32_t rgb; char type[8]; int8_t remain; };      // type[0] == 0: nothing known
struct AmsUnitState {
  bool present;
  int8_t extruder;        // nozzle id this AMS feeds on a dual-nozzle printer (from the AMS info bits), -1 unknown
  int8_t humidity;        // percent, -1 unknown
  float temp;
  AmsTrayState tray[4];
};

struct PrinterState { bool connected; bool gotReport; uint32_t lastReport; char gcode[12]; int percent; int err; int hms; uint32_t since;
  uint8_t has;            // bit0 legacy nozzle, bit1 bed, bit2 chamber, bit3 remaining time, bit4 layers,
                          // bit5 active nozzle known, bit6 bed from device block, bit7 chamber from device block
  float nozzle, bed, chamber;
  int remainMin, layer, totalLayer;
  char job[40];           // current job name
  // Dual-nozzle (H2D) and device-level values. Encoded temps are: low 16 bits = current, high 16 bits = target.
  int bedT, chamberT;     // target temps (0 = off / unknown)
  int nozCur[2], nozTgt[2];
  uint8_t nozMask;        // bit k = nozzle k reported
  int8_t active;          // active nozzle index (valid when has bit5 is set)
  AmsUnitState ams[MAX_AMS];
  uint32_t trayBits;      // tray_exist_bits: bit (amsId*4 + slot)
  int trayNow;            // ams.tray_now (255 = none). On dual-nozzle printers this is only the slot number, NOT amsId*4+slot
  int snow[2];            // device.extruder.info[k].snow: filament in use by nozzle k = (amsId << 8) | slot, 0xFFFF = none
  uint8_t snowMask;       // bit k = snow[k] reported
  AmsTrayState ext[2];    // external spools (vir_slot, in report order)
  int extId[2];
  uint8_t extMask;
  uint32_t jobStartAt;    // millis() when a new print started (state went to PREPARE / RUNNING from FAILED / FINISH / IDLE), 0 = never
};

void printersBegin();                       // starts the network task (SSDP + MQTT)
void printersReload();                      // call after config change
const PrinterState& printerState(int idx);
int ssdpCount();
const Discovered* ssdpGet(int i);

// Next event from the network task. 'ignored' = filtered out (severity / custom code rules): only log it.
// 'detail' receives an error code in hex (may be empty).
bool printersNextEvent(EventType* ev, int* idx, bool* ignored, char* detail, size_t detailLen);

// Does the reason for this kind of alarm still exist? (used for snooze and auto-clear)
bool printersEventStillActive(EventType ev);

// Has a new print started on this printer after the given millis() time? (a ringing failed / error / finish alarm is
// acknowledged by that)
bool printerJobStartedSince(int idx, uint32_t since);

// Raw MQTT payloads, kept in PSRAM for the web debug page. kind 0 = latest message, 1 = latest big (full) snapshot.
// Returns nullptr when nothing has been received yet.
const uint8_t* printersRawGet(int idx, int kind, size_t* len, uint32_t* ageSec);
