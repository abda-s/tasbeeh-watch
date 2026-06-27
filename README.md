# Tasbeeh Smartwatch — ESP32-S3 Touch LCD 1.28

A complete smartwatch application for Islamic prayer counting (tasbeeh/isteghfar) with
clock, reminders, WiFi-synced time, and a browser-based configuration portal —
all built with LVGL v9 on the Waveshare ESP32-S3-Touch-LCD-1.28.

---

## Screenshots (conceptual)

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ [⚙]      🔋  │    │   القائمة    │    │   تسبيح      │    │ ╔══════════╗  │
│              │    │              │    │              │    │ ║ تذكير    ║  │
│    08:42     │    │  [تسبيح   ]  │    │   ╭───╮      │    │ ║  08:00   ║  │
│  16/06/2026  │    │              │    │  ╱     ╲     │    │ ║ صلاة     ║  │
│              │    │ [استغفار  ]  │    │ │  33  │     │    │ ║ الفجر    ║  │
│         [→]  │    │              │    │  ╲     ╱     │    │ ╚══════════╝  │
│              │    │              │    │   ╰───╯      │    │              │
│              │    │              │    │الإجمالي:4210 │    │ [تم] [تأجيل] │
└──────────────┘    └──────────────┘    │  [اضغط]      │    └──────────────┘
    HOME              THIKER             └──────────────┘      REMINDER
                                          COUNTER
```

---

## Folder Structure

```
esp32-S3-Touch-LCD-1.28-sample-pio-project/
├── platformio.ini                  # Build config, library deps, build flags
├── README.md                       # This file
│
├── src/
│   ├── config/
│   │   ├── CST816S_pin_config.h    # Touch pin definitions (unchanged)
│   │   └── lv_conf.h               # LVGL v9.2 configuration (1111 lines)
│   │
│   ├── ui/
│   │   ├── styles.h                # Shared color/style declarations
│   │   ├── styles.cpp              # Style initialization (103 lines)
│   │   ├── screens.h               # Screen/widget extern declarations
│   │   └── screens.cpp             # Screen creation + update functions (475 lines)
│   │
│   └── main.cpp                    # App entry point: setup, loop, WiFi, NTP,
│                                     WebServer, LVGL callbacks (573 lines)
│
├── boards/                         # Wave share board definition
├── lib/                            # Empty (all deps via PlatformIO registry)
├── include/                        # Empty
└── test/                           # Empty

Total source: ~2,317 lines
```

### File responsibilities

| File | Lines | What it does |
|------|-------|--------------|
| `main.cpp` | 573 | Hardware init (touch, display, battery ADC), LVGL init, WiFi/NTP/webserver setup, clock/reminder/counter timers, all application logic |
| `ui/screens.h` | 26 | Exports all screen objects and updatable widget pointers |
| `ui/screens.cpp` | 475 | Builds each screen's widget tree once (labels, buttons, arcs, msgbox), event callbacks for navigation and counter taps, milestone flash effect |
| `ui/styles.h` | 29 | Exports shared color palette and LVGL style objects |
| `ui/styles.cpp` | 103 | Initializes all styles: background, clock, titles, buttons, arcs, cards |
| `lv_conf.h` | 1111 | LVGL build config: fonts, features, BIDI/Arabic, theme, widget enable/disable |

---

## How the Architecture Works

### Before → After comparison

The original code (the "template") used **raw TFT_eSPI pixel calls** for everything:
every screen was a function that called `tft.fillScreen()`, then manually drew every
rectangle, circle, triangle, and text string with coordinate‑matched hit‑testing in
`loop()`. There was no concept of a persistent widget — everything was ephemeral pixels.

The LVGL rewrite replaces that entirely with a **persistent widget tree**:

| Concern | Old approach | New approach |
|---------|-------------|-------------|
| Screens | `enum Page` + `drawHome()` functions that blast the screen | Static `lv_obj_t*` trees built once in `setup()` |
| Navigation | Manual `if (x >= N && y >= M)` in `loop()` | `lv_screen_load_anim()` slide/fade transitions |
| Buttons | Manual coordinate checks, no visual feedback | `lv_btn` objects with built-in pressed states |
| Clock | Partial redraw via `refreshHomeClock()` (hand‑written twin of `drawHome()`) | `lv_label_set_text_fmt()` in a 1‑second `lv_timer` |
| Counter ring | 360 `tft.fillCircle()` dots hand‑drawn per update | `lv_arc` widget with anti‑aliased arcs |
| Reminders | Full‑screen hand‑painted circle page | `lv_msgbox_create(NULL)` modal with backdrop |
| Battery ADC | 16ms blocking read inside every redraw | 10‑second `lv_timer`, decoupled from render |
| Animations | None; `delay(300)` for milestone "flash" froze the device | 400ms green flash via one‑shot `lv_timer`, non‑blocking |

### Screen lifecycle

```
┌──────────┐                              ┌─────────────┐
│ setup()  │ → builds all 6 screens       │ screens.cpp │
│          │   (scr_home, scr_thiker,     │             │
│          │    scr_tasbeeh, etc.)        │ one-time    │
│          │                              │ constructor │
│          │ → timers start               │ per screen  │
└──────────┘                              └─────────────┘
      │
      ▼
