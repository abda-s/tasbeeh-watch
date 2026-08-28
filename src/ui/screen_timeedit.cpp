#include "screens.h"
#include "styles.h"
#include <Preferences.h>
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);
LV_FONT_DECLARE(font_alexandria_16);

extern Preferences prefs;
extern int hour_, minute_, second_, day_, month_, year_;

int timeedit_hour, timeedit_min, timeedit_day, timeedit_month, timeedit_year;
int timeedit_field = 0;
int timeedit_ampm  = 0;

int     timeedit_mode = TIMEEDIT_MODE_CLOCK;
int     timeedit_reminder_idx = 0;
uint8_t timeedit_days = REMINDER_DAYS_ALL;

lv_obj_t *te_hour_label = NULL, *te_min_label = NULL;
lv_obj_t *te_day_label = NULL, *te_mon_label = NULL, *te_year_label = NULL;
lv_obj_t *te_ampm_label = NULL;

static lv_obj_t *te_conts[6];
static lv_obj_t *te_title_label = NULL;

// Date row (day/month/year fields + their "/" separators) — CLOCK mode only.
static lv_obj_t *te_date_row_objs[5];
#define TE_DATE_ROW_N 5

// Day-of-week toggles (Sun..Sat) — REMINDER mode only.
static lv_obj_t *te_days_cont = NULL;
static lv_obj_t *te_day_toggles[7];
static const char *DAY_INITIALS_AR[7] = {
    "ح", "ن", "ث", "ر", "خ", "ج", "س"
    // Sun  Mon  Tue  Wed  Thu  Fri  Sat
};

void update_timeedit_highlight(void) {
    lv_obj_t **labels[6] = {&te_hour_label, &te_min_label, &te_day_label, &te_mon_label, &te_year_label, &te_ampm_label};
    for (int i = 0; i < 6; i++) {
        if (!te_conts[i]) continue;
        if (i == timeedit_field) {
            lv_obj_set_style_bg_color(te_conts[i], color_teal, 0);
            lv_obj_set_style_text_color(*(labels[i]), color_bg, 0);
        } else {
            lv_obj_set_style_bg_color(te_conts[i], color_surface, 0);
            lv_obj_set_style_text_color(*(labels[i]), color_ivory, 0);
        }
    }
}

static void timeedit_up_cb(lv_event_t *e) {
    if (timeedit_field == 5) {
        timeedit_ampm = !timeedit_ampm;
        lv_label_set_text(te_ampm_label, timeedit_ampm ? "\331\205" : "\330\265");
        return;
    }
    int max_vals[5] = {12, 59, 31, 12, 2099};
    int min_vals[5] = {1, 0, 1, 1, 2024};
    int *vals[5] = {&timeedit_hour, &timeedit_min, &timeedit_day, &timeedit_month, &timeedit_year};
    lv_obj_t **labels[5] = {&te_hour_label, &te_min_label, &te_day_label, &te_mon_label, &te_year_label};
    const char *fmts[5] = {"%02d", "%02d", "%02d", "%02d", "%04d"};
    int f = timeedit_field;
    (*(vals[f]))++;
    if (*(vals[f]) > max_vals[f]) *(vals[f]) = min_vals[f];
    lv_label_set_text_fmt(*(labels[f]), fmts[f], *(vals[f]));
}

static void timeedit_down_cb(lv_event_t *e) {
    if (timeedit_field == 5) {
        timeedit_ampm = !timeedit_ampm;
        lv_label_set_text(te_ampm_label, timeedit_ampm ? "\331\205" : "\330\265");
        return;
    }
    int max_vals[5] = {12, 59, 31, 12, 2099};
    int min_vals[5] = {1, 0, 1, 1, 2024};
    int *vals[5] = {&timeedit_hour, &timeedit_min, &timeedit_day, &timeedit_month, &timeedit_year};
    lv_obj_t **labels[5] = {&te_hour_label, &te_min_label, &te_day_label, &te_mon_label, &te_year_label};
    const char *fmts[5] = {"%02d", "%02d", "%02d", "%02d", "%04d"};
    int f = timeedit_field;
    (*(vals[f]))--;
    if (*(vals[f]) < min_vals[f]) *(vals[f]) = max_vals[f];
    lv_label_set_text_fmt(*(labels[f]), fmts[f], *(vals[f]));
}

static void timeedit_select_field_cb(lv_event_t *e) {
    timeedit_field = (int)(uintptr_t)lv_event_get_user_data(e);
    update_timeedit_highlight();
}

