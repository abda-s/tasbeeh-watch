// TFT_eSPI mock — captures pixels into the simulator's framebuffer instead of
// talking SPI to a GC9A01A. Rotation/sleep commands are honoured so the
// picture (and "panel off") match what the real panel would show.
#pragma once
#include <Arduino.h>
#define TFT_BL 2
#define TFT_BLACK 0x0000
class TFT_eSPI {
public:
    TFT_eSPI(int16_t w = 240, int16_t h = 240) {}
    void begin();
    void setRotation(uint8_t r);
    void fillScreen(uint32_t color);
    void writecommand(uint8_t cmd);
    void startWrite() {}
    void endWrite() {}
    void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
    void pushColors(uint16_t *data, uint32_t len, bool swap = true);
};
