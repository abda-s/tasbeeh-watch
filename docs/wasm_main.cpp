// Browser entry point: runs the firmware's own setup()/loop() unmodified —
// same shape as sim/sim_main.cpp's native entry point, minus the HTTP
// server (the page talks to this module directly through wasm_bridge.cpp's
// exported functions, not by polling a socket).
//
// This can be a plain infinite loop, exactly like the native version,
// because Asyncify (build.sh's -sASYNCIFY=1) is what lets it coexist with a
// browser tab instead of freezing it: every delay() and every light/deep
// sleep busy-loop call emscripten_sleep() (see sim/sim_hal.cpp), which
// unwinds back to the browser's event loop and resumes right here on the
// next tick. Nothing about main() itself needs to know that's happening.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include "sim_hal.h"
#include "sim_state.h"

void setup();
void loop();

// No sockets in this build — see wasm_bridge.cpp for how the page drives
// touch/battery/reminders/etc. instead of HTTP polling.
void sim_pump() {}

int main() {
    sim_init(0, nullptr);
    setup();
    for (;;) {
        sim_apply_actions();
        loop();
    }
}
