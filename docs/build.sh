#!/usr/bin/env bash
# Builds the browser simulator: the real firmware (src/*, unmodified) +
# sim/sim_hal.cpp + sim/sim_state.cpp (shared with the native simulator) +
# this folder's wasm_main.cpp/wasm_bridge.cpp, compiled to WebAssembly with
# Emscripten. Output lands right in this folder (watch.js + watch.wasm),
# ready for GitHub Pages to serve alongside index.html — no build step on
# the Pages side.
#
#   ./docs/build.sh
#
# Needs the Emscripten SDK (https://emscripten.org/docs/getting_started/downloads.html):
#   git clone https://github.com/emscripten-core/emsdk.git ~/.emsdk
#   ~/.emsdk/emsdk install latest && ~/.emsdk/emsdk activate latest
#   source ~/.emsdk/emsdk_env.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/docs"

if ! command -v emcc >/dev/null 2>&1; then
    if [ -f "$HOME/.emsdk/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        source "$HOME/.emsdk/emsdk_env.sh" >/dev/null
    fi
fi
command -v emcc >/dev/null 2>&1 || { echo "emcc not found. Install the Emscripten SDK (see this script's header)." >&2; exit 1; }

# LVGL 9.2.0 source: reuse a PlatformIO-fetched copy if one exists (fastest,
# and guaranteed to be the exact same tree the native simulator/firmware
# build against), otherwise fetch it directly so this script also works on
# a clean checkout / in CI with no PlatformIO cache.
LVGL_DIR="$ROOT/.pio/libdeps/simulator/lvgl"
if [ ! -f "$LVGL_DIR/lvgl.h" ]; then
    LVGL_DIR="$ROOT/docs/.lvgl-cache"
    if [ ! -f "$LVGL_DIR/lvgl.h" ]; then
        echo "Fetching LVGL 9.2.0 source into docs/.lvgl-cache (one-time)..."
        rm -rf "$LVGL_DIR"
        git clone --branch v9.2.0 --depth 1 https://github.com/lvgl/lvgl.git "$LVGL_DIR" >/dev/null
    fi
fi

CFLAGS=(
    -I "$ROOT/src"
    -I "$ROOT/sim"
    -I "$ROOT/sim/include"
    -I "$LVGL_DIR"
    -I "$LVGL_DIR/src"
    -D SIMULATOR=1
    -D "LV_CONF_PATH=$ROOT/sim/lv_conf.h"
    -Wno-unused-result -Wno-format
    -O2
)
LINKFLAGS=(
    -sASYNCIFY=1
    -sASYNCIFY_STACK_SIZE=131072
    -sALLOW_MEMORY_GROWTH=1
    -sMODULARIZE=1
    -sEXPORT_NAME=WatchModule
    -sEXPORT_ES6=0
    -sEXIT_RUNTIME=0
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","HEAPU16","FS"]'
    -sEXPORTED_FUNCTIONS='["_main","_sim_web_frame_ptr","_sim_web_frame_seq","_sim_web_state_json","_sim_web_touch","_sim_web_set_battery","_sim_web_wake","_sim_web_fire","_sim_web_buzz","_sim_web_reboot","_sim_web_factory"]'
    -lidbfs.js
    --pre-js "$OUT/pre.js"
)

OBJDIR="$(mktemp -d)"
trap 'rm -rf "$OBJDIR"' EXIT
OBJS=()

echo "Compiling C sources (LVGL + generated fonts)..."
for f in "$ROOT"/src/*.c $(find "$LVGL_DIR/src" -name '*.c'); do
    o="$OBJDIR/$(basename "$f" .c)-$(echo -n "$f" | cksum | cut -d' ' -f1).o"
    emcc -c "${CFLAGS[@]}" "$f" -o "$o"
    OBJS+=("$o")
done

echo "Compiling C++ sources (firmware + sim + web bridge)..."
for f in "$ROOT"/src/*.cpp "$ROOT"/src/ui/*.cpp "$ROOT"/sim/sim_hal.cpp "$ROOT"/sim/sim_state.cpp "$OUT"/wasm_main.cpp "$OUT"/wasm_bridge.cpp; do
    o="$OBJDIR/$(basename "$f" .cpp)-$(echo -n "$f" | cksum | cut -d' ' -f1).o"
    em++ -c "${CFLAGS[@]}" "$f" -o "$o"
    OBJS+=("$o")
done

echo "Linking (this is the slow step — Asyncify instrumentation)..."
em++ "${OBJS[@]}" "${LINKFLAGS[@]}" -O2 -o "$OUT/watch.js"

echo "Done -> docs/watch.js + docs/watch.wasm"
