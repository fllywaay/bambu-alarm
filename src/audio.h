#pragma once
#include <stdint.h>
void audioBegin();
void audioPlay(const char* path, bool loop);
void audioStop();
void audioSetGain(float g);   // 0..1

// Sounds are cached in PSRAM while idle. Call this after a file in /sounds was uploaded, replaced or deleted so the
// cached copies are dropped and loaded again.
void audioInvalidate();

// When a sound file cannot be opened / read the player beeps instead and keeps retrying the file. These tell how often
// that happened since boot and why the last time (open / header / format / read), shown in the status JSON.
uint32_t audioFallbackCount();
const char* audioFallbackReason();
const char* audioFallbackDetail();   // file name and the numbers behind the last failure
