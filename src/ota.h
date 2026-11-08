#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Firmware version shown on the web page and compared with the one on GitHub (release tag "v1.2.3" -> 1.2.3).
// The release workflow (.github/workflows/release.yml) overrides this with the tag; a local build is a dev build.
#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

// Two ways to update the firmware over Wi-Fi (the filesystem partition, i.e. all settings, is never touched):
//  1. Web upload: Settings > Firmware > pick firmware.bin  (POST /api/ota, multipart)
//  2. From GitHub: the device reads version.json + firmware.bin from the latest release of the repo set in
//     Settings > Firmware (or from any https/http folder you host yourself).
// The new image is written to the OTA slot that is not running; the device only switches to it after the whole
// image was received and checked, so a failed transfer leaves the running firmware as it was.

// --- GitHub / URL update (runs in its own task, poll otaStatus()) ---
bool otaCheckStart();      // read version.json; false = busy / not possible (see otaStatus "err")
bool otaInstallStart();    // download firmware.bin of the version found by the last check; false = not possible

// --- web upload (called from the web server's upload handler) ---
bool otaUploadBegin();
bool otaUploadWrite(const uint8_t* buf, size_t len);
bool otaUploadEnd();       // true = image complete and accepted, device reboots shortly
void otaUploadAbort();

void otaStatus(JsonObject o);     // version, state, progress, last error

// Automatic rollback. A freshly installed image boots as "pending verify": if the device resets before the
// firmware confirms itself, the bootloader switches back to the previous image on its own. We confirm after the
// main loop has run for a minute (otaLoop, called from loop()), so a firmware that crashes or boot-loops within
// that minute is dropped automatically. (The Arduino core would otherwise confirm it at once, before setup().)
void otaLoop();
