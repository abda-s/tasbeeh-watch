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
#include <sys/time.h>
#include <math.h>
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "config/CST816S_pin_config.h"
#include "config/reminders_config.h"
#include "ui/styles.h"
#include "ui/screens.h"
#include "indev/lv_indev_private.h"

LV_FONT_DECLARE(font_alexandria_16);
LV_FONT_DECLARE(font_alexandria_28);
LV_FONT_DECLARE(font_alexandria_12);

// Reminder struct / MAX_REMINDERS / REMINDER_DAYS_ALL now live in screens.h
// (shared with screen_notifications.cpp and screen_timeedit.cpp).
#define BAT_ADC 1

// Free GPIO broken out on connector P2 (see schematic) — change to whichever
// pin the motor is actually soldered to (15/16/17/18/21/33 are all free).
#define VIBRATOR_PIN 21
#define VIBRATOR_ENABLED 1

// Sakamoto's algorithm: weekday of a Gregorian date, 0=Sun..6=Sat.
// Needed because day_/month_/year_ track a calendar date but nothing
// currently derives a day-of-week from it.
static int weekday_from_date(int d, int m, int y) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

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

// GC9A01A datasheet (SLPOUT §6.2.4): only a 5ms wait is required after
// SLPOUT before the next command (supply/clock stabilization) — the 120ms
// figure elsewhere in that datasheet governs a different thing (minimum
// dwell time before flipping back into the opposite sleep state, which our
// 15s SLEEP_TIMEOUT_MS is nowhere near anyway). 20ms gives 4x margin over
// the real 5ms minimum while cutting most of the old, over-conservative delay.
#define WAKE_DISPON_DELAY_MS 20

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

// Deep-sleep test: DEEP_SLEEP_TEST_ENABLED itself lives in screens.h (shared
// with screen_settings.cpp, which triggers it). Auto-wake after this long
// even if never touched, so a test run can't get stuck — also wakes
// immediately on touch (same TOUCH_IRQ pin, EXT0).
#define DEEP_SLEEP_TEST_DURATION_US (5ULL * 60 * 1000 * 1000)

// ── Battery lockdown (<5%) ─────────────────────────────────────
// This board has no hardware battery protection (checked the ETA6098
// charger datasheet and this board's schematic: the charger has no
// battery-side undervoltage disconnect, and the VBAT->VSYS path when
// unplugged is a plain always-on P-FET with no protection logic at all) —
// below this threshold, firmware is the only thing standing between normal
// operation and running the cell down to a damaging voltage. Survives deep
// sleep + the reboot it causes (the only case that matters here, since
// normal operation never touches deep sleep at all).
RTC_DATA_ATTR bool in_lockdown = false;
// Set by enterLockdownTest() to force lockdown regardless of real battery %,
// for bench testing. Only ever cleared by a genuine physical reset (not a
// deep-sleep wake — those preserve RTC_DATA_ATTR on purpose).
RTC_DATA_ATTR bool lockdown_test_forced = false;
// Set from a long-press on the lockdown screen itself (screen_lockdown.cpp,
// via requestLockdownTestExit()) — plain RAM, not RTC_DATA_ATTR, since it
// only matters within the current boot's display-wait loop in setup().
static bool lockdown_test_exit_requested = false;
#define LOCKDOWN_ENTER_PCT   5
#define LOCKDOWN_EXIT_PCT    10   // hysteresis — a jittery reading right at
                                  // 5% can't flap the mode back and forth
#define LOCKDOWN_BL_DUTY     8   // ~3% — legible, backlight is the dominant
                                  // screen-on power cost (85mA@100% vs
                                  // 52mA@27%, measured — see README)
#define LOCKDOWN_RECHECK_US  (5ULL * 60 * 1000 * 1000)  // silent battery
                                  // recheck cadence while locked down
#define LOCKDOWN_DISPLAY_MS  4000  // much shorter than the normal
                                  // SLEEP_TIMEOUT_MS (15s) — there's nothing
                                  // to read here but one short message

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

