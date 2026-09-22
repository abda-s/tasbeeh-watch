// See sim_state.h. Moved out of sim_server.cpp unchanged so the native HTTP
// server and the browser/WASM bridge both apply actions and report state
// through this one implementation.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include <lvgl.h>
#include "ui/screens.h"
#include "sim_hal.h"
#include "sim_state.h"

#include <vector>
#include <unistd.h>   // unlink()

// firmware globals the control panel pokes (defined in src/main.cpp)
extern int  hour_, minute_, second_;
extern bool reminderFired[MAX_REMINDERS];
extern bool reminderActive;

static std::vector<std::string> pending;   // queued actions, applied at safe points

void sim_queue_action(const std::string &action) { pending.push_back(action); }

static std::string jesc(const std::string &s) {
    std::string r;
    for (unsigned char c : s) {
        if (c == '"') r += "\\\""; else if (c == '\\') r += "\\\\";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); r += b; }
        else r += (char)c;
    }
    return r;
}

static const char *screen_name() {
    if (!lv_display_get_default()) return "booting";
    lv_obj_t *s = lv_screen_active();
    if (s == scr_home) return "home";
    if (s == scr_istighfar) return "istighfar";
    if (s == scr_tasbeeh) return "tasbeeh";
    if (s == scr_settings) return "settings";
    if (s == scr_timeedit) return "timeedit";
    if (s == scr_notifications) return "notifications";
    if (s == scr_lockdown) return "lockdown";
    return "?";
}

std::string sim_build_state_json(uint32_t edge_since, uint32_t log_since) {
    char b[512];
    std::string j = "{";
    snprintf(b, sizeof b, "\"boot\":%d,\"t\":%lu,\"mode\":\"%s\",\"frame\":%u,\"bl\":%d,\"slpin\":%d,\"dispoff\":%d,\"battery\":%d,\"screen\":\"%s\",\"time\":\"%02d:%02d:%02d\",",
             sim.boot_id, millis(), sim.mode == SimState::AWAKE ? "awake" : sim.mode == SimState::LIGHT ? "light" : "deep",
             sim.frame, sim.bl_duty, sim.panel_slpin, sim.panel_dispoff, sim.battery_pct, screen_name(), hour_, minute_, second_);
    j += b;
    snprintf(b, sizeof b, "\"fromDeep\":%d,\"lastWake\":%d,\"vibEnabled\":%d,\"motor\":{\"pin\":%d,\"level\":%d,\"seq\":%u},",
             sim.from_deep, sim.last_wake, vibration_enabled, sim.motor_pin, sim.motor_level, sim.motor_seq);
    j += b;
    j += "\"edges\":[";
    bool first = true;
    for (auto &e : sim.edges) if (e.seq > edge_since) {
        snprintf(b, sizeof b, "%s[%u,%u,%d]", first ? "" : ",", e.seq, e.t, e.level); j += b; first = false;
    }
    j += "],\"log\":{";
    snprintf(b, sizeof b, "\"seq\":%u,\"lines\":[", sim.log_seq); j += b;
    // lines carry their own seq: line i has seq (log_seq - size + i + 1)
    uint32_t base = sim.log_seq - (uint32_t)sim.log.size();
    first = true;
    for (size_t i = 0; i < sim.log.size(); i++) if (base + i + 1 > log_since) {
        j += (first ? "\"" : ",\"") + jesc(sim.log[i]) + "\""; first = false;
    }
    j += "]},\"reminders\":[";
    for (int i = 0; i < MAX_REMINDERS; i++) {
        if (!reminders[i].label[0]) continue;
        snprintf(b, sizeof b, "%s{\"i\":%d,\"h\":%d,\"m\":%d,\"on\":%d,\"label\":\"", i && j.back() != '[' ? "," : "", i, reminders[i].hour, reminders[i].minute, reminders[i].enabled);
        j += b; j += jesc(reminders[i].label); j += "\"}";
    }
    j += "]}";
    return j;
}

void sim_apply_actions() {
    std::vector<std::string> acts; acts.swap(pending);
    for (auto &a : acts) {
        std::string k = a.substr(0, a.find('=')), v = a.substr(a.find('=') + 1);
        if (k == "fire") {
            int i = atoi(v.c_str());
            if (sim.mode == SimState::DEEP) { sim_log("[SIM] watch is in deep sleep — wake it first"); continue; }
            if (i < 0 || i >= MAX_REMINDERS || !reminders[i].label[0]) continue;
            if (reminderActive) { sim_log("[SIM] a reminder popup is already showing"); continue; }
            // make it due right now: enabled, every day, clock set to its time
            reminders[i].enabled = true; reminders[i].days = REMINDER_DAYS_ALL;
            reminderFired[i] = false;
            hour_ = reminders[i].hour; minute_ = reminders[i].minute; second_ = 0;
            resetClockTick();
            if (sim.mode == SimState::LIGHT) sim.wake_request = 1;          // timer wake -> checkReminders()
            char b[96]; snprintf(b, sizeof b, "[SIM] reminder %d due now (%02d:%02d)", i, hour_, minute_); sim_log(b);
        } else if (k == "buzz") {
            sim_log("[SIM] vibrate_notification() test");
            vibrate_notification();
        } else if (k == "reboot") {
            sim_log("[SIM] power cycle (RTC memory cleared)");
            sim_restart(0, false, true);
        } else if (k == "factory") {
            sim_log("[SIM] factory reset (NVS + RTC cleared)");
            unlink((sim_state_dir() + "/prefs.txt").c_str());
            sim_restart(0, false, true);
        }
    }
}
