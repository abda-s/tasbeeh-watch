#include "screens.h"
#include "styles.h"
#include <Arduino.h>

static void thiker_tasbeeh_btn_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_thiker) return;
#if NAV_DEBUG
    Serial.println("[CLICK] thiker -> tasbeeh");
#endif
    navigate_to_ring(2);
}
static void thiker_isteghfar_btn_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_thiker) return;
#if NAV_DEBUG
    Serial.println("[CLICK] thiker -> isteghfar");
#endif
    navigate_to_ring(3);
}

void create_screen_thiker(void) {
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
}
