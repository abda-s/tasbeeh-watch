#include "screens.h"
#include "styles.h"
#include <Preferences.h>
#include <stdlib.h>
#include <Arduino.h>

extern Preferences prefs;
extern uint32_t tasbeehCount;
extern uint32_t isteghfarCount;

static void tap_counter_cb(lv_event_t *e) {
    CounterType type = (CounterType)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *owner = (type == COUNTER_TASBEEH) ? scr_tasbeeh : scr_isteghfar;
    if (lv_screen_active() != owner) return;
#if NAV_DEBUG
    Serial.printf("[TAP] counter  type=%d\n", type);
#endif
    uint32_t *counter;
    const char *key;
    lv_obj_t *arc, *counter_label, *total_label;
    lv_color_t arc_color, accent;
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

    lv_label_set_text_fmt(total_label, "%s: %lu",
        (type == COUNTER_TASBEEH) ? "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212" : "\330\247\331\204\330\245\330\254\331\205\330\247\331\204\331\212",
        *counter);

    if (val == 0 && *counter > 0) {
        lv_arc_set_value(arc, 99);
        lv_label_set_text_fmt(counter_label, "%d", 99);
        lv_obj_set_style_arc_color(arc, accent, LV_PART_INDICATOR);
        FlashData *d = (FlashData *)malloc(sizeof(FlashData));
        d->arc           = arc;
        d->counter_label = counter_label;
        d->revert_color  = arc_color;
        lv_timer_t *t = lv_timer_create(revert_arc_color_cb, 400, d);
        lv_timer_set_repeat_count(t, 1);
    } else {
        lv_arc_set_value(arc, val);
        lv_label_set_text_fmt(counter_label, "%d", val);
    }
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
}

void create_screen_tasbeeh(void) {
    create_counter_screen(&scr_tasbeeh, &tasbeeh_arc, &tasbeeh_counter_label,
        &tasbeeh_total_label, "\330\252\330\263\330\250\331\212\330\255",
        tasbeehCount, color_teal, COUNTER_TASBEEH);
}

void create_screen_isteghfar(void) {
    create_counter_screen(&scr_isteghfar, &isteghfar_arc, &isteghfar_counter_label,
        &isteghfar_total_label, "\330\247\330\263\330\252\330\272\331\201\330\247\330\261",
        isteghfarCount, color_blue, COUNTER_ISTEGHFAR);
}
