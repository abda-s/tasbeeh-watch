#include "screens.h"
#include "styles.h"
#include <stdlib.h>
#include <Arduino.h>

void make_screen_base(lv_obj_t *scr) {
    lv_obj_add_style(scr, &style_bg, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
}

lv_obj_t *create_title(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_add_style(label, &style_title, 0);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    return label;
}

void touch_debug_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
#if NAV_DEBUG
    Serial.printf("[TOUCH] event=%d (PRESSED=%d RELEASED=%d)  x=%d y=%d\n",
        code, LV_EVENT_PRESSED, LV_EVENT_RELEASED, pt.x, pt.y);
#endif
}

void revert_arc_color_cb(lv_timer_t *timer) {
    FlashData *d = (FlashData *)lv_timer_get_user_data(timer);
    lv_obj_set_style_arc_color(d->arc, d->revert_color, LV_PART_INDICATOR);
    lv_arc_set_value(d->arc, 0);
    lv_label_set_text(d->counter_label, "0");
    free(d);
    lv_timer_delete(timer);
}
