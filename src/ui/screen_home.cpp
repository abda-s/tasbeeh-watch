/*
 * screen_home.c  —  Clock screen with Islamic geometric background
 */

#include "screens.h"
#include "styles.h"
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_12);

extern int hour_, minute_, second_, day_, month_, year_;

lv_obj_t *home_day_label    = NULL;
lv_obj_t *home_clock_label  = NULL;
lv_obj_t *home_date_label   = NULL;
lv_obj_t *home_prayer_label = NULL;
lv_obj_t *home_bat_label    = NULL;
lv_obj_t *home_sec_arc      = NULL;
lv_obj_t *home_ampm_label   = NULL;

// Gregorian Arabic month names
static const char *MONTHS_AR[] = {
    "\331\212\331\206\330\247\331\212\330\261",
    "\331\201\330\250\330\261\330\247\331\212\330\261",
    "\331\205\330\247\330\261\330\263",
    "\330\243\330\250\330\261\331\212\331\204",
    "\331\205\330\247\331\212\331\210",
    "\331\212\331\210\331\206\331\212\331\210",
    "\331\212\331\210\331\204\331\212\331\210",
    "\330\243\330\272\330\263\330\267\330\263",
    "\330\263\330\250\330\252\331\205\330\250\330\261",
    "\330\243\331\203\330\252\331\210\330\250\330\261",
    "\331\206\331\210\331\201\331\205\330\250\330\261",
    "\330\257\331\212\330\263\331\205\330\250\330\261",
};

static const char *DAYS_AR[] = {
    "\330\247\331\204\330\243\330\255\330\257",
    "\330\247\331\204\330\245\330\253\331\206\331\212\331\206",
    "\330\247\331\204\330\253\331\204\330\247\330\253\330\247\330\241",
    "\330\247\331\204\330\243\330\261\330\250\330\271\330\247\330\241",
    "\330\247\331\204\330\256\331\205\331\212\330\263",
    "\330\247\331\204\330\254\331\205\330\271\330\251",
    "\330\247\331\204\330\263\330\250\330\252"
};

static int day_of_week(int d, int m, int y) {
    static int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

static void gear_click_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_home) return;
    push_modal(scr_settings);
}

void update_home_clock(void)
{
    if (!home_clock_label) return;

    int h12 = hour_ % 12;
    if (h12 == 0) h12 = 12;
    lv_label_set_text_fmt(home_clock_label, "%02d\n%02d", h12, minute_);
    lv_arc_set_value(home_sec_arc, second_);

    if (home_ampm_label) {
        lv_label_set_text(home_ampm_label, (hour_ >= 12) ? "\331\205" : "\330\265");
    }

    int dow = day_of_week(day_, month_, year_);
    lv_label_set_text(home_day_label, DAYS_AR[dow]);

    if (month_ >= 1 && month_ <= 12) {
        lv_label_set_text_fmt(home_date_label, "%d %s %d",
             year_, MONTHS_AR[month_ - 1], day_);
    }
}

void create_screen_home(void)
{
    scr_home = lv_obj_create(NULL);
    make_screen_base(scr_home);

    // ── Seconds arc ──────────────────────────────────────
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

    // ── Day name ─────────────────────────────────────────
    home_day_label = lv_label_create(scr_home);
    lv_obj_set_style_text_font(home_day_label, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(home_day_label, color_gold, 0);
    lv_obj_set_style_text_align(home_day_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_day_label, DAYS_AR[6]);
    lv_obj_align(home_day_label, LV_ALIGN_CENTER, 0, -75);

    // ── Clock digits (long-press to edit) ────────────────
    home_clock_label = lv_label_create(scr_home);
    lv_obj_add_style(home_clock_label, &style_clock, 0);
    lv_obj_set_style_text_color(home_clock_label, color_ivory, 0);
    lv_obj_set_style_text_align(home_clock_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_clock_label, "00\n00");
    lv_obj_align(home_clock_label, LV_ALIGN_CENTER, 0, 0);

    // AM/PM indicator — to the right of the clock
    home_ampm_label = lv_label_create(scr_home);
    lv_obj_set_style_text_font(home_ampm_label, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(home_ampm_label, color_gold, 0);
    lv_label_set_text(home_ampm_label, "\330\265");
    lv_obj_align(home_ampm_label, LV_ALIGN_CENTER, 55, 0);

    // ── Gregorian date ───────────────────────────────────
    home_date_label = lv_label_create(scr_home);
    lv_obj_set_style_text_font(home_date_label, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(home_date_label, color_cream, 0);
    lv_obj_set_style_text_align(home_date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(home_date_label, " 2026 \331\212\331\206\330\247\331\212\330\261 1 ");
    lv_obj_align(home_date_label, LV_ALIGN_CENTER, 0, 76);

    // ── Battery ──────────────────────────────────────────
    home_bat_label = lv_label_create(scr_home);
    lv_obj_add_style(home_bat_label, &style_label_sm, 0);
    lv_obj_set_style_text_color(home_bat_label, color_cream_dim, 0);
    lv_label_set_text(home_bat_label, "--%");
    lv_obj_align(home_bat_label, LV_ALIGN_TOP_LEFT, 35, 56);

    // ── Swipe arrows — static (animation removed: a running LVGL anim
    // invalidates + redraws at the full refresh rate forever, measured at
    // ~10% of total screen-on battery charge) ─────────────────────────
    lv_obj_t *arrow_l = lv_label_create(scr_home);
    lv_obj_set_style_text_font(arrow_l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow_l, color_gold, 0);
    lv_obj_set_style_text_opa(arrow_l, LV_OPA_50, 0);
    lv_label_set_text(arrow_l, LV_SYMBOL_LEFT);
    lv_obj_align(arrow_l, LV_ALIGN_LEFT_MID, 30, 0);

    lv_obj_t *arrow_r = lv_label_create(scr_home);
    lv_obj_set_style_text_font(arrow_r, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow_r, color_gold, 0);
    lv_obj_set_style_text_opa(arrow_r, LV_OPA_50, 0);
    lv_label_set_text(arrow_r, LV_SYMBOL_RIGHT);
    lv_obj_align(arrow_r, LV_ALIGN_RIGHT_MID, -30, 0);

    // ── Gear icon (52×52 touch area) ─────────────────────
    lv_obj_t *gear_btn = lv_btn_create(scr_home);
    lv_obj_set_size(gear_btn, 52, 52);
    lv_obj_add_style(gear_btn, &style_icon_btn, 0);
    lv_obj_set_style_shadow_width(gear_btn, 0, 0);
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gear_btn, 0, 0);
    lv_obj_align(gear_btn, LV_ALIGN_TOP_RIGHT, -25, 52);
    lv_obj_t *gear_lbl = lv_label_create(gear_btn);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_lbl, color_cream_dim, 0);
    lv_obj_center(gear_lbl);
    lv_obj_add_event_cb(gear_btn, gear_click_cb, LV_EVENT_CLICKED, NULL);
}
