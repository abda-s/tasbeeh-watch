# Power Management

> Every power optimization, with measurements, from V1 to V1.5.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Power Management (V1 → V1.5)

Measured with the dual-port INA226 profiler in [`power_profiler/`](../power_profiler/README.md)
(fuses current samples with firmware `[SCREEN_ON]` / `[SCREEN_OFF]` / `[WAKE]`
serial markers on one timeline).

### Real battery capacity: ~48 mAh, not the advertised 100 mAh

Every test below except the last three ran until the battery hit cutoff, so
each `Duration × Avg I` back-calculates the battery's actual usable capacity
(`Capacity (mAh) = Current (mA) × Duration (min) / 60`). Doing that for all
10 independently-run tests — different firmware versions, different states,
different current draws — gives:

| Test | Calculated capacity |
|---|---|
| V1 screen off (51 mA × 56 min) | 47.60 mAh |
| V1 screen on (90 mA × 32 min) | 48.00 mAh |
| V1.1 screen off (35 mA × 82 min) | 47.83 mAh |
| V1.1 screen on (85 mA × 34 min) | 48.17 mAh |
| V1.2 screen off (29 mA × 98 min) | 47.37 mAh |
| V1.2 70% (70 mA × 41 min) | 47.83 mAh |
| V1.2 50% (62 mA × 46 min) | 47.53 mAh |
| V1.2 27% (54 mA × 53 min) | 47.70 mAh |
| V1.21 screen off (28 mA × 102 min) | 47.60 mAh |
| V1.21 screen on (48 mA × 60 min) | 48.00 mAh |

All ten land within 47.4-48.2 mAh (average **47.76 mAh**) — that's every
test independently agreeing, not a one-off calculation, so this is a solid
result: **the battery is actually ~48 mAh, roughly half of the ~100 mAh the
manufacturer/listing states.**