┌──────────────┐     tap/swipe     ┌─────────────────┐
│ lv_screen_   │ ───────────────→  │ lv_screen_load_ │
│ active()     │ ←───────────────  │ anim(scr,       │
│              │  (200ms slide)    │   MOVE_LEFT,     │
│              │                   │   200ms, ...)    │
└──────────────┘                   └─────────────────┘

Windows are never destroyed. Only the active screen is rendered.
LVGL automatically tracks dirty regions and repaints only changed pixels.
```

### Data flow

```
┌──────────┐   1s timer    ┌──────────────┐   lv_label_set_text_fmt
│ hour_    │ ───────────→  │ clock_timer_cb│ ──────────────────→ home_clock_label
│ minute_  │               └──────────────┘                     (dirty rect auto-
│ second_  │                                                     invalidated)
└──────────┘

┌──────────┐   tap event   ┌───────────────┐   lv_arc_set_value
│ tasbeeh  │ ───────────→  │tap_counter_cb  │ ──────────────→ tasbeeh_arc
│ Count    │               │ (increment,     │   lv_label_set_text_fmt
│          │               │  save, update)  │ ──────────────→ tasbeeh_counter_label
└──────────┘               └───────────────┘

┌──────────┐   1s timer    ┌──────────────┐   lv_msgbox_create(NULL)
│ hour_    │ ───────────→  │checkReminders │ ──────────────→ reminder popup
│ minute_  │               │ (checked on   │
│ reminders│               │  second==0)   │
└──────────┘               └──────────────┘
```

### Navigation map

```
               ┌──────────┐
               │  LOADING  │  (WiFi setup → NTP sync)
               └────┬─────┘
                    ▼
               ┌──────────┐
      ┌───────│   HOME   │───────┐
      │       └────┬─────┘       │
      │            │             │
      ▼            ▼             ▼
┌──────────┐ ┌──────────┐ ┌──────────┐
│ SETTINGS │ │  THIKER  │ │ TIMEEDIT │
│ (IP,      │ │          │ │          │
│  config)  │ └────┬─────┘ └──────────┘
└──────────┘      │
            ┌─────┴─────┐
            ▼           ▼
      ┌──────────┐ ┌──────────────┐
      │ TASBEEH  │ │  ISTEGHFAR   │
      │ counter  │ │  counter     │
      └──────────┘ └──────────────┘

Navigation methods:
  • Tap icons/buttons → LV_EVENT_CLICKED callback
  • Swipe left/right → LV_EVENT_GESTURE callback (on counter & thiker screens)
  • Screen transitions use lv_screen_load_anim() with slide or fade
