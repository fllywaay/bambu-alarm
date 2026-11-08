#pragma once
#include <Arduino.h>

// Official Bambu error texts (from Bambu Studio's open-source HMS data), packed in LittleFS by tools/build_hms.py.
// One file per language: /hms_zh.bin and /hms_en.bin (an old single /hms.bin counts as the Chinese one).
// Lookup runs straight from flash with a binary search: no RAM table. The language follows cfg.lang; if that
// language's file is not installed, the other one is used so a code still gets a description.
void hmsDbBegin();                 // call once after LittleFS.begin()
int  hmsDbCount(int lang = -1);    // number of codes in the file of that language (0 = ZH, 1 = EN), -1 = current cfg.lang; 0 = not installed

// normCode = upper-case hex digits as used everywhere else in the firmware:
//   8 digits  = print_error (e.g. 0500808F)
//   16 digits = HMS entry, attr+code (e.g. 0300010000010007)
// Copies the official text into out (cut at a UTF-8 character boundary if it does not fit).
// Returns false when there is no database or no text for that code (out is then empty).
bool hmsDbLookup(const char* normCode, char* out, size_t n);

// Like hmsDbLookup, but a 16-digit HMS code that is not in the official table still gets a meaning decoded from its
// own digits (module and alert level, as documented on the Bambu wiki), e.g. "Not in the official list · Motion
// Controller (MC) · Warning: print pauses". Returns false only when there is nothing to say (8-digit codes not in the table).
bool hmsDbDescribe(const char* normCode, char* out, size_t n);

// Check an uploaded database file and make it the active one for that language (0 = ZH, 1 = EN).
// Deletes the file when invalid.
bool hmsDbInstall(const char* tmpPath, int lang);
