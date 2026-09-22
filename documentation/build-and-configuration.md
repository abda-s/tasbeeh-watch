# Build & Configuration

> Building and flashing, dependencies, LVGL configuration and patches to vendored libraries.
>
> [← Documentation index](README.md) · [Project README](../README.md)

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
| Wire | — | Raw I2C write to power down the unused onboard IMU |

WiFi, WebServer, WiFiManager, and ArduinoJson were removed — the device is fully
offline. All state persists via ESP32 Preferences (NVS flash storage).

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

---

## Known Patches to Vendored Libraries

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

**`CST816S.h`/`CST816S.cpp`** — added a `rearm(int interrupt = RISING)` method:
just re-attaches the touch IRQ interrupt, without `begin()`'s hardware reset
sequence (`delay(50)+delay(5)+delay(50)`, ~110ms) or its two I2C version
reads. Used in `wake_display()` instead of `begin()` — added for the V1.32
wake-latency work, see [Power Management](power-management.md). Path:

```
.pio/libdeps/waveshare_esp32s3_touch_lcd_128/CST816S/CST816S.h
.pio/libdeps/waveshare_esp32s3_touch_lcd_128/CST816S/CST816S.cpp
```

**Note:** all patches above live in `.pio/libdeps/`, which PlatformIO can
regenerate from the pinned version in `platformio.ini` on a clean install —
if that happens, these need to be reapplied by hand from the descriptions above.
