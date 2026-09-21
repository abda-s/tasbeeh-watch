# Desktop simulator

Runs the **real firmware** — `src/main.cpp` and everything in `src/ui/`, unmodified — on your
computer, with the watch shown in a browser tab. No board, no upload.

```bash
./sim/run.sh                      # build + run (finds PlatformIO even if `pio` isn't on your PATH)
# or:  pio run -e simulator -t exec
# then open  http://localhost:8080      (`SIM_PORT=9000 ./sim/run.sh` to change the port)
```

Needs only `g++` and PlatformIO (the `native` platform downloads itself the first time).

## How it works

Instead of re-implementing the UI, `sim/` fakes the *hardware* underneath it:

| Real hardware | In the simulator |
|---|---|
| GC9A01A display over SPI (`TFT_eSPI`) | framebuffer, honouring the same rotation + sleep/display-off commands |
| CST816S touch | your mouse in the browser (click = tap, drag = swipe, hold ≈1 s = long-press) |
| NVS (`Preferences`) | `.sim_state/prefs.txt` — settings, counters, reminders and the clock persist across runs |
| Backlight PWM | screen dims / goes black with the duty cycle |
| Battery ADC | the battery slider |
| Light sleep | blocks until a touch or the wake timer — same 15 s timeout, same wake path |
| Deep sleep | a real process restart (`exec`), with `RTC_DATA_ATTR` variables carried across — so lockdown behaves like the device |
| Vibration motor pin | see below |

The browser page (`sim/web/index.html`) is a plain HTML file, read from disk on every request — edit
it and refresh, no rebuild.

## The vibration motor

Whatever `OUTPUT` pin the firmware configures (other than the backlight) is treated as the motor, so it
follows `VIBRATOR_PIN` automatically. Every pin edge is recorded with its timestamp and shown as:

- a **logic-analyser trace** of the last 4 seconds (you can read the pulse pattern straight off it),
- the whole watch **shaking** and **haptic rings** pulsing while the pin is high,
- optional **buzz sound** and **phone haptics** (`navigator.vibrate`) checkboxes.

`Test vibration pattern` calls `vibrate_notification()` directly; `Fire` on a reminder makes it due
right now and goes through the real `checkReminders()` path (waking the watch first if it's asleep).
The "Vibration" switch in the app's Notifications screen is honoured, and the panel says when it's off.

## Control panel

Battery slider (try 4% for lockdown, ≥10% to leave it) · timer / touch wake · fire a reminder ·
power-cycle · factory reset · live firmware `Serial` log.

## Not simulated

CPU frequency, real backlight brightness/current, power draw, touch-controller timing quirks, and
flash/RAM limits (the sim uses a bigger LVGL heap because 64-bit pointers make objects larger).
Timing-sensitive hardware behaviour — like the boot-time backlight glitch — has to be checked on the board.
Developed on Linux; the deep-sleep RTC persistence relies on GNU `ld` section symbols.
