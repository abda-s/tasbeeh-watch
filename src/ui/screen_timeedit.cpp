#include "screens.h"
#include "styles.h"
#include <Preferences.h>
#include <Arduino.h>

extern Preferences prefs;
extern int hour_, minute_, second_, day_, month_, year_;

int timeedit_hour, timeedit_min, timeedit_day, timeedit_month, timeedit_year;
int timeedit_field = 0;
lv_obj_t *te_hour_label = NULL, *te_min_label = NULL;
lv_obj_t *te_day_label = NULL, *te_mon_label = NULL, *te_year_label = NULL;

static void timeedit_up_cb(lv_event_t *e) {
    int max_vals[5] = {23, 59, 31, 12, 2099};
    int min_vals[5] = {0, 0, 1, 1, 2024};
    int *vals[5] = {&timeedit_hour, &timeedit_min, &timeedit_day, &timeedit_month, &timeedit_year};
    lv_obj_t **labels[5] = {&te_hour_label, &te_min_label, &te_day_label, &te_mon_label, &te_year_label};
    const char *fmts[5] = {"%02d", "%02d", "%02d", "%02d", "%04d"};

    int f = timeedit_field;
    (*(vals[f]))++;
    if (*(vals[f]) > max_vals[f]) *(vals[f]) = min_vals[f];
    lv_label_set_text_fmt(*(labels[f]), fmts[f], *(vals[f]));
}
static void timeedit_down_cb(lv_event_t *e) {
    int max_vals[5] = {23, 59, 31, 12, 2099};
    int min_vals[5] = {0, 0, 1, 1, 2024};
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
}
static void timeedit_save_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_timeedit) return;
#if NAV_DEBUG
    Serial.println("[CLICK] timeedit save -> pop_modal");
#endif
    hour_ = timeedit_hour;
    minute_ = timeedit_min;
    second_ = 0;
    day_ = timeedit_day;
    month_ = timeedit_month;
    year_ = timeedit_year;
    prefs.putInt("hour", hour_);
    prefs.putInt("minute", minute_);
    prefs.putInt("second", second_);
    prefs.putInt("day", day_);
    prefs.putInt("month", month_);
    prefs.putInt("year", year_);
    update_home_clock();
    pop_modal();
}

void create_screen_timeedit(void) {
    scr_timeedit = lv_obj_create(NULL);
    make_screen_base(scr_timeedit);

    create_title(scr_timeedit, "\330\266\330\250\330\267 \330\247\331\204\331\210\331\202\330\252");

    const char *val_fmts[5] = {"%02d", "%02d", "%02d", "%02d", "%04d"};
    lv_obj_t **val_labels[5] = {&te_hour_label, &te_min_label, &te_day_label, &te_mon_label, &te_year_label};
    const char *val_names[5] = {"H", "M", "D", "M", "Y"};
    int val_x[5] = {60, 140, 40, 100, 170};
    int val_y[5] = {70, 70, 120, 120, 120};

    for (int i = 0; i < 5; i++) {
        lv_obj_t *cont = lv_obj_create(scr_timeedit);
        lv_obj_set_size(cont, 60, 40);
        lv_obj_set_pos(cont, val_x[i] - 30, val_y[i] - 12);
        lv_obj_set_style_bg_color(cont, color_surface, 0);
        lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(cont, 8, 0);
        lv_obj_set_style_border_width(cont, 1, 0);
        lv_obj_set_style_border_color(cont, color_border, 0);
        lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cont, timeedit_select_field_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *name_lbl = lv_label_create(scr_timeedit);
        lv_obj_add_style(name_lbl, &style_label_sm, 0);
        lv_label_set_text(name_lbl, val_names[i]);
        lv_obj_set_pos(name_lbl, val_x[i] - lv_obj_get_width(name_lbl) / 2, val_y[i] - 22);

        *(val_labels[i]) = lv_label_create(cont);
        lv_obj_add_style(*(val_labels[i]), &style_counter_val, 0);
        lv_obj_center(*(val_labels[i]));
    }

    lv_obj_t *up_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(up_btn, 60, 24);
    lv_obj_align(up_btn, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_t *up_lbl = lv_label_create(up_btn);
    lv_label_set_text(up_lbl, "+");
    lv_obj_center(up_lbl);
    lv_obj_add_event_cb(up_btn, timeedit_up_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *down_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(down_btn, 60, 24);
    lv_obj_align(down_btn, LV_ALIGN_TOP_MID, 0, 175);
    lv_obj_t *down_lbl = lv_label_create(down_btn);
    lv_label_set_text(down_lbl, "-");
    lv_obj_center(down_lbl);
    lv_obj_add_event_cb(down_btn, timeedit_down_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_btn = lv_btn_create(scr_timeedit);
    lv_obj_set_size(save_btn, 120, 36);
    lv_obj_add_style(save_btn, &style_btn_pill_teal, 0);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "\330\255\331\201\330\270");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, timeedit_save_cb, LV_EVENT_CLICKED, NULL);
}
