#!/usr/bin/env bash
# Build + run the desktop simulator. Finds PlatformIO even when `pio` isn't on PATH
# (the VS Code extension installs it to ~/.platformio/penv/bin/pio).
#   ./sim/run.sh              -> http://localhost:8080
#   SIM_PORT=9000 ./sim/run.sh
set -e
cd "$(dirname "$0")/.."
if   command -v pio >/dev/null 2>&1;            then PIO=pio
elif [ -x "$HOME/.platformio/penv/bin/pio" ];   then PIO="$HOME/.platformio/penv/bin/pio"
else echo "PlatformIO not found. Install it (https://platformio.org/install) or the VS Code extension." >&2; exit 1
fi
exec "$PIO" run -e simulator -t exec
