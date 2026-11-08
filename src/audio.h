#pragma once
void audioBegin();
void audioPlay(const char* path, bool loop);
void audioStop();
void audioSetGain(float g);   // 0..1
