# User Interface

> Screens, gestures, navigation model, colors and the time editor.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Screens

Layout sketch of the three main screens:

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
**Swipe arrows** (`◂` / `▸`) at screen edges indicate more screens (static — the
sway animation was removed in V1.21: it forced full-rate redraws forever and
measured ~10% of screen-on battery draw).
**Modals:** Settings (tap gear or swipe-down from home) → Change time / Notifications
entries inside it — swipe down or left to dismiss (Settings: tap the title to go back
too; TimeEdit/Notifications: tap `<` or the title).

### Screen accent colors

| Screen | Color | Hex | Arc / Title |
|--------|-------|-----|-------------|
| Home | Gold | `#d4af37` | Seconds arc, day name, AM/PM |
| Istighfar | Green | `#33cc55` | Progress arc, title |
| Tasbeeh | Blue | `#7fd6a3` / `#3b82f6` | Progress arc, title, dots |

### Gesture Map

- **Ring screens**: Swipe L/R = navigate ring, swipe **down** (home only) = settings
- **Settings**: tap **تغيير الوقت** = time edit, tap **الإشعارات** = notifications list,
  tap title or `< رجوع` = back (long-press-the-clock was removed — Settings is now
  the only way in, see [Reminders & Notifications](reminders-and-notifications.md))
- **Modals**: Swipe down = back, swipe **left** = back (Settings/TimeEdit/Notifications:
  tap the title bar also goes back — Settings no longer dismisses on a random
  background tap, only the title, since that was misfiring when tapping an option
  near its edge)
- **Notifications list**: tap a row = edit its time/days, tap its switch = toggle
  enabled without navigating
- **Tasbeeh/Istighfar**: Tap anywhere = increment counter

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
   ┌──────────┐   ┌───────────────┐   ┌──────────────┐
   │ SETTINGS │──►│ NOTIFICATIONS │──►│ TIMEEDIT     │
   │ ← gear/↓ │   │ ← تغيير الوقت │   │ (reminder    │
   │ tap title│   │ tap title/<   │   │  or clock    │
   │ or < back│   │ swipe ↓/← back│   │  mode)       │
   └──────────┘   └───────────────┘   │ tap </title  │
        │                             │ swipe ↓/← back│
        └────────────────────────────►└──────────────┘
              تغيير الوقت (clock mode)
```

`push_modal(dest)` remembers the current screen, slides modal up.
`pop_modal()` slides back down. Swipe-down, swipe-left, or the header bar dismiss.
Since `pop_modal()` just returns to whichever screen was active when it was pushed,
this chains correctly to any depth — Settings → Notifications → TimeEdit → back →
back → back all land where you'd expect.

Settings no longer dismisses on a random background tap (that was misfiring when a
tap near — but not quite on — an option button registered as "go back" instead);
only its title is a back target now. TimeEdit and Notifications both dismiss on a
background tap still, and on tapping their header (back arrow `<` or title) — that
header's *whole* row is now the click target, not just the tight glyph bounds of
the `<`/title text, which were too small to reliably hit.

### Safe event handling

Every button callback checks `lv_screen_active()` before acting — prevents
animation-race bugs where a tap during a 200ms transition lands on the wrong screen.

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

This is clock mode specifically (system time). The same screen also has a
reminder-editing mode now — see [Reminders & Notifications](reminders-and-notifications.md).

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
- Header bar (back arrow `<` + title, whole row clickable) dismisses without saving
- Layout respects circular display chord limits
