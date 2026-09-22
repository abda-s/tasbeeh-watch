// The JS-facing API for the browser build. Same surface as sim_server.cpp's
// HTTP routes (/state, /frame, /touch, /ctl) — just exported as direct
// functions instead of parsed off a socket, and sharing the exact same
// underlying state/action code (sim_state.cpp) the native server uses, so
// neither front-end is a re-implementation of the other.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include "sim_hal.h"
#include "sim_state.h"
#include <emscripten.h>
#include <string>

extern "C" {

// Pixels: RGB565, 240x240, row-major, upright on-screen orientation — same
// layout /frame has always served. JS reads it straight out of wasm memory
// (HEAPU16 view over this pointer) instead of copying it over HTTP.
EMSCRIPTEN_KEEPALIVE
const uint16_t *sim_web_frame_ptr(void) { return sim.fb; }

EMSCRIPTEN_KEEPALIVE
uint32_t sim_web_frame_seq(void) { return sim.frame; }

// Same JSON shape /state has always returned. The returned pointer is only
// valid until the next call — JS copies it out (UTF8ToString) immediately,
// same as it would read an HTTP response body.
EMSCRIPTEN_KEEPALIVE
const char *sim_web_state_json(uint32_t edges_since, uint32_t log_since) {
    static std::string s;   // keeps the buffer alive after return
    s = sim_build_state_json(edges_since, log_since);
    return s.c_str();
}

EMSCRIPTEN_KEEPALIVE
void sim_web_touch(int x, int y, int down) {
    sim.touch_down = down != 0;
    sim.touch_x = x;
    sim.touch_y = y;
}

EMSCRIPTEN_KEEPALIVE
void sim_web_set_battery(int pct) { sim.battery_pct = constrain(pct, 0, 100); }

// mode: 1 = timer wake, 2 = touch wake — same as /ctl?wake=timer|touch.
EMSCRIPTEN_KEEPALIVE
void sim_web_wake(int mode) { sim.wake_request = mode; }

EMSCRIPTEN_KEEPALIVE
void sim_web_fire(int idx) { sim_queue_action("fire=" + std::to_string(idx)); }

EMSCRIPTEN_KEEPALIVE
void sim_web_set_time(int hour, int minute) {
    sim_queue_action("settime=" + std::to_string(hour) + ":" + std::to_string(minute));
}

EMSCRIPTEN_KEEPALIVE
void sim_web_set_reminder(int idx, int hour, int minute, int enabled) {
    sim_queue_action("setrem=" + std::to_string(idx) + ":" + std::to_string(hour) + ":" +
                      std::to_string(minute) + ":" + std::to_string(enabled));
}

EMSCRIPTEN_KEEPALIVE
void sim_web_buzz(void) { sim_queue_action("buzz=1"); }

EMSCRIPTEN_KEEPALIVE
void sim_web_reboot(void) { sim_queue_action("reboot=1"); }

EMSCRIPTEN_KEEPALIVE
void sim_web_factory(void) { sim_queue_action("factory=1"); }

}  // extern "C"
