#include "screens.h"
#include <Arduino.h>

extern int hour_, minute_, day_, month_, year_;
extern uint32_t tasbeehCount;
extern uint32_t isteghfarCount;

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

void screens_init(void) {
    create_screen_home();
    create_screen_thiker();
    create_screen_tasbeeh();
    create_screen_isteghfar();
    create_screen_settings();
    create_screen_timeedit();

    ring_screens[0] = scr_home;
    ring_screens[1] = scr_thiker;
    ring_screens[2] = scr_tasbeeh;
    ring_screens[3] = scr_isteghfar;

#if NAV_DEBUG
    Serial.println("[INIT] screens created:");
    Serial.printf("  [0] scr_home      = %p\n", scr_home);
    Serial.printf("  [1] scr_thiker    = %p\n", scr_thiker);
    Serial.printf("  [2] scr_tasbeeh   = %p\n", scr_tasbeeh);
    Serial.printf("  [3] scr_isteghfar = %p\n", scr_isteghfar);
    Serial.printf("      scr_settings  = %p\n", scr_settings);
    Serial.printf("      scr_timeedit  = %p\n", scr_timeedit);
#endif

    for (int i = 0; i < RING_LEN; i++) {
        lv_obj_t *scr = ring_screens[i];
        lv_obj_add_event_cb(scr, gesture_ring_cb, LV_EVENT_GESTURE, NULL);
        lv_obj_add_event_cb(scr, touch_debug_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(scr, touch_debug_cb, LV_EVENT_RELEASED, NULL);

        uint32_t child_cnt = lv_obj_get_child_cnt(scr);
        for (uint32_t c = 0; c < child_cnt; c++) {
            lv_obj_t *child = lv_obj_get_child(scr, c);
            lv_obj_add_event_cb(child, gesture_ring_cb, LV_EVENT_GESTURE, NULL);
            lv_obj_add_event_cb(child, touch_debug_cb, LV_EVENT_PRESSED, NULL);
            lv_obj_add_event_cb(child, touch_debug_cb, LV_EVENT_RELEASED, NULL);
        }
#if NAV_DEBUG
        Serial.printf("[INIT] gesture+touch attached to ring[%d]=%p + %lu children\n",
            i, scr, child_cnt);
#endif
    }

    lv_obj_t *modal_screens[] = { scr_settings, scr_timeedit };
    for (int m = 0; m < 2; m++) {
        lv_obj_t *ms = modal_screens[m];
        lv_obj_add_event_cb(ms, gesture_modal_cb, LV_EVENT_GESTURE, NULL);
        lv_obj_add_flag(ms, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ms, modal_bg_tap_cb, LV_EVENT_CLICKED, NULL);
        uint32_t child_cnt = lv_obj_get_child_cnt(ms);
        for (uint32_t c = 0; c < child_cnt; c++) {
            lv_obj_t *child = lv_obj_get_child(ms, c);
            lv_obj_add_event_cb(child, gesture_modal_cb, LV_EVENT_GESTURE, NULL);
        }
#if NAV_DEBUG
        Serial.printf("[INIT] gesture_modal attached to modal[%d]=%p + %lu children\n",
            m, ms, child_cnt);
#endif
    }

#if NAV_DEBUG
    Serial.println("[INIT] === ready — now try swiping LEFT / RIGHT / DOWN ===");
#endif
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
