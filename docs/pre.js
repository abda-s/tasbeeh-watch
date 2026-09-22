// Mounts an IndexedDB-backed filesystem at /sim_state before main() runs, so
// the firmware's ordinary fopen()/fwrite() calls (NVS prefs, RTC bytes —
// unchanged code, see sim/sim_hal.cpp) persist across reloads exactly like
// files on disk do for the native simulator. addRunDependency/
// removeRunDependency makes Emscripten wait for the initial (async)
// IndexedDB read to finish before main() starts, so nothing races it.
if (typeof Module === "undefined") var Module = {};
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    FS.mkdir("/sim_state");
    FS.mount(IDBFS, { autoPersist: true }, "/sim_state");
    addRunDependency("sim-idbfs-sync");
    FS.syncfs(true, function () {
        removeRunDependency("sim-idbfs-sync");
    });
});