// ── Vibration motor ─────────────────────────────────────────────
// vibration_enabled is the user-facing on/off preference (Notifications
// screen switch, persisted below) and is independent of VIBRATOR_ENABLED
// (screens.h) — that one's a build-time master switch for whether any
// hardware is even present to drive.
bool vibration_enabled = true;

#if VIBRATOR_ENABLED
static lv_timer_t *vib_timer = NULL;
static bool        vib_on = false;
static int         vib_pulses_left = 0;
static int         vib_pulse_ms = 0;
static int         vib_gap_ms = 0;

static void vib_timer_cb(lv_timer_t *t) {
    if (vib_on) {
        digitalWrite(VIBRATOR_PIN, LOW);
        vib_on = false;
        if (--vib_pulses_left <= 0) {
            lv_timer_delete(t);
            vib_timer = NULL;
            return;
        }
        lv_timer_set_period(t, vib_gap_ms);
    } else {
        digitalWrite(VIBRATOR_PIN, HIGH);
        vib_on = true;
        lv_timer_set_period(t, vib_pulse_ms);
    }
}

// Non-blocking: pulse_ms-on / gap_ms-off, repeated `count` times, advanced
// by a self-deleting lv_timer rather than delay() — this gets called from
// inside checkReminders(), which both the awake main loop and the
// light-sleep wake path go through, so blocking here would stall LVGL and
// the rest of the wake path along with it.
void vibrate(int pulse_ms, int gap_ms, int count) {
    if (!vibration_enabled || count <= 0) return;
    if (vib_timer) { lv_timer_delete(vib_timer); vib_timer = NULL; }
    vib_pulse_ms    = pulse_ms;
    vib_gap_ms      = gap_ms;
    vib_pulses_left = count;
    digitalWrite(VIBRATOR_PIN, HIGH);
    vib_on = true;
    vib_timer = lv_timer_create(vib_timer_cb, pulse_ms, NULL);
}

void vibrate_notification() {
    vibrate(200, 100, 5);   // five pulses, 200ms on / 100ms off
}
#else
void vibrate_notification() { }
#endif

