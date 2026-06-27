#include "screens.h"
#include "styles.h"
#include "../config/CST816S_pin_config.h"
#include <CST816S.h>
#include <Preferences.h>

extern CST816S touch;
extern Preferences prefs;
extern uint32_t tasbeehCount;
extern uint32_t isteghfarCount;
extern int hour_, minute_, second_, day_, month_, year_;
extern String deviceIP;

lv_obj_t *scr_home      = NULL;
lv_obj_t *scr_thiker    = NULL;
lv_obj_t *scr_tasbeeh   = NULL;
lv_obj_t *scr_isteghfar = NULL;
lv_obj_t *scr_settings  = NULL;
lv_obj_t *scr_timeedit  = NULL;

lv_obj_t *home_clock_label      = NULL;
lv_obj_t *home_date_label       = NULL;
lv_obj_t *home_bat_label        = NULL;
lv_obj_t *tasbeeh_arc           = NULL;
lv_obj_t *tasbeeh_counter_label = NULL;
lv_obj_t *tasbeeh_total_label   = NULL;
lv_obj_t *isteghfar_arc         = NULL;
lv_obj_t *isteghfar_counter_label = NULL;
lv_obj_t *isteghfar_total_label = NULL;
lv_obj_t *settings_ip_label     = NULL;

static int timeedit_hour, timeedit_min, timeedit_day, timeedit_month, timeedit_year;
static int timeedit_field = 0;
static lv_obj_t *te_hour_label = NULL, *te_min_label = NULL;
static lv_obj_t *te_day_label = NULL, *te_mon_label = NULL, *te_year_label = NULL;

enum CounterType { COUNTER_TASBEEH, COUNTER_ISTEGHFAR };

static lv_obj_t  *g_flash_arc   = NULL;
static lv_color_t  g_flash_color;

static void revert_arc_color_cb(lv_timer_t *timer) {
    lv_obj_set_style_arc_color(g_flash_arc, g_flash_color, LV_PART_INDICATOR);
    lv_timer_delete(timer);
}

static void nav_to_home(void) {
    lv_screen_load_anim(scr_home, LV_SCR_LOAD_ANIM_FADE_IN, 150, 0, false);
}
static void nav_to_thiker(void) {
    lv_screen_load_anim(scr_thiker, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
}
static void nav_to_tasbeeh(void) {
    lv_screen_load_anim(scr_tasbeeh, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
static void nav_to_isteghfar(void) {
    lv_screen_load_anim(scr_isteghfar, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
static void nav_to_settings(void) {
    lv_screen_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}
static void nav_to_timeedit(void) {
    lv_screen_load_anim(scr_timeedit, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
}

static void gesture_home_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) nav_to_thiker();
}
static void gesture_thiker_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT) nav_to_home();
}
static void gesture_counter_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT) nav_to_thiker();
}

static void gear_click_cb(lv_event_t *e) {
    nav_to_settings();
}
static void arrow_click_cb(lv_event_t *e) {
    nav_to_thiker();
}
static void settings_back_cb(lv_event_t *e) {
    nav_to_home();
}
static void clock_tap_cb(lv_event_t *e) {
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
    nav_to_timeedit();
}

static void tap_counter_cb(lv_event_t *e) {
    uint32_t *counter;
    const char *key;
    lv_obj_t *arc, *counter_label, *total_label;
    lv_color_t arc_color, accent;

    CounterType type = (CounterType)(uintptr_t)lv_event_get_user_data(e);
    if (type == COUNTER_TASBEEH) {
        counter = &tasbeehCount;
        key = "tasbeeh";
        arc = tasbeeh_arc;
        counter_label = tasbeeh_counter_label;
        total_label = tasbeeh_total_label;
        arc_color = color_teal;
        accent = color_green;
    } else {
        counter = &isteghfarCount;
        key = "isteghfar";
        arc = isteghfar_arc;
        counter_label = isteghfar_counter_label;
        total_label = isteghfar_total_label;
        arc_color = color_blue;
        accent = color_green;
    }

    (*counter)++;
    prefs.putUInt(key, *counter);

    int val = (int)(*counter % 99);

    lv_arc_set_value(arc, val);
    lv_label_set_text_fmt(counter_label, "%d", val);
    lv_label_set_text_fmt(total_label, "%s: %lu",
        (type == COUNTER_TASBEEH) ? "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212" : "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212",
        *counter);

    if (val == 0 && *counter > 0) {
        lv_obj_set_style_arc_color(arc, accent, LV_PART_INDICATOR);
        g_flash_arc   = arc;
        g_flash_color = arc_color;
        lv_timer_t *t = lv_timer_create(revert_arc_color_cb, 400, NULL);
        lv_timer_set_repeat_count(t, 1);
    }
}

