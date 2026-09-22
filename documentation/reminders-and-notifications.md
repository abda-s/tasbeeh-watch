# Reminders & Notifications

> The preset reminder system: data model, config table, UI and tap-target tuning.
>
> [← Documentation index](README.md) · [Project README](../README.md)

---

## Reminders / Notifications (V1.4)

A settings sub-page listing preset daily reminders (this is a children's watch —
routine nudges like "drink water" or "bedtime", not prayer times) that can each be
individually enabled, retimed, and set to repeat on chosen days of the week.
**Implemented and builds clean; not yet verified on hardware.**

### Data model — `main.cpp`

```cpp
struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
    uint8_t days;   // bitmask, bit0=Sun .. bit6=Sat. 0x7F = every day.
};
```

`weekday_from_date()` (Sakamoto's algorithm) derives day-of-week from
`day_/month_/year_` — nothing previously tracked that, only the calendar date.
`checkReminders()` now also requires `(reminders[i].days >> today) & 1` before
firing.

**A light-sleep interaction bug found while wiring this up:** `checkReminders()`
used to also require `second_ == 0` to fire. That's harmless while awake (the 1Hz
clock timer guarantees it's checked exactly at `:00`), but while the screen is
asleep, `checkReminders()` only runs at sparse wake events — a touch, or every
`SLEEP_WAKE_INTERVAL_US` (60s) — which are essentially never phase-aligned to an
exact second. A reminder could go the entire time the watch was asleep (which is
most of the time, by design) without its firing instant ever landing on `second_
== 0`, so it would silently never fire. Fixed by dropping that check — the
existing `reminderFired[]` edge-flag already dedups correctly (fires once on
entering the target minute, resets once the minute passes) regardless of *when*
within the minute it's observed, so it doesn't need the exact-second gate. Worst-case
firing latency while asleep is now bounded by the 60s wake cadence, which is fine
for routine reminders — deliberately didn't tighten that interval, since it exists
to bound RC-oscillator drift (see [Power Management](power-management.md)) and tightening it would
cost the battery life the rest of this project has been optimizing for.

### Preset table — `config/reminders_config.h`

The **only place** to edit to change what shows up on the notifications screen:

```cpp
static const ReminderPreset REMINDER_PRESETS[] = {
    {"شرب الماء",       10, 0},  // drink water
    {"غسل اليدين",      12, 0},  // wash hands
    {"وقت الواجب",      17, 0},  // homework time
    {"تنظيف الأسنان",    20, 0},  // brush teeth
    {"وقت النوم",       21, 0},  // bedtime
};
```

Rename, retime, add, or delete rows freely (capped at `MAX_REMINDERS` = 10 in
`screens.h` — a `static_assert` fails the build if exceeded), then bump
`REMINDER_PRESET_VERSION` by 1 and reflash — that version bump is what tells the
watch to re-sync from the table; without it, a device that's already been
flashed once keeps whatever it already saved in NVS and won't notice the table
changed. On a version bump, a preset still present at the same position keeps
whatever time/enabled/days you'd already set *on the watch* — only its label
text always refreshes (so renames take effect), and only genuinely new rows get
the table's default hour/minute. Rows removed from the table get cleared out.
`screen_notifications.cpp`'s row count comes directly from this table
(`REMINDER_PRESET_COUNT`), so nothing else needs updating to add or remove one.

### UI — `screen_notifications.cpp` + `screen_timeedit.cpp` (reused)

```
       <  الإشعارات

   ┌──────────────────────┐
   │ شرب الماء     10:00 ○│
   │ غسل اليدين    12:00 ○│  ← scrollable —
   │ وقت الواجب    17:00 ●│    5 rows don't fit
   │ تنظيف الأسنان  20:00 ○│    statically in the
   │ وقت النوم     21:00 ○│    round safe area
   └──────────────────────┘
```

Tapping a row's switch toggles it on/off in place (no navigation). Tapping
anywhere else on the row opens the time editor for that reminder. Rather than
build a second time-picker screen, `screen_timeedit.cpp` gained a
`TIMEEDIT_MODE_REMINDER` mode alongside its original `TIMEEDIT_MODE_CLOCK`:

- **Clock mode** (`open_timeedit_clock()`): unchanged — HH:MM AM/PM + DD/MM/YYYY,
  writes to the system clock.
- **Reminder mode** (`open_timeedit_reminder(idx)`): same HH:MM AM/PM fields,
  but the date row is hidden and replaced with 7 day-of-week toggle circles
  (ح ن ث ر خ ج س — Sun..Sat), and Save writes into `reminders[idx]` instead.

The screen is built once at boot and reused (like every other modal here), so
these two entry points populate all the labels/toggle states and show/hide the
right row before `push_modal()` — centralizing that logic instead of duplicating
it in every caller like the old clock-only version did.

### Settings screen changes

Long-press-the-clock (the old way into the time editor) was removed entirely —
Settings now has two explicit entries instead: **تغيير الوقت** (change time) and
**الإشعارات** (notifications), both reusing `open_timeedit_*()`/`push_modal()`.

### Hitbox tuning

Two rounds of "my tap didn't do what I expected" fixes, both via
`lv_obj_set_ext_click_area()` (extends the *hit-test* region without changing
layout or appearance):

- **Header bars** (Settings title, TimeEdit/Notifications `<`+title): these were
  individually-clickable labels, so the tappable area was only the tight glyph
  bounds of a single `<` character or a short title — easy to miss. Made the
  whole header row the click target instead (plus a few px of extra margin),
  on all three screens.
- **Notification row switches**: at their native 38×20 size, taps meant for the
  switch were landing on the row instead (opening the time editor by mistake).
  Extended the switch's hit area by 8px on each side — capped there deliberately:
  rows are only 40px apart center-to-center (34px row + 6px gap), so anything
  bigger would make adjacent rows' switches overlap and risk toggling the wrong
  reminder, which is worse than the mis-tap being fixed.
