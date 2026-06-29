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
#include <esp_wifi.h>
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
static bool wifi_bg_retry  = false;     // true = silent background retry, no modal on timeout
static bool had_credentials = false;    // set when creds found/saved, avoids NVS re-reads
static unsigned long last_conn_check  = 0;
static unsigned long last_bg_retry    = 0;
#define CONN_LOST_CHECK_MS   2000
#define BG_RETRY_INTERVAL_MS 30000

// ── Two-phase AP startup ─────────────────────────────────────
enum ApStartPhase {
    AP_PHASE_IDLE,
    AP_PHASE_RESETTING,  // waiting for WIFI_OFF → WIFI_STA to settle (non-blocking)
    AP_PHASE_SCANNING,   // scan running in STA mode, waiting for results
    AP_PHASE_STARTING,   // scan done, switching to AP + starting server
};
static ApStartPhase ap_phase = AP_PHASE_IDLE;
static unsigned long ap_phase_start = 0;

// ── Display sleep ───────────────────────────────────────────
static lv_display_t *lv_disp = NULL;
static bool display_sleeping = false;
#define SLEEP_TIMEOUT_MS 30000
String scan_ssids[30];
int    scan_rssi[30];
int    scan_enc[30];
int    scan_count = 0;

ServerHelper server(80);
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

static void wake_display(void);

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
                if (display_sleeping) wake_display();
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

static bool ntp_pending = false;
static int  ntp_tries   = 0;

static void sync_ntp_start() {
    configTime(3 * 3600, 0, "pool.ntp.org");
    ntp_pending = true;
    ntp_tries   = 0;
}

static void sync_ntp_poll() {
    if (!ntp_pending) return;
    struct tm t;
    if (getLocalTime(&t)) {
        hour_   = t.tm_hour; minute_ = t.tm_min; second_ = t.tm_sec;
        day_    = t.tm_mday; month_   = t.tm_mon + 1; year_ = t.tm_year + 1900;
        saveTime();
        update_home_clock();
        ntp_pending = false;
        Serial.println("[NTP] Time synced");
    } else if (++ntp_tries >= 20) {
        ntp_pending = false;
        Serial.println("[NTP] Sync failed (timeout)");
    }
}

static void start_dashboard_server() {
    if (server_running) return;
    server.clearAllHandlers();
    server.stop();
    setup_dashboard_server(&server);
    server_running = true;
}

static void stop_dashboard_server() {
    if (server_running) {
        server.stop();
        server_running = false;
    }
}

static bool has_saved_wifi_credentials() {
    // WiFi.psk() reads NVS via esp_wifi_get_config — needs WiFi initialized
    WiFi.mode(WIFI_STA);
    wifi_config_t conf;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &conf);
    String ssid = (err == ESP_OK) ? String((const char*)conf.sta.ssid) : String();
    WiFi.mode(WIFI_OFF);

    bool has = (ssid.length() > 0);
    Serial.printf("[WiFi] Stored SSID: \"%s\" -> %s\n",
        ssid.c_str(), has ? "will attempt" : "none saved");
    return has;
}

static void attempt_wifi_connect() {
    Serial.println("[WiFi] attempt_wifi_connect() ENTER");

    if (!has_saved_wifi_credentials()) {
        Serial.println("[WiFi] No stored credentials — showing choice modal");
        wifi_state = WIFI_IDLE;
        extern void wifi_setup_show_choice(void);
        push_modal(scr_wifi_setup);
        wifi_setup_show_choice();
        wifi_modal_shown = true;
        return;
    }

    wifi_state         = WIFI_CONNECTING;
    wifi_connect_start = millis();
    wifi_modal_shown   = false;
    had_credentials    = true;

    WiFi.persistent(true);              // ensure credentials stay written to NVS
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin();                       // use saved NVS credentials
    Serial.println("[WiFi] WiFi.begin() called with saved credentials");
}

void wifi_setup_start_ap() {
    if (server_running) { server.stop(); server_running = false; }
    dnsServer.stop();

    prefs.putBool("wifi_offline", false);

    Serial.println("[WiFi] Phase 0 — reset to OFF, async...");
    WiFi.mode(WIFI_OFF);
    ap_phase = AP_PHASE_RESETTING;
    ap_phase_start = millis();
    scan_count = 0;
}

void wifi_setup_stop_ap() {
    dnsServer.stop();
    if (server_running) { server.stop(); server_running = false; }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_state = WIFI_IDLE;
    ap_phase = AP_PHASE_IDLE;
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

    WiFi.persistent(true);             // must be ON for disconnect(true,true) to erase NVS
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);       // first true = disconnect, second = erase NVS
    delay(300);
    WiFi.mode(WIFI_OFF);
    // do NOT set persistent(false) — leave it ON for the next WiFi.begin()

    prefs.putBool("wifi_offline", false);
    wifi_state      = WIFI_IDLE;
    wifi_ready      = false;
    wifi_local_ip   = "";
    wifi_local_ssid = "";
    update_wifi_status_display();
    Serial.println("[WiFi] Credentials erased");
}