void saveVibrationSetting() {
    prefs.putBool("vib_en", vibration_enabled);
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

// ESP-IDF's RTC-timer-backed system clock (gettimeofday()/settimeofday())
// is documented to keep running correctly across deep sleep, unlike
// esp_timer_get_time()/millis() (which the rest of this app's clock is
// built on) — that one explicitly resets to 0 on deep-sleep wake. These two
// helpers exercise that path — used by deepSleepUntil() below (battery
// lockdown, and the deep-sleep bench test), not by the normal light-sleep
// clock at all.
static void wallClockToSystemTime() {
    struct tm t = {};
    t.tm_year = year_ - 1900;
    t.tm_mon  = month_ - 1;
    t.tm_mday = day_;
    t.tm_hour = hour_;
    t.tm_min  = minute_;
    t.tm_sec  = second_;
    struct timeval tv = { .tv_sec = mktime(&t), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

static void systemTimeToWallClock() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm t;
    localtime_r(&tv.tv_sec, &t);
    year_   = t.tm_year + 1900;
    month_  = t.tm_mon + 1;
    day_    = t.tm_mday;
    hour_   = t.tm_hour;
    minute_ = t.tm_min;
    second_ = t.tm_sec;
}

void saveReminders() {
    for (int i = 0; i < MAX_REMINDERS; i++) {
        String b = "r" + String(i);
        prefs.putInt((b + "h").c_str(), reminders[i].hour);
        prefs.putInt((b + "m").c_str(), reminders[i].minute);
        prefs.putBool((b + "e").c_str(), reminders[i].enabled);
        prefs.putString((b + "l").c_str(), reminders[i].label);
        prefs.putUChar((b + "d").c_str(), reminders[i].days);
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
        reminders[i].days = prefs.getUChar((b + "d").c_str(), REMINDER_DAYS_ALL);
        reminderFired[i] = false;
    }
}

// Applies REMINDER_PRESETS (config/reminders_config.h) to reminders[0 ..
// REMINDER_PRESET_COUNT-1] whenever that table has changed since the last
// boot (tracked via REMINDER_PRESET_VERSION). Labels are always refreshed
// from the table so renames take effect; hour/minute/enabled/days are only
// (re)initialized for slots that are newly added, so a parent's existing
// customization on a preset that's still there survives editing the table.
// Slots that used to be presets but were removed from the table get cleared.
static void syncReminderPresets() {
    int storedVersion = prefs.getInt("remseed_v", -1);
    if (storedVersion == REMINDER_PRESET_VERSION) return;

    int oldCount = prefs.getInt("remseed_n", 0);

    for (unsigned i = 0; i < REMINDER_PRESET_COUNT; i++) {
        strncpy(reminders[i].label, REMINDER_PRESETS[i].label, 31);
        reminders[i].label[31] = '\0';
        if ((int)i >= oldCount) {   // newly added row — apply its defaults
            reminders[i].hour    = REMINDER_PRESETS[i].hour;
            reminders[i].minute  = REMINDER_PRESETS[i].minute;
            reminders[i].enabled = false;
            reminders[i].days    = REMINDER_DAYS_ALL;
        }
    }
    for (int i = REMINDER_PRESET_COUNT; i < oldCount && i < MAX_REMINDERS; i++) {
        reminders[i].label[0] = '\0';
        reminders[i].hour = 8; reminders[i].minute = 0;
        reminders[i].enabled = false;
        reminders[i].days = REMINDER_DAYS_ALL;
    }

    saveReminders();
    prefs.putInt("remseed_v", REMINDER_PRESET_VERSION);
    prefs.putInt("remseed_n", (int)REMINDER_PRESET_COUNT);
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
static void checkBatteryLockdown(void);

static void checkReminders() {
    if (reminderActive) return;
    // Deliberately no "second_ == 0" gate: while asleep, checkReminders() only
    // runs at sparse wake events (touch, or every SLEEP_WAKE_INTERVAL_US), not
    // every second, so the exact-second instant is almost never observed. The
    // reminderFired[] flag below already dedups (fires once on entering the
    // target minute, resets once we leave it), so matching on hour+minute
    // alone is both sufficient and required for reminders to fire reliably
    // while light-sleeping.
    int today = weekday_from_date(day_, month_, year_);
    for (int i = 0; i < MAX_REMINDERS; i++) {
        if (!reminders[i].enabled) { reminderFired[i] = false; continue; }
        bool dayOk = reminders[i].days & (1 << today);
        if (dayOk && hour_ == reminders[i].hour && minute_ == reminders[i].minute) {
            if (!reminderFired[i]) {
                reminderFired[i] = true;
                popupRemIdx = i;
                reminderActive = true;
                prefs.putInt("popup_idx", i);
                if (display_sleeping) wake_display();
                vibrate_notification();
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
    checkBatteryLockdown();   // catches a drop below 5% within 5s while awake —
                              // the sleeping-branch check below is the one that
                              // matters most (screen off is the majority state)
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
    touch.rearm();                               // reattach ISR only, no hardware reset —
                                                  // chip already back in dynamic mode on its
                                                  // own (that's what generated this wake);
                                                  // begin()'s ~110ms reset was pure latency

    setCpuFrequencyMhz(AWAKE_CPU_MHZ);
    tft.writecommand(0x11);                     // TFT_SLPOUT
    display_sleeping = false;                   // mark awake immediately so touch works
    lv_display_trigger_activity(lv_disp);       // reset inactivity NOW — otherwise
                                                // screen_sleep_cb can fire inside the
                                                // WAKE_DISPON_DELAY_MS window below and
                                                // re-sleep the panel (backlight on + black screen)
    wake_pending = true;
    wake_time = millis();                       // defer DISPON + backlight briefly
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
//  Deep sleep entry — shared by the bench test and battery lockdown below.
//
//  Wake sources: touch (EXT0 on TOUCH_IRQ, LOW level — same polarity light
//  sleep needed on real hardware, main.cpp sleep_display()) or the given
//  timer duration, whichever comes first. Pressing the physical RESET
//  button always works too, regardless of deep sleep state.
//
//  esp_deep_sleep_start() never returns — waking from it is a full reset,
//  back through setup() from the top.
// ══════════════════════════════════════════════════════════════
static void deepSleepUntil(uint64_t timer_wake_us) {
    ledcWrite(TFT_BL, BL_DUTY_OFF);
    // Paint solid black directly (bypassing LVGL) right before sleeping, so
    // whatever's in GRAM going into deep sleep is neutral, not whatever
    // screen was active (e.g. Settings) — confirmed this was the source of
    // an earlier "shows Settings on wake" bug.
    tft.fillScreen(TFT_BLACK);
    tft.writecommand(0x10);          // TFT_SLPIN

    // Between physical wake and the first line of the sketch (ROM
    // bootloader + 2nd-stage boot), GPIO2 is outside any of our code's
    // control — nothing in setup() can reach that window. gpio_hold_en()
    // latches its output level through deep sleep *and* that whole boot
    // window, until we explicitly release it ourselves early in setup().
    ledcDetach(TFT_BL);              // release from the PWM peripheral first —
    pinMode(TFT_BL, OUTPUT);         // hold latches a plain digital level,
    digitalWrite(TFT_BL, LOW);       // not "mid PWM cycle"
    gpio_hold_en((gpio_num_t)TFT_BL);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    gpio_deep_sleep_hold_en();
#endif

    detachInterrupt(TOUCH_IRQ);      // same reset-glitch guard as sleep_display()
    touch.sleep();
    touch.available();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_IRQ, 0);
    esp_sleep_enable_timer_wakeup(timer_wake_us);

    Serial.flush();
    esp_deep_sleep_start();
}

// Entry into REAL battery lockdown during normal operation — the check in
// setup() only ever runs once per boot, and normal (light-sleep) operation
// never reboots on its own, so a battery that drains gradually while the
// watch keeps running would otherwise never trip it at all. Called from
// battery_timer_cb() (every 5s, while awake) and from loop()'s sleeping
// branch (every wake, ~60s — the one that matters most, since screen-off
// is where the watch actually spends most of its time).
static void checkBatteryLockdown() {
    if (in_lockdown) return;
    int battPct = getBatteryPercent();
    if (battPct < LOCKDOWN_ENTER_PCT) {
        Serial.printf("[BATT] pct=%d — dropped below %d%% during normal "
            "operation, entering lockdown now\n", battPct, LOCKDOWN_ENTER_PCT);
        in_lockdown = true;
        saveTime();
        wallClockToSystemTime();
        deepSleepUntil(LOCKDOWN_RECHECK_US);
        // unreachable — deepSleepUntil() never returns
    }
}

#if DEEP_SLEEP_TEST_ENABLED
// Bench tool — triggered by long-pressing the Settings screen's back button
// (deliberately not a normal-looking button — shouldn't be reachable by a
// casual/accidental tap). See the "option 2" power investigation: does
// esp_deep_sleep_start() actually draw less than the ~4.8mA already
// measured for real light sleep on this whole board?
void enterDeepSleepTest() {
    Serial.println("[DEEPSLEEP_TEST] entering — wake on touch, or after "
                    "DEEP_SLEEP_TEST_DURATION_US");
    saveTime();              // NVS checkpoint — fallback if the RTC-time
                              // path doesn't survive as expected
    wallClockToSystemTime(); // what this test is actually trying to verify
    deepSleepUntil(DEEP_SLEEP_TEST_DURATION_US);
}
#endif

#if LOCKDOWN_TEST_ENABLED
// Bench tool — triggered by long-pressing the Settings screen's back button
// (same "not a normal-looking button" reasoning as enterDeepSleepTest()).
// Forces in_lockdown regardless of real battery %, then goes to sleep
// immediately — the *next* wake (touch or the lockdown recheck timer) lands
// straight in setup()'s real in_lockdown==true path, exercising the actual
// lockdown code, not a separate mock of it.
void enterLockdownTest() {
    Serial.println("[LOCKDOWN_TEST] forcing lockdown for testing");
    lockdown_test_forced = true;
    in_lockdown = true;
    saveTime();
    wallClockToSystemTime();
    deepSleepUntil(LOCKDOWN_RECHECK_US);
}

void requestLockdownTestExit() {
    if (!lockdown_test_forced) return;   // real lockdown — no escape from a touch
    Serial.println("[LOCKDOWN_TEST] exit requested");
    lockdown_test_exit_requested = true;
}
#endif

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
#if DEEP_SLEEP_TEST_ENABLED
        if (millis() < 3000) {   // only right after boot — is a touch already
            Serial.printf("[BOOT] touch press at (%d,%d) t=%lums\n",
                data->point.x, data->point.y, millis());
        }
#endif
    }
}

static uint32_t my_tick(void) {
    return millis();
}

void setup() {
    // Absolute first thing, before anything else touches this pin. If we
    // came from enterDeepSleepTest(), GPIO2 is still gpio_hold_en()-latched
    // LOW all the way from before sleep through the ROM bootloader and up to
    // this exact line — release it before reconfiguring, otherwise the hold
    // just overrides whatever pinMode()/digitalWrite() below try to do.
    // Harmless no-op if the hold was never engaged (e.g. a normal power-on
    // boot, not a deep-sleep wake).
    gpio_hold_dis((gpio_num_t)TFT_BL);
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
    gpio_deep_sleep_hold_dis();
#endif

    // tft.begin() alone takes 140ms+ (its own internal SLPOUT+delay(120)+
    // DISPON+delay(20)) — drive the backlight LOW ourselves before any of
    // that runs, rather than leaving GPIO2 uncontrolled for that window.
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== BOOT START ===");
    Serial.printf("[BOOT] reset_reason=%d wakeup_cause=%d\n",
        (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());

    setCpuFrequencyMhz(AWAKE_CPU_MHZ);  // otherwise boot stays at the default clock until the first sleep/wake cycle

    Serial.println("[1] touch.begin...");
    touch.begin();
    Serial.println("[1] OK");

    imu_power_down();   // unused IMU: drop from ~15uA power-on default to ~6uA power-down

#if VIBRATOR_ENABLED
    pinMode(VIBRATOR_PIN, OUTPUT);
    digitalWrite(VIBRATOR_PIN, LOW);
#endif

    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC, ADC_11db);

    int battPct = getBatteryPercent();
    if (lockdown_test_forced) {
        in_lockdown = true;   // ignore real battery entirely while under test
    } else {
        if (!in_lockdown && battPct < LOCKDOWN_ENTER_PCT) in_lockdown = true;
        else if (in_lockdown && battPct >= LOCKDOWN_EXIT_PCT) in_lockdown = false;
    }
    Serial.printf("[BATT] pct=%d in_lockdown=%d forced=%d\n", battPct, in_lockdown, lockdown_test_forced);

    Serial.println("[2] prefs.begin...");
    prefs.begin("watch", false);
    tasbeehCount      = prefs.getUInt("tasbeeh", 0);
    tasbeeh_phrase_idx = prefs.getInt("tasbeeh_phrase", 0);
    istighfarCount    = prefs.getUInt("isteghfar", 0);
    totalTasbeeh      = prefs.getUInt("totaltasbeeh", 0);
    totalIstighfar    = prefs.getUInt("totalisteghfar", 0);
    loadReminders();
    syncReminderPresets();
    loadTime();
    vibration_enabled = prefs.getBool("vib_en", true);

    // Any deep-sleep-originated wake (bench test or battery lockdown below)
    // gets its wall clock restored from the RTC-backed system time instead
    // of the (possibly stale) NVS checkpoint — confirmed correct on real
    // hardware this session (logged NVS-vs-RTC values matched).
    esp_sleep_wakeup_cause_t wake_cause = esp_sleep_get_wakeup_cause();
    bool woke_from_deep_sleep = (wake_cause == ESP_SLEEP_WAKEUP_TIMER || wake_cause == ESP_SLEEP_WAKEUP_EXT0);
    if (woke_from_deep_sleep) {
        int nvs_h = hour_, nvs_m = minute_, nvs_d = day_;
        systemTimeToWallClock();
        Serial.printf("[BOOT] woke from deep sleep, cause=%s\n",
            wake_cause == ESP_SLEEP_WAKEUP_TIMER ? "TIMER" : "TOUCH(EXT0)");
        Serial.printf("[BOOT] NVS said %02d:%02d day %d — RTC/gettimeofday says %02d:%02d day %d\n",
            nvs_h, nvs_m, nvs_d, hour_, minute_, day_);
    }

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
    // TFT_BL was already forced LOW at the very top of setup() (see there for
    // why). Arduino-ESP32's ledcAttach() reads the channel's *current* duty
    // via ledc_get_duty() and feeds it back in as the pin's initial duty —
    // on a channel that's never been configured before, i.e. an unreliable
    // read (Espressif's own issue tracker flags this exact pattern:
    // espressif/arduino-esp32#11373 — "the impact of duty being invalid...
    // is unclear") — so pin it down explicitly again right before attaching,
    // no gap between the two.
    digitalWrite(TFT_BL, LOW);
    ledcAttach(TFT_BL, BL_PWM_FREQ_HZ, BL_PWM_RES_BITS);
    ledcWrite(TFT_BL, BL_DUTY_OFF);   // stay dark until scr_home is actually on screen —
                                      // same reasoning as WAKE_DISPON_DELAY_MS below: turning
                                      // the backlight on this early lights up whatever's in
                                      // GRAM through the entire lv_init/theme/styles/screens_init
                                      // sequence (screens_init() builds all 6 screens' widget
                                      // trees before scr_home is ever loaded), which is exactly
                                      // the visible glitch on every deep-sleep wake (a full
                                      // reboot through setup(), unlike light sleep's instant
                                      // resume that never re-runs any of this)
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

    if (in_lockdown) {
        // Battery <5%: skip the whole normal UI (screens_init() builds 6
        // screens' worth of widgets we won't use), show only the lockdown
        // message, and go straight back to deep sleep. A TIMER wake is the
        // silent recheck — nobody's there to see it, so if we're still
        // under the recovery threshold, don't even render/light the screen.
        bool silent_recheck = (wake_cause == ESP_SLEEP_WAKEUP_TIMER);
        Serial.printf("[LOCKDOWN] silent_recheck=%d\n", silent_recheck);
        if (!silent_recheck) {
            create_screen_lockdown();
            lv_screen_load(scr_lockdown);
            // Same half-screen-double-buffer reasoning as the normal path
            // below — a few passes guarantees the whole frame is flushed
            // before anything lights it up.
            for (int i = 0; i < 4; i++) {
                lv_timer_handler();
                delay(20);
            }
            const int FADE_STEPS = 24;
            for (int i = 1; i <= FADE_STEPS; i++) {
                float t = (float)i / FADE_STEPS;
                int duty = (int)(LOCKDOWN_BL_DUTY * powf(t, 2.2f) + 0.5f);
                ledcWrite(TFT_BL, duty);
                delay(6);
            }
            ledcWrite(TFT_BL, LOCKDOWN_BL_DUTY);

            uint32_t shown_at = millis();
            while (millis() - shown_at < LOCKDOWN_DISPLAY_MS && !lockdown_test_exit_requested) {
                lv_timer_handler();
                delay(20);
            }
        }
        if (lockdown_test_exit_requested) {
            // Test-only escape (requestLockdownTestExit() already refused to
            // set this unless lockdown_test_forced was true) — clear the
            // state and fall through into the normal boot path below instead
            // of going back to sleep.
            Serial.println("[LOCKDOWN_TEST] exiting — resuming normal boot");
            in_lockdown = false;
            lockdown_test_forced = false;
        } else {
            Serial.println("[LOCKDOWN] returning to deep sleep");
            saveTime();
            // Only re-seed the RTC-backed system clock if it was never
            // seeded to begin with (a genuine cold boot straight into
            // lockdown). If we woke from a *previous* deep sleep cycle,
            // hour_/minute_/second_ were already read from that
            // continuously-ticking clock at the top of setup() and haven't
            // been touched since — writing them back here would overwrite
            // the still-accurate system clock with a now-stale snapshot,
            // silently discarding however long this boot took to run
            // (touch.begin(), ADC reads, up to LOCKDOWN_DISPLAY_MS on
            // screen) on every single cycle. That was the actual source of
            // the accuracy regression versus light sleep — not the RTC
            // hardware, this bug.
            if (!woke_from_deep_sleep) wallClockToSystemTime();
            deepSleepUntil(LOCKDOWN_RECHECK_US);
            // unreachable — deepSleepUntil() never returns
        }
    }

    Serial.println("[8] screens_init...");
    screens_init();
    Serial.println("[8] OK");
    Serial.printf("[BOOT] after screens_init: active=%p home=%p settings=%p t=%lums\n",
        lv_screen_active(), scr_home, scr_settings, millis());

    update_home_clock();
    lv_screen_load(scr_home);
    Serial.printf("[BOOT] after lv_screen_load(home): active=%p t=%lums\n",
        lv_screen_active(), millis());
    // draw_buf_1/2 are each only HALF the screen (LV_DISPLAY_RENDER_MODE_PARTIAL),
    // so a full-screen invalidation like lv_screen_load() needs at least two
    // flush passes (top half, bottom half) — one lv_timer_handler() call isn't
    // reliably enough to guarantee both have gone out. If the backlight came on
    // before the second half flushed, whatever was on the panel before this
    // boot (e.g. the Settings screen, if that's what deep sleep was entered
    // from — SLPIN doesn't clear GRAM) would still be showing in that half.
    // A few passes leaves no doubt the whole frame is actually on the panel.
    for (int i = 0; i < 4; i++) {
        lv_timer_handler();
        delay(20);
    }
    Serial.printf("[BOOT] before backlight-on: active=%p (home=%p) t=%lums\n",
        lv_screen_active(), scr_home, millis());
    // 500ms diagnostic delay tested and removed — made no difference, which
    // rules out a settling/timing race (500ms is far more than a full-frame
    // SPI transfer needs, ~23ms at 40MHz for 240x240x16bpp).
    //
    // Fade in gamma-corrected, not linear. LED brightness perception is
    // roughly a power curve (~duty^2.2), so a *linear* duty ramp (0,7,14...)
    // looks dark/flat for most of its steps and then jumps to full brightness
    // in the last one or two — reported back as "better but still flashes".
    // Ramping duty as step^2.2 instead spends more of the sequence at
    // perceptually-low brightness, so it actually looks gradual. Only runs
    // once per boot/deep-sleep-wake, not on light-sleep touch-wake (that
    // path stays instant — V1.32 tuned it for sub-100ms latency).
    const int FADE_STEPS = 24;
    for (int i = 1; i <= FADE_STEPS; i++) {
        float t = (float)i / FADE_STEPS;
        int duty = (int)(BL_DUTY_ON * powf(t, 2.2f) + 0.5f);
        ledcWrite(TFT_BL, duty);
        delay(6);
    }
    ledcWrite(TFT_BL, BL_DUTY_ON);

    clock_timer_obj  = lv_timer_create(clock_timer_cb, 1000, NULL);
    battery_timer_obj = lv_timer_create(battery_timer_cb, 5000, NULL);
    lv_timer_create(screen_sleep_cb, 1000, NULL);

    update_tasbeeh_display();
    update_istighfar_display();

    Serial.println("=== SETUP DONE ===\n");
}

void loop() {
    if (wake_pending && millis() - wake_time >= WAKE_DISPON_DELAY_MS) {
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
        checkBatteryLockdown();                  // every wake, screen on or off — this is what
                                                  // actually catches a drained battery in time

        if (cause == ESP_SLEEP_WAKEUP_GPIO && display_sleeping) wake_display();
    } else {
        // Adaptive idle: lv_timer_handler() returns the ms until the next
        // scheduled LVGL timer — sleep exactly that long instead of a fixed
        // 5ms, so the CPU spends the gaps in the FreeRTOS idle task (WFI)
        // rather than waking ~200x/s to find nothing to do.
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms == LV_NO_TIMER_READY) wait_ms = LV_DEF_REFR_PERIOD;
        delay(constrain(wait_ms, 1, 50));   // cap keeps wake_pending's DISPON check responsive
                                            // (worst case ~50ms past WAKE_DISPON_DELAY_MS,
                                            // still well under the touch-to-visible budget)
    }
}
