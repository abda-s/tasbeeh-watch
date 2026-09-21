// CST816S touch mock — "touched" while the mouse button is held in the browser.
#pragma once
#include <Arduino.h>
struct data_struct {
    byte gestureID; byte points; byte event;
    int x; int y;
    uint8_t version; uint8_t versionInfo[3];
};
class CST816S {
public:
    CST816S(int sda, int scl, int rst, int irq) {}
    void begin(int interrupt = RISING) {}
    void rearm(int interrupt = RISING) {}
    void sleep() {}
    bool available();
    data_struct data{};
    String gesture() { return String("NONE"); }
};
