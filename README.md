# Tasbeeh Smartwatch — ESP32-S3 Touch LCD 1.28

Islamic prayer counter smartwatch with clock, counters, and settings — built on
LVGL v9, TFT_eSPI, and the Waveshare ESP32-S3-Touch-LCD-1.28 round display.

---

## Screens

```
         HOME                          ISTIGHFAR                      TASBEEH
    ┌──────────────┐              ┌──────────────┐              ┌──────────────┐
    │ ◂   [⚙]     │              │   ╭─────╮    │              │   ╭─────╮    │
    │              │              │  ╱ green ╲   │              │  ╱  blue ╲   │
    │   السبت      │              │ │  arc    │  │              │ │  arc    │  │
    │    08        │              │  ╲       ╱   │              │  ╲       ╱   │
    │    30    م   │              │   ╰─────╯    │              │   ╰─────╯    │
    │              │              │              │              │              │
    │ 12 يناير 2026│              │   استغفار    │              │   تسبيح      │
    │              │              │              │              │              │
    │  85%         │              │ أستغفر الله  │              │ سبحان الله   │
    └──────────────┘              │              │              │              │
                                  │      0       │              │      0       │
    ◂ SWIPE ▸                     │              │              │              │
    arrows show                   │من 100·اضغط   │              │اضغط·من 33    │
    more screens                  └──────────────┘              │   ■ ■ ■      │
                                                               └──────────────┘
```

**3-screen ring:** Home ↔ Istighfar ↔ Tasbeeh (swipe left/right)
**Swipe arrows** (`◂` / `▸`) at screen edges animate subtly to indicate more screens.
**Modals:** Settings (tap gear or swipe-down from home), TimeEdit (long-press clock) — swipe down or left to dismiss.

### Screen accent colors

| Screen | Color | Hex | Arc / Title |
|--------|-------|-----|-------------|
| Home | Gold | `#d4af37` | Seconds arc, day name, AM/PM |
| Istighfar | Green | `#33cc55` | Progress arc, title |
| Tasbeeh | Blue | `#7fd6a3` / `#3b82f6` | Progress arc, title, dots |

### Gesture Map

- **Ring screens**: Swipe L/R = navigate ring, swipe **down** (home only) = settings, **long-press** clock = time edit
- **Modals**: Swipe down = back, swipe **left** = back (timeedit: tap `<` or title also goes back)
- **Tasbeeh/Istighfar**: Tap anywhere = increment counter

---

## Project Structure

```
src/
├── main.cpp                      # Hardware init, LVGL setup, timers, loop, battery
├── config/
│   ├── CST816S_pin_config.h      # Touch I2C pin definitions
│   └── lv_conf.h                 # LVGL v9 build configuration
├── ui/
│   ├── screens.h                 # Public API — screen pointers, nav, fonts, helpers
│   ├── screens.cpp               # Global pointers + screens_init()
│   ├── styles.h / styles.cpp     # Color palette + style definitions
│   ├── nav.cpp                   # Ring navigation + modal push/pop + gesture handlers
│   ├── screen_base.cpp           # Shared helpers (make_screen_base, create_title, touch_debug)
│   ├── screen_home.cpp           # Clock screen: 12h stacked time + AM/PM, day, date, seconds arc, swipe arrows
│   ├── screen_istighfar.cpp      # Istighfar counter: tap to count, 0→100 green arc
│   ├── screen_tasbeeh.cpp        # Tasbeeh counter: 3 phrases × 33, blue arc, progress dots, phrase persists across boot
│   ├── screen_settings.cpp       # Settings (IP display, back button)
│   └── screen_timeedit.cpp       # 12h time editor: HH:MM AM/PM, DD/MM/YYYY, validation, back arrow
├── font_reem_kufi_72.c           # Clock digits — Reem Kufi 72px 4bpp
├── font_reem_kufi_48.c           # Counter digits — Reem Kufi 48px 4bpp
├── font_alexandria_12.c          # Small labels, AM/PM — Alexandria 12px 2bpp
├── font_alexandria_16.c          # Titles, day names, hints, theme — Alexandria 16px 2bpp
└── font_alexandria_28.c          # Arabic phrases — Alexandria 28px 4bpp
```

