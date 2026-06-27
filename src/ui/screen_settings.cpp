#include "screens.h"
#include "styles.h"

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
    lv_obj_add_style(settings_ip_label, &style_counter_val, 0);
    lv_label_set_text(settings_ip_label, "--");
    lv_obj_align(settings_ip_label, LV_ALIGN_BOTTOM_MID, 0, -5);
}
