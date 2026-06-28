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

lv_obj_t *te_hour_label = NULL, *te_min_label = NULL;
lv_obj_t *te_day_label = NULL, *te_mon_label = NULL, *te_year_label = NULL;
lv_obj_t *te_ampm_label = NULL;

static lv_obj_t *te_conts[6];

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

static void timeedit_save_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_timeedit) return;
    if (timeedit_ampm == 0)
        hour_ = (timeedit_hour == 12) ? 0 : timeedit_hour;
    else
        hour_ = (timeedit_hour == 12) ? 12 : timeedit_hour + 12;
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

    lv_obj_t *back_arrow = lv_label_create(title_cont);
    lv_obj_set_style_text_font(back_arrow, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(back_arrow, color_gold, 0);
    lv_label_set_text(back_arrow, "<");
    lv_obj_add_flag(back_arrow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_arrow, timeedit_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *title = lv_label_create(title_cont);
    lv_obj_set_style_text_font(title, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(title, color_ivory, 0);
    lv_label_set_text(title,
        "\330\266\330\250\330\267 \330\247\331\204\331\210\331\202\330\252");
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, timeedit_cancel_cb, LV_EVENT_CLICKED, NULL);

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