void wifi_ap_save_credentials(String ssid, String pass) {
    prefs.putBool("wifi_offline", false);

    dnsServer.stop();
    server.stop();
    server_running = false;
    ap_phase = AP_PHASE_IDLE;
    WiFi.softAPdisconnect(true);

    WiFi.persistent(true);              // ensure credentials written to NVS
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    wifi_state         = WIFI_CONNECTING;
    wifi_connect_start = millis();
    wifi_modal_shown   = true;
    had_credentials    = true;
    ap_timed_out       = false;

    Serial.printf("[WiFi] Saving + connecting to \"%s\"\n", ssid.c_str());
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
            extern void wifi_setup_show_scanning(void);
            wifi_setup_show_scanning();
        }
    }
}

// ── Async scan poll ─────────────────────────────────────────
static void poll_wifi_scan() {
    if (ap_phase != AP_PHASE_SCANNING) return;

    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;

    ap_phase = AP_PHASE_IDLE;

    if (n > 0) {
        scan_count = n < 30 ? n : 30;
        for (int i = 0; i < scan_count; i++) {
            scan_ssids[i] = WiFi.SSID(i);
            scan_rssi[i]  = WiFi.RSSI(i);
            scan_enc[i]   = WiFi.encryptionType(i);
            Serial.printf("[WiFi] scan[%d]: %s (%d dBm)\n", i,
                scan_ssids[i].c_str(), scan_rssi[i]);
        }
        Serial.printf("[WiFi] Scan done: %d networks found.\n", scan_count);
    } else {
        scan_count = 0;
        Serial.printf("[WiFi] Scan failed or empty (n=%d).\n", n);
    }
    WiFi.scanDelete();

    // Phase 2 — switch to AP, start portal
    Serial.println("[WiFi] Phase 2 — switching to AP...");
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP("TasbeehWatch");

    dnsServer.start(53, "*", WiFi.softAPIP());
    server.clearAllHandlers();
    server.stop();
    setup_wifi_portal(&server);
    server_running = true;

    wifi_state    = WIFI_AP_MODE;
    ap_start_time = millis();
    ap_timed_out  = false;

    Serial.printf("[WiFi] AP ready. IP: %s  Networks: %d\n",
        WiFi.softAPIP().toString().c_str(), scan_count);

    // Notify UI: AP is live
    if (lv_screen_active() == scr_wifi_setup) {
        extern void wifi_setup_set_ap_view(void);
        wifi_setup_set_ap_view();
    }
}

#define WIFI_CONNECT_TIMEOUT_MS  15000UL

static void handle_wifi_connecting() {
    if (wifi_state != WIFI_CONNECTING) return;

    wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        wifi_ready      = true;
        wifi_local_ip   = WiFi.localIP().toString();
        wifi_local_ssid = WiFi.SSID();
        wifi_state      = WIFI_CONNECTED;
        prefs.putBool("wifi_offline", false);
        wifi_bg_retry   = false;

        Serial.printf("[WiFi] Connected to \"%s\"  IP=%s  in %lu ms\n",
            wifi_local_ssid.c_str(),
            wifi_local_ip.c_str(),
            millis() - wifi_connect_start);

        sync_ntp_start();
        start_dashboard_server();
        update_wifi_status_display();

        if (lv_screen_active() == scr_wifi_setup) pop_modal();
        return;
    }

    static unsigned long last_log = 0;
    if (millis() - last_log > 3000) {
        last_log = millis();
        Serial.printf("[WiFi] status=%d  elapsed=%lu / %lu ms\n",
            (int)status, millis() - wifi_connect_start, WIFI_CONNECT_TIMEOUT_MS);
    }

    if (millis() - wifi_connect_start < WIFI_CONNECT_TIMEOUT_MS) return;

    Serial.printf("[WiFi] Timed out after %lu ms\n", WIFI_CONNECT_TIMEOUT_MS);

    bool was_bg = wifi_bg_retry;
    wifi_bg_retry = false;

    WiFi.disconnect(false);       // lightweight — keep radio in STA, don't wipe config
    wifi_state = WIFI_IDLE;

    if (was_bg) {
        Serial.println("[WiFi] Background retry timed out — will retry later");
        return;
    }

    if (!wifi_modal_shown) {
        Serial.println("[WiFi] Showing choice modal (boot timeout)");
        extern void wifi_setup_show_choice(void);
        push_modal(scr_wifi_setup);
        wifi_setup_show_choice();
        wifi_modal_shown = true;
    } else {
        Serial.println("[WiFi] Portal save timed out — restarting AP");
        wifi_setup_start_ap();
        if (lv_screen_active() == scr_wifi_setup) {
            extern void wifi_setup_show_timeout(void);
            wifi_setup_show_timeout();
        }
    }
}

// ── Background connection monitoring ──────────────────────────

