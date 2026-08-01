#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
using namespace fs;
#include <CST816S.h>
#include <Wire.h>
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
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "config/CST816S_pin_config.h"
#include "ui/styles.h"
#include "ui/screens.h"
#include "indev/lv_indev_private.h"

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);

// ── Types relocated from web_dashboard.h (now removed) ───────
struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
};
#define BAT_ADC 1
#define MAX_REMINDERS 10

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

// ── Display sleep ───────────────────────────────────────────
static lv_display_t *lv_disp = NULL;
static bool display_sleeping = false;
#define SLEEP_TIMEOUT_MS 15000

// #define AWAKE_CPU_MHZ 240
// #define AWAKE_CPU_MHZ 160  // UI is light (partial redraws over 40MHz SPI+DMA); 240 not needed
#define AWAKE_CPU_MHZ 80  // testing: how low can awake CPU go before UI feels laggy

// Real light sleep clock-gates the CPU directly, so it doesn't matter what
// frequency was set beforehand. Wake at least this often so ESP-IDF's
// automatic RC-oscillator-vs-main-crystal RTC recalibration stays fresh
// (bounds temperature drift) and so a touch has a timer backstop in case
// the GPIO-level wake (see sleep_display()) doesn't latch it. ~1 minute
// matches the cadence reported to achieve ~10ppm RTC accuracy on ESP32:
// https://esp32.com/viewtopic.php?t=35490
// https://medium.com/@raypcb/a-complete-guide-to-checking-rtc-accuracy-on-the-esp32-3f83a5c70ec7
#define SLEEP_WAKE_INTERVAL_US (60ULL * 1000 * 1000)

// Backlight PWM (replaces plain digitalWrite on/off)
#define BL_PWM_FREQ_HZ 5000
#define BL_PWM_RES_BITS 8
// #define BL_DUTY_ON  180   // ~70% — dimmer, less LED current
// #define BL_DUTY_ON  128   // ~50% — dimmer, less LED current
#define BL_DUTY_ON  70   // ~27% — measured ~52mA avg screen-on (was 85mA at 100%)

#define BL_DUTY_OFF 0

// Onboard QMI8658A IMU (accel+gyro) — unused by this firmware, but present
// on the board and left running at its power-on default otherwise. Per its
// datasheet (Table 31, Operating Modes): the reset-default "Power-On
// Default" state (both sensors off, high-speed clock still running) draws
// ~15uA. Setting CTRL1 bit0 (SensorDisable) drops it into "Power-Down"
// mode (~6uA), the lowest documented state that still keeps the I2C
// interface responsive. SA0 is grounded on this board (schematic), so the
// device address is 0x6B.
#define QMI8658_I2C_ADDR 0x6B
#define QMI8658_REG_CTRL1 0x02

static void imu_power_down() {
    Wire.beginTransmission(QMI8658_I2C_ADDR);
    Wire.write(QMI8658_REG_CTRL1);
    Wire.write(0x21);   // CTRL1 default (0x20, BE=1) | SensorDisable=1
    uint8_t err = Wire.endTransmission();
    Serial.printf("[IMU] power-down write %s\n", err == 0 ? "OK" : "FAILED");
}

static int getBatteryPercent() {
    long sum = 0;
    for (int i = 0; i < 8; i++) { sum += analogReadMilliVolts(BAT_ADC); delay(1); }
    float v_adc = sum / 8.0f / 1000.0f;
    float v_bat = v_adc * 3.0f;
    int pct = (int)((v_bat - 3.5f) / (4.15f - 3.5f) * 100.0f);
    return constrain(pct, 0, 100);
}

// Advances the clock by however many whole seconds actually elapsed since
// the last call (millis()-based, not a flat +1) — so a late or skipped
// clock_timer_cb (e.g. throttled sleep polling, or a future real sleep
// implementation that halts the CPU) doesn't silently lose time. Any
// leftover sub-second remainder is kept in last_clock_ms rather than
// discarded, so rounding doesn't accumulate into drift either.
static uint32_t last_clock_ms = 0;

// Called whenever hour_/minute_/second_ are set directly (e.g. from the
// TimeEdit screen) so the next updateClock() measures elapsed time from
// the moment of the edit, instead of adding on top of however long the
// user spent on the TimeEdit screen itself.
void resetClockTick() { last_clock_ms = millis(); }

