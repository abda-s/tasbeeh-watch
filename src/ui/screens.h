#pragma once
#include <lvgl.h>

// Debug macro — set to 1 to enable trace, 0 to silence
#define NAV_DEBUG 1

// ── Screen pointers ──────────────────────────────────────────
extern lv_obj_t *scr_home;
extern lv_obj_t *scr_thiker;
extern lv_obj_t *scr_tasbeeh;
extern lv_obj_t *scr_isteghfar;
extern lv_obj_t *scr_settings;
extern lv_obj_t *scr_timeedit;

// ── Widget pointers ──────────────────────────────────────────
extern lv_obj_t *home_clock_label;
extern lv_obj_t *home_date_label;
extern lv_obj_t *home_bat_label;
extern lv_obj_t *tasbeeh_arc;
extern lv_obj_t *tasbeeh_counter_label;
extern lv_obj_t *tasbeeh_total_label;
extern lv_obj_t *isteghfar_arc;
extern lv_obj_t *isteghfar_counter_label;
extern lv_obj_t *isteghfar_total_label;
extern lv_obj_t *settings_ip_label;

// ── Public API ───────────────────────────────────────────────
void screens_init(void);
void update_home_clock(void);
void update_home_battery(void);
void update_tasbeeh_display(void);
void update_isteghfar_display(void);

// ═══════════════════════════════════════════════════════════════
//  Internal — shared across screen implementation files
// ═══════════════════════════════════════════════════════════════

// ── Counter type ─────────────────────────────────────────────
enum CounterType { COUNTER_TASBEEH, COUNTER_ISTEGHFAR };

// ── Arc flash data (heap-allocated per flash) ────────────────
typedef struct {
    lv_obj_t  *arc;
    lv_obj_t  *counter_label;
    lv_color_t revert_color;
} FlashData;

void revert_arc_color_cb(lv_timer_t *timer);

// ── Screen builder helpers ───────────────────────────────────
void make_screen_base(lv_obj_t *scr);
lv_obj_t *create_title(lv_obj_t *parent, const char *text);
void touch_debug_cb(lv_event_t *e);

// ── Ring navigation ──────────────────────────────────────────
extern lv_obj_t *ring_screens[4];
#define RING_LEN 4

void navigate_ring(int delta);
void navigate_to_ring(int idx);
void gesture_ring_cb(lv_event_t *e);

// ── Modal push/pop ───────────────────────────────────────────
void push_modal(lv_obj_t *dest);
void pop_modal(void);
void gesture_modal_cb(lv_event_t *e);
void modal_bg_tap_cb(lv_event_t *e);

// ── TimeEdit state (shared with clock_tap_cb in home) ────────
extern int timeedit_hour, timeedit_min, timeedit_day, timeedit_month, timeedit_year;
extern int timeedit_field;
extern lv_obj_t *te_hour_label, *te_min_label;
extern lv_obj_t *te_day_label, *te_mon_label, *te_year_label;

// ── Screen create functions (called from screens_init) ───────
void create_screen_home(void);
void create_screen_thiker(void);
void create_screen_tasbeeh(void);
void create_screen_isteghfar(void);
void create_screen_settings(void);
void create_screen_timeedit(void);
