#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Debug macro — set to 1 to enable trace, 0 to silence
#define NAV_DEBUG 0

// ── Ring order: Home → Istighfar → Tasbeeh ──────────────────
#define RING_LEN  3

extern lv_obj_t *ring_screens[RING_LEN];

// ── Reminders (shared: main.cpp owns storage/firing, UI reads/writes it) ──
#define MAX_REMINDERS 10
#define REMINDER_DAYS_ALL 0x7F
struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
    uint8_t days;   // bitmask, bit0=Sun .. bit6=Sat. 0x7F = every day.
};
extern Reminder reminders[MAX_REMINDERS];
void saveReminders(void);

// ── Deep-sleep test (bench measurement only — see main.cpp) ────
// Single master switch: flip to 0 and reflash to remove this feature
// completely — the trigger in screen_settings.cpp and its implementation
// in main.cpp both compile out, nothing left running either side.
// Currently OFF — kept in the codebase, not wired up while the Settings
// back-button long-press is doing lockdown testing instead (see below).
#define DEEP_SLEEP_TEST_ENABLED 0
#if DEEP_SLEEP_TEST_ENABLED
void enterDeepSleepTest(void);
#endif

// ── Battery lockdown test (bench tool — see main.cpp) ───────────
// Same "disable at any time" pattern as DEEP_SLEEP_TEST_ENABLED above:
// flip to 0 and reflash to remove entirely. Forces in_lockdown regardless
// of real battery level, so it can be tested without actually running the
// battery down — escape hatch is the physical RESET button (a real reset,
// not a deep-sleep wake, reinitializes RTC_DATA_ATTR back to false), same
// guarantee the deep-sleep test relied on.
#define LOCKDOWN_TEST_ENABLED 1
#if LOCKDOWN_TEST_ENABLED
void enterLockdownTest(void);
// Long-press anywhere on the lockdown screen to bail out early — only takes
// effect if this lockdown was itself test-forced (enterLockdownTest()); a
// real low-battery lockdown stays inescapable except by actually charging.
void requestLockdownTestExit(void);
#endif

// Ring screens
extern lv_obj_t *scr_home;
extern lv_obj_t *scr_istighfar;
extern lv_obj_t *scr_tasbeeh;

// Modal screens
extern lv_obj_t *scr_settings;
extern lv_obj_t *scr_timeedit;
extern lv_obj_t *scr_notifications;

// Battery lockdown screen (<5%) — deliberately outside the normal
// navigation graph, see main.cpp and screen_lockdown.cpp.
extern lv_obj_t *scr_lockdown;
void create_screen_lockdown(void);

// ── Home / Clock screen widgets ───────────────────────────────
extern lv_obj_t *home_day_label;
extern lv_obj_t *home_clock_label;
extern lv_obj_t *home_date_label;
extern lv_obj_t *home_prayer_label;
extern lv_obj_t *home_bat_label;
extern lv_obj_t *home_sec_arc;
extern lv_obj_t *home_ampm_label;
extern lv_obj_t *home_sec_label;
extern int current_ring_idx;

// ── Istighfar screen widgets ──────────────────────────────────
extern lv_obj_t *istighfar_arc;
extern lv_obj_t *istighfar_count_label;
extern uint32_t  istighfarCount;
extern uint32_t  totalIstighfar;

// ── Tasbeeh screen widgets ────────────────────────────────────
extern lv_obj_t *tasbeeh_arc;
extern lv_obj_t *tasbeeh_count_label;
extern lv_obj_t *tasbeeh_phrase_label;
extern lv_obj_t *tasbeeh_title_label;
extern lv_obj_t *tasbeeh_of_label;
extern lv_obj_t *tasbeeh_dots[3];
extern uint32_t  tasbeehCount;
extern uint32_t  totalTasbeeh;
extern int       tasbeeh_phrase_idx;

// ── TimeEdit ──────────────────────────────────────────────────
extern lv_obj_t *te_hour_label;
extern lv_obj_t *te_min_label;
extern lv_obj_t *te_day_label;
extern lv_obj_t *te_mon_label;
extern lv_obj_t *te_year_label;
extern int timeedit_hour, timeedit_min, timeedit_day, timeedit_month, timeedit_year, timeedit_field;
extern int timeedit_ampm;
void update_timeedit_highlight(void);
void resetClockTick(void);

// timeedit doubles as a reminder-time/day editor: CLOCK mode edits the
// system clock (existing behavior, HH:MM + date); REMINDER mode edits
// reminders[timeedit_reminder_idx]'s HH:MM + day-of-week mask instead
// (no date fields). Don't set these directly — call open_timeedit_clock()
// or open_timeedit_reminder() below, which populate everything and push.
enum { TIMEEDIT_MODE_CLOCK = 0, TIMEEDIT_MODE_REMINDER = 1 };
extern int timeedit_mode;
extern int timeedit_reminder_idx;
extern uint8_t timeedit_days;   // working copy of the day-of-week mask, REMINDER mode only
void open_timeedit_clock(void);       // populate from hour_/minute_/day_/month_/year_, then push_modal
void open_timeedit_reminder(int idx); // populate from reminders[idx], then push_modal

// ── Navigation helpers ────────────────────────────────────────
void navigate_to_ring(int idx);
void navigate_ring(int delta);
void gesture_ring_cb(lv_event_t *e);
void push_modal(lv_obj_t *dest);
void pop_modal(void);
void gesture_modal_cb(lv_event_t *e);
void modal_bg_tap_cb(lv_event_t *e);

// ── Screen builders ───────────────────────────────────────────
void screens_init(void);
void create_screen_home(void);
void create_screen_istighfar(void);
void create_screen_tasbeeh(void);
void create_screen_settings(void);
void create_screen_timeedit(void);
void create_screen_notifications(void);
void refresh_notifications_list(void);

// ── Update helpers (called from main timer) ───────────────────
void update_home_clock(void);
void update_istighfar_display(void);
void update_tasbeeh_display(void);

// ── Shared structs ────────────────────────────────────────────
typedef struct {
    lv_obj_t  *arc;
    lv_obj_t  *counter_label;
    lv_color_t revert_color;
} FlashData;

// ── Shared helpers ────────────────────────────────────────────
void make_screen_base(lv_obj_t *scr);
lv_obj_t *create_title(lv_obj_t *parent, const char *text);
void touch_debug_cb(lv_event_t *e);
void revert_arc_color_cb(lv_timer_t *timer);