---

## Font System — How It Works

### The Problem

LVGL's Arabic shaper (`LV_USE_ARABIC_PERSIAN_CHARS`) converts base Arabic letters
to **presentation forms** (contextual shapes for connected writing). These
presentation forms live at Unicode codepoints U+FB50–U+FDFF and U+FE70–U+FEFF.

**Modern TTF fonts** (Reem Kufi, Amiri, Cairo, almost all Google Fonts) store
presentation form glyphs in OpenType **GSUB tables**, NOT at Unicode codepoints.
`lv_font_conv` extracts glyphs by **codepoint only** — it can't read GSUB tables.
Result: the shaper looks for U+FEA1 (beh initial form), the font has no glyph at
that codepoint → **empty box**.

**Older fonts** (DejaVu, Traditional Arabic, several Microsoft fonts) explicitly
map presentation form glyphs to Unicode codepoints. They work out-of-the-box.

### The Solution: fonttools Remap

[fonttools](https://github.com/fonttools/fonttools) reads/writes TTF cmap tables
in Python. We wrote a script that:

1. Opens the TTF
2. Examines all glyph names (e.g., `behDotless-ar.init`, `lam-ar.medi`)
3. Maps each contextual form glyph to its standard Unicode presentation form codepoint
4. Adds those codepoint→glyph mappings to the font's cmap table
5. Saves a remapped TTF
6. Runs `lv_font_conv` on the remapped TTF — now all presentation forms are extractable

This works for **any** modern Arabic font.

### Known quirk: missing isolated forms

Some isolated presentation forms (notably ص U+FEB9 and م U+FEE1) are not in the
font because the remap script couldn't find matching glyph names (`sad-ar.isol`,
`meem-ar.isol`) in the Alexandria TTF. Two layers of fixes are applied:

1. **Font ofs_list patch** (`font_alexandria_28.c`, `font_alexandria_12.c`):
   U+FEB9 and U+FEE1 are remapped to share the final-form glyphs (U+FEBA/U+FEE2)
   which are identical for standalone display.

2. **LVGL shaper patch** (`lv_text_ap.c:212`): When a character has no connections
   in either direction (standalone), the original base form character is kept
   instead of being converted to the missing isolated form.

The `remap_arabic.py` script was also updated with `ISOL_FALLBACK` entries for
future font regenerations (`0xFEB9: 0xFEBA`, `0xFEE1: 0xFEE2`).

### Font Layout

| Font | Size | BPP | Glyphs | Used for |
|------|------|-----|--------|----------|
| Reem Kufi 72 | 72px | 4bpp | 11 | Clock time (stacked HH\nMM) |
| Reem Kufi 48 | 48px | 4bpp | 10 | Counter digits |
| Alexandria 28 | 28px | 4bpp | 370 | Arabic phrases (أستغفر الله, etc.), timeedit numbers |
| Alexandria 16 | 16px | 2bpp | 370 | Titles, day names, date, hints, theme default |
| Alexandria 12 | 12px | 2bpp | 370 | Small labels, battery %, AM/PM on home clock |

### Regenerating Fonts

```bash
# 1. Remap the TTF (one-time per font)
python3 remap_arabic.py ReemKufi-Regular.ttf

# 2. Generate LVGL C files
lv_font_conv --no-compress --format lvgl \
  --font /path/to/remapped.ttf \
  --size 16 --bpp 2 \
  --range 0x0020-0x007F,0x0600-0x06FF,0xFB50-0xFDFF,0xFE70-0xFEFF \
  --symbols "$(printf '\u200c\u200d\u200b\u00a0')" \
  -o font_alexandria_16.c

# 3. Fix include path
sed -i 's|#include "lvgl/lvgl.h"|#include <lvgl.h>|' font_alexandria_16.c
```

