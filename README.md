<div align="center">

# Tasbeeh Smartwatch

**A small, fully offline remembrance (dhikr) counter watch for kids — an ESP32-S3 with a 1.28″ round touchscreen.**

<p>
  <img alt="Platform" src="https://img.shields.io/badge/platform-ESP32--S3-blue">
  <img alt="Framework" src="https://img.shields.io/badge/framework-Arduino-00979D">
  <img alt="UI" src="https://img.shields.io/badge/UI-LVGL%209.2-1f6feb">
  <img alt="Build" src="https://img.shields.io/badge/build-PlatformIO-orange">
  <img alt="Connectivity" src="https://img.shields.io/badge/connectivity-offline-lightgrey">
</p>

<img src="documentation/screenshots/hero.png" alt="Home, Istighfar and Tasbeeh screens" width="760">

<p>
  <a href="#about">About</a> ·
  <a href="#features">Features</a> ·
  <a href="#screenshots">Screenshots</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#power-profiler">Power profiler</a> ·
  <a href="#documentation">Documentation</a> ·
  <a href="#roadmap">Roadmap</a>
</p>

</div>

---

## About

Tasbeeh Smartwatch is firmware for the [Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm)
that turns a round touchscreen dev board into a simple watch for children. It tells the time, counts
*istighfar* and *tasbeeh* with a tap, and gently reminds a child of daily routines — drink water, wash
hands, homework, brush teeth, bedtime — with a popup and, optionally, a vibration.

<p align="center">
  <img src="documentation/screenshots/on-device.jpg" alt="The firmware running on the real board, in a 3D-printed case" width="320"><br>
  <sub>Running on the real board, in a 3D-printed case — not just the simulator above.</sub>
</p>

### What it is — and what it isn't

- **It is** a clock, two tap-to-count remembrance counters, and a small set of routine reminders. The whole
  interface is in Arabic (right-to-left).
