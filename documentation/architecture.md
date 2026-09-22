# Architecture

> Source layout, data persistence and rendering performance.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Project Structure

```
src/
├── main.cpp                      # Hardware init, LVGL setup, timers, loop, battery
├── config/
│   ├── CST816S_pin_config.h      # Touch I2C pin definitions
│   ├── lv_conf.h                 # LVGL v9 build configuration
│   └── reminders_config.h        # Reminder presets table — edit here to add/remove/rename notifications
├── ui/
│   ├── screens.h                 # Public API — screen pointers, nav, fonts, helpers
│   ├── screens.cpp               # Global pointers + screens_init()
│   ├── styles.h / styles.cpp     # Color palette + style definitions
│   ├── nav.cpp                   # Ring navigation + modal push/pop + gesture handlers
│   ├── screen_base.cpp           # Shared helpers (make_screen_base, create_title, touch_debug)
│   ├── screen_home.cpp           # Clock screen: 12h stacked time + AM/PM, day, date, seconds arc, swipe arrows
│   ├── screen_istighfar.cpp      # Istighfar counter: tap to count, 0→100 green arc
│   ├── screen_tasbeeh.cpp        # Tasbeeh counter: 3 phrases × 33, blue arc, progress dots, phrase persists across boot
│   ├── screen_settings.cpp       # Settings: تغيير الوقت + الإشعارات entries, title-tap back
│   ├── screen_timeedit.cpp       # 12h time editor — dual mode: system clock (HH:MM AM/PM, DD/MM/YYYY) or a reminder (HH:MM AM/PM + day-of-week toggles)
│   ├── screen_notifications.cpp  # Scrollable list of reminder presets — enable switch + tap-to-edit
│   └── screen_lockdown.cpp       # Battery <5% lockdown message — outside the normal nav graph, see [Deep Sleep & Battery Lockdown](deep-sleep-and-lockdown.md)
├── font_reem_kufi_72.c           # Clock digits — Reem Kufi 72px 4bpp
├── font_reem_kufi_48.c           # Counter digits — Reem Kufi 48px 4bpp
├── font_alexandria_12.c          # Small labels, AM/PM — Alexandria 12px 2bpp
├── font_alexandria_16.c          # Titles, day names, hints, theme — Alexandria 16px 2bpp
└── font_alexandria_28.c          # Arabic phrases — Alexandria 28px 4bpp
```

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
| 40 MHz SPI | `SPI_FREQUENCY 40000000` in the TFT_eSPI setup header | `Setup302_...GC9A01.h` |
| `-O2` optimization | Compiler optimizes for speed not size | `platformio.ini` |
| Flush batching | SPI transaction stays open across dirty rectangles within a frame | `main.cpp` flush callback |
| Adaptive idle | `loop()` sleeps until LVGL's next scheduled timer (`lv_timer_handler()` return value) instead of a fixed short delay | `main.cpp` |
| Circle cache 16 | Anti-alias cache for round display elements | `lv_conf.h` |
| FPS counter off | `LV_USE_SYSMON=0`, `LV_USE_PERF_MONITOR=0` | `lv_conf.h` |

Current: **RAM 60.4%** (198KB / 328KB), **Flash ~24%** (753KB / 3.1MB).
