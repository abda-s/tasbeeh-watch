# Deep Sleep & Battery Lockdown

> Real deep sleep, keeping time across it, the <5% battery lockdown mode and its bench tools.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Deep Sleep & Battery Lockdown Mode (V1.5)

### Why

This board has no hardware battery protection. The ETA6098 charger's own
quiescent draw is negligible (1µA, per its datasheet) but it only handles
*charging* — it has no battery-side undervoltage disconnect. When unplugged,
the battery reaches the rest of the board through a plain always-on P-FET
(Q2/AO3401) with no protection logic at all. Below 5% battery, firmware is the
only thing standing between normal operation and running the cell down to a
damaging voltage before it gets charged.

### Real deep sleep vs. light sleep

`esp_deep_sleep_start()` is a different primitive from the light sleep this
watch normally uses, not just "sleep for longer": light sleep clock-gates the
CPU but keeps RAM/state retained, so `esp_light_sleep_start()` blocks and
*returns* — `loop()` just continues once woken, instantly, with everything
exactly as it was. Deep sleep powers off the CPU/RAM domain entirely — it
never returns; every wake is a full reset back through `setup()`. That's a
real architectural fork, not a parameter change, which is why lockdown mode
below (built on deep sleep) needed its own boot path rather than a tweak to
the existing light-sleep loop.

Measured on this hardware: **~0.6-0.8mA whole-board** in deep sleep, a real,
large drop versus light sleep either way (~4.8 mA at V1.31, ~1.9 mA measured
later — see the [Power Management](power-management.md) table for what that later number
does and doesn't explain). The datasheet's own chip-only deep-sleep figure
(Table 4-8, ~7-8µA) is far lower still, so something else on the board
(display panel/touch-controller quiescent draw, most likely) is the current
floor now, not the ESP32-S3 itself. A separate, lower-resolution measurement
pass read deep sleep as almost 0mA — that's the sensor's floor, not a real
reading of near-zero current, so ~0.6-0.8mA remains the number to trust here.

### Keeping accurate time across deep sleep

`esp_timer_get_time()`/`millis()` (what the normal light-sleep clock is built
on) explicitly **reset to zero** on deep-sleep wake — confirmed directly in
ESP-IDF's own docs. `gettimeofday()`/`settimeofday()` (POSIX system time,
backed by the RTC timer) are documented to survive deep sleep correctly
instead. Two small helpers in `main.cpp` bridge this app's `hour_`/`minute_`/
etc. globals to that RTC-backed clock: `wallClockToSystemTime()` (seed it,
called once when *first* transitioning from normal operation into a
deep-sleep-based mode) and `systemTimeToWallClock()` (read it back, called on
every deep-sleep-originated boot). Verified matching on real hardware via
`[BOOT] NVS said ... — RTC/gettimeofday says ...` log output.

**A real bug found and fixed here**: the lockdown boot path was calling
`wallClockToSystemTime()` again every time it went back to sleep — including
on cycles where the clock had *already* been correctly restored from the RTC
at the top of that same boot. Since nothing updates `hour_`/`minute_` during
the time in between (there's no `updateClock()` call anywhere in the lockdown
path), that second call wrote a now-stale snapshot back over the still-
accurate, continuously-ticking system clock — silently discarding however
long that boot's processing took (up to `LOCKDOWN_DISPLAY_MS`, several
seconds) on **every single lockdown cycle**. Fixed by only re-seeding when
transitioning in from normal operation (`!woke_from_deep_sleep`), never on a
deep-sleep-to-deep-sleep cycle where the clock is already correct.

### Battery lockdown (<5%)

