#include "screens.h"
#include "styles.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_12);
LV_FONT_DECLARE(font_dejavu_sans_16);

extern bool wifi_ready;
extern String wifi_local_ip;
extern void wifi_setup_start_ap(void);
extern void wifi_setup_stop_ap(void);
extern void wifi_setup_cancel(void);

static lv_obj_t *choice_cont = NULL;
static lv_obj_t *ap_cont = NULL;
static lv_obj_t *wifi_qr = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *wifi_status_text = NULL;
static lv_obj_t *retry_btn = NULL;
static lv_obj_t *cancel_btn = NULL;

static const char *AP_IP = "http://192.168.4.1";
static const char *AP_NAME = "TasbeehWatch";

void wifi_setup_set_ap_view(void);

// ── Choice: "استمرار بدون اتصال" ──────────────────────────
static void choice_decline_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_wifi_setup) return;
    extern void wifi_setup_stop_ap(void);
    extern bool wifi_ready;
    extern String wifi_local_ip;
    extern String wifi_local_ssid;
    extern Preferences prefs;
    wifi_ready = false;
    wifi_local_ip = "";
    wifi_local_ssid = "";
    prefs.putBool("wifi_offline", true);
    update_wifi_status_display();
    pop_modal();
}

// ── Choice: "اتصل بالشبكة" ───────────────────────────────
static void choice_connect_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_wifi_setup) return;
    wifi_setup_start_ap();
    wifi_setup_set_ap_view();
}

// ── AP: Cancel setup ──────────────────────────────────────
static void setup_cancel_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_wifi_setup) return;
    wifi_setup_cancel();
    pop_modal();
}

// ── AP: Retry ─────────────────────────────────────────────
static void setup_retry_cb(lv_event_t *e) {
    if (lv_screen_active() != scr_wifi_setup) return;
    wifi_setup_start_ap();
    lv_qrcode_update(wifi_qr, AP_IP, strlen(AP_IP));
    lv_label_set_text(wifi_ip_label, AP_IP);
    if (wifi_status_text)
        lv_label_set_text(wifi_status_text,
            "\330\250\330\247\331\206\330\252\330\270\330\247\330\261 \330\247\331\204\330\247\330\252\330\265\330\247\331\204...");
    if (retry_btn) lv_obj_add_flag(retry_btn, LV_OBJ_FLAG_HIDDEN);
    if (cancel_btn) lv_obj_clear_flag(cancel_btn, LV_OBJ_FLAG_HIDDEN);
}

static void setup_retry_btn_cb(lv_event_t *e) {
    setup_retry_cb(e);
}

// ── Called by main.cpp on AP scan complete ────────────────
void wifi_setup_on_scan_done(void) {
    // Scan results are already stored globally — nothing UI to update directly
    // The captive portal web page will fetch /api/scan when loaded
}

// ── Called by main.cpp when AP was already started elsewhere ──
// and we just need to show the AP view (no choice, skip straight to QR)
void wifi_setup_set_ap_view(void) {
    if (choice_cont) lv_obj_add_flag(choice_cont, LV_OBJ_FLAG_HIDDEN);
    if (ap_cont)     lv_obj_clear_flag(ap_cont, LV_OBJ_FLAG_HIDDEN);
    lv_qrcode_update(wifi_qr, AP_IP, strlen(AP_IP));
    lv_label_set_text(wifi_ip_label, AP_IP);
    if (wifi_status_text)
        lv_label_set_text(wifi_status_text, "\330\250\330\247\331\206\330\252\330\270\330\247\330\261 \330\247\331\204\330\247\330\252\330\265\330\247\331\204...");
    if (retry_btn) lv_obj_add_flag(retry_btn, LV_OBJ_FLAG_HIDDEN);
    if (cancel_btn) lv_obj_clear_flag(cancel_btn, LV_OBJ_FLAG_HIDDEN);
}

