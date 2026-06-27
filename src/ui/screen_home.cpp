/*
 * screen_home.c  —  Clock screen
 *
 * Layout (240×240 circle):
 *
 *        [ السبت ]            ← day name, gold, top-center
 *
 *        [ 23:13 ]            ← time, ivory, huge, center
 *
 *        [ 50 ثانية ]         ← seconds, muted, small
 *
 *   ┌─ 12 ربيع الآخر 1447 ─┐  ← pill-border chip, cream
 *
 *    الصلاة القادمة · العصر 15:42   ← next prayer, gold/small
 *
 *  Seconds arc ring sweeps around the full circle (r≈110px).
 *  Gear (⚙) top-right navigates to settings modal.
 *  Battery top-left.
 */

#include "screens.h"
#include "styles.h"
#include <Arduino.h>

extern int hour_, minute_, second_, day_, month_, year_;

// ── Widget handles ────────────────────────────────────────────
lv_obj_t *home_day_label    = NULL;
lv_obj_t *home_clock_label  = NULL;
lv_obj_t *home_sec_label    = NULL;
lv_obj_t *home_date_label   = NULL;
lv_obj_t *home_prayer_label = NULL;
lv_obj_t *home_bat_label    = NULL;
lv_obj_t *home_sec_arc      = NULL;

// ── Arabic day names (Sunday = index 0) ───────────────────────
static const char *DAYS_AR[] = {
    "\330\247\331\204\330\243\330\255\330\257",          // الأحد
    "\330\247\331\204\330\245\330\253\331\206\331\212\331\206", // الإثنين
    "\330\247\331\204\330\253\331\204\330\247\330\253\330\247\330\241", // الثلاثاء
    "\330\247\331\204\330\243\330\261\330\250\330\271\330\247\330\241", // الأربعاء
    "\330\247\331\204\330\256\331\205\331\212\330\263", // الخميس
    "\330\247\331\204\330\254\331\205\330\271\330\251", // الجمعة
    "\330\247\331\204\330\263\330\250\330\252"           // السبت
};

// day-of-week from stored date (Tomohiko Sakamoto algorithm)
static int day_of_week(int d, int m, int y) {
    static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

// ── Callbacks ─────────────────────────────────────────────────
static void gear_click_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_home) return;
    push_modal(scr_settings);
}

static void clock_tap_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_home) return;
    timeedit_hour  = hour_;
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
    push_modal(scr_timeedit);
}

// ── update_home_clock (called every second from main.cpp) ─────
void update_home_clock(void)
{
    if (!home_clock_label) return;

    lv_label_set_text_fmt(home_clock_label, "%02d:%02d", hour_, minute_);

    lv_label_set_text_fmt(home_sec_label,
        "%d \330\253\330\247\331\206\331\212\330\251", second_);

    lv_arc_set_value(home_sec_arc, second_);

    int dow = day_of_week(day_, month_, year_);
    lv_label_set_text(home_day_label, DAYS_AR[dow]);
}