Below 5% battery, the watch stops being a normal watch: touching it shows
only a dim, dead-end message (`screen_lockdown.cpp` — "البطارية منخفضة جدًا" /
"الرجاء الشحن", no back button, no navigation, nothing else works) instead of
the ring/settings/reminders — and otherwise stays in deep sleep, silently
rechecking the battery every 5 minutes (`LOCKDOWN_RECHECK_US`) without ever
lighting the screen if nothing's changed. Recovers automatically once charged
back above 10% (hysteresis above the 5% entry point, so a jittery reading
right at the line can't flap the mode back and forth) — no special
charger-detection needed, since a connected charger makes the battery-voltage
ADC read jump to ~100% almost immediately (it's reading the charger's
regulated output, not the resting cell), so the normal recovery check catches
it on its own.

State (`in_lockdown`, `RTC_DATA_ATTR`) survives deep sleep and the reboot it
causes — the only case that matters, since normal operation never touches
deep sleep at all. Brightness while locked down is `LOCKDOWN_BL_DUTY` (~3%,
vs. the normal ~27%) — backlight is the dominant screen-on power cost (85mA
@ 100% vs. 52mA @ 27%, measured), so every bit matters here.

Deliberately out of scope for now: the wider 20%→5% "power-saving" tier
discussed alongside this (deep sleep instead of light sleep, but otherwise
fully normal behavior — reminders still fire, same brightness) — only the
<5% lockdown tier described above is implemented.

### Boot-sequence fixes needed to make this reliable

Building lockdown surfaced (and fixed) several boot-time bugs that also
apply to *any* deep-sleep wake, not just lockdown:

- **Stale panel content on wake** — the GC9A01A's GRAM is on a separate,
  continuously-powered chip, completely unaffected by the ESP32's sleep
  state. A deep-sleep wake used to briefly show whatever screen was active
  right before sleeping (e.g. Settings) before the real content caught up.
  Fixed by painting solid black directly (bypassing LVGL) right before
  `SLPIN`, so GRAM going into sleep is always neutral.
- **Backlight briefly flashing bright on wake** — traced to two compounding
  causes: (1) Arduino-ESP32's `ledcAttach()` reads the LEDC channel's
  *current* duty and reuses it as the initial value, on a channel that's
  never been configured before — a genuinely unreliable read, flagged as
  such in Espressif's own issue tracker
  ([espressif/arduino-esp32#11373](https://github.com/espressif/arduino-esp32/issues/11373));
  and (2) the window between physical wake and the first line of the sketch
  (ROM bootloader + 2nd-stage boot) is outside *any* Arduino code's control —
  moving init earlier inside `setup()` can never reach it. Fixed with
  `gpio_hold_en()`, which latches the backlight pin's output level through
  deep sleep *and* that whole boot window, released only once `setup()`
  explicitly takes over.
- **Backlight fade-in** — even with the above fixed, jumping straight to the
  target brightness read as a harsh flash. Fades in over ~150ms now, gamma-
  corrected (`duty ∝ t^2.2`) rather than linear — LED brightness perception is
  roughly logarithmic, so a linear duty ramp looks flat for most of its steps
  and then jumps to full brightness in the last one or two.
- **Partial double-buffer flush** — `draw_buf_1`/`draw_buf_2` are each only
  half the screen (`LV_DISPLAY_RENDER_MODE_PARTIAL`), so a full-screen
  invalidation like `lv_screen_load()` needs more than one `lv_timer_handler()`
  pass to guarantee both halves actually flushed before anything lights up.

### Bench-test tooling (kept in the codebase, not wired up by default)

Two flags in `screens.h`, same "flip to 0 and reflash to remove entirely"
pattern:

- **`DEEP_SLEEP_TEST_ENABLED`** (currently `0`) — the tool used to directly
  measure deep sleep's real current (`enterDeepSleepTest()`); superseded by
  the real lockdown feature but kept in source rather than deleted.
- **`LOCKDOWN_TEST_ENABLED`** (currently `1`) — long-press the Settings back
  button to force lockdown regardless of real battery % (`enterLockdownTest()`
  / `lockdown_test_forced`), so the feature can be exercised on the bench
  without actually running the battery down. Long-pressing the lockdown
  screen itself exits it early — but only while test-forced; a real
  low-battery lockdown ignores that gesture entirely, staying inescapable
  except by charging, same as production behavior. Escape hatch if a test
  run gets stuck: the physical RESET button always works, since a genuine
  reset (not a deep-sleep wake) reinitializes `RTC_DATA_ATTR` back to `false`.

Both bench tools share `deepSleepUntil(timer_wake_us)`, the extracted common
sleep-entry sequence (backlight guard, stale-GRAM fix, EXT0 touch wake +
timer wake) — so the real lockdown feature and both test tools all exercise
the exact same code path, not separate mocks of it.
