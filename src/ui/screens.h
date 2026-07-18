#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Debug macro — set to 1 to enable trace, 0 to silence
#define NAV_DEBUG 0

// ── Ring order: Home → Istighfar → Tasbeeh ──────────────────
#define RING_LEN  3

extern lv_obj_t *ring_screens[RING_LEN];

// Ring screens
extern lv_obj_t *scr_home;
extern lv_obj_t *scr_istighfar;
extern lv_obj_t *scr_tasbeeh;

// Modal screens
extern lv_obj_t *scr_settings;
extern lv_obj_t *scr_timeedit;

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

// ── Update helpers (called from main timer) ───────────────────
void update_home_clock(void);
void update_istighfar_display(void);
void update_tasbeeh_display(void);

// ── Home screen swipe-hint animation (paused during display sleep) ────
void pause_home_swipe_hint(void);
void resume_home_swipe_hint(void);

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
