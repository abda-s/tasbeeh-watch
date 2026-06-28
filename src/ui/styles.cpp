#include "styles.h"

LV_FONT_DECLARE(font_reem_kufi_72);
LV_FONT_DECLARE(font_reem_kufi_48);
LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);

// ── Colour definitions ────────────────────────────────────────
lv_color_t color_bg        = {0};
lv_color_t color_surface   = {0};
lv_color_t color_gold      = {0};
lv_color_t color_ivory     = {0};
lv_color_t color_cream     = {0};
lv_color_t color_cream_dim = {0};
lv_color_t color_teal      = {0};
lv_color_t color_blue       = {0};
lv_color_t color_border    = {0};
lv_color_t color_red       = {0};
lv_color_t color_white     = {0};
lv_color_t color_grey      = {0};

// ── Style objects ─────────────────────────────────────────────
lv_style_t style_bg;
lv_style_t style_clock;
lv_style_t style_title;
lv_style_t style_label_sm;
lv_style_t style_label_ar;
lv_style_t style_label_ar_lg;
lv_style_t style_counter_val;
lv_style_t style_arc_gold;
lv_style_t style_arc_teal;
lv_style_t style_pill_border;
lv_style_t style_icon_btn;
lv_style_t style_btn_pill_teal;
lv_style_t style_btn_pill_blue;
lv_style_t style_card;

void ui_styles_init(void)
{
    // ── Palette ───────────────────────────────────────────────
    color_bg        = lv_color_hex(0x0b1410);
    color_surface   = lv_color_hex(0x0e2820);
    color_gold      = lv_color_hex(0xd4af37);
    color_ivory     = lv_color_hex(0xf6e6b3);
    color_cream     = lv_color_hex(0xe9d9a8);
    color_cream_dim = lv_color_hex(0x7a6e56);
    color_teal      = lv_color_hex(0x33cc55);  // green
    color_blue      = lv_color_hex(0x7fd6a3);  // sky blue
    color_border    = lv_color_hex(0x2a2410);
    color_red       = lv_color_hex(0xff4444);
    color_white     = lv_color_hex(0xf6e6b3);
    color_grey      = lv_color_hex(0x7a6e56);

    // ── Screen background ─────────────────────────────────────
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, color_bg);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
    lv_style_set_text_color(&style_bg, color_cream);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_border_width(&style_bg, 0);

    // ── Large clock digits "23:13" ────────────────────────────
    lv_style_init(&style_clock);
    lv_style_set_text_font(&style_clock, &font_reem_kufi_72);
    lv_style_set_text_color(&style_clock, color_ivory);
    lv_style_set_text_align(&style_clock, LV_TEXT_ALIGN_CENTER);

    // ── Screen title (gold, Arabic) ───────────────────────────
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &font_alexandria_16);
    lv_style_set_text_color(&style_title, color_gold);
    lv_style_set_text_align(&style_title, LV_TEXT_ALIGN_CENTER);

    // ── Small label (muted) ───────────────────────────────────
    lv_style_init(&style_label_sm);
    lv_style_set_text_font(&style_label_sm, &font_alexandria_16);
    lv_style_set_text_color(&style_label_sm, color_cream_dim);
    lv_style_set_text_align(&style_label_sm, LV_TEXT_ALIGN_CENTER);

    // ── Normal Arabic body text (cream) ──────────────────────
    lv_style_init(&style_label_ar);
    lv_style_set_text_font(&style_label_ar, &font_alexandria_16);
    lv_style_set_text_color(&style_label_ar, color_cream);
    lv_style_set_text_align(&style_label_ar, LV_TEXT_ALIGN_CENTER);

    // ── Large Arabic phrase  "أَستَغفِرُ الله" ─────────────────
    lv_style_init(&style_label_ar_lg);
    lv_style_set_text_font(&style_label_ar_lg, &font_alexandria_28);
    lv_style_set_text_color(&style_label_ar_lg, color_ivory);
    lv_style_set_text_align(&style_label_ar_lg, LV_TEXT_ALIGN_CENTER);

    // ── Big counter digit e.g. "0" ────────────────────────────
    lv_style_init(&style_counter_val);
    lv_style_set_text_font(&style_counter_val, &font_reem_kufi_48);
    lv_style_set_text_color(&style_counter_val, color_ivory);
    lv_style_set_text_align(&style_counter_val, LV_TEXT_ALIGN_CENTER);

    // ── Arc – gold (istighfar) ────────────────────────────────
    lv_style_init(&style_arc_gold);
    lv_style_set_arc_color(&style_arc_gold, color_gold);
    lv_style_set_arc_width(&style_arc_gold, 4);
    lv_style_set_arc_rounded(&style_arc_gold, true);

    // ── Arc – teal (tasbeeh) ──────────────────────────────────
    lv_style_init(&style_arc_teal);
    lv_style_set_arc_color(&style_arc_teal, color_teal);
    lv_style_set_arc_width(&style_arc_teal, 4);
    lv_style_set_arc_rounded(&style_arc_teal, true);

    // ── Pill border (date chip) ───────────────────────────────
    lv_style_init(&style_pill_border);
    lv_style_set_bg_opa(&style_pill_border, LV_OPA_TRANSP);
    lv_style_set_border_color(&style_pill_border, color_gold);
    lv_style_set_border_opa(&style_pill_border, LV_OPA_30);
    lv_style_set_border_width(&style_pill_border, 1);
    lv_style_set_radius(&style_pill_border, LV_RADIUS_CIRCLE);
    lv_style_set_pad_hor(&style_pill_border, 12);
    lv_style_set_pad_ver(&style_pill_border, 4);
    lv_style_set_text_font(&style_pill_border, &font_alexandria_16);
    lv_style_set_text_color(&style_pill_border, color_cream);
    lv_style_set_text_align(&style_pill_border, LV_TEXT_ALIGN_CENTER);

    // ── Icon button ───────────────────────────────────────────
    lv_style_init(&style_icon_btn);
    lv_style_set_bg_opa(&style_icon_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_icon_btn, color_cream_dim);
    lv_style_set_text_font(&style_icon_btn, &lv_font_montserrat_14);
    lv_style_set_border_width(&style_icon_btn, 0);
    lv_style_set_shadow_width(&style_icon_btn, 0);

    // ── Teal pill button ──────────────────────────────────────
    lv_style_init(&style_btn_pill_teal);
    lv_style_set_bg_color(&style_btn_pill_teal, color_teal);
    lv_style_set_bg_opa(&style_btn_pill_teal, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_pill_teal, 26);
    lv_style_set_text_color(&style_btn_pill_teal, color_bg);
    lv_style_set_border_width(&style_btn_pill_teal, 0);

    // ── Blue pill button (unused but kept for compat) ─────────
    lv_style_init(&style_btn_pill_blue);
    lv_style_set_bg_color(&style_btn_pill_blue, lv_color_hex(0x3b82f6));
    lv_style_set_bg_opa(&style_btn_pill_blue, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_pill_blue, 26);
    lv_style_set_text_color(&style_btn_pill_blue, color_white);
    lv_style_set_border_width(&style_btn_pill_blue, 0);

    // ── Card ──────────────────────────────────────────────────
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, color_surface);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_border_color(&style_card, color_border);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_pad_all(&style_card, 12);
}