Key options:
- `--range 0x0020-0x007F` — full ASCII (digits, Latin, punctuation)
- `--range 0x0600-0x06FF` — Arabic base block
- `--range 0xFB50-0xFDFF,0xFE70-0xFEFF` — Arabic Presentation Forms (contextual shapes)
- `--symbols "\u200c\u200d\u200b\u00a0"` — zero-width control chars the shaper needs

---

## Navigation Architecture

### Ring (circular, swipe)

```
   ┌──────┐   swipe left    ┌───────────┐   swipe left    ┌──────────┐
   │ HOME │ ──────────────→ │ ISTIGHFAR │ ──────────────→ │ TASBEEH  │
   │      │ ←────────────── │           │ ←────────────── │          │
   └──────┘   swipe right   └───────────┘   swipe right   └──────────┘
```

3 screens in a circular buffer. `navigate_ring(+1)` and `navigate_ring(-1)` wrap
around with `% RING_LEN`. Swipe-left = next screen (MOVE_LEFT animation),
swipe-right = previous (MOVE_RIGHT animation).

### Modal (overlay, swipe-down/left to dismiss)

```
   ┌──────────┐                ┌──────────────┐
   │ SETTINGS │ ← gear / ↓     │ TIMEEDIT     │ ← long-press clock
   │ swipe ↓  │                │ < swipe ↓/←  │
   │ tap bg   │                │ tap < or title│
   └──────────┘                └──────────────┘
```

`push_modal(dest)` remembers the current screen, slides modal up.
`pop_modal()` slides back down. Swipe-down, swipe-left, or back arrow dismiss.

TimeEdit no longer has background-tap-to-dismiss — user must tap the back arrow `<`
or title to cancel.

### Safe event handling

Every button callback checks `lv_screen_active()` before acting — prevents
animation-race bugs where a tap during a 200ms transition lands on the wrong screen.

---

## Battery Monitoring

- **Pin**: GPIO1 (ADC1_CH0) via 200K + 100K voltage divider (ratio 3:1)
- **Method**: `analogReadMilliVolts()` — uses ESP32-S3 factory ADC calibration
- **Formula**: V_bat = V_adc × 3.0, mapped 3.5V→0% to 4.15V→100%
- **Update**: Every 5 seconds, displayed at top-left of home screen
- **Color**: Red below 20%, grey otherwise

---

## Tasbeeh Phrase Persistence

The tasbeeh counter remembers both the **count** and the **current phrase**
(سبحان الله / الحمد لله / الله أكبر) across reboots via NVS:

- `tasbeeh` key: count (uint32)
- `tasbeeh_phrase` key: phrase index (int, 0–2)

Saved when the phrase advances (at 33) and loaded on boot.

---

## Performance Optimizations

| Optimization | What | Where |
|---|---|---|
| Double buffering | 2× 57KB buffers (partial mode) — renders into buf2 while buf1 flushes | `main.cpp` |
| 80 MHz SPI | `-D SPI_FREQUENCY=80000000` in build flags | `platformio.ini` |
| `-O2` optimization | Compiler optimizes for speed not size | `platformio.ini` |
| Flush batching | SPI transaction stays open across dirty rectangles within a frame | `main.cpp` flush callback |
| `delay(1)` in loop | Minimal idle between LVGL renders | `main.cpp` |
| Circle cache 16 | Anti-alias cache for round display elements | `lv_conf.h` |
| FPS counter off | `LV_USE_SYSMON=0`, `LV_USE_PERF_MONITOR=0` | `lv_conf.h` |

Current: **RAM 60.4%** (198KB / 328KB), **Flash ~26%** (815KB / 3.1MB).

---

## Color Palette