```

---

## Research Foundation

Before writing any code, we studied how professional LVGL projects are built.

### Projects studied

| Project | Stars | Hardware | Key takeaway |
|---------|-------|----------|--------------|
| [PrintSphere](https://github.com/cptkirki/PrintSphere) | 241 | ESP32‑S3 round AMOLED | Most polished LVGL UI: dark theme, progress ring (`lv_arc`), multi‑page touch nav, web‑configurable colors |
| [zephyr‑watch](https://github.com/electricalgorithm/zephyr‑watch) | 20 | **Same board** (ESP32‑S3‑Touch‑LCD‑1.28) | Best structural reference: `screens/` + `styles/` directories, work queues for non‑blocking UI, dark theme |
| [lvgl‑watch](https://github.com/lxydiy/lvgl‑watch) | 59 | ESP32 Open‑Smartwatch | PlatformIO project (same build system), haptic motor support |
| [esp32‑lvgl‑watchface](https://github.com/fbiego/esp32‑lvgl‑watchface) | 53 | ESP32 240×240 | Watchface rendering on 240×240 displays |

### LVGL v9.1 docs used

- **Performance**: partial render mode (buffer = 1/10 screen), `LV_OBJ_STYLE_CACHE`, disabling unused widgets, `LV_DRAW_SW_CIRCLE_CACHE_SIZE`, `lv_snapshot_take()` for static backgrounds
- **UI/UX**: `lv_screen_load_anim()` for transitions, `lv_anim_t` for smooth value changes, `lv_msgbox_create(NULL)` for modal popups, flex/grid layouts, shadow/radius/gradient style properties
- **Arabic support**: `LV_USE_BIDI`, `LV_BIDI_BASE_DIR_DEF`, `LV_USE_ARABIC_PERSIAN_CHARS`, DejaVu 16 font

### Modern LVGL UI principles applied

1. **Consistent corner radius** — pill buttons use half‑height radius (26px), cards use 12px
2. **Depth through shadows** — buttons have `shadow_width: 8`, `shadow_offset_y: 4`
3. **Typography hierarchy** — exactly 3 font sizes (14/24/42) + Arabic font (16)
4. **Pressed state feedback** — provided free by LVGL default theme (`LV_THEME_DEFAULT_GROW`)
5. **Animated transitions** — screens slide (200ms), messages fade in
6. **Semantic color** — teal = primary action, gold = time/highlight, green = success/milestone, red = low battery
7. **Single accent** — one teal brand color used everywhere, not different colors per screen
8. **Dark‑theme baseline** — `LV_THEME_DEFAULT_DARK 1` sets the foundation

---

## Color System

| Role | Hex | Used in |
|------|-----|---------|
| Background | `#0a0e1a` | All screens |
| Card surface | `#111827` | Settings card, elevated containers |
| Primary accent | `#00c8a0` (Teal) | Buttons, arcs, titles |
| Secondary accent | `#fea020` (Gold) | Clock time |
| Isteghfar accent | `#3b82f6` (Blue) | Isteghfar button + arc |
| Success | `#00e000` (Green) | Milestone flash |
| Danger | `#ff4040` (Red) | Low battery (< 20%) |
| Text primary | `#f9fafb` | Labels, counter values |
| Text secondary | `#6b7280` | Date, descriptions |
| Border | `#1f2937` | Card borders |

---

## Fonts

| Font | Size | Purpose |
|------|------|---------|
| Montserrat 14 | 14px | Secondary labels, date, battery % |
| Montserrat 24 | 24px | Counter value inside arc |
| Montserrat 42 | 42px | Clock time on home screen |
| DejaVu 16 Persian/Hebrew | 16px | Arabic labels (buttons, titles, total) |

Arabic labels used on‑device: تسبيح, استغفار, القائمة, الإجمالي, اضغط, تذكير, تم, تأجيل, الإعدادات, ضبط الوقت, حفظ.

---

## Libraries & Dependencies

```
lvgl/lvgl @ 9.2.0            # LVGL graphics library
Bodmer/TFT_eSPI @ 2.5.43     # Display driver (GC9A01 over SPI)
fbiego/CST816S @ 1.1.1       # Touch driver (CST816S over I2C)
tzapu/WiFiManager @ 2.0.17   # Captive portal WiFi configuration
bblanchon/ArduinoJson @ 6.21.3  # JSON serialization for web API
```

Platform: `espressif32`, Arduino framework 3.0.1
Board: `waveshare_esp32s3_touch_lcd_128` (ESP32-S3, 16MB Flash, 2MB PSRAM)

---

## Features

### Device UI (6 screens)

- **Home** — large clock (HH:MM), date, battery percentage, gear icon → settings, arrow icon → thiker
- **Thiker** — two pill buttons: تسبيح and استغفار
- **Tasbeeh counter** — `lv_arc` ring (0–99), counter value, running total, TAP button
- **Isteghfar counter** — same layout with blue accent
- **Settings** — device IP address display, back button
- **Time Editor** — 5‑field stepper (hour, minute, day, month, year) with +/− buttons
- **Reminder popup** — modal `lv_msgbox` with time, label, dismiss (تم) and snooze (تأجيل) buttons

### Counter features

- Tap to increment (saved to ESP32 Preferences/NVS)
- Arc ring shows modulo‑99 progress
- Green 400ms flash on setiap 99‑count milestone (non‑blocking)
- Total label: الإجمالي: N — persisted across power cycles

### Reminders