- **It is not a prayer app.** There are no prayer times, adhan, Qibla direction or other prayer features —
  what's here is exactly what's listed under [Features](#features).
- **It is offline.** No WiFi, no Bluetooth, no phone app, no cloud. The counters, settings, reminders and the
  time all live in the watch's own flash memory.
- **The clock is set by hand** (Settings) and kept by the chip's internal oscillator — there's no network time
  and no RTC battery, so it will slowly drift and may need re-setting now and then.

### Why it's built the way it is

- **Battery first.** The cell on this board measures about **48 mAh**, roughly half the advertised 100 mAh, so
  the firmware is designed around power: the screen sleeps after 15 s, the CPU really halts between wakes
  (light sleep), and below 5% battery the watch drops into deep sleep. Screen-off draw went from ~51 mA in the
  first version to under 5 mA, and about 0.7 mA in deep sleep — all measured.
- **Protecting the battery.** This board has no hardware over-discharge protection, so firmware is the only
  safeguard: under 5% the watch locks down to a dim "please charge" message until it's charged again.
- **Simple for a child.** Three screens in a swipe ring, large tap targets, no menus to get lost in.
- **Arabic that renders properly.** LVGL's Arabic shaper needs a font with presentation-form glyphs, which
  modern fonts don't ship — this project generates its own (see [Font System](documentation/fonts.md)).
- **Testable without the hardware.** A desktop simulator runs the real firmware in a browser tab.

## Features

- **Round 240×240 UI** with a three-screen swipe ring: Home ↔ Istighfar ↔ Tasbeeh.
- **Clock** — 12-hour time with a seconds arc, day, date and battery percentage.
- **Istighfar counter** — tap to count 0 → 100 with a progress arc.
- **Tasbeeh counter** — three phrases (سبحان الله / الحمد لله / الله أكبر) × 33, with progress dots. Counts and
  the current phrase survive reboots.
- **Settings** — set the time and manage notifications.
- **Reminders** — five preset routines, each with its own on/off switch, time and days of the week, a popup,
  and optional vibration. Add, rename or remove presets by editing one config table.
- **Power management** — display sleep, real light sleep with instant touch wake, drift-managed timekeeping,
  deep sleep and battery lockdown.
- **Desktop simulator** — run and poke at the UI on your computer, no board needed.

## Screenshots

Captured from the [desktop simulator](sim/README.md) — the real firmware, running unmodified.

<table>
  <tr>
    <td align="center"><img src="documentation/screenshots/01-home.png" width="220" alt="Home"><br><sub><b>Home</b><br>12h clock, seconds arc, battery</sub></td>
    <td align="center"><img src="documentation/screenshots/02-istighfar.png" width="220" alt="Istighfar"><br><sub><b>Istighfar</b><br>tap to count, 0 → 100</sub></td>
    <td align="center"><img src="documentation/screenshots/03-tasbeeh.png" width="220" alt="Tasbeeh"><br><sub><b>Tasbeeh</b><br>3 phrases × 33</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="documentation/screenshots/04-settings.png" width="220" alt="Settings"><br><sub><b>Settings</b></sub></td>
    <td align="center"><img src="documentation/screenshots/05-time-editor.png" width="220" alt="Time editor"><br><sub><b>Set time</b></sub></td>
    <td align="center"><img src="documentation/screenshots/06-notifications.png" width="220" alt="Notifications"><br><sub><b>Notifications</b><br>preset reminders + vibration switch</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="documentation/screenshots/07-reminder-editor.png" width="220" alt="Reminder editor"><br><sub><b>Reminder editor</b><br>time + repeat days</sub></td>
    <td align="center"><img src="documentation/screenshots/08-reminder-popup.png" width="220" alt="Reminder popup"><br><sub><b>Reminder popup</b><br>wakes the screen and vibrates</sub></td>
    <td align="center"><img src="documentation/screenshots/09-battery-lockdown.png" width="220" alt="Battery lockdown"><br><sub><b>Battery lockdown</b><br>&lt;5% — dim, dead end until charged</sub></td>
  </tr>
</table>

## Quick start

**Requirements:** [PlatformIO](https://platformio.org/) and, for the real watch, a
Waveshare ESP32-S3-Touch-LCD-1.28 board.

### Build and flash

```bash
pio run                                                        # build
pio run --target upload --upload-port /dev/ttyUSB0            # flash
pio device monitor --port /dev/ttyUSB0 --baud 115200          # serial log
```

If the board isn't detected, enter download mode first: hold **BOOT**, press **RESET**, release **BOOT**.

### Try it without hardware

```bash
./sim/run.sh          # then open http://localhost:8080
```

Runs the real UI on your computer with a browser control panel — battery slider, reminders, sleep/wake, and a
visualizer for the vibration motor. Needs only `g++` and PlatformIO. See [`sim/README.md`](sim/README.md).

## Hardware

| | |
|---|---|
| **Board** | Waveshare ESP32-S3-Touch-LCD-1.28 |
| **MCU** | ESP32-S3 — 240 MHz capable, runs at 80 MHz awake; 320 KB SRAM, 16 MB flash, 2 MB PSRAM |
| **Display** | 1.28″ round TFT, 240×240, GC9A01 over SPI (40 MHz) |
| **Touch** | CST816S capacitive, I²C |
| **Power** | Li-ion cell with an ETA6098 charger; battery level read through a voltage divider on GPIO1 |
| **Vibration** | Optional motor on a free GPIO (`VIBRATOR_PIN`), switchable in the app |

More in [Hardware](documentation/hardware.md); component datasheets are in [`datasheets/`](datasheets/).

## Power profiler

The battery numbers in this project weren't estimated — they were **measured** with a small power profiler
built for it, and kept in [`power_profiler/`](power_profiler/README.md). It's a hand-built current/voltage
meter (Arduino Nano + INA226 + OLED) that streams to a desktop app, which plots the watch's current draw in
real time and lines it up with the firmware's own log markers (`[SLEEP]`, `[WAKE]`, `[SCREEN_ON]`…). That's how
each optimization was proven, and how the real battery capacity turned out to be **~48 mAh, not 100 mAh**.

<p align="center">
  <img src="power_profiler/screenshots/hardware-with-ui.jpg" alt="The profiler hardware next to its desktop UI" width="640"><br>
  <sub>The profiler module next to the desktop app, mid-measurement.</sub>
</p>

### The hardware

<table>
  <tr>
    <td width="52%"><img src="power_profiler/screenshots/hardware-module.jpg" alt="Profiler module: Arduino Nano, INA226 and OLED on a perfboard"></td>
    <td valign="top">
      <ul>
        <li><b>INA226</b> current/voltage sensor with a 10&nbsp;mΩ shunt, wired in series with the watch's supply</li>
        <li><b>Arduino Nano</b> reads it over I²C and streams <code>voltage,current</code> over USB serial</li>
        <li><b>0.96″ OLED</b> shows live volts and milliamps, so it works as a standalone meter too</li>
        <li>Sketch: <a href="power_profiler/firmware/ina226_timed.ino"><code>ina226_timed.ino</code></a> (steady 100&nbsp;Hz)</li>
      </ul>
    </td>
  </tr>
</table>

### The desktop app

A PyQt5 / pyqtgraph oscilloscope-style viewer: dual-axis current and voltage plot, span / zoom / follow
controls, an ROI cursor for per-cycle charge and energy, a 1S LiPo battery model that estimates remaining
life, and automatic duty-cycle analysis from the watch's `[SLEEP]` / `[WAKE]` markers. A second serial port
carries the watch's debug log, drawn as red markers at the exact moment each line arrived. Everything is also
written to CSV. Full feature list in the [profiler README](power_profiler/README.md).

### What it measured

| Version | What changed | Screen | Avg current |
|---|---|---|---|
| V1 | Baseline, WiFi connected | off / on | 51 mA / 90 mA |
| V1.1 | WiFi removed | off / on | 35 mA / 85 mA |
| V1.2 | 80 MHz during sleep, touch standby, PWM backlight | off / on | 29 mA / 70 mA |
| V1.21 | Static swipe arrows, 160 MHz awake, adaptive loop | off / on | 28 mA / 48 mA |
| V1.22 | 80 MHz awake | on | 43 mA |
| V1.3 | Real light sleep | off (asleep) | **~5 mA** |
| V1.5 | Deep sleep (lockdown mode) | asleep | **~0.7 mA** |

<table>
  <tr>
    <td align="center"><img src="power_profiler/screenshots/v1-screen-off.png" alt="V1, screen off, 51 mA average"><br><sub><b>V1</b> · screen off, WiFi on — <b>51 mA</b> average</sub></td>
    <td align="center"><img src="power_profiler/screenshots/v1-screen-on-wifi.png" alt="V1, screen on, WiFi bursts, 90 mA average"><br><sub><b>V1</b> · screen on — WiFi bursts on top of a <b>90 mA</b> average</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="power_profiler/screenshots/v1.22-screen-on.png" alt="V1.22, screen on, 43 mA average"><br><sub><b>V1.22</b> · screen on at 80 MHz — <b>43 mA</b> average</sub></td>
    <td align="center"><img src="power_profiler/screenshots/v1.3-light-sleep.png" alt="V1.3, light sleep, ~5 mA floor with periodic wake spikes"><br><sub><b>V1.3</b> · light sleep — a ~5 mA floor, with the screen-on stretch highlighted</sub></td>
  </tr>
  <tr>
    <td align="center" colspan="2"><img src="power_profiler/screenshots/charging-near-full.png" alt="Battery charging curve near full, with sleep and wake markers" width="60%"><br><sub><b>Charging</b> · the current tapering off as the battery nears full, with wake markers from the watch's log</sub></td>
  </tr>
</table>

Every version, with the reasoning behind each change, is in [Power Management](documentation/power-management.md).

### Try it

```bash
cd power_profiler
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python profiler.py --demo        # synthetic data, no hardware needed
python profiler.py               # pick the INA226 (and optionally ESP32 log) port
```

## Repository layout

```
├── src/                  Firmware — main.cpp, ui/ (screens), config/, generated fonts
├── sim/                  Desktop simulator (mock hardware layer + browser front-end)
├── documentation/        Technical documentation and screenshots
├── datasheets/           Schematic and component datasheets
├── power_profiler/       INA226 power profiler — desktop app, sensor firmware, captured logs
├── scripts/              Helper scripts
├── boards/               Board definition and partition table
├── generate_fonts.sh     Font generation pipeline
├── remap_arabic.py       Adds Arabic presentation-form glyphs to modern fonts
└── platformio.ini        Build configuration (firmware + simulator environments)
```

## Documentation

Everything technical lives in [`documentation/`](documentation/README.md):

| Document | What's in it |
|---|---|
| [User Interface](documentation/user-interface.md) | Screens, gestures, navigation model, color palette, time editor |
| [Architecture](documentation/architecture.md) | Source layout, data persistence, rendering performance |
| [Reminders & Notifications](documentation/reminders-and-notifications.md) | Data model, the preset config table, UI, tap-target tuning |
| [Power Management](documentation/power-management.md) | Every optimization from V1 to V1.5, with measurements |
| [Deep Sleep & Battery Lockdown](documentation/deep-sleep-and-lockdown.md) | Deep sleep, timekeeping across it, the <5% lockdown mode |
| [Font System](documentation/fonts.md) | How Arabic text is shaped and how the fonts are generated |
| [Hardware](documentation/hardware.md) | Board, display, touch, battery and charger details |
| [Build & Configuration](documentation/build-and-configuration.md) | Building, dependencies, LVGL settings, library patches |
| [Simulator](sim/README.md) | How the desktop simulator works and what it fakes |
| [Power Profiler](power_profiler/README.md) | The INA226 measurement rig and desktop app used for all the power numbers |

## Roadmap

- A brightness setting in the Settings screen.
- The 20% → 5% "power-saving" battery tier (deep sleep in place of light sleep, otherwise unchanged behavior) —
  only the <5% lockdown tier exists so far.

## Acknowledgements

Built on [LVGL](https://lvgl.io/), [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) and
[CST816S](https://github.com/fbiego/CST816S). Arabic text uses the Alexandria and Reem Kufi typefaces from
Google Fonts.
