#include "screens.h"
#include "styles.h"

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_12);

static lv_obj_t *qr_obj = NULL;
static lv_obj_t *qr_url_label = NULL;

static void qrcode_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_qrcode) return;
    pop_modal();
}

void create_screen_qrcode(void) {
    scr_qrcode = lv_obj_create(NULL);
    make_screen_base(scr_qrcode);

    create_title(scr_qrcode, "\330\261\331\205\330\262 QR");

    // ── Scrollable Flex Container ────────────────────────────
    lv_obj_t * cont = lv_obj_create(scr_qrcode);
    lv_obj_set_size(cont, 240, 190);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 12, 0);
    lv_obj_set_style_pad_bottom(cont, 40, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_CHAIN);

    // 1. QR Code
    qr_obj = lv_qrcode_create(cont);
    lv_qrcode_set_size(qr_obj, 140);
    lv_qrcode_set_dark_color(qr_obj, lv_color_hex(0x0b1410));
    lv_qrcode_set_light_color(qr_obj, lv_color_hex(0xf6e6b3));
    lv_qrcode_update(qr_obj, "http://0.0.0.0", 14);

    // 2. URL Label
    qr_url_label = lv_label_create(cont);
    lv_obj_set_style_text_font(qr_url_label, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(qr_url_label, color_cream_dim, 0);
    lv_label_set_text(qr_url_label, "http://0.0.0.0");

    // 3. Hint Text (Wrapped)
    lv_obj_t *hint = lv_label_create(cont);
    lv_obj_set_style_text_font(hint, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(hint, color_cream_dim, 0);
    lv_label_set_text(hint,
        "\330\247\331\205\330\263\330\255 \330\247\331\204\330\261\331\205\330\262 "
        "\331\204\331\204\330\257\330\256\331\210\331\204 "
        "\331\204\331\204\331\210\330\255\330\251 \330\247\331\204\330\252\330\255\331\203\331\205");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, 200);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    // 4. Back button
    lv_obj_t *back_btn = lv_btn_create(cont);
    lv_obj_set_size(back_btn, 100, 36);
    lv_obj_add_style(back_btn, &style_card, 0);
    lv_obj_set_style_border_color(back_btn, color_border, 0);
    lv_obj_set_style_border_width(back_btn, 1, 0);
    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_obj_set_style_text_font(back_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(back_lbl, color_cream, 0);
    lv_label_set_text(back_lbl, "< \330\261\330\254\331\210\330\271");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, qrcode_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(scr_qrcode, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_qrcode, modal_bg_tap_cb, LV_EVENT_CLICKED, NULL);
}

void qrcode_set_url(const char *url) {
    if (qr_obj) lv_qrcode_update(qr_obj, url, strlen(url));
    if (qr_url_label) lv_label_set_text(qr_url_label, url);
}