static void thiker_tasbeeh_btn_cb(lv_event_t *e) { nav_to_tasbeeh(); }
static void thiker_isteghfar_btn_cb(lv_event_t *e) { nav_to_isteghfar(); }

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
    nav_to_home();
}

static void make_screen_base(lv_obj_t *scr) {
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
}

static void add_swipe_back(lv_obj_t *scr, lv_event_cb_t cb) {
    lv_obj_add_event_cb(scr, cb, LV_EVENT_GESTURE, NULL);
}

static lv_obj_t *create_title(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_add_style(label, &style_title, 0);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    return label;
}

static void create_screen_home_internal(void) {
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
    lv_obj_align(gear_btn, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_t *gear_lbl = lv_label_create(gear_btn);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_center(gear_lbl);
    lv_obj_add_event_cb(gear_btn, gear_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *arrow_btn = lv_btn_create(scr_home);
    lv_obj_set_size(arrow_btn, 40, 40);
    lv_obj_add_style(arrow_btn, &style_icon_btn, 0);
    lv_obj_align(arrow_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_t *arrow_lbl = lv_label_create(arrow_btn);
    lv_label_set_text(arrow_lbl, LV_SYMBOL_RIGHT);
    lv_obj_center(arrow_lbl);
    lv_obj_add_event_cb(arrow_btn, arrow_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clock_area = lv_obj_create(scr_home);
    lv_obj_set_size(clock_area, 240, 80);
    lv_obj_align(clock_area, LV_ALIGN_CENTER, 0, -15);
    lv_obj_set_style_bg_opa(clock_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_area, 0, 0);
    lv_obj_add_flag(clock_area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(clock_area, clock_tap_cb, LV_EVENT_CLICKED, NULL);

    add_swipe_back(scr_home, gesture_home_cb);
}

static void create_screen_thiker_internal(void) {
    scr_thiker = lv_obj_create(NULL);
    make_screen_base(scr_thiker);

    create_title(scr_thiker, "\330\247\331\204\331\202\330\247\330\246\331\205\330\251");

    lv_obj_t *btn1 = lv_btn_create(scr_thiker);
    lv_obj_set_size(btn1, 180, 52);
    lv_obj_add_style(btn1, &style_btn_pill_teal, 0);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *lbl1 = lv_label_create(btn1);
    lv_label_set_text(lbl1, "\330\252\330\263\330\250\331\212\330\255");
    lv_obj_center(lbl1);
    lv_obj_add_event_cb(btn1, thiker_tasbeeh_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn2 = lv_btn_create(scr_thiker);
    lv_obj_set_size(btn2, 180, 52);
    lv_obj_add_style(btn2, &style_btn_pill_blue, 0);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 30);
    lv_obj_t *lbl2 = lv_label_create(btn2);
    lv_label_set_text(lbl2, "\330\247\330\263\330\252\330\272\331\201\330\247\330\261");
    lv_obj_center(lbl2);
    lv_obj_add_event_cb(btn2, thiker_isteghfar_btn_cb, LV_EVENT_CLICKED, NULL);

    add_swipe_back(scr_thiker, gesture_thiker_cb);
}

static void create_counter_screen(lv_obj_t **scr_out, lv_obj_t **arc_out,
    lv_obj_t **counter_out, lv_obj_t **total_out,
    const char *title, uint32_t count, lv_color_t arc_color,
    CounterType ctype)
{
    *scr_out = lv_obj_create(NULL);
    make_screen_base(*scr_out);

    lv_obj_t *header = lv_obj_create(*scr_out);
    lv_obj_set_size(header, 240, 40);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x001830), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t *title_lbl = lv_label_create(header);
    lv_obj_add_style(title_lbl, &style_title, 0);
    lv_label_set_text(title_lbl, title);
    lv_obj_center(title_lbl);

    *arc_out = lv_arc_create(*scr_out);
    lv_obj_set_size(*arc_out, 140, 140);
    lv_arc_set_range(*arc_out, 0, 99);
    lv_arc_set_value(*arc_out, count % 99);
    lv_arc_set_bg_angles(*arc_out, 0, 360);
    lv_arc_set_rotation(*arc_out, 270);
    lv_obj_remove_style(*arc_out, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(*arc_out, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(*arc_out, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_style(*arc_out, (ctype == COUNTER_TASBEEH) ? &style_arc_teal : &style_arc_blue, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(*arc_out, lv_color_hex(0x1f2937), LV_PART_MAIN);

    *counter_out = lv_label_create(*scr_out);
    lv_obj_add_style(*counter_out, &style_counter_val, 0);
    lv_label_set_text_fmt(*counter_out, "%lu", count % 99);
    lv_obj_align(*counter_out, LV_ALIGN_CENTER, 0, -10);

    *total_out = lv_label_create(*scr_out);
    lv_obj_add_style(*total_out, &style_label_sm, 0);
    lv_obj_align(*total_out, LV_ALIGN_CENTER, 0, 65);

    lv_obj_t *tap_btn = lv_btn_create(*scr_out);
    lv_obj_set_size(tap_btn, 180, 44);
    lv_obj_add_style(tap_btn, (ctype == COUNTER_TASBEEH) ? &style_btn_pill_teal : &style_btn_pill_blue, 0);
    lv_obj_align(tap_btn, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_t *tap_lbl = lv_label_create(tap_btn);
    lv_label_set_text(tap_lbl, "\330\247\330\266\330\272\330\267");
    lv_obj_center(tap_lbl);
    lv_obj_add_event_cb(tap_btn, tap_counter_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)ctype);

    add_swipe_back(*scr_out, gesture_counter_cb);
}

static void create_screen_tasbeeh_internal(void) {
    create_counter_screen(&scr_tasbeeh, &tasbeeh_arc, &tasbeeh_counter_label,
        &tasbeeh_total_label, "\330\252\330\263\330\250\331\212\330\255",
        tasbeehCount, color_teal, COUNTER_TASBEEH);
}

static void create_screen_isteghfar_internal(void) {
    create_counter_screen(&scr_isteghfar, &isteghfar_arc, &isteghfar_counter_label,
        &isteghfar_total_label, "\330\247\330\263\330\252\330\272\331\201\330\247\330\261",
        isteghfarCount, color_blue, COUNTER_ISTEGHFAR);
}

static void create_screen_settings_internal(void) {
    scr_settings = lv_obj_create(NULL);
    make_screen_base(scr_settings);

    create_title(scr_settings, "\330\247\331\204\330\245\330\271\330\257\330\247\330\257\330\247\330\252");

    lv_obj_t *card = lv_obj_create(scr_settings);
    lv_obj_set_size(card, 200, 60);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *label = lv_label_create(card);
    lv_obj_add_style(label, &style_label_sm, 0);
    lv_label_set_text(label, "Open in browser:");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);

    settings_ip_label = lv_label_create(card);
    lv_obj_add_style(settings_ip_label, &style_counter_val, 0);
    lv_label_set_text(settings_ip_label, "--");
    lv_obj_align(settings_ip_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    lv_obj_t *back_btn = lv_btn_create(scr_settings);
    lv_obj_set_size(back_btn, 100, 36);
    lv_obj_add_style(back_btn, &style_btn_pill_teal, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "< " LV_SYMBOL_LEFT);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, settings_back_cb, LV_EVENT_CLICKED, NULL);
}

static void create_screen_timeedit_internal(void) {
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

void screens_init(void) {
    create_screen_home_internal();
    create_screen_thiker_internal();
    create_screen_tasbeeh_internal();
    create_screen_isteghfar_internal();
    create_screen_settings_internal();
    create_screen_timeedit_internal();
}

void update_home_clock(void) {
    if (!home_clock_label || !home_date_label) return;
    lv_label_set_text_fmt(home_clock_label, "%02d:%02d", hour_, minute_);
    lv_label_set_text_fmt(home_date_label, "%02d/%02d/%04d", day_, month_, year_);
}

void update_home_battery(void) {
    if (!home_bat_label) return;
}

void update_tasbeeh_display(void) {
    if (!tasbeeh_counter_label || !tasbeeh_total_label || !tasbeeh_arc) return;
    int val = tasbeehCount % 99;
    lv_arc_set_value(tasbeeh_arc, val);
    lv_label_set_text_fmt(tasbeeh_counter_label, "%d", val);
    lv_label_set_text_fmt(tasbeeh_total_label, "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212: %lu", tasbeehCount);
}

void update_isteghfar_display(void) {
    if (!isteghfar_counter_label || !isteghfar_total_label || !isteghfar_arc) return;
    int val = isteghfarCount % 99;
    lv_arc_set_value(isteghfar_arc, val);
    lv_label_set_text_fmt(isteghfar_counter_label, "%d", val);
    lv_label_set_text_fmt(isteghfar_total_label, "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212: %lu", isteghfarCount);
}