| Role | Hex | Used in |
|------|-----|---------|
| Background | `#0b1410` | All screens — deep dark green-black |
| Surface | `#0e2820` | Cards, elevated containers |
| Gold | `#d4af37` | Home: day name, seconds arc, AM/PM, arrows |
| Ivory | `#f6e6b3` | Clock digits, counter values, phrases |
| Cream | `#e9d9a8` | Body text, date |
| Cream dim | `#7a6e56` | Secondary text, hints |
| Teal/Green | `#33cc55` | Istighfar: title, arc. Save button |
| Blue | `#7fd6a3` | Tasbeeh: title, arc, active dot |
| Red | `#ff4444` | Battery warning |

---

## TimeEdit — 12-Hour with AM/PM

```
       <  ضبط الوقت

       [ HH ]  :  [ MM ]  [ص/م]
       [ DD ]  /  [ MM ]  /  [ YYYY ]

         ╔══╗  ╔══════╗  ╔══╗
         ║− ║  ║ حفظ  ║  ║+ ║
         ╚══╝  ╚══════╝  ╚══╝
```

**Features:**
- 12-hour format with AM/PM toggle (ص/م, field index 5, selectable via +/−)
- 24h↔12h conversion on open/save
- Active field highlighted teal with inverted text
- Per-field validation (hour 1–12, minute 0–59, day 1–31, month 1–12, year 2024–2099)
- Back arrow `<` + title tap dismisses without saving
- Layout respects circular display chord limits

---

## LVGL Configuration

Key settings in `src/config/lv_conf.h`:

```c
#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (64 * 1024)
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_BIDI 1
#define LV_BIDI_BASE_DIR_DEF LV_BASE_DIR_AUTO
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_TFT_ESPI 1
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_FONT_MONTSERRAT_14 1    // Gear icon, swipe arrows
#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 16
#define LV_USE_SYSMON 0            // FPS counter disabled
#define LV_USE_PERF_MONITOR 0
```

## Known Patches to LVGL Library

**`lv_text_ap.c:212`** — Arabic shaper: when a character has no conjunction in either
direction (standalone), keep the original base-form character instead of converting
to the isolated presentation form. This fixes missing isolated-form glyphs (ص/م)
in the Alexandria font. Path:

```
.pio/libdeps/waveshare_esp32s3_touch_lcd_128/lvgl/src/misc/lv_text_ap.c
```

**`font_alexandria_28.c`** / **`font_alexandria_12.c`** — `glyph_id_ofs_list_7`:
Entries for U+FEB9 (index 57) and U+FEE1 (index 97) changed from `0` to match
their final-form counterparts (`44` and `74`), pointing to glyph_ids 318 and 348.

---

## Build & Upload

### Prerequisites

1. Install [PlatformIO](https://platformio.org/) (`pip install platformio`)
2. Connect the Waveshare board via USB
3. Enter download mode: hold BOOT, press RESET, release BOOT

### Build

```bash
pio run
```

### Upload & Monitor

```bash
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

---

## Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| lvgl/lvgl | 9.2.0 | Graphics framework |
| Bodmer/TFT_eSPI | 2.5.43 | Display driver (GC9A01, SPI) |
| fbiego/CST816S | 1.1.1 | Touch driver (I2C) |
| Preferences | — | Counter + time + phrase persistence (NVS) |

WiFi, WebServer, WiFiManager, and ArduinoJson were removed — the device is fully
offline. All state persists via ESP32 Preferences (NVS flash storage).

---

## Hardware

- **Board**: Waveshare ESP32-S3-Touch-LCD-1.28
- **MCU**: ESP32-S3 (240 MHz, 320KB SRAM, 16MB Flash, 2MB PSRAM)
- **Display**: 1.28" round TFT, 240×240, GC9A01, SPI (80 MHz)
- **Touch**: CST816S capacitive, I2C (pins 6/7)
- **Battery**: ADC pin 1 (GPIO1), 200K+100K voltage divider (3:1 ratio), ETA6096 charger
