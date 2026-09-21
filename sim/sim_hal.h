// State shared between the mock hardware layer (sim_hal.cpp), the web server
// (sim_server.cpp) and the entry point (sim_main.cpp).
#pragma once
#include <stdint.h>
#include <deque>
#include <string>

struct MotorEdge { uint32_t seq; uint32_t t; int level; };

struct SimState {
    // ── display ──────────────────────────────────────────────
    uint16_t fb[240 * 240];          // RGB565, in on-screen (upright) orientation
    uint32_t frame = 1;              // bumps whenever pixels change
    bool     panel_slpin = false;    // GC9A01A SLPIN (0x10) / SLPOUT (0x11)
    bool     panel_dispoff = false;  // DISPOFF (0x28) / DISPON (0x29)
    uint8_t  tft_rot = 0;
    int      win_x = 0, win_y = 0, win_w = 240, win_h = 240;
    int      bl_duty = 0;            // backlight PWM duty, 0..255
    bool     bl_pwm = false;
    // ── vibration motor (whatever OUTPUT pin isn't the backlight) ──
    int      motor_pin = -1;
    bool     motor_level = false;
    uint32_t motor_seq = 0;
    std::deque<MotorEdge> edges;
    // ── inputs / environment ─────────────────────────────────
    int      battery_pct = 80;
    bool     touch_down = false;
    int      touch_x = 0, touch_y = 0;   // on-screen coordinates, 0..239
    // ── sleep ────────────────────────────────────────────────
    enum Mode { AWAKE = 0, LIGHT = 1, DEEP = 2 } mode = AWAKE;
    uint64_t timer_us = 0;
    bool     ext0_armed = false, gpio_wake_armed = false;
    int      wake_request = 0;       // 0 none, 1 timer, 2 touch (from the web UI)
    int      last_wake = 0;          // esp_sleep_source_t of the most recent wake
    // ── boot ─────────────────────────────────────────────────
    int      boot_id = 1;
    bool     from_deep = false;
    int      env_wake = 0;
    // ── serial log ring ──────────────────────────────────────
    std::deque<std::string> log;
    uint32_t log_seq = 0;
};
extern SimState sim;

void sim_init(int argc, char **argv);
void sim_pump();             // poll the web server; cheap, safe to call anywhere
void sim_apply_actions();    // apply queued UI actions; only at safe points
void sim_log(const char *line);
std::string sim_root();      // project root (where platformio.ini lives)
std::string sim_state_dir();
[[noreturn]] void sim_restart(int wake_cause, bool deep_sleep, bool clear_rtc);

bool sim_server_start(int port);
void sim_server_poll();