// ── Screen builder ────────────────────────────────────────────
void create_screen_home(void)
{
    scr_home = lv_obj_create(NULL);
    make_screen_base(scr_home);

    // ── Seconds arc (full-circle) ─────────────────────────────
    home_sec_arc = lv_arc_create(scr_home);
    lv_obj_set_size(home_sec_arc, 224, 224);
    lv_obj_center(home_sec_arc);
    lv_arc_set_bg_angles(home_sec_arc, 0, 360);
    lv_arc_set_range(home_sec_arc, 0, 59);
    lv_arc_set_value(home_sec_arc, 0);
    lv_arc_set_mode(home_sec_arc, LV_ARC_MODE_NORMAL);
    lv_obj_remove_style(home_sec_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(home_sec_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(home_sec_arc, lv_color_hex(0x2a2410), LV_PART_MAIN);
    lv_obj_set_style_arc_width(home_sec_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(home_sec_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(home_sec_arc, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_arc_color(home_sec_arc, color_gold, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(home_sec_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(home_sec_arc, true, LV_PART_INDICATOR);

    lv_arc_set_rotation(home_sec_arc, 270);

    // ── Day name  "السبت" ─────────────────────────────────────
    home_day_label = lv_label_create(scr_home);
    lv_obj_set_style_text_font(home_day_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(home_day_label, color_gold, 0);
    lv_obj_set_style_text_align(home_day_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_day_label, DAYS_AR[6]);
    lv_obj_align(home_day_label, LV_ALIGN_CENTER, 0, -60);

    // ── Clock  "23:13" ────────────────────────────────────────
    home_clock_label = lv_label_create(scr_home);
    lv_obj_add_style(home_clock_label, &style_clock, 0);
    lv_obj_set_style_text_color(home_clock_label, color_ivory, 0);
    lv_label_set_text(home_clock_label, "00:00");
    lv_obj_align(home_clock_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *clock_tap = lv_obj_create(scr_home);
    lv_obj_set_size(clock_tap, 200, 60);
    lv_obj_align(clock_tap, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(clock_tap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_tap, 0, 0);
    lv_obj_add_flag(clock_tap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(clock_tap, clock_tap_cb, LV_EVENT_CLICKED, NULL);

// ── Seconds text  "50 ثانية" ──────────────────────────────
    home_sec_label = lv_label_create(scr_home);
    lv_obj_add_style(home_sec_label, &style_label_sm, 0);
    
    // ADD THIS: Override the font to one that supports Arabic
    lv_obj_set_style_text_font(home_sec_label, &lv_font_dejavu_16_persian_hebrew, 0); 
    
    lv_obj_set_style_text_color(home_sec_label, color_cream_dim, 0);
    lv_label_set_text(home_sec_label, "0 \330\253\330\247\331\206\331\212\330\251");
    lv_obj_align(home_sec_label, LV_ALIGN_CENTER, 0, 36);

    // ── Date chip  "12 ربيع الآخر 1447" ──────────────────────
    lv_obj_t *date_chip = lv_obj_create(scr_home);
    lv_obj_set_style_bg_opa(date_chip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(date_chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(date_chip, color_gold, 0);
    lv_obj_set_style_border_opa(date_chip, LV_OPA_30, 0);
    lv_obj_set_style_border_width(date_chip, 1, 0);
    lv_obj_set_style_pad_hor(date_chip, 14, 0);
    lv_obj_set_style_pad_ver(date_chip, 5, 0);
    lv_obj_set_size(date_chip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(date_chip, LV_ALIGN_CENTER, 0, 65);
    lv_obj_clear_flag(date_chip, LV_OBJ_FLAG_SCROLLABLE);

    home_date_label = lv_label_create(date_chip);
    lv_obj_set_style_text_font(home_date_label, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(home_date_label, color_cream, 0);
    lv_obj_set_style_text_align(home_date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_date_label,
        "12 \330\261\330\250\331\212\330\271 \330\247\331\204\330\242\330\256\330\261 1447");
    lv_obj_center(home_date_label);


    // ── Battery  top-left ─────────────────────────────────────
    home_bat_label = lv_label_create(scr_home);
    lv_obj_add_style(home_bat_label, &style_label_sm, 0);
    lv_obj_set_style_text_color(home_bat_label, color_cream_dim, 0);
    lv_label_set_text(home_bat_label, "--%");
    lv_obj_align(home_bat_label, LV_ALIGN_TOP_LEFT, 20, 18);

    // ── Gear  top-right ───────────────────────────────────────
    lv_obj_t *gear_btn = lv_btn_create(scr_home);
    lv_obj_set_size(gear_btn, 36, 36);
    lv_obj_add_style(gear_btn, &style_icon_btn, 0);
    lv_obj_set_style_shadow_width(gear_btn, 0, 0);
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_TRANSP, 0);
    lv_obj_align(gear_btn, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_t *gear_lbl = lv_label_create(gear_btn);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_lbl, color_cream_dim, 0);
    lv_obj_center(gear_lbl);
    lv_obj_add_event_cb(gear_btn, gear_click_cb, LV_EVENT_CLICKED, NULL);
}
