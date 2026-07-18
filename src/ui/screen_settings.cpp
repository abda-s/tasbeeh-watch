#include "screens.h"
#include "styles.h"
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);

static void settings_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    pop_modal();
}

void create_screen_settings(void) {
    scr_settings = lv_obj_create(NULL);
    make_screen_base(scr_settings);

    create_title(scr_settings,
        "\330\247\331\204\330\245\330\271\330\257\330\247\330\257\330\247\330\252");

    // ── Back Button ────────────────────────────────────────
    lv_obj_t *back_btn = lv_btn_create(scr_settings);
    lv_obj_set_size(back_btn, 100, 36);
    lv_obj_add_style(back_btn, &style_card, 0);
    lv_obj_set_style_border_color(back_btn, color_border, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(back_lbl, color_cream, 0);
    lv_label_set_text(back_lbl, "< \330\261\330\254\331\210\330\271");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, settings_back_cb, LV_EVENT_CLICKED, NULL);
}
