#pragma once
#include <stdint.h>
void clockBegin();
void clockLoop();   // call every loop; redraws at most every 100 ms and only when something changed
void clockShowProgress(uint32_t ms);   // show print progress on the digits for ms (extended while called, e.g. while a button is held)
void clockShow(const uint8_t seg[4], bool on);   // show raw segments (self-test)