- Up to 10 reminders configured via web browser
- Each has: hour, minute, label text, enable/disable toggle
- Fires a modal popup at the set time
- Snooze option dismisses and suppresses re‑fire for that minute

### WiFi & Time

- WiFiManager captive portal (`TasbeehWatch` SSID, 192.168.4.1)
- NTP time sync on connect (`pool.ntp.org`, UTC+3)
- Time persisted to Preferences

### Web Config Portal

Accessible at `http://<device-ip>` after WiFi connect:

- View current time, date, battery, counter values
- Set time/date
- Configure up to 10 reminders (hour, minute, label, enabled)
- RTL Arabic interface

### Performance

- RAM: 36.0% used (118 KB / 320 KB free)
- Flash: 46.3% used (1.45 MB / 3.1 MB free)
- LVGL partial render mode (only changed regions sent to display)
- LVGL FPS monitor (top‑right corner) during development
- Battery ADC read runs on a 10‑second timer (decoupled from render path)
- 15 unused LVGL widgets disabled at compile time

---

## LVGL Configuration Highlights

All settings are in `src/config/lv_conf.h`. Key changes from default:

```c
#define LV_OBJ_STYLE_CACHE      1    // Faster style lookups
#define LV_THEME_DEFAULT_DARK    1    // Dark theme baseline
#define LV_USE_BIDI             1    // Bidirectional text
#define LV_USE_ARABIC_PERSIAN_CHARS 1    // Arabic letter shaping
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 1  // Arabic font
#define LV_FONT_MONTSERRAT_24    1    // Counter value font
#define LV_FONT_MONTSERRAT_42    1    // Clock font
#define LV_USE_SYSMON           1    // System monitor
#define LV_USE_PERF_MONITOR     1    // FPS display
// Disabled: calendar, chart, keyboard, table, tabview,
//           tileview, win, roller, spinbox, led, line,
//           list, menu, scale (15 widgets)
```

---

## Build & Upload

### Prerequisites

1. Install [PlatformIO](https://platformio.org/) (`pip install platformio`)
2. Install CH34x USB‑UART drivers:
   - [macOS](https://www.wch.cn/downloads/CH34XSER_MAC_ZIP.html)
   - [Windows](https://www.wch.cn/downloads/CH341SER_EXE.html)
3. Connect the board via USB
4. Enter download mode: **hold BOOT**, press **RESET**, release **BOOT**

### Build

```bash
pio run
```

### Upload

```bash
# Find your port:
pio device list

# Upload:
pio run --target upload --upload-port /dev/cu.wchusbserialXXXX
```

### Monitor

```bash
pio device monitor --port /dev/cu.wchusbserialXXXX --baud 115200
```

---

## How to Extend

### Adding a new counter type

1. Add a global `uint32_t` in `main.cpp` and load/save it from `Preferences`
2. Call `create_counter_screen()` in `screens_init()` with new title, color, type
3. Add a button to `scr_thiker` that navigates to the new screen
4. Add `extern lv_obj_t*` to `screens.h` for external access

### Changing the color palette

Edit the `color_*` globals in `src/ui/styles.cpp`. All widgets reference these single-color definitions — changes propagate everywhere.

### Adding Arabic labels

Use UTF‑8 sequences. The DejaVu 16 font covers Arabic, Persian, and Hebrew scripts.
Enable `LV_USE_BIDI`, `LV_BIDI_BASE_DIR_DEF`, and `LV_USE_ARABIC_PERSIAN_CHARS` in `lv_conf.h`.

### Disabling the FPS counter (for production)

Set `LV_USE_PERF_MONITOR 0` in `lv_conf.h`.

### Tuning animation durations

Screen transitions: change the `200` parameter in navigation functions in `screens.cpp`.
Milestone flash: change the `400` parameter in `tap_counter_cb`.

---

## Hardware

- **Board**: Waveshare ESP32‑S3‑Touch‑LCD‑1.28
- **MCU**: ESP32‑S3 (dual‑core Xtensa LX7, 240 MHz)
- **Flash**: 16 MB
- **PSRAM**: 2 MB (Octal)
- **Display**: 1.28" round TFT, 240×240, GC9A01 driver, SPI
- **Touch**: CST816S capacitive touch, I2C
- **Battery**: ADC pin 1, voltage divider (×2)
- **USB**: CH343 UART bridge

---

## License

This project builds on open‑source libraries (LVGL, TFT_eSPI, CST816S, WiFiManager, ArduinoJson).
The application code is provided as a sample/starting point. Check individual library licenses
for redistribution requirements.
