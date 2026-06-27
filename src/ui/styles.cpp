#include "styles.h"

lv_color_t color_bg       = lv_color_hex(0x0a0e1a);
lv_color_t color_surface  = lv_color_hex(0x111827);
lv_color_t color_teal     = lv_color_hex(0x00c8a0);
lv_color_t color_teal_dark= lv_color_hex(0x00997a);
lv_color_t color_blue     = lv_color_hex(0x3b82f6);
lv_color_t color_gold     = lv_color_hex(0xfea020);
lv_color_t color_green    = lv_color_hex(0x00e000);
lv_color_t color_red      = lv_color_hex(0xff4040);
lv_color_t color_white    = lv_color_hex(0xf9fafb);
lv_color_t color_grey     = lv_color_hex(0x6b7280);
lv_color_t color_border   = lv_color_hex(0x1f2937);

lv_style_t style_bg;
lv_style_t style_clock;
lv_style_t style_title;
lv_style_t style_label_sm;
lv_style_t style_label_ar;
lv_style_t style_btn_pill_teal;
lv_style_t style_btn_pill_blue;
lv_style_t style_counter_val;
lv_style_t style_arc_teal;
lv_style_t style_arc_blue;
lv_style_t style_card;
lv_style_t style_icon_btn;

void ui_styles_init(void)
{
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, color_bg);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_text_color(&style_bg, color_white);
    lv_style_set_pad_all(&style_bg, 0);

    lv_style_init(&style_clock);
    lv_style_set_text_font(&style_clock, &lv_font_montserrat_42);
    lv_style_set_text_color(&style_clock, color_gold);
    lv_style_set_text_align(&style_clock, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_dejavu_16_persian_hebrew);
    lv_style_set_text_color(&style_title, color_teal);
    lv_style_set_text_align(&style_title, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_label_sm);
    lv_style_set_text_font(&style_label_sm, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_label_sm, color_grey);
    lv_style_set_text_align(&style_label_sm, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_label_ar);
    lv_style_set_text_font(&style_label_ar, &lv_font_dejavu_16_persian_hebrew);
    lv_style_set_text_color(&style_label_ar, color_white);
    lv_style_set_text_align(&style_label_ar, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_btn_pill_teal);
    lv_style_set_bg_color(&style_btn_pill_teal, color_teal);
    lv_style_set_bg_opa(&style_btn_pill_teal, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_pill_teal, 26);
    lv_style_set_text_font(&style_btn_pill_teal, &lv_font_dejavu_16_persian_hebrew);
    lv_style_set_text_color(&style_btn_pill_teal, color_white);
    lv_style_set_shadow_width(&style_btn_pill_teal, 8);
    lv_style_set_shadow_offset_y(&style_btn_pill_teal, 4);
    lv_style_set_shadow_opa(&style_btn_pill_teal, LV_OPA_30);

    lv_style_init(&style_btn_pill_blue);
    lv_style_set_bg_color(&style_btn_pill_blue, color_blue);
    lv_style_set_bg_opa(&style_btn_pill_blue, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_pill_blue, 26);
    lv_style_set_text_font(&style_btn_pill_blue, &lv_font_dejavu_16_persian_hebrew);
    lv_style_set_text_color(&style_btn_pill_blue, color_white);
    lv_style_set_shadow_width(&style_btn_pill_blue, 8);
    lv_style_set_shadow_offset_y(&style_btn_pill_blue, 4);
    lv_style_set_shadow_opa(&style_btn_pill_blue, LV_OPA_30);

    lv_style_init(&style_counter_val);
    lv_style_set_text_font(&style_counter_val, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_counter_val, color_white);
    lv_style_set_text_align(&style_counter_val, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&style_arc_teal);
    lv_style_set_arc_color(&style_arc_teal, color_teal);
    lv_style_set_arc_width(&style_arc_teal, 6);
    lv_style_set_arc_rounded(&style_arc_teal, true);

    lv_style_init(&style_arc_blue);
    lv_style_set_arc_color(&style_arc_blue, color_blue);
    lv_style_set_arc_width(&style_arc_blue, 6);
    lv_style_set_arc_rounded(&style_arc_blue, true);

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, color_surface);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, color_border);
    lv_style_set_pad_all(&style_card, 16);

    lv_style_init(&style_icon_btn);
    lv_style_set_bg_opa(&style_icon_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_icon_btn, color_grey);
    lv_style_set_text_font(&style_icon_btn, &lv_font_montserrat_14);
}
