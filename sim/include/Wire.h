// I2C mock — the only I2C the firmware does directly is powering down the IMU.
#pragma once
#include <Arduino.h>
class TwoWire {
public:
    void begin(int = 0, int = 0) {}
    void beginTransmission(int) {}
    size_t write(uint8_t) { return 1; }
    uint8_t endTransmission() { return 0; }
};
extern TwoWire Wire;
