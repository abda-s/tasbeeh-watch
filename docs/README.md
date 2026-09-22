# Browser Simulator

> The real firmware, compiled to WebAssembly, running entirely client-side — no server, no install.
>
> [← Documentation index](../documentation/README.md) · [Project README](../README.md)

---

**Live:** https://abda-s.github.io/tasbeeh-watch/ *(once GitHub Pages is enabled — see below)*

This is a second build of the exact same firmware the [desktop simulator](../sim/README.md) runs — same
`src/*` unmodified, same `sim/sim_hal.cpp` mock hardware layer, same `sim/sim_state.cpp` state/action
logic — just compiled with [Emscripten](https://emscripten.org/) instead of a native `g++`, so the whole
thing runs inside a browser tab with nothing installed and nothing uploaded anywhere. It's meant for
showing the project to someone who isn't going to `git clone` and run PlatformIO — send them the link.

## Why this needed its own build (not just "compile the same thing differently")

The native simulator (`sim/`) runs as a desktop process: an HTTP server on one thread, the firmware's
`setup()`/`loop()` blocking on real OS sleep calls on the same thread between requests. None of that
translates directly to a browser tab, which has exactly one thread and no blocking:

- **The sleep loops.** `esp_light_sleep_start()`/`esp_deep_sleep_start()` are emulated as busy-loops
  (see `sim/sim_hal.cpp`) that block until a wake condition is met. Blocking the browser's only thread
  freezes the tab. Fixed with **Asyncify** (`-sASYNCIFY=1` in `build.sh`): the same loop, but
  `usleep()` is swapped for `emscripten_sleep()` under `#ifdef __EMSCRIPTEN__`, which unwinds the call
  stack back to the browser's event loop and resumes right where it left off.
- **Persistence.** NVS prefs and the RTC-memory bytes are ordinary `fopen()`/`fwrite()` calls
  (unchanged). In the browser, `sim_state_dir()` points at `/sim_state`, which `pre.js` mounts as an
  `IDBFS` (IndexedDB-backed) filesystem before `main()` runs — the C code doesn't know the difference.
- **Deep sleep / reboot.** The native build restarts the whole process (`execv`) to stand in for the
  reset a real deep-sleep wake causes. A browser tab can't `execv`; `sim_restart()` calls
  `location.reload()` instead, with the three tiny "boot flags" env vars used to carry (deep-wake
  cause, boot id, battery %) going through `localStorage` so they survive the reload.
- **No sockets.** `wasm_bridge.cpp` exports the same operations the native `/state`, `/frame`,
  `/touch` and `/ctl` HTTP routes provide, as plain C functions (`sim_web_state_json()`,
  `sim_web_touch()`, `sim_web_fire()`, …) that `app.js` calls directly through Emscripten's `ccall`.
  Both front ends share the exact same underlying implementation in `sim/sim_state.cpp` — neither is a
  reimplementation of the other.

## Files

| File | What it is |
|---|---|
| `wasm_main.cpp` | Entry point — `setup()` then an infinite `loop()`, same shape as `sim/sim_main.cpp` |
| `wasm_bridge.cpp` | The JS-facing API: frame buffer pointer, state JSON, touch/battery/reminder controls |
| `pre.js` | Mounts the IDBFS persistence layer before `main()` runs |
| `build.sh` | Compiles everything with Emscripten; run this after changing firmware or simulator source |
| `index.html` / `style.css` / `app.js` | The page itself |
| `watch.js` / `watch.wasm` | Build output — **committed**, so GitHub Pages needs no build step |

## Rebuilding

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/.emsdk
~/.emsdk/emsdk install latest && ~/.emsdk/emsdk activate latest
./docs/build.sh
```

Reuses the LVGL 9.2.0 source PlatformIO already fetched for the desktop simulator
(`.pio/libdeps/simulator/lvgl`) if present, or clones it fresh otherwise — so `pio run -e simulator`
once first makes rebuilds here faster, but isn't required.

To try it locally before pushing:

```bash
cd docs && python3 -m http.server 8000   # then open http://localhost:8000
```

(`file://` won't work — the browser blocks `fetch()`-ing the `.wasm` file from a local file URL.)

## Enabling GitHub Pages

Not automatic — a one-time setting: **Settings → Pages → Source: Deploy from a branch → `main` /
`/docs`**. No Actions workflow needed; the committed `watch.js`/`watch.wasm` are served as-is.

## What's different from the desktop simulator

Everything that's firmware behavior is identical — same screens, same reminders, same vibration
timing, same sleep/wake logic, because it's the same source. What differs is purely about being a
static, shareable page instead of a local dev tool:

- No `pio device monitor`-style serial log streaming from real hardware — obviously, there's no
  hardware. (Same is true of the desktop simulator.)
- Saved state (reminders, counts, the clock) lives in *your browser's* IndexedDB for *this* site, not
  a `.sim_state/` folder — clearing site data resets it, same as a factory reset would.
- No CPU-speed or battery-current measurement — those numbers come from the
  [power profiler](../power_profiler/README.md) on real hardware; the simulator's `battery_pct` slider
  is a UI input, not a live reading of anything.
