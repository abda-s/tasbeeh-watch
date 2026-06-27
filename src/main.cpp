#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
using namespace fs;
#include <CST816S.h>
#include <lvgl.h>
#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
#endif
#include <Preferences.h>
#include <time.h>
#include <math.h>

#include "config/CST816S_pin_config.h"
#include "ui/styles.h"
#include "ui/screens.h"
#include "indev/lv_indev_private.h"

#define BAT_ADC       1
#define MAX_REMINDERS 10

struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
};

CST816S     touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);
Preferences prefs;

int   hour_ = 8, minute_ = 0, second_ = 0;
int   day_  = 1, month_  = 1, year_ = 2026;

uint32_t tasbeehCount   = 0;
uint32_t isteghfarCount = 0;

Reminder reminders[MAX_REMINDERS];
bool     reminderFired[MAX_REMINDERS];
int      popupRemIdx = 0;
bool     reminderActive = false;
lv_obj_t *reminder_mbox = NULL;

static lv_timer_t *clock_timer_obj = NULL;
static lv_timer_t *battery_timer_obj = NULL;

static int getBatteryPercent() {
    long sum = 0;
    for (int i = 0; i < 16; i++) { sum += analogRead(BAT_ADC); delay(1); }
    float raw     = sum / 16.0f;
    float voltage = (raw / 4095.0f) * 3.3f * 2.0f;
    int pct = (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100.0f);
    return constrain(pct, 0, 100);
}

static void updateClock() {
    second_++;
    if (second_ >= 60) { second_ = 0; minute_++; }
    if (minute_ >= 60) { minute_ = 0; hour_++;   }
    if (hour_   >= 24) { hour_   = 0; day_++;    }
}

static void saveTime() {
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

static void saveReminders() {
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
        lv_msgbox_close(reminder_mbox);
        reminder_mbox = NULL;
    }
    reminderActive = false;
    prefs.putInt("popup_idx", -1);
}

static void snooze_cb(lv_event_t *e) {
    if (reminder_mbox) {
        lv_msgbox_close(reminder_mbox);
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

    reminder_mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(reminder_mbox, "\330\252\330\260\331\203\331\212\330\261");
    lv_obj_t *content = lv_msgbox_get_content(reminder_mbox);

    lv_obj_t *time_lbl = lv_label_create(content);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(time_lbl, color_gold, 0);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *text_lbl = lv_label_create(content);
    lv_obj_set_style_text_font(text_lbl, &lv_font_dejavu_16_persian_hebrew, 0);
    lv_obj_set_style_text_color(text_lbl, color_white, 0);
    lv_label_set_text(text_lbl, label_buf);
    lv_obj_align(text_lbl, LV_ALIGN_CENTER, 0, 10);

    lv_obj_set_size(content, 180, 120);

    lv_obj_t *btn_dismiss = lv_msgbox_add_footer_button(reminder_mbox, "\330\252\331\205");
    lv_obj_add_event_cb(btn_dismiss, dismiss_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_snooze = lv_msgbox_add_footer_button(reminder_mbox, "\330\252\330\243\330\254\331\212\331\204");
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
    char icon[4] = "";
    if (pct <= 10) snprintf(icon, sizeof(icon), "!");
    else if (pct <= 30) snprintf(icon, sizeof(icon), "");
    lv_label_set_text_fmt(home_bat_label, "%s %d%%", icon, pct);
    if (pct <= 20)
        lv_obj_set_style_text_color(home_bat_label, color_red, 0);
    else
        lv_obj_set_style_text_color(home_bat_label, color_grey, 0);
}

#define TFT_HOR_RES   240
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

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
    tasbeehCount   = prefs.getUInt("tasbeeh", 0);
    isteghfarCount = prefs.getUInt("isteghfar", 0);
    loadReminders();
    loadTime();
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

    Serial.println("[4] lv_tft_espi_create...");
    lv_display_t *disp;
#if LV_USE_TFT_ESPI
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
    Serial.printf("[4] disp = %p\n", (void*)disp);
    if (!disp) {
        Serial.println("[4] FATAL: display creation failed!");
        while(1) { delay(1000); }
    }
    lv_display_set_rotation(disp, TFT_ROTATION);
#else
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    Serial.println("[4] OK");

    Serial.println("[5] touch input...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);
    indev->gesture_limit = 20;  // lower threshold for 240×240 screen (default 50)
    Serial.println("[5] OK");

    // Quick sanity: render a test label BEFORE any complex init
    Serial.println("[6] test label...");
    lv_obj_t *test = lv_label_create(lv_screen_active());
    lv_label_set_text(test, "TEST OK");
    lv_obj_set_style_text_color(test, lv_color_hex(0x00FF00), 0);
    lv_obj_center(test);
    lv_timer_handler();
    delay(500);
    Serial.println("[6] OK (should see TEST OK on screen)");
    delay(2000);

    Serial.println("[7] theme init...");
    lv_theme_t *th = lv_theme_default_init(disp,
        color_teal, color_gold, true, &lv_font_dejavu_16_persian_hebrew);
    Serial.printf("[7] theme = %p\n", (void*)th);
    lv_display_set_theme(disp, th);
    Serial.println("[7] OK");

    Serial.println("[8] ui_styles_init...");
    ui_styles_init();
    Serial.println("[8] OK");

    Serial.println("[9] screens_init...");
    screens_init();
    Serial.printf("[9] scr_home = %p\n", (void*)scr_home);
    Serial.println("[9] OK");

    update_home_clock();

    // Skip WiFi for now – just show home
    Serial.println("[10] load home screen...");
    lv_screen_load(scr_home);
    lv_timer_handler();
    delay(100);
    Serial.println("[10] OK (should see home screen)");

    clock_timer_obj = lv_timer_create(clock_timer_cb, 1000, NULL);
    update_tasbeeh_display();
    update_isteghfar_display();

    if (settings_ip_label) {
        lv_label_set_text(settings_ip_label, "WiFi disabled");
    }

    Serial.println("=== SETUP DONE ===\n");
}

void loop() {
    lv_timer_handler();
    delay(5);
}
