#pragma once
#include "ui/screens.h"   // for MAX_REMINDERS, REMINDER_DAYS_ALL

// ═══════════════════════════════════════════════════════════════
//  Reminder presets — the ONLY place you need to edit to change
//  what shows up on the notifications screen (Settings > الإشعارات).
//
//  To rename one:  edit its label text below.
//  To retime one:  edit its hour/minute below.
//  To add one:     add a row (capped at MAX_REMINDERS, currently 10, in
//                   ui/screens.h — the compiler will refuse to build if
//                   you go over that).
//  To remove one:  delete its row.
//
//  After editing, bump REMINDER_PRESET_VERSION by 1 and reflash. That's
//  what tells the watch "the preset table changed, re-sync it" — without
//  it, a device that's already been flashed once will keep using
//  whatever it already saved and won't notice the table changed.
//
//  Existing reminders that are still present (by position) keep whatever
//  time/enabled/days you'd already set on the watch itself — only the
//  label text is refreshed, and only genuinely new rows get these
//  hour/minute defaults. Rows you delete are cleared out.
// ═══════════════════════════════════════════════════════════════

#define REMINDER_PRESET_VERSION 1

struct ReminderPreset {
    const char *label;
    int hour, minute;   // default time, 24h — only applied the first time a row is added
};

static const ReminderPreset REMINDER_PRESETS[] = {
    {"شرب الماء",       10, 0},  // drink water
    {"غسل اليدين",      12, 0},  // wash hands
    {"وقت الواجب",      17, 0},  // homework time
    {"تنظيف الأسنان",    20, 0},  // brush teeth
    {"وقت النوم",       21, 0},  // bedtime
};

#define REMINDER_PRESET_COUNT (sizeof(REMINDER_PRESETS) / sizeof(REMINDER_PRESETS[0]))

static_assert(REMINDER_PRESET_COUNT <= MAX_REMINDERS,
    "Too many reminder presets — raise MAX_REMINDERS in screens.h or trim the table");
