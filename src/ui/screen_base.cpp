#include "screens.h"
#include "styles.h"
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
#if NAV_DEBUG
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    Serial.printf("[TOUCH] event=%d x=%d y=%d\n", code, pt.x, pt.y);
#endif
}

// revert_arc_color_cb removed — FlashData no longer used