// Detect silent disconnect while we thought we were connected
static void check_connection_lost() {
    if (wifi_state != WIFI_CONNECTED) return;
    if (millis() - last_conn_check < CONN_LOST_CHECK_MS) return;
    last_conn_check = millis();

    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return;

    Serial.printf("[WiFi] Connection lost (status=%d) — was CONNECTED\n", (int)s);
    wifi_ready      = false;
    wifi_local_ip   = "";
    wifi_local_ssid = "";
    wifi_state      = WIFI_IDLE;
    stop_dashboard_server();
    update_wifi_status_display();
}

// Periodically retry when idle and credentials exist
static void background_retry_connect() {
    if (wifi_state == WIFI_CONNECTING || wifi_state == WIFI_AP_MODE) return;
    if (wifi_state == WIFI_CONNECTED) return;
    if (millis() - last_bg_retry < BG_RETRY_INTERVAL_MS) return;
    last_bg_retry = millis();

    // Only attempt if we have saved credentials (try begin — safe, returns immediately)
    if (!had_credentials) return;   // never had saved creds, nothing to retry

    Serial.println("[WiFi] Background retry — attempting reconnect");
    wifi_state         = WIFI_CONNECTING;
    wifi_connect_start = millis();
    wifi_bg_retry      = true;

    // Only switch mode if necessary — getMode() is cheap, mode() is expensive
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin();
}

// ── Async AP start sequence (non-blocking) ───────────────────
static void poll_ap_start_sequence() {
    if (ap_phase != AP_PHASE_RESETTING) return;
    if (millis() - ap_phase_start < 200) return;   // let OFF settle, no blocking
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true, false, true, 400);
    ap_phase = AP_PHASE_SCANNING;
    Serial.println("[WiFi] Async scan started.");
}

static void wifi_timer_cb(lv_timer_t *timer) {
    handle_wifi_connecting();
    sync_ntp_poll();
    poll_ap_start_sequence();
    poll_wifi_scan();
    check_wifi_ap_timeout();
    check_connection_lost();
    background_retry_connect();

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
//  Display sleep / wake
// ══════════════════════════════════════════════════════════════

static void sleep_display() {
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
    tft.writecommand(0x10);                     // TFT_SLPIN
    display_sleeping = true;
}

static bool wake_pending = false;
static uint32_t wake_time = 0;

static void wake_display() {
    tft.writecommand(0x11);                     // TFT_SLPOUT
    display_sleeping = false;                   // mark awake immediately so touch works
    wake_pending = true;
    wake_time = millis();                       // defer DISPON + backlight by 120ms
}

static void screen_sleep_cb(lv_timer_t *t) {
    if (!display_sleeping && lv_disp
        && lv_display_get_inactive_time(lv_disp) > SLEEP_TIMEOUT_MS) {
        sleep_display();
    }
}

// ══════════════════════════════════════════════════════════════
//  Display
// ══════════════════════════════════════════════════════════════

#define TFT_HOR_RES   240
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_270
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
    } else if (display_sleeping) {
        wake_display();
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = 240 - touch.data.x;
        data->point.y = 240 - touch.data.y;
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
    totalTasbeeh      = prefs.getUInt("totaltasbeeh", 0);
    totalIstighfar    = prefs.getUInt("totalisteghfar", 0);
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
    lv_disp = disp;
    Serial.println("[4] OK");

    Serial.println("[5] touch input...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    indev->gesture_limit = 20;
    lv_indev_set_long_press_time(indev, 1000);
    Serial.println("[5] OK");

    Serial.println("[6] theme init...");
    lv_theme_t *th = lv_theme_default_init(disp,
        color_teal, color_gold, true, &font_alexandria_16);
    lv_display_set_theme(disp, th);
    Serial.println("[6] OK");

    Serial.println("[7] ui_styles_init...");
    ui_styles_init();
    Serial.println("[7] OK");

    Serial.println("[8] screens_init...");
    screens_init();
    Serial.println("[8] OK");

    update_home_clock();
    lv_screen_load(scr_home);
    lv_timer_handler();
    delay(100);

    clock_timer_obj  = lv_timer_create(clock_timer_cb, 1000, NULL);
    battery_timer_obj = lv_timer_create(battery_timer_cb, 5000, NULL);
    wifi_timer_obj    = lv_timer_create(wifi_timer_cb,   500, NULL);
    lv_timer_create(screen_sleep_cb, 1000, NULL);

    update_tasbeeh_display();
    update_istighfar_display();

    Serial.println("=== SETUP DONE ===\n");

    // Always attempt WiFi connection (async, non-blocking)
    // If it fails, the timeout handler will show the choice modal
    attempt_wifi_connect();
}

void loop() {
    if (wake_pending && millis() - wake_time >= 120) {
        wake_pending = false;
        tft.writecommand(0x29);                 // TFT_DISPON
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
        lv_display_trigger_activity(lv_disp);
    }
    lv_timer_handler();
    delay(5);
}