static void updateClock() {
    uint32_t now = millis();
    uint32_t elapsed_ms = now - last_clock_ms;   // unsigned subtraction wraps correctly across millis() overflow
    int elapsed_s = elapsed_ms / 1000;
    if (elapsed_s <= 0) return;
    last_clock_ms += (uint32_t)elapsed_s * 1000;

    while (elapsed_s-- > 0) {
        second_++;
        if (second_ >= 60) { second_ = 0; minute_++; }
        if (minute_ >= 60) { minute_ = 0; hour_++;   }
        if (hour_   >= 24) { hour_   = 0; day_++;    }
    }
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
//  Display sleep / wake
// ══════════════════════════════════════════════════════════════

// touch.sleep()'s reset pulse (RST low→high, inside the CST816S library)
// glitches the chip's IRQ line. The library's own interrupt handler is
// RISING-mode and stays attached forever, so it catches that glitch and
// looks exactly like a real touch. Detach it around the reset so it isn't
// mistaken for a real one.
static void sleep_display() {
    ledcWrite(TFT_BL, BL_DUTY_OFF);
    tft.writecommand(0x10);                     // TFT_SLPIN
    display_sleeping = true;

    detachInterrupt(TOUCH_IRQ);                  // don't catch sleep()'s own reset glitch below
    touch.sleep();                               // reset pulse + standby register write
    touch.available();                           // drain any stale flag set by that reset glitch

    // Hardware wake sources for esp_light_sleep_start() — no software ISR
    // needed, the CPU wakes directly from the halted state.
    //
    // Measured on real hardware: with GPIO_INTR_HIGH_LEVEL, every sleep
    // attempt was rejected instantly (ESP_ERR_SLEEP_REJECT — "wakeup source
    // set before the sleep request"), meaning TOUCH_IRQ actually idles HIGH,
    // opposite of what the RISING attachInterrupt convention implied. Wake
    // on LOW level instead.
    gpio_wakeup_enable((gpio_num_t)TOUCH_IRQ, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    esp_sleep_enable_timer_wakeup(SLEEP_WAKE_INTERVAL_US);

    Serial.println("[SCREEN_OFF]");
}

static bool wake_pending = false;
static uint32_t wake_time = 0;

static void wake_display() {
    detachInterrupt(TOUCH_IRQ);
    touch.begin();                               // re-init touch chip + reattach its real ISR

    setCpuFrequencyMhz(AWAKE_CPU_MHZ);
    tft.writecommand(0x11);                     // TFT_SLPOUT
    display_sleeping = false;                   // mark awake immediately so touch works
    lv_display_trigger_activity(lv_disp);       // reset inactivity NOW — otherwise
                                                // screen_sleep_cb can fire inside the
                                                // 120ms window below and re-sleep the
                                                // panel (backlight on + black screen)
    wake_pending = true;
    wake_time = millis();                       // defer DISPON + backlight by 120ms
}

static void screen_sleep_cb(lv_timer_t *t) {
    if (reminderActive) return;
    if (wake_pending) return;   // mid-wake: never re-sleep between SLPOUT and DISPON
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

    setCpuFrequencyMhz(AWAKE_CPU_MHZ);  // otherwise boot stays at the default clock until the first sleep/wake cycle

    Serial.println("[1] touch.begin...");
    touch.begin();
    Serial.println("[1] OK");

    imu_power_down();   // unused IMU: drop from ~15uA power-on default to ~6uA power-down

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
    last_clock_ms = millis();
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
    ledcAttach(TFT_BL, BL_PWM_FREQ_HZ, BL_PWM_RES_BITS);
    ledcWrite(TFT_BL, BL_DUTY_ON);
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
    lv_timer_create(screen_sleep_cb, 1000, NULL);

    update_tasbeeh_display();
    update_istighfar_display();

    Serial.println("=== SETUP DONE ===\n");
}

void loop() {
    if (wake_pending && millis() - wake_time >= 120) {
        wake_pending = false;
        tft.writecommand(0x29);                 // TFT_DISPON
        ledcWrite(TFT_BL, BL_DUTY_ON);
        lv_display_trigger_activity(lv_disp);
        Serial.println("[SCREEN_ON]");
    }

    if (display_sleeping) {
        Serial.flush();   // finish sending any pending bytes before UART's clock gates —
                           // otherwise a mid-transmission string gets cut off and its
                           // remainder doesn't print until the next wake, garbling the log
        esp_err_t sleep_err = esp_light_sleep_start(); // blocks here until GPIO or timer wakeup
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (sleep_err != ESP_OK) {
            Serial.printf("[SLEEP] rejected: %s\n", esp_err_to_name(sleep_err));
        }
        Serial.printf("[WAKE] cause=%s\n", cause == ESP_SLEEP_WAKEUP_GPIO ? "GPIO" : "TIMER");

        updateClock();
        checkReminders();                        // may itself call wake_display() if a reminder is due
        if (second_ == 0) saveTime();

        if (cause == ESP_SLEEP_WAKEUP_GPIO && display_sleeping) wake_display();
    } else {
        // Adaptive idle: lv_timer_handler() returns the ms until the next
        // scheduled LVGL timer — sleep exactly that long instead of a fixed
        // 5ms, so the CPU spends the gaps in the FreeRTOS idle task (WFI)
        // rather than waking ~200x/s to find nothing to do.
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY) wait_ms = LV_DEF_REFR_PERIOD;
        delay(constrain(wait_ms, 1, 50));   // cap keeps wake_pending's 120ms deferral timely
    }
}
