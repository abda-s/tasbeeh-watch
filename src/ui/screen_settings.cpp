#include "screens.h"
#include "styles.h"
#include <Arduino.h>

LV_FONT_DECLARE(font_alexandria_16);

static void settings_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    pop_modal();
}

static void settings_notifications_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    refresh_notifications_list();
    push_modal(scr_notifications);
}

static void settings_time_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    open_timeedit_clock();
}

void create_screen_settings(void) {
    scr_settings = lv_obj_create(NULL);
    make_screen_base(scr_settings);

    lv_obj_t *title = create_title(scr_settings,
        "\330\247\331\204\330\245\330\271\330\257\330\247\330\257\330\247\330\252");
    lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(title, settings_back_cb, LV_EVENT_CLICKED, NULL);

    // ── Change time entry ─────────────────────────────────────
    lv_obj_t *time_btn = lv_btn_create(scr_settings);
    lv_obj_set_size(time_btn, 150, 36);
    lv_obj_add_style(time_btn, &style_card, 0);
    lv_obj_set_style_border_color(time_btn, color_border, 0);
    lv_obj_set_style_border_width(time_btn, 1, 0);
    lv_obj_align(time_btn, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *time_lbl_btn = lv_label_create(time_btn);
    lv_obj_set_style_text_font(time_lbl_btn, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(time_lbl_btn, color_cream, 0);
    lv_label_set_text(time_lbl_btn, "تغيير الوقت");
    lv_obj_center(time_lbl_btn);
    lv_obj_add_event_cb(time_btn, settings_time_cb, LV_EVENT_CLICKED, NULL);

    // ── Notifications entry ──────────────────────────────────
    lv_obj_t *notif_btn = lv_btn_create(scr_settings);
    lv_obj_set_size(notif_btn, 150, 36);
    lv_obj_add_style(notif_btn, &style_card, 0);
    lv_obj_set_style_border_color(notif_btn, color_border, 0);
    lv_obj_set_style_border_width(notif_btn, 1, 0);
    lv_obj_align(notif_btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t *notif_lbl = lv_label_create(notif_btn);
    lv_obj_set_style_text_font(notif_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(notif_lbl, color_cream, 0);
    lv_label_set_text(notif_lbl, "الإشعارات");
    lv_obj_center(notif_lbl);
    lv_obj_add_event_cb(notif_btn, settings_notifications_cb, LV_EVENT_CLICKED, NULL);

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
