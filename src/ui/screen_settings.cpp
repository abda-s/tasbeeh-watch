#include "screens.h"
#include "styles.h"
#include <Arduino.h>

// Set to 0 to hide the debug "Erase WiFi" button for production
#define DEBUG_ERASE_WIFI 1

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_12);

lv_obj_t *settings_status_label = NULL;
lv_obj_t *settings_ssid_label   = NULL;
lv_obj_t *settings_wifi_btn     = NULL;
lv_obj_t *settings_qr_card      = NULL;
lv_obj_t *settings_dashboard_btn = NULL;

static lv_obj_t *status_dot = NULL;

static void settings_back_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    pop_modal();
}

static void settings_wifi_setup_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    extern void wifi_setup_start_ap(void);
    extern void wifi_setup_set_ap_view(void);
    wifi_setup_start_ap();
    push_modal(scr_wifi_setup);
    wifi_setup_set_ap_view();
}

static void settings_qrcode_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    extern void wifi_setup_show_qr(void);
    wifi_setup_show_qr();
    push_modal(scr_qrcode);
}

#if DEBUG_ERASE_WIFI
static void settings_erase_wifi_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_settings) return;
    extern void erase_wifi_credentials(void);
    erase_wifi_credentials();
}
#endif

void update_wifi_status_display(void) {
    if (!settings_status_label) return;

    if (wifi_ready) {
        lv_label_set_text(settings_status_label, "\331\205\330\252\330\265\331\204");
        lv_obj_set_style_text_color(settings_status_label, color_teal, 0);

        if (settings_ssid_label && wifi_local_ssid.length() > 0) {
            lv_label_set_text(settings_ssid_label, wifi_local_ssid.c_str());
        }

        if (settings_ip_label) {
            String ipText = "http://" + wifi_local_ip;
            lv_label_set_text(settings_ip_label, ipText.c_str());
            lv_obj_clear_flag(settings_ip_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (status_dot) {
            lv_obj_set_style_bg_color(status_dot, color_teal, 0);
        }

        if (settings_wifi_btn)
            lv_obj_add_flag(settings_wifi_btn, LV_OBJ_FLAG_HIDDEN);
        if (settings_qr_card)
            lv_obj_clear_flag(settings_qr_card, LV_OBJ_FLAG_HIDDEN);
        if (settings_dashboard_btn)
            lv_obj_clear_flag(settings_dashboard_btn, LV_OBJ_FLAG_HIDDEN);

    } else {
        lv_label_set_text(settings_status_label,
            "\330\272\331\212\330\261 \331\205\330\252\330\265\331\204");
        lv_obj_set_style_text_color(settings_status_label, color_red, 0);

        if (settings_ssid_label)
            lv_label_set_text(settings_ssid_label, "");

        if (settings_ip_label) {
            lv_obj_add_flag(settings_ip_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (status_dot) {
            lv_obj_set_style_bg_color(status_dot, color_red, 0);
        }

        if (settings_wifi_btn)
            lv_obj_clear_flag(settings_wifi_btn, LV_OBJ_FLAG_HIDDEN);
        if (settings_qr_card)
            lv_obj_add_flag(settings_qr_card, LV_OBJ_FLAG_HIDDEN);
        if (settings_dashboard_btn)
            lv_obj_add_flag(settings_dashboard_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void create_screen_settings(void) {
    scr_settings = lv_obj_create(NULL);
    make_screen_base(scr_settings);

    create_title(scr_settings,
        "\330\247\331\204\330\245\330\271\330\257\330\247\330\257\330\247\330\252");

    // ── Main Scrollable Flex Container ───────────────────────
    lv_obj_t * cont = lv_obj_create(scr_settings);
    lv_obj_set_size(cont, 240, 190);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);
    lv_obj_set_style_pad_row(cont, 15, 0);
    lv_obj_set_style_pad_bottom(cont, 30, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLL_CHAIN);

    // ── Card 1: WiFi status ─────────────────────────────────
    lv_obj_t *wifi_card = lv_obj_create(cont);
    lv_obj_set_size(wifi_card, 210, LV_SIZE_CONTENT);
    lv_obj_add_style(wifi_card, &style_card, 0);
    lv_obj_clear_flag(wifi_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(wifi_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wifi_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wifi_card, 12, 0);
    lv_obj_set_style_pad_row(wifi_card, 8, 0);

    // Header row inside card (Dot + SSID <---> Status)
    lv_obj_t * card_top_row = lv_obj_create(wifi_card);
    lv_obj_set_size(card_top_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(card_top_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card_top_row, 0, 0);
    lv_obj_set_style_pad_all(card_top_row, 0, 0);
    lv_obj_set_layout(card_top_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card_top_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card_top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Group for Dot + SSID
    lv_obj_t * ssid_group = lv_obj_create(card_top_row);
    lv_obj_set_size(ssid_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ssid_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ssid_group, 0, 0);
    lv_obj_set_style_pad_all(ssid_group, 0, 0);
    lv_obj_set_layout(ssid_group, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ssid_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ssid_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ssid_group, 8, 0);

    status_dot = lv_obj_create(ssid_group);
    lv_obj_set_size(status_dot, 12, 12);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_dot, color_red, 0);
    lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_dot, 0, 0);

    settings_ssid_label = lv_label_create(ssid_group);
    lv_obj_set_style_text_font(settings_ssid_label, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(settings_ssid_label, color_cream, 0);
    lv_label_set_text(settings_ssid_label, "");

    settings_status_label = lv_label_create(card_top_row);
    lv_obj_set_style_text_font(settings_status_label, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(settings_status_label, color_red, 0);
    lv_label_set_text(settings_status_label, "\330\272\331\212\330\261 \331\205\330\252\330\265\331\204");

    // IP Label (Hidden by default, shown via update_wifi_status_display)
    settings_ip_label = lv_label_create(wifi_card);
    lv_obj_set_style_text_font(settings_ip_label, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(settings_ip_label, color_cream_dim, 0);
    lv_label_set_text(settings_ip_label, "");
    lv_obj_add_flag(settings_ip_label, LV_OBJ_FLAG_HIDDEN);

    // ── Card 2: Web dashboard QR ──────────────────────────
    settings_qr_card = lv_obj_create(cont);
    lv_obj_set_size(settings_qr_card, 210, LV_SIZE_CONTENT);
    lv_obj_add_style(settings_qr_card, &style_card, 0);
    lv_obj_clear_flag(settings_qr_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settings_qr_card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_layout(settings_qr_card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_qr_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settings_qr_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(settings_qr_card, 12, 0);
    lv_obj_set_style_pad_row(settings_qr_card, 10, 0);

    lv_obj_t *dash_title = lv_label_create(settings_qr_card);
    lv_obj_set_style_text_font(dash_title, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(dash_title, color_cream, 0);
    lv_label_set_text(dash_title, "\331\204\331\210\330\255\330\251 \330\247\331\204\330\252\330\255\331\203\331\205");

    settings_dashboard_btn = lv_btn_create(settings_qr_card);
    lv_obj_set_size(settings_dashboard_btn, 130, 32);
    lv_obj_add_style(settings_dashboard_btn, &style_btn_pill_teal, 0);
    lv_obj_t *qr_btn_lbl = lv_label_create(settings_dashboard_btn);
    lv_obj_set_style_text_font(qr_btn_lbl, &font_alexandria_12, 0);
    lv_label_set_text(qr_btn_lbl, "\330\271\330\261\330\266 \330\261\331\205\330\262 QR");
    lv_obj_center(qr_btn_lbl);
    lv_obj_add_event_cb(settings_dashboard_btn, settings_qrcode_cb, LV_EVENT_CLICKED, NULL);

    // ── WiFi Setup Button ──────────────────────────────────
    settings_wifi_btn = lv_btn_create(cont);
    lv_obj_set_size(settings_wifi_btn, 160, 34);
    lv_obj_add_style(settings_wifi_btn, &style_btn_pill_teal, 0);
    lv_obj_t *wifi_btn_lbl = lv_label_create(settings_wifi_btn);
    lv_obj_set_style_text_font(wifi_btn_lbl, &font_alexandria_16, 0);
    lv_label_set_text(wifi_btn_lbl, "\330\245\330\271\330\257\330\247\330\257 \330\247\331\204\331\210\330\247\331\212 \331\201\330\247\331\212");
    lv_obj_center(wifi_btn_lbl);
    lv_obj_add_event_cb(settings_wifi_btn, settings_wifi_setup_cb, LV_EVENT_CLICKED, NULL);

#if DEBUG_ERASE_WIFI
    // ── Debug: Erase WiFi Credentials ─────────────────────
    lv_obj_t *erase_btn = lv_btn_create(cont);
    lv_obj_set_size(erase_btn, 170, 34);
    lv_obj_add_style(erase_btn, &style_card, 0);
    lv_obj_set_style_border_color(erase_btn, color_red, 0);
    lv_obj_set_style_border_width(erase_btn, 1, 0);
    lv_obj_t *erase_lbl = lv_label_create(erase_btn);
    lv_obj_set_style_text_font(erase_lbl, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(erase_lbl, color_red, 0);
    lv_label_set_text(erase_lbl, "\331\205\330\263\330\255 \330\250\331\212\330\247\331\206\330\247\330\252 \330\247\331\204\331\210\330\247\331\212 \331\201\330\247\331\212");
    lv_obj_center(erase_lbl);
    lv_obj_add_event_cb(erase_btn, settings_erase_wifi_cb, LV_EVENT_CLICKED, NULL);
#endif

    // ── Back Button ────────────────────────────────────────
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
    lv_obj_add_event_cb(back_btn, settings_back_cb, LV_EVENT_CLICKED, NULL);

    update_wifi_status_display();
}
