#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
using namespace fs;
#include <CST816S.h>
#include <lvgl.h>
#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
static TFT_eSPI tft(240, 240);

static void my_disp_flush_dma(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    if (lv_display_flush_is_last(disp)) {
        tft.endWrite();
    }
    lv_display_flush_ready(disp);
}
#endif
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

#include "config/CST816S_pin_config.h"
#include "ui/styles.h"
#include "ui/screens.h"
#include "indev/lv_indev_private.h"

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);

#define BAT_ADC       1
#define MAX_REMINDERS 10

struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
};

#include "web_dashboard.h"

CST816S     touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);
Preferences prefs;

int   hour_ = 8, minute_ = 0, second_ = 0;
int   day_  = 1, month_  = 1, year_ = 2026;

Reminder reminders[MAX_REMINDERS];
bool     reminderFired[MAX_REMINDERS];
int      popupRemIdx = 0;
bool     reminderActive = false;
lv_obj_t *reminder_mbox = NULL;

static lv_timer_t *clock_timer_obj = NULL;
static lv_timer_t *battery_timer_obj = NULL;
static lv_timer_t *wifi_timer_obj = NULL;

// ── WiFi state ──────────────────────────────────────────────
enum WifiState { WIFI_IDLE, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_AP_MODE, WIFI_OFFLINE };
static WifiState wifi_state = WIFI_IDLE;
static unsigned long wifi_connect_start = 0;
static unsigned long ap_start_time = 0;
static bool ap_timed_out = false;
static bool wifi_modal_shown = false;
static bool server_running = false;
String scan_ssids[30];
int    scan_rssi[30];
int    scan_count = 0;

WebServer server(80);
DNSServer  dnsServer;

static int getBatteryPercent() {
    long sum = 0;
    for (int i = 0; i < 16; i++) { sum += analogReadMilliVolts(BAT_ADC); delay(1); }
    float v_adc = sum / 16.0f / 1000.0f;
    float v_bat = v_adc * 3.0f;
    int pct = (int)((v_bat - 3.5f) / (4.15f - 3.5f) * 100.0f);
    return constrain(pct, 0, 100);
}

static void updateClock() {
    second_++;
    if (second_ >= 60) { second_ = 0; minute_++; }
    if (minute_ >= 60) { minute_ = 0; hour_++;   }
    if (hour_   >= 24) { hour_   = 0; day_++;    }
}

void saveTime() {
    prefs.putInt("hour", hour_);
    prefs.putInt("minute", minute_);
    prefs.putInt("second", second_);
    prefs.putInt("day", day_);
    prefs.putInt("month", month_);
    prefs.putInt("year", year_);
}

static void loadTime() {
    hour_   = prefs.getInt("hour", 8);
    minute_ = prefs.getInt("minute", 0);
    second_ = prefs.getInt("second", 0);
    day_    = prefs.getInt("day", 1);
    month_  = prefs.getInt("month", 1);
    year_   = prefs.getInt("year", 2026);
}

void saveReminders() {
    for (int i = 0; i < MAX_REMINDERS; i++) {
        String b = "r" + String(i);
        prefs.putInt((b + "h").c_str(), reminders[i].hour);
        prefs.putInt((b + "m").c_str(), reminders[i].minute);
        prefs.putBool((b + "e").c_str(), reminders[i].enabled);
        prefs.putString((b + "l").c_str(), reminders[i].label);
    }
}

static void loadReminders() {
    for (int i = 0; i < MAX_REMINDERS; i++) {
        String b = "r" + String(i);
        reminders[i].hour    = prefs.getInt((b + "h").c_str(), 8);
        reminders[i].minute  = prefs.getInt((b + "m").c_str(), 0);
        reminders[i].enabled = prefs.getBool((b + "e").c_str(), false);
        String lbl = prefs.getString((b + "l").c_str(), "");
        strncpy(reminders[i].label, lbl.c_str(), 31);
        reminders[i].label[31] = '\0';
        reminderFired[i] = false;
    }
}

static void dismiss_cb(lv_event_t *e) {
    if (reminder_mbox) {
        lv_obj_delete(reminder_mbox);
        reminder_mbox = NULL;
    }
    reminderActive = false;
    prefs.putInt("popup_idx", -1);
}

