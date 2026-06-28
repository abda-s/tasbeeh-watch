#include "screens.h"
#include "styles.h"

LV_FONT_DECLARE(font_alexandria_16);

static void settings_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    pop_modal();
}

void create_screen_settings(void) {
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
    lv_obj_add_style(settings_ip_label, &style_label_sm, 0);
    lv_label_set_text(settings_ip_label, "--");
    lv_obj_align(settings_ip_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // ── Back button ────────────────────────────────────────
    lv_obj_t *back_btn = lv_btn_create(scr_settings);
    lv_obj_set_size(back_btn, 100, 36);
    lv_obj_add_style(back_btn, &style_card, 0);
    lv_obj_set_style_border_color(back_btn, color_border, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(back_lbl, color_cream, 0);
    lv_label_set_text(back_lbl, "< \330\261\330\254\331\210\330\271");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, settings_back_cb, LV_EVENT_CLICKED, NULL);
}