static void timeedit_day_toggle_cb(lv_event_t *e) {
    int bit = (int)(uintptr_t)lv_event_get_user_data(e);
    timeedit_days ^= (1 << bit);
    bool on = timeedit_days & (1 << bit);
    lv_obj_set_style_bg_color(te_day_toggles[bit], on ? color_teal : color_surface, 0);
}

static void timeedit_save_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_timeedit) return;

    int h24;
    if (timeedit_ampm == 0)
        h24 = (timeedit_hour == 12) ? 0 : timeedit_hour;
    else
        h24 = (timeedit_hour == 12) ? 12 : timeedit_hour + 12;

    if (timeedit_mode == TIMEEDIT_MODE_REMINDER) {
        reminders[timeedit_reminder_idx].hour   = h24;
        reminders[timeedit_reminder_idx].minute = timeedit_min;
        reminders[timeedit_reminder_idx].days   = timeedit_days;
        saveReminders();
        refresh_notifications_list();
        pop_modal();
        return;
    }

    hour_   = h24;
    minute_ = timeedit_min;
    second_ = 0;
    day_    = timeedit_day;
    month_  = timeedit_month;
    year_   = timeedit_year;
    prefs.putInt("hour", hour_);
    prefs.putInt("minute", minute_);
    prefs.putInt("second", second_);
    prefs.putInt("day", day_);
    prefs.putInt("month", month_);
    prefs.putInt("year", year_);
    resetClockTick();
    update_home_clock();
    pop_modal();
}

static void timeedit_cancel_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_timeedit) return;
    pop_modal();
}

static lv_obj_t *make_field(lv_obj_t *scr, int x, int y, int w, int h,
    lv_obj_t **label_out, const char *font_name, const char *fmt, int val, int field_idx)
{
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, w, h);
    lv_obj_align(cont, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cont, 6, 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_border_color(cont, color_border, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont, timeedit_select_field_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)field_idx);

    *label_out = lv_label_create(cont);
    if (strcmp(font_name, "28") == 0)
        lv_obj_set_style_text_font(*label_out, &font_alexandria_28, 0);
    else
        lv_obj_set_style_text_font(*label_out, &font_alexandria_12, 0);
    lv_label_set_text_fmt(*label_out, fmt, val);
    lv_obj_center(*label_out);
    return cont;
}