static void snooze_cb(lv_event_t *e) {
    if (reminder_mbox) {
        lv_obj_delete(reminder_mbox);
        reminder_mbox = NULL;
    }
    reminderActive = false;
    reminderFired[popupRemIdx] = true;
    prefs.putInt("popup_idx", -1);
}

static void showReminderPopup(int idx) {
    char time_buf[6];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", reminders[idx].hour, reminders[idx].minute);

    char label_buf[32];
    strncpy(label_buf, reminders[idx].label, 31);
    label_buf[31] = '\0';

    // ── Full-Screen Overlay ────────────────────────────────
    reminder_mbox = lv_obj_create(lv_layer_top());
    lv_obj_set_size(reminder_mbox, 240, 240);
    lv_obj_center(reminder_mbox);
    lv_obj_set_style_bg_color(reminder_mbox, color_bg, 0);
    lv_obj_set_style_bg_opa(reminder_mbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(reminder_mbox, 0, 0);
    lv_obj_set_style_pad_all(reminder_mbox, 10, 0);

    lv_obj_set_layout(reminder_mbox, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(reminder_mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(reminder_mbox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(reminder_mbox, 12, 0);

    // 1. Header
    lv_obj_t *title = lv_label_create(reminder_mbox);
    lv_obj_set_style_text_font(title, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(title, color_cream_dim, 0);
    lv_label_set_text(title, "\330\252\330\260\331\203\331\212\330\261");

    // 2. Time
    lv_obj_t *time_lbl = lv_label_create(reminder_mbox);
    lv_obj_set_style_text_font(time_lbl, &font_alexandria_28, 0);
    lv_obj_set_style_text_color(time_lbl, color_gold, 0);
    lv_label_set_text(time_lbl, time_buf);

    // 3. Label text
    lv_obj_t *text_lbl = lv_label_create(reminder_mbox);
    lv_obj_set_style_text_font(text_lbl, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(text_lbl, color_cream, 0);
    lv_label_set_long_mode(text_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text_lbl, 180);
    lv_obj_set_style_text_align(text_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(text_lbl, label_buf);

    // 4. Buttons row
    lv_obj_t *btn_row = lv_obj_create(reminder_mbox);
    lv_obj_set_size(btn_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, 0);

    // Dismiss
    lv_obj_t *btn_dismiss = lv_btn_create(btn_row);
    lv_obj_set_size(btn_dismiss, 80, 36);
    lv_obj_add_style(btn_dismiss, &style_btn_pill_teal, 0);
    lv_obj_t *lbl_dismiss = lv_label_create(btn_dismiss);
    lv_obj_set_style_text_font(lbl_dismiss, &font_alexandria_16, 0);
    lv_obj_set_style_text_color(lbl_dismiss, color_bg, 0);
    lv_label_set_text(lbl_dismiss, "\330\252\331\205");
    lv_obj_center(lbl_dismiss);
    lv_obj_add_event_cb(btn_dismiss, dismiss_cb, LV_EVENT_CLICKED, NULL);

    // Snooze
    lv_obj_t *btn_snooze = lv_btn_create(btn_row);
    lv_obj_set_size(btn_snooze, 80, 36);
    lv_obj_add_style(btn_snooze, &style_card, 0);
    lv_obj_set_style_border_color(btn_snooze, color_border, 0);
    lv_obj_set_style_border_width(btn_snooze, 1, 0);
    lv_obj_t *lbl_snooze = lv_label_create(btn_snooze);
    lv_obj_set_style_text_font(lbl_snooze, &font_alexandria_12, 0);
    lv_obj_set_style_text_color(lbl_snooze, color_cream_dim, 0);
    lv_label_set_text(lbl_snooze, "\330\252\330\243\330\254\331\212\331\204");
    lv_obj_center(lbl_snooze);
    lv_obj_add_event_cb(btn_snooze, snooze_cb, LV_EVENT_CLICKED, NULL);
}

static void checkReminders() {
    if (reminderActive) return;
    for (int i = 0; i < MAX_REMINDERS; i++) {
        if (!reminders[i].enabled) { reminderFired[i] = false; continue; }
        if (hour_ == reminders[i].hour && minute_ == reminders[i].minute && second_ == 0) {
            if (!reminderFired[i]) {
                reminderFired[i] = true;
                popupRemIdx = i;
                reminderActive = true;
                prefs.putInt("popup_idx", i);
                showReminderPopup(i);
                return;
            }
        } else {
            reminderFired[i] = false;
        }
    }
}

static void clock_timer_cb(lv_timer_t *timer) {
    updateClock();
    checkReminders();
    if (second_ == 0) saveTime();
    update_home_clock();
}

static void battery_timer_cb(lv_timer_t *timer) {
    int pct = getBatteryPercent();
    lv_label_set_text_fmt(home_bat_label, "%d%%", pct);
    if (pct <= 20)
        lv_obj_set_style_text_color(home_bat_label, color_red, 0);
    else
        lv_obj_set_style_text_color(home_bat_label, color_grey, 0);
}

// ══════════════════════════════════════════════════════════════
//  WiFi Management
// ══════════════════════════════════════════════════════════════

static void sync_ntp() {
    configTime(3 * 3600, 0, "pool.ntp.org");
    struct tm t;
    int tries = 0;
    while (!getLocalTime(&t) && tries < 20) { delay(500); tries++; }
    if (getLocalTime(&t)) {
        hour_   = t.tm_hour; minute_ = t.tm_min; second_ = t.tm_sec;
        day_    = t.tm_mday; month_   = t.tm_mon + 1; year_ = t.tm_year + 1900;
        saveTime();
        update_home_clock();
    }
}

static void start_dashboard_server() {
    if (server_running) return;
    server.stop();  // ensure clean state, kill any captive portal routes
    setup_dashboard_server(server);
    server_running = true;
}

static void stop_dashboard_server() {
    if (server_running) {
        server.stop();
        server_running = false;
    }
}

static void attempt_wifi_connect() {
    Serial.println("[WiFi] attempt_wifi_connect() ENTER");
    wifi_state = WIFI_CONNECTING;
    wifi_connect_start = millis();
    Serial.println("[WiFi] setting mode STA...");
    WiFi.mode(WIFI_STA);
    Serial.println("[WiFi] calling WiFi.begin()...");
    wl_status_t s = WiFi.begin();
    Serial.printf("[WiFi] begin() returned %d (0=IDLE 1=NO_SSID 3=CONNECTED 4=FAILED 6=DISCONNECTED)\n", (int)s);

    if (s == WL_NO_SSID_AVAIL) {
        Serial.println("[WiFi] No stored credentials — showing choice modal now");
        wifi_state = WIFI_IDLE;
        WiFi.mode(WIFI_OFF);
        extern void wifi_setup_show_choice(void);
        push_modal(scr_wifi_setup);
        wifi_setup_show_choice();
        wifi_modal_shown = true;
        Serial.println("[WiFi] Choice modal pushed");
    } else {
        Serial.printf("[WiFi] Will poll status every 500ms (timeout 5s)\n");
    }
}

void wifi_setup_start_ap() {
    if (server_running) { server.stop(); server_running = false; }
    dnsServer.stop();

    prefs.putBool("wifi_offline", false);

    // Phase 1 — full reset then scan (WiFiManager-style)
    Serial.println("[WiFi] Phase 1 — full reset...");
    WiFi.mode(WIFI_OFF);
    int deadline = millis() + 1200;
    while (WiFi.getMode() != WIFI_OFF && millis() < deadline) {
        delay(0);
    }
    Serial.printf("[WiFi] mode OFF confirmed after %lu ms\n",
        millis() + 1200 - deadline);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(500);

    Serial.println("[WiFi] starting scan...");
    scan_count = WiFi.scanNetworks(false, false, true, 500);
    Serial.printf("[WiFi] scan complete: %d networks\n", scan_count);
    for (int i = 0; i < scan_count && i < 30; i++) {
        scan_ssids[i] = WiFi.SSID(i);
        scan_rssi[i]  = WiFi.RSSI(i);
        Serial.printf("[WiFi]   %d: %s (%d dBm)\n", i + 1,
            scan_ssids[i].c_str(), scan_rssi[i]);
    }
    WiFi.scanDelete();

    // Phase 2 — start AP-only portal
    Serial.println("[WiFi] Phase 2 — starting AP...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("TasbeehWatch");

    dnsServer.start(53, "*", WiFi.softAPIP());
    setup_wifi_portal(server);

    wifi_state = WIFI_AP_MODE;
    ap_start_time = millis();
    ap_timed_out = false;
    Serial.println("[WiFi] AP portal ready");
}

void wifi_setup_stop_ap() {
    dnsServer.stop();
    if (server_running) { server.stop(); server_running = false; }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_state = WIFI_IDLE;
}

void wifi_setup_cancel() {
    wifi_setup_stop_ap();
    wifi_state = WIFI_OFFLINE;
    wifi_ready = false;
    wifi_local_ip = "";
    wifi_local_ssid = "";
    prefs.putBool("wifi_offline", true);
    update_wifi_status_display();
}

void erase_wifi_credentials() {
    Serial.println("[WiFi] Erasing stored credentials...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.persistent(true);
    WiFi.disconnect(true, true);
    delay(500);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    prefs.putBool("wifi_offline", false);
    wifi_state = WIFI_IDLE;
    wifi_ready = false;
    wifi_local_ip = "";
    wifi_local_ssid = "";
    update_wifi_status_display();
    Serial.println("[WiFi] Credentials erased");
}

void wifi_ap_save_credentials(String ssid, String pass) {
    prefs.putBool("wifi_offline", false);

    dnsServer.stop();
    server.stop();
    server_running = false;
    WiFi.softAPdisconnect(true);

    // Connect to provided network
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    wifi_state = WIFI_CONNECTING;
    wifi_connect_start = millis();
    ap_timed_out = false;
}

void wifi_setup_show_qr() {
    if (scr_qrcode) {
        String url = "http://" + wifi_local_ip;
        qrcode_set_url(url.c_str());
    }
}

static void check_wifi_ap_timeout() {
    if (wifi_state != WIFI_AP_MODE) return;
    if (ap_timed_out) return;
    if (millis() - ap_start_time > 180000) {
        ap_timed_out = true;
        wifi_setup_stop_ap();
        wifi_setup_start_ap(); // restart AP + async scan for retry
        if (lv_screen_active() == scr_wifi_setup) {
            extern void wifi_setup_show_timeout(void);
            wifi_setup_show_timeout();
        }
    }
}

// ── Async scan poll ─────────────────────────────────────────
static void poll_wifi_scan() {
    // scan is now synchronous in wifi_setup_start_ap() — nothing to poll
}

static void handle_wifi_connecting() {
    if (wifi_state != WIFI_CONNECTING) return;

    static unsigned long last_status_print = 0;
    if (millis() - last_status_print > 2000) {
        last_status_print = millis();
        Serial.printf("[WiFi] polling... status=%d  elapsed=%lu ms\n",
            (int)WiFi.status(), millis() - wifi_connect_start);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifi_ready = true;
        wifi_local_ip = WiFi.localIP().toString();
        wifi_local_ssid = WiFi.SSID();
        wifi_state = WIFI_CONNECTED;
        prefs.putBool("wifi_offline", false);
        Serial.printf("[WiFi] Connected to %s IP=%s\n",
            wifi_local_ssid.c_str(), wifi_local_ip.c_str());

        // NTP sync
        sync_ntp();

        // Start dashboard server
        start_dashboard_server();

        // Update settings UI
        update_wifi_status_display();

        // If WiFi setup modal is active, dismiss it
        if (lv_screen_active() == scr_wifi_setup) {
            pop_modal();
        }
        return;
    }

    // Check timeout — 5 seconds
    unsigned long timeout = 5000;
    if (wifi_connect_start > 0 && millis() - wifi_connect_start > timeout) {
        Serial.println("[WiFi] Timeout — connection failed");
        wifi_state = WIFI_IDLE;
        WiFi.disconnect();

        // Initial boot: show choice modal (connect or stay offline)
        if (!wifi_modal_shown && !prefs.getBool("wifi_offline", false)) {
            extern void wifi_setup_show_choice(void);
            push_modal(scr_wifi_setup);
            wifi_setup_show_choice();
            wifi_modal_shown = true;
        }
        // Captive portal save failed — restart AP + show timeout
        else if (wifi_modal_shown) {
            wifi_setup_start_ap();
            if (lv_screen_active() == scr_wifi_setup) {
                extern void wifi_setup_show_timeout(void);
                wifi_setup_show_timeout();
            }
        }
    }
}

static void wifi_timer_cb(lv_timer_t *timer) {
    handle_wifi_connecting();
    poll_wifi_scan();
    check_wifi_ap_timeout();

    // Handle AP mode DNS + HTTP in timer (called every 100ms from loop indirectly)
    if (wifi_state == WIFI_AP_MODE) {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    // Handle dashboard server requests
    if (wifi_state == WIFI_CONNECTED && server_running) {
        server.handleClient();
    }
}

// ══════════════════════════════════════════════════════════════
//  Display
// ══════════════════════════════════════════════════════════════

#define TFT_HOR_RES   240
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 2 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf_1[DRAW_BUF_SIZE / 4];
uint32_t draw_buf_2[DRAW_BUF_SIZE / 4];

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (!touch.available()) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
    }
}

static uint32_t my_tick(void) {
    return millis();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== BOOT START ===");

    Serial.println("[1] touch.begin...");
    touch.begin();
    Serial.println("[1] OK");

    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC, ADC_11db);

    Serial.println("[2] prefs.begin...");
    prefs.begin("watch", false);
    tasbeehCount      = prefs.getUInt("tasbeeh", 0);
    tasbeeh_phrase_idx = prefs.getInt("tasbeeh_phrase", 0);
    istighfarCount    = prefs.getUInt("isteghfar", 0);
    loadReminders();
    loadTime();
    prefs.putInt("popup_idx", -1);  // clear any stale popup from previous crash
    Serial.println("[2] OK");

    int savedPopup = prefs.getInt("popup_idx", -1);
    if (savedPopup >= 0 && savedPopup < MAX_REMINDERS) {
        popupRemIdx = savedPopup;
        reminderActive = true;
    }

    Serial.println("[3] lv_init...");
    lv_init();
    lv_tick_set_cb(my_tick);
    Serial.println("[3] OK");

    Serial.println("[4] display init...");
    lv_display_t *disp;
#if LV_USE_TFT_ESPI
    tft.begin();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush_dma);
    lv_display_set_rotation(disp, TFT_ROTATION);
    lv_display_set_buffers(disp, draw_buf_1, draw_buf_2, sizeof(draw_buf_1),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.printf("[4] disp=%p DMA flush  buf1=%p buf2=%p size=%u\n",
        disp, draw_buf_1, draw_buf_2, sizeof(draw_buf_1));
#else
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf_1, NULL, sizeof(draw_buf_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    if (!disp) {
        Serial.println("[4] FATAL: display creation failed!");
        while(1) { delay(1000); }
    }
    Serial.println("[4] OK");

    Serial.println("[5] touch input...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    indev->gesture_limit = 20;
    lv_indev_set_long_press_time(indev, 1000);
    Serial.println("[5] OK");

    Serial.println("[6] test label...");
    lv_obj_t *test = lv_label_create(lv_screen_active());
    lv_label_set_text(test, "TEST OK");
    lv_obj_set_style_text_color(test, lv_color_hex(0x00FF00), 0);
    lv_obj_center(test);
    lv_timer_handler();
    delay(500);
    Serial.println("[6] OK");
    delay(1500);

    Serial.println("[7] theme init...");
    lv_theme_t *th = lv_theme_default_init(disp,
        color_teal, color_gold, true, &font_alexandria_16);
    lv_display_set_theme(disp, th);
    Serial.println("[7] OK");

    Serial.println("[8] ui_styles_init...");
    ui_styles_init();
    Serial.println("[8] OK");

    Serial.println("[9] screens_init...");
    screens_init();
    Serial.println("[9] OK");

    update_home_clock();
    lv_screen_load(scr_home);
    lv_timer_handler();
    delay(100);

    clock_timer_obj  = lv_timer_create(clock_timer_cb, 1000, NULL);
    battery_timer_obj = lv_timer_create(battery_timer_cb, 5000, NULL);
    wifi_timer_obj    = lv_timer_create(wifi_timer_cb, 500, NULL);

    update_tasbeeh_display();
    update_istighfar_display();

    Serial.println("=== SETUP DONE ===\n");

    // Attempt WiFi connection (async, non-blocking)
    bool offline = prefs.getBool("wifi_offline", false);
    Serial.printf("[WiFi] wifi_offline flag = %s\n", offline ? "TRUE → skipping WiFi" : "FALSE → attempting connect");
    if (!offline) {
        attempt_wifi_connect();
    } else {
        wifi_state = WIFI_OFFLINE;
        wifi_ready = false;
        update_wifi_status_display();
    }
}

void loop() {
    lv_timer_handler();
    delay(5);
}
