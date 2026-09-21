// Minimal Arduino/ESP32 shim — just enough for the real firmware to compile
// and run on a desktop. Implementations live in sim/sim_hal.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define RISING 1
#define FALLING 2
#define CHANGE 3
#define IRAM_ATTR
// Real ESP32: RTC memory survives deep sleep. Simulator: a named section whose
// bytes sim_hal.cpp saves/restores around the process restart that stands in
// for a deep-sleep reboot.
#define RTC_DATA_ATTR __attribute__((section("rtc_data")))
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
typedef uint8_t byte;
enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db };

// unsigned long matches real Arduino-ESP32 so the firmware's "%lu" formats are right
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);          // also services the simulator's web server
void delayMicroseconds(unsigned int us);

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int  digitalRead(uint8_t pin);
void analogReadResolution(uint8_t bits);
void analogSetPinAttenuation(uint8_t pin, int atten);
uint32_t analogReadMilliVolts(uint8_t pin);
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);
bool ledcWrite(uint8_t pin, uint32_t duty);
bool ledcDetach(uint8_t pin);
bool setCpuFrequencyMhz(uint32_t mhz);
void attachInterrupt(uint8_t pin, void (*fn)(void), int mode);
void detachInterrupt(uint8_t pin);

// Real settimeofday() needs root and gettimeofday() can't model "RTC keeps
// counting through deep sleep" — route both through the simulator's clock.
int sim_gettimeofday(struct timeval *tv, void *tz);
int sim_settimeofday(const struct timeval *tv, const void *tz);
#define gettimeofday sim_gettimeofday
#define settimeofday sim_settimeofday

class String {
    std::string s;
public:
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(int v) : s(std::to_string(v)) {}
    String(unsigned v) : s(std::to_string(v)) {}
    String(long v) : s(std::to_string(v)) {}
    String(unsigned long v) : s(std::to_string(v)) {}
    const char *c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    String operator+(const String &o) const { String r; r.s = s + o.s; return r; }
    String operator+(const char *c) const { String r; r.s = s + (c ? c : ""); return r; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(const char *c) { s += (c ? c : ""); return *this; }
    bool operator==(const String &o) const { return s == o.s; }
    friend String operator+(const char *a, const String &b) { String r; r.s = std::string(a ? a : "") + b.s; return r; }
};

struct SimSerial {
    void begin(unsigned long) {}
    void flush();
    size_t print(const char *s);
    size_t print(int v);
    size_t println(const char *s = "");
    size_t println(const String &s);
    size_t println(int v);
    size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
};
extern SimSerial Serial;