| Version | Change | State | Duration | Avg I |
|---|---|---|---|---|
| V1 | Baseline, WiFi connected | Screen off | 56 min | 51 mA |
| V1 | Baseline, WiFi connected | Screen on | ~32 min | 90 mA |
| V1.1 | WiFi removed entirely | Screen off | ~82 min | 35 mA |
| V1.1 | WiFi removed entirely | Screen on | ~34 min | 85 mA |
| V1.2 | CPU 240→80MHz during sleep, touch standby, animation pause, PWM backlight | Screen off | ~98 min | 29 mA |
| V1.2 | same, brightness 70% | Screen on | ~41 min | 70 mA |
| V1.2 | same, brightness 50% (PWM 120) | Screen on | ~46 min | 62 mA |
| V1.2 | same, brightness 27% (PWM 70) | Screen on | ~53 min | 54 mA |
| V1.21 | Static swipe arrows, CPU 240→160MHz awake, adaptive loop idle | Screen off | 102 min | 28 mA |
| V1.21 | same | Screen on | ~60 min | 48 mA |
| V1.22 | CPU 160→80MHz awake | Screen on | ~66 min *(calc, 48mAh÷43.4mA)* | 43.4 mA |
| V1.23 | Bug fix: CPU now starts at 80MHz at boot (was defaulting to 240MHz until the first sleep/wake cycle) | — | — | — |
| V1.3 | Real light sleep (`esp_light_sleep_start`), GPIO wake polarity fix | **Screen off (asleep)** | ~576 min / 9.6h *(calc, 48mAh÷5mA)* | **~5 mA** |
| V1.31 | Wake-timer interval 2s→60s, onboard IMU powered down | **Screen off (asleep)** | ~600 min / 10h *(calc, 48mAh÷4.8mA)* | **~4.8 mA** |
| V1.32 | Touch wake latency: skip hardware reset on wake, shrink DISPON defer | **Screen off (asleep)** | not yet measured | **~1.9 mA** *(measured at V1.5, see note)* |
| V1.5 | Deep sleep + battery lockdown added (separate mode, doesn't touch this path — see below) | Deep sleep (`<5%` lockdown / bench test) | — | **~0.6-0.8 mA**, whole board *(sensor-limited — see below)* |

*(V1.22/V1.3/V1.31 durations are calculated from the 48 mAh figure above, not
measured directly — those tests weren't run to full depletion.)*

**On the V1.32 number**: V1.31's ~4.8 mA was the last independently-measured
screen-off figure; V1.32's touch-wake-latency change (`wake_display()`
skipping `touch.begin()`'s ~110 ms reset, `WAKE_DISPON_DELAY_MS` cut from
120ms to 20ms) went in afterward but was never separately re-measured at the
time (the table above literally said "not yet measured" for both columns).
~1.9 mA is what a later measurement pass came back with. Diffed the actual
code between the V1.31 commit and now to check what could explain it: the
core light-sleep mechanism itself — `sleep_display()`, the
`esp_light_sleep_start()` call, `SLEEP_WAKE_INTERVAL_US`, the GPIO wake
source — is **byte-for-byte unchanged** since V1.31. The *only* functionally
relevant change touching this path at all is that same V1.32 wake-latency
work, which only affects the cost of an actual touch-wake event, not the
passive current while dwelling in light sleep between wakes. Whether that
alone accounts for the full 4.8→1.9 mA drop depends on whether the test that
produced each number involved periodic touches or was a pure untouched-dwell
measurement — that detail isn't confirmed, so this is documented as the only
candidate mechanism found in the diff, not a verified root cause.

**On the deep-sleep number**: unchanged from the dedicated bench-test
measurement described below (~0.6-0.8 mA, whole board). A separate,
lower-resolution measurement pass reported deep sleep reading almost 0 mA —
that's the sensor's floor, not a real measurement of near-zero current
(most current sensors lose accuracy well before true zero), so the ~0.6-0.8
mA figure from the dedicated test remains the number to trust.

What each version changed:

- **Backlight PWM** (`ledcAttach`/`ledcWrite` on `TFT_BL`, 5 kHz, 8-bit) —
  duty `BL_DUTY_ON = 70` (~27%) instead of always-on full brightness. The
  backlight was roughly half the screen-on budget.
- **CPU frequency scaling while awake** — tuned down from 240 MHz → 160 MHz
  (V1.21) → 80 MHz (V1.22); this UI's redraw load never needed more.
  V1.23 fixed a bug where the boot-time clock wasn't set at all, so the
  device ran at the 240 MHz default until the first sleep/wake cycle.
- **Display sleep timeout** — 15 s of inactivity (`SLEEP_TIMEOUT_MS`, reduced
  from 30 s) triggers backlight off + panel `SLPIN` + touch controller into
  hardware standby. Any touch wakes it.
- **Real light sleep (V1.3)** — replaced the old "soft sleep" (CPU merely
  downclocked, `lv_timer_handler()` polled every 200 ms) with an actual
  `esp_light_sleep_start()` call: the CPU fully halts between wake events
  instead of spinning. Chosen over deep sleep specifically because deep sleep
  wipes RAM and would force a full `setup()`/LVGL rebuild (~1 s) on every
  wake — light sleep keeps state intact and resumes instantly. Wake sources:
  GPIO on the touch IRQ pin (level-triggered — had to flip to
  `GPIO_INTR_LOW_LEVEL` after discovering the idle level was the opposite of
  what the `RISING`-edge convention implied, which was silently rejecting
  every sleep attempt) plus a periodic timer backstop (60 s as of V1.31, up
  from 2 s at V1.3 launch — see below; means a missed GPIO wake could in
  theory delay a touch response by up to that long, though the polarity fix
  above has made GPIO wake reliable in testing). Dropped screen-off current
  from ~28 mA to **~5 mA**.
- **RC-oscillator drift management (V1.3, interval tuned in V1.31)** — a 32.768kHz crystal was bought
  for this board with the intent of using it for accurate RTC timekeeping,
  but it turned out not to be practically usable: the ESP32-S3's
  `XTAL_32K_P`/`XTAL_32K_N` pins aren't broken out to any header or test pad
  on this board, only to the two chip legs directly on the QFN package
  itself (0.4mm pin pitch). Wiring a crystal there would mean soldering
  bodge wires onto adjacent fine-pitch chip pins by hand — that needs
  hot-air rework and microscope-level precision, with real risk of
  solder-bridging a neighboring pin and bricking the module, so it wasn't
  attempted. Without it, the RTC timekeeping used during light sleep runs
  on the internal RC oscillator instead. **No custom resync
  code was written for this** — there's no function anywhere that reads the
  RC oscillator and compares it against the main crystal. ESP-IDF already
  does that recalibration automatically and internally on every sleep/wake
  transition, as part of `esp_light_sleep_start()` itself; it isn't exposed
  as something app code calls. Our only lever is *how often* that transition
  happens — the periodic wake (`SLEEP_WAKE_INTERVAL_US`, 60 s) exists to
  keep giving that built-in recalibration a fresh chance to run, rather than
  letting temperature drift accumulate uncorrected across a much longer
  sleep window. The 60 s cadence matches what's reported (external sources
  below) to achieve ~10ppm RTC accuracy this way — that figure has not been
  independently measured on this specific device, it's the cadence a
  similar published technique used:
  - [Managing RTC clock drift in deep sleep? — ESP32 Forum](https://esp32.com/viewtopic.php?t=35490)
  - [A Complete Guide to Checking RTC Accuracy on the ESP32 — Medium](https://medium.com/@raypcb/a-complete-guide-to-checking-rtc-accuracy-on-the-esp32-3f83a5c70ec7)
- **Clock drift fix** — `updateClock()` now advances by the actual elapsed
  `millis()` delta instead of a flat +1 per callback, so a late or skipped
  tick (from sleep) can't silently lose time.
- **Static swipe arrows** — a running LVGL animation invalidates its object
  every refresh cycle forever; the old arrow sway cost ~10% of screen-on
  charge.
- **Touch wake path** — `touch.sleep()`'s internal reset pulse glitches the
  IRQ line; the library ISR is detached around it so the glitch isn't
  mistaken for a real touch. The wake-from-sleep path itself is now the
  hardware GPIO-wake mechanism above, not a software interrupt.
- **Wake race fix (V1.21)** — inactivity is reset immediately on wake and
  `screen_sleep_cb` is blocked during the deferred backlight window,
  otherwise the panel could re-sleep mid-wake leaving a lit black screen.
  This guard is independent of how long that window actually is, which
  mattered for the next item.
- **Touch wake latency (V1.32)** — `wake_display()` used to call
  `touch.begin()` on every wake, which runs the CST816S library's full
  hardware reset sequence (`delay(50)+delay(5)+delay(50)`, ~110ms) plus two
  I2C reads, on every single touch wake. That reset is redundant: per the
  CST816S datasheet, Standby mode autonomously returns to Dynamic mode the
  moment it detects the touch that woke us in the first place — no reset
  needed, and the chip was already freshly reset moments earlier anyway
  (inside `touch.sleep()`'s own reset pulse when going *to* sleep). Patched
  the vendored library (see [Known Patches](build-and-configuration.md#known-patches-to-vendored-libraries)) to add a `rearm()` method
  that just reattaches the interrupt, skipping the reset entirely. Also
  shrank the DISPON/backlight defer from 120ms to 20ms
  (`WAKE_DISPON_DELAY_MS`) after re-reading the GC9A01A datasheet: the
  120ms figure actually governs a different restriction (minimum dwell
  time before flipping back into the opposite sleep state — our 15s
  `SLEEP_TIMEOUT_MS` is nowhere near that limit anyway), not how soon
  `DISPON` can follow `SLPOUT`, which only requires 5ms. Combined, touch-to-visible
  latency should drop from ~240ms+ to well under 100ms — not yet measured
  on hardware.
- **IMU power-down (V1.31)** — the onboard QMI8658A (accel+gyro, address `0x6B`,
  SA0 grounded) is never used by this firmware, so it was left sitting in
  its power-on-reset default state indefinitely: per its datasheet
  (Table 31, Operating Modes) that's "Power-On Default" — both sensors off
  but the internal high-speed clock still running, ~15µA. One I2C write in
  `setup()` (`CTRL1` register, `SensorDisable` bit) drops it into
  "Power-Down" mode, ~6µA — the lowest documented state that still keeps
  the I2C interface responsive. That's the datasheet's floor for a powered
  chip; going lower would mean physically cutting `VDD`/`VDDIO`, which on
  this board are hard-wired to the shared `3V3` rail with no dedicated load
  switch, so not pursued (6µA against a ~5mA total sleep budget is a small
  fraction anyway).

Not yet done (known next steps): brightness setting in the settings screen;
a physical vibration motor — **not a from-scratch design**, it turns out: the
schematic already has two unpopulated low-side N-FET load-switch footprints
(same `DMG1012T-7` part the backlight driver uses), one gated by GPIO5, one by
GPIO4. GPIO5 is already claimed by this firmware (`TOUCH_IRQ`), but **GPIO4 is
free** — populating that footprint and wiring the motor to its header is all
that's needed, no new driver circuit; and the 20%→5% "power-saving" battery
tier discussed alongside [lockdown mode](deep-sleep-and-lockdown.md) (deep sleep instead of light
sleep, otherwise identical behavior) — deferred, only the <5% lockdown tier
is implemented so far.

The V1.4 reminders/notifications feature doesn't touch any of the sleep, CPU,
or IMU paths above, so it isn't expected to move these numbers. V1.5's deep
sleep / battery lockdown mode is a genuinely separate, third power state (not
a further optimization of light sleep) — see [Deep Sleep & Battery Lockdown](deep-sleep-and-lockdown.md) for
its own measurements.
