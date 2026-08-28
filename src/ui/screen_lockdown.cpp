/*
 * screen_lockdown.c — battery <5% lockdown screen
 *
 * Deliberately a dead end for a *real* lockdown: no back button, no
 * navigation. Only ever reached via main.cpp's lockdown boot path (not
 * part of ring_screens[] or screens_init()'s modal_screens[]) and only
 * ever left by the battery recovering enough to boot normally again.
 *
 * The one exception is a long-press, wired to requestLockdownTestExit()
 * (main.cpp) purely for bench testing — it refuses to do anything unless
 * this lockdown was itself test-forced, so it has no effect on a real
 * low-battery lockdown.
 */

#include "screens.h"
#include "styles.h"
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);

lv_obj_t *scr_lockdown = NULL;

#if LOCKDOWN_TEST_ENABLED
static void lockdown_long_press_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_lockdown) return;
    requestLockdownTestExit();
}
#endif

void create_screen_lockdown(void) {
    scr_lockdown = lv_obj_create(NULL);
    make_screen_base(scr_lockdown);
#if LOCKDOWN_TEST_ENABLED
    lv_obj_add_flag(scr_lockdown, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_lockdown, lockdown_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);
#endif

    lv_obj_t *cont = lv_obj_create(scr_lockdown);
    lv_obj_set_size(cont, 200, 160);
    lv_obj_center(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
#if LOCKDOWN_TEST_ENABLED
    // lv_obj_create() defaults to CLICKABLE — without bubbling, this
    // container (covering most of the screen) would swallow the long-press
    // before it ever reached scr_lockdown's handler above.
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
#endif
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 14, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_obj_add_style(title, &style_label_ar, 0);   // 16px — style_label_ar_lg (28px) read too large
    lv_obj_set_style_text_color(title, color_red, 0);
    lv_label_set_text(title, "البطارية منخفضة جدًا");

    lv_obj_t *sub = lv_label_create(cont);
    lv_obj_add_style(sub, &style_label_sm, 0);
    lv_label_set_text(sub, "الرجاء الشحن");
}
