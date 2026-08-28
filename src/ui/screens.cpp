#include "screens.h"
#include <Arduino.h>

lv_obj_t *scr_home       = NULL;
lv_obj_t *scr_istighfar  = NULL;
lv_obj_t *scr_tasbeeh    = NULL;
lv_obj_t *scr_settings   = NULL;
lv_obj_t *scr_timeedit   = NULL;
lv_obj_t *scr_notifications = NULL;

void screens_init(void) {
    create_screen_home();
    create_screen_istighfar();
    create_screen_tasbeeh();
    create_screen_settings();
    create_screen_timeedit();
    create_screen_notifications();

    ring_screens[0] = scr_home;
    ring_screens[1] = scr_istighfar;
    ring_screens[2] = scr_tasbeeh;

#if NAV_DEBUG
    Serial.println("[INIT] screens created:");
    Serial.printf("  [0] scr_home      = %p\n", scr_home);
    Serial.printf("  [1] scr_istighfar = %p\n", scr_istighfar);
    Serial.printf("  [2] scr_tasbeeh   = %p\n", scr_tasbeeh);
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

    lv_obj_t *modal_screens[] = { scr_settings, scr_timeedit, scr_notifications };
    for (int m = 0; m < 3; m++) {
        lv_obj_t *ms = modal_screens[m];
        lv_obj_add_event_cb(ms, gesture_modal_cb, LV_EVENT_GESTURE, NULL);
        // scr_settings excluded: tapping its background used to dismiss it,
        // which made taps near (but not quite on) an option button
        // occasionally register as "go back" instead. Its title label now
        // carries its own back handler (screen_settings.cpp) — that's the
        // only tap-to-dismiss path there; the swipe gesture above still works.
        if (ms != scr_timeedit && ms != scr_settings) {
            if (!lv_obj_has_flag(ms, LV_OBJ_FLAG_CLICKABLE))
                lv_obj_add_flag(ms, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(ms, modal_bg_tap_cb, LV_EVENT_CLICKED, NULL);
        }
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
    Serial.println("[INIT] === ready ===");
#endif
}
