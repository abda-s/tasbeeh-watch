// Shared between every front-end that drives the simulated hardware (the
// native HTTP server in sim_server.cpp, and the browser/WASM bridge in
// docs/wasm_bridge.cpp): the JSON state snapshot and the queued-action
// mechanism. Kept in one place so both front-ends apply actions and report
// state identically — neither is a reimplementation of the other.
#pragma once
#include <string>

// Same JSON shape sim_server.cpp's /state endpoint has always returned.
// edge_since/log_since: only edges/log lines newer than these seq numbers
// are included (0 = everything).
std::string sim_build_state_json(uint32_t edge_since, uint32_t log_since);

// Queue an action for sim_apply_actions() to perform at the next safe point
// (main loop top, or a sleep loop's pump). Same strings /ctl has always
// accepted: "fire=<idx>", "buzz=1", "reboot=1", "factory=1".
void sim_queue_action(const std::string &action);

// Applies every queued action, then clears the queue. Called from the main
// loop and from inside the light/deep sleep pump loops (sim_hal.cpp).
void sim_apply_actions();