void create_screen_timeedit(void) {
    scr_timeedit = lv_obj_create(NULL);
    make_screen_base(scr_timeedit);

    // ── Title + back arrow (flex container, centered as one unit) ─
    lv_obj_t *title_cont = lv_obj_create(scr_timeedit);
    lv_obj_set_size(title_cont, 180, 36);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_clear_flag(title_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(title_cont, LV_ALIGN_CENTER, 0, -82);
    // Whole bar is the back target, not just the two labels' tight glyph
    // bounds — a single "<" or a short title is a tiny precision target on
    // its own; the container is 180x36, much easier to actually hit.
    lv_obj_add_flag(title_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title_cont, timeedit_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(title_cont, 6);   // small margin; time row starts ~5px below

    lv_obj_t *back_arrow = lv_label_create(title_cont);
    lv_obj_set_style_text_font(back_arrow, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(back_arrow, color_gold, 0);
    lv_label_set_text(back_arrow, "<");

    te_title_label = lv_label_create(title_cont);
    lv_obj_set_style_text_font(te_title_label, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(te_title_label, color_ivory, 0);
    lv_label_set_text(te_title_label,
        "\330\266\330\250\330\267 \330\247\331\204\331\210\331\202\330\252");

    int *vals[5] = {&timeedit_hour, &timeedit_min, &timeedit_day, &timeedit_month, &timeedit_year};
    const char *fmts[5] = {"%02d", "%02d", "%02d", "%02d", "%04d"};

    // ── Time row  (y = -38) ─────────────────────────────────
    //   [ HH ]   :   [ MM ]   [ص/م]
    te_conts[0] = make_field(scr_timeedit, -65, -38, 50, 42, &te_hour_label, "28", fmts[0], *(vals[0]), 0);
    te_conts[1] = make_field(scr_timeedit,   1, -38, 50, 42, &te_min_label,  "28", fmts[1], *(vals[1]), 1);

    // AM/PM
    te_conts[5] = make_field(scr_timeedit, 66, -38, 48, 42, &te_ampm_label, "28", "%d", 0, 5);
    lv_label_set_text(te_ampm_label, "\330\265");
    lv_obj_set_style_text_color(te_ampm_label, color_ivory, 0);
    lv_obj_center(te_ampm_label);

    // Colon
    lv_obj_t *colon = lv_label_create(scr_timeedit);
    lv_obj_set_style_text_font(colon, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(colon, color_ivory, 0);
    lv_label_set_text(colon, ":");
    lv_obj_align(colon, LV_ALIGN_CENTER, -32, -40);

    // ── Date row  (y = 10) ───────────────────────────────────
    //   [ DD ]  /  [ MM ]  /  [    YYYY    ]
    te_conts[2] = make_field(scr_timeedit, -75, 10, 42, 42, &te_day_label, "28", fmts[2], *(vals[2]), 2);
    te_conts[3] = make_field(scr_timeedit, -21, 10, 42, 42, &te_mon_label, "28", fmts[3], *(vals[3]), 3);

    // Year — double the width of day/month
    te_conts[4] = lv_obj_create(scr_timeedit);
    lv_obj_set_size(te_conts[4], 84, 42);
    lv_obj_align(te_conts[4], LV_ALIGN_CENTER, 54, 10);
    lv_obj_set_style_bg_opa(te_conts[4], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(te_conts[4], 6, 0);
    lv_obj_set_style_border_width(te_conts[4], 1, 0);
    lv_obj_set_style_border_color(te_conts[4], color_border, 0);
    lv_obj_clear_flag(te_conts[4], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(te_conts[4], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(te_conts[4], timeedit_select_field_cb, LV_EVENT_CLICKED, (void *)4);
    te_year_label = lv_label_create(te_conts[4]);
    lv_obj_set_style_text_font(te_year_label, &font_alexandria_28, 0);
    lv_label_set_text_fmt(te_year_label, fmts[4], *(vals[4]));
    lv_obj_center(te_year_label);

    // Date separators
    lv_obj_t *s1 = lv_label_create(scr_timeedit);
    lv_obj_set_style_text_font(s1, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(s1, color_cream_dim, 0);
    lv_label_set_text(s1, "/");
    lv_obj_align(s1, LV_ALIGN_CENTER, -48, 8);

    lv_obj_t *s2 = lv_label_create(scr_timeedit);
    lv_obj_set_style_text_font(s2, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(s2, color_cream_dim, 0);
    lv_label_set_text(s2, "/");
    lv_obj_align(s2, LV_ALIGN_CENTER, 6, 8);

    te_date_row_objs[0] = te_conts[2];
    te_date_row_objs[1] = te_conts[3];
    te_date_row_objs[2] = te_conts[4];
    te_date_row_objs[3] = s1;
    te_date_row_objs[4] = s2;

    // ── Day-of-week toggles (y = 10, same slot as the date row) ──
    //   REMINDER mode only — built here so create_screen_timeedit() only
    //   runs once (screens are created once in screens_init() and reused,
    //   not rebuilt per open); open_timeedit_reminder()/open_timeedit_clock()
    //   below just show/hide this row vs. the date row per open.
    lv_obj_t *days_cont = lv_obj_create(scr_timeedit);
    lv_obj_set_size(days_cont, 210, 32);
    lv_obj_align(days_cont, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_bg_opa(days_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(days_cont, 0, 0);
    lv_obj_set_style_pad_all(days_cont, 0, 0);
    lv_obj_clear_flag(days_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(days_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(days_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int d = 0; d < 7; d++) {
        lv_obj_t *tgl = lv_obj_create(days_cont);
        lv_obj_set_size(tgl, 26, 26);
        lv_obj_set_style_radius(tgl, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(tgl, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(tgl, color_surface, 0);
        lv_obj_set_style_border_width(tgl, 1, 0);
        lv_obj_set_style_border_color(tgl, color_border, 0);
        lv_obj_clear_flag(tgl, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tgl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tgl, timeedit_day_toggle_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)d);

        lv_obj_t *dl = lv_label_create(tgl);
        lv_obj_set_style_text_font(dl, &font_alexandria_12, 0);
        lv_obj_set_style_text_color(dl, color_ivory, 0);
        lv_label_set_text(dl, DAY_INITIALS_AR[d]);
        lv_obj_center(dl);

        te_day_toggles[d] = tgl;
    }
    te_days_cont = days_cont;
    lv_obj_add_flag(te_days_cont, LV_OBJ_FLAG_HIDDEN);   // hidden until REMINDER mode

    timeedit_field = 0;
    update_timeedit_highlight();

    // ── Buttons  (y = 60) ───────────────────────────────────
    //   [ − ]      [ حفظ ]      [ + ]
    lv_obj_t *down_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(down_btn, 42, 42);
    lv_obj_add_style(down_btn, &style_card, 0);
    lv_obj_set_style_radius(down_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(down_btn, LV_ALIGN_CENTER, -62, 60);
    lv_obj_t *down_lbl = lv_label_create(down_btn);
    lv_obj_set_style_text_font(down_lbl, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(down_lbl, color_ivory, 0);
    lv_label_set_text(down_lbl, "-");
    lv_obj_center(down_lbl);
    lv_obj_add_event_cb(down_btn, timeedit_down_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(save_btn, 78, 36);
    lv_obj_add_style(save_btn, &style_btn_pill_teal, 0);
    lv_obj_align(save_btn, LV_ALIGN_CENTER, 0, 60);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "\330\255\331\201\330\270");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, timeedit_save_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *up_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(up_btn, 42, 42);
    lv_obj_add_style(up_btn, &style_card, 0);
    lv_obj_set_style_radius(up_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(up_btn, LV_ALIGN_CENTER, 62, 60);
    lv_obj_t *up_lbl = lv_label_create(up_btn);
    lv_obj_set_style_text_font(up_lbl, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(up_lbl, color_ivory, 0);
    lv_label_set_text(up_lbl, "+");
    lv_obj_center(up_lbl);
    lv_obj_add_event_cb(up_btn, timeedit_up_cb, LV_EVENT_CLICKED, NULL);
}

// ── Shared entry points ──────────────────────────────────────────
// The screen is built once (screens_init()) and reused, so opening it in
// either mode means: pick which row is visible (date vs. day-toggles),
// load the right source data into the timeedit_* working vars + labels,
// then push_modal(). Keeps all of scr_timeedit's field-population logic
// in one place instead of duplicating it in every caller.

static void show_date_row(bool show) {
    for (int i = 0; i < TE_DATE_ROW_N; i++) {
        if (show) lv_obj_remove_flag(te_date_row_objs[i], LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(te_date_row_objs[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (show) lv_obj_add_flag(te_days_cont, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_remove_flag(te_days_cont, LV_OBJ_FLAG_HIDDEN);
}

void open_timeedit_clock(void) {
    timeedit_mode  = TIMEEDIT_MODE_CLOCK;
    timeedit_ampm  = (hour_ >= 12) ? 1 : 0;
    timeedit_hour  = hour_ % 12;
    if (timeedit_hour == 0) timeedit_hour = 12;
    timeedit_min   = minute_;
    timeedit_day   = day_;
    timeedit_month = month_;
    timeedit_year  = year_;
    timeedit_field = 0;

    lv_label_set_text_fmt(te_hour_label, "%02d", timeedit_hour);
    lv_label_set_text_fmt(te_min_label,  "%02d", timeedit_min);
    lv_label_set_text_fmt(te_day_label,  "%02d", timeedit_day);
    lv_label_set_text_fmt(te_mon_label,  "%02d", timeedit_month);
    lv_label_set_text_fmt(te_year_label, "%04d", timeedit_year);
    lv_label_set_text(te_ampm_label, timeedit_ampm ? "\331\205" : "\330\265");
    lv_label_set_text(te_title_label,
        "\330\266\330\250\330\267 \330\247\331\204\331\210\331\202\330\252");

    show_date_row(true);
    update_timeedit_highlight();
    push_modal(scr_timeedit);
}

void open_timeedit_reminder(int idx) {
    timeedit_mode          = TIMEEDIT_MODE_REMINDER;
    timeedit_reminder_idx  = idx;
    timeedit_ampm  = (reminders[idx].hour >= 12) ? 1 : 0;
    timeedit_hour  = reminders[idx].hour % 12;
    if (timeedit_hour == 0) timeedit_hour = 12;
    timeedit_min   = reminders[idx].minute;
    timeedit_days  = reminders[idx].days;
    timeedit_field = 0;

    lv_label_set_text_fmt(te_hour_label, "%02d", timeedit_hour);
    lv_label_set_text_fmt(te_min_label,  "%02d", timeedit_min);
    lv_label_set_text(te_ampm_label, timeedit_ampm ? "\331\205" : "\330\265");
    lv_label_set_text(te_title_label, reminders[idx].label);

    for (int d = 0; d < 7; d++) {
        bool on = timeedit_days & (1 << d);
        lv_obj_set_style_bg_color(te_day_toggles[d], on ? color_teal : color_surface, 0);
    }

    show_date_row(false);
    update_timeedit_highlight();
    push_modal(scr_timeedit);
}