void wifi_setup_show_timeout(void) {
    if (!wifi_status_text) return;
    lv_label_set_text(wifi_status_text,
        "\331\204\331\205 \331\212\330\252\331\205 "
        "\330\247\331\204\330\247\330\252\330\265\330\247\331\204 "
        "\330\255\330\247\331\210\331\204 \331\205\330\261\330\251 \330\243\330\256\330\261\331\211");
    if (retry_btn) lv_obj_clear_flag(retry_btn, LV_OBJ_FLAG_HIDDEN);
}

// ── Switch to choice mode (called when boot WiFi fails) ──
void wifi_setup_show_choice(void) {
    if (choice_cont) lv_obj_clear_flag(choice_cont, LV_OBJ_FLAG_HIDDEN);
    if (ap_cont)     lv_obj_add_flag(ap_cont, LV_OBJ_FLAG_HIDDEN);
    if (wifi_status_text)
        lv_label_set_text(wifi_status_text, "");
}

void create_screen_wifi_setup(void) {
    scr_wifi_setup = lv_obj_create(NULL);
    make_screen_base(scr_wifi_setup);

    lv_obj_add_flag(scr_wifi_setup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr_wifi_setup, modal_bg_tap_cb, LV_EVENT_CLICKED, NULL);

    create_title(scr_wifi_setup, "\330\245\330\271\330\257\330\247\330\257 \330\247\331\204\331\210\330\247\331\212 \331\201\330\247\331\212");

    // ═══ CHOICE VIEW ══════════════════════════════════════
    choice_cont = lv_obj_create(scr_wifi_setup);
    lv_obj_set_size(choice_cont, 240, 190);
    lv_obj_align(choice_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(choice_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(choice_cont, 0, 0);
    lv_obj_set_layout(choice_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(choice_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(choice_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(choice_cont, 10, 0);

    lv_obj_t *q1 = lv_label_create(choice_cont);
    lv_obj_set_style_text_font(q1, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(q1, color_cream, 0);
    lv_label_set_text(q1, "\331\207\331\204 \330\252\330\261\331\212\330\257 \330\247\331\204\330\247\330\252\330\265\330\247\331\204");

    lv_obj_t *q2 = lv_label_create(choice_cont);
    lv_obj_set_style_text_font(q2, &font_dejavu_sans_16, 0);
    lv_obj_set_style_text_color(q2, color_cream_dim, 0);
    lv_label_set_text(q2, "\330\250\330\264\330\250\331\203\330\251 \331\210\330\247\331\212 \331\201\330\247\331\212\330\237");

    lv_obj_t *connect_btn = lv_btn_create(choice_cont);
    lv_obj_set_size(connect_btn, 170, 42);
    lv_obj_add_style(connect_btn, &style_btn_pill_teal, 0);
    lv_obj_t *connect_lbl = lv_label_create(connect_btn);
    lv_obj_set_style_text_font(connect_lbl, &font_dejavu_sans_16, 0);
    lv_label_set_text(connect_lbl, "\330\247\330\252\330\265\331\204 \330\250\330\247\331\204\330\264\330\250\331\203\330\251");
    lv_obj_center(connect_lbl);
    lv_obj_add_event_cb(connect_btn, choice_connect_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *decline_btn = lv_btn_create(choice_cont);
    lv_obj_set_size(decline_btn, 170, 38);
    lv_obj_add_style(decline_btn, &style_card, 0);
    lv_obj_t *decline_lbl = lv_label_create(decline_btn);
    lv_obj_set_style_text_font(decline_lbl, &font_dejavu_sans_16, 0);
    lv_obj_set_style_text_color(decline_lbl, color_cream_dim, 0);
    lv_label_set_text(decline_lbl, "\330\247\331\204\330\247\330\263\330\252\331\205\330\261\330\247\330\261 \330\250\330\257\331\210\331\206 \330\247\330\252\330\265\330\247\331\204");
    lv_obj_add_event_cb(decline_btn, choice_decline_cb, LV_EVENT_CLICKED, NULL);

    // ═══ AP SETUP VIEW ════════════════════════════════════
    ap_cont = lv_obj_create(scr_wifi_setup);
    lv_obj_set_size(ap_cont, 240, 190);
    lv_obj_align(ap_cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(ap_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ap_cont, 0, 0);
    lv_obj_set_layout(ap_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ap_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ap_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ap_cont, 6, 0);
    lv_obj_set_style_pad_bottom(ap_cont, 30, 0);
    lv_obj_add_flag(ap_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ap_cont, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(ap_cont, LV_OBJ_FLAG_HIDDEN);

    // 1. Instruction
    lv_obj_t *instr1 = lv_label_create(ap_cont);
    lv_obj_set_style_text_font(instr1, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(instr1, color_cream_dim, 0);
    lv_label_set_text(instr1, "\330\247\330\252\330\265\331\204 \330\250\331\207\330\247\330\252\331\201\331\203 \330\271\331\204\331\211 \330\247\331\204\330\264\330\250\331\203\330\251:");

    // 2. AP Name
    String ap_text = String("\"") + AP_NAME + String("\"");
    lv_obj_t *ap_label = lv_label_create(ap_cont);
    lv_obj_set_style_text_font(ap_label, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(ap_label, color_gold, 0);
    lv_label_set_text(ap_label, ap_text.c_str());

    // 3. QR Code
    wifi_qr = lv_qrcode_create(ap_cont);
    lv_qrcode_set_size(wifi_qr, 120);
    lv_qrcode_set_dark_color(wifi_qr, lv_color_hex(0x0b1410));
    lv_qrcode_set_light_color(wifi_qr, lv_color_hex(0xf6e6b3));
    lv_qrcode_update(wifi_qr, AP_IP, strlen(AP_IP));

    // 4. IP label
    wifi_ip_label = lv_label_create(ap_cont);
    lv_obj_set_style_text_font(wifi_ip_label, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(wifi_ip_label, color_gold, 0);
    lv_label_set_text(wifi_ip_label, AP_IP);

    // 5. Status / timeout text
    wifi_status_text = lv_label_create(ap_cont);
    lv_obj_set_style_text_font(wifi_status_text, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(wifi_status_text, color_cream, 0);
    lv_label_set_text(wifi_status_text, "\330\250\330\247\331\206\330\252\330\270\330\247\330\261 \330\247\331\204\330\247\330\252\330\265\330\247\331\204...");

    // ── Retry Button ─────────────────────────────────────────
    retry_btn = lv_btn_create(ap_cont);
    lv_obj_set_size(retry_btn, 120, 34);
    lv_obj_add_style(retry_btn, &style_btn_pill_teal, 0);
    lv_obj_t *retry_lbl = lv_label_create(retry_btn);
    lv_obj_set_style_text_font(retry_lbl, &font_alexandria_12, 0);
    lv_label_set_text(retry_lbl, "\330\245\330\271\330\247\330\257\330\251 \330\247\331\204\331\205\330\255\330\247\331\210\331\204\330\251");
    lv_obj_center(retry_lbl);
    lv_obj_add_event_cb(retry_btn, setup_retry_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(retry_btn, LV_OBJ_FLAG_HIDDEN);

    // ── Cancel Button ────────────────────────────────────────
    cancel_btn = lv_btn_create(ap_cont);
    lv_obj_set_size(cancel_btn, 100, 36);
    lv_obj_add_style(cancel_btn, &style_card, 0);
    lv_obj_set_style_border_color(cancel_btn, color_red, 0);
    lv_obj_set_style_border_width(cancel_btn, 1, 0);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_obj_set_style_text_font(cancel_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(cancel_lbl, color_red, 0);
    lv_label_set_text(cancel_lbl, "\330\245\331\204\330\272\330\247\330\241");
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, setup_cancel_cb, LV_EVENT_CLICKED, NULL);
}
