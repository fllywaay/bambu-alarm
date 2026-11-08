#pragma once
// ESP32-S3 N16R8 pin map.
// Do NOT use GPIO26-37 (flash/PSRAM), GPIO19/20 (USB), GPIO0/3/45/46 (strapping).
#define PIN_BUZZER     4    // -> 1k -> MOSFET gate (100k pulldown to GND)
#define PIN_I2S_BCLK   5    // MAX98357A BCLK
#define PIN_I2S_LRC    6    // MAX98357A LRC
#define PIN_I2S_DIN    7    // MAX98357A DIN
#define PIN_TM_CLK     8    // TM1637 clock-display CLK
#define PIN_TM_DIO     9    // TM1637 clock-display DIO
#define PIN_BTN_ACK    15   // stop / snooze button (other side to GND)
#define PIN_BTN_SETUP  14   // setup-mode button (other side to GND)
#define PIN_LED        16   // WS2812 data (330R in series)
#define LED_COUNT      12
#define BUZZER_ACTIVE  0    // 0 = passive piezo (PWM tone), 1 = active buzzer (DC on/off)
#define LED_IDLE_LEVEL 12   // 0-255, brightness of the idle status pixel
