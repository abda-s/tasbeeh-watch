#include "screens.h"
#include "styles.h"
#include <Arduino.h>

extern int hour_, minute_, day_, month_, year_;

static void gear_click_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_home) return;
#if NAV_DEBUG
    Serial.println("[CLICK] gear -> push_modal(scr_settings)");
#endif
    push_modal(scr_settings);
}

static void clock_tap_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_home) return;
#if NAV_DEBUG
    Serial.println("[CLICK] clock -> push_modal(scr_timeedit)");
#endif
    timeedit_hour = hour_;
    timeedit_min  = minute_;
    timeedit_day  = day_;
    timeedit_month = month_;
    timeedit_year = year_;
    timeedit_field = 0;
    lv_label_set_text_fmt(te_hour_label, "%02d", timeedit_hour);
    lv_label_set_text_fmt(te_min_label,  "%02d", timeedit_min);
    lv_label_set_text_fmt(te_day_label,  "%02d", timeedit_day);
    lv_label_set_text_fmt(te_mon_label,  "%02d", timeedit_month);
    lv_label_set_text_fmt(te_year_label, "%04d", timeedit_year);
    push_modal(scr_timeedit);
}

void create_screen_home(void) {
    scr_home = lv_obj_create(NULL);
    make_screen_base(scr_home);

    home_bat_label = lv_label_create(scr_home);
    lv_obj_add_style(home_bat_label, &style_label_sm, 0);
    lv_obj_align(home_bat_label, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(home_bat_label, "--%");

    home_clock_label = lv_label_create(scr_home);
    lv_obj_add_style(home_clock_label, &style_clock, 0);
    lv_obj_align(home_clock_label, LV_ALIGN_CENTER, 0, -15);

    home_date_label = lv_label_create(scr_home);
    lv_obj_add_style(home_date_label, &style_label_sm, 0);
    lv_obj_align(home_date_label, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *gear_btn = lv_btn_create(scr_home);
    lv_obj_set_size(gear_btn, 36, 36);
    lv_obj_add_style(gear_btn, &style_icon_btn, 0);
    lv_obj_align(gear_btn, LV_ALIGN_TOP_RIGHT, -72, 22);
    lv_obj_t *gear_lbl = lv_label_create(gear_btn);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(gear_lbl);
    lv_obj_add_event_cb(gear_btn, gear_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clock_area = lv_obj_create(scr_home);
    lv_obj_set_size(clock_area, 240, 80);
    lv_obj_align(clock_area, LV_ALIGN_CENTER, 0, -15);
    lv_obj_set_style_bg_opa(clock_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_area, 0, 0);
    lv_obj_add_flag(clock_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(clock_area, clock_tap_cb, LV_EVENT_CLICKED, NULL);
}
