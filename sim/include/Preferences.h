// NVS mock — a key/value store persisted to .sim_state/prefs.txt so settings,
// counters, reminders and the saved clock survive simulator restarts, like NVS.
#pragma once
#include <Arduino.h>
class Preferences {
public:
    bool begin(const char *name, bool readOnly = false);
    void end() {}
    bool clear();
    bool isKey(const char *key);
    size_t putInt(const char *k, int32_t v);
    int32_t getInt(const char *k, int32_t def = 0);
    size_t putUInt(const char *k, uint32_t v);
    uint32_t getUInt(const char *k, uint32_t def = 0);
    size_t putBool(const char *k, bool v);
    bool getBool(const char *k, bool def = false);
    size_t putUChar(const char *k, uint8_t v);
    uint8_t getUChar(const char *k, uint8_t def = 0);
    size_t putString(const char *k, const char *v);
    String getString(const char *k, const String &def = String());
};
