// Mock hardware layer: everything the firmware asks of an ESP32-S3 + GC9A01A +
// CST816S + Arduino core, implemented for a desktop process.
#include <Arduino.h>
#undef gettimeofday
#undef settimeofday
#include "sim_hal.h"
#include "Preferences.h"
#include "TFT_eSPI.h"
#include "CST816S.h"
#include "Wire.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include <map>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

SimState sim;
SimSerial Serial;
TwoWire   Wire;

// ── paths ───────────────────────────────────────────────────────────────
std::string sim_root() {
    static std::string root;
    if (!root.empty()) return root;
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    std::string p = n > 0 ? std::string(buf, n) : std::string(".");
    for (int i = 0; i < 8; i++) {                 // walk up until platformio.ini
        size_t s = p.find_last_of('/');
        if (s == std::string::npos) break;
        p = p.substr(0, s);
        struct stat st;
        if (stat((p + "/platformio.ini").c_str(), &st) == 0) { root = p; return root; }
    }
    root = ".";
    return root;
}
std::string sim_state_dir() {
    std::string d = sim_root() + "/.sim_state";
    mkdir(d.c_str(), 0755);
    return d;
}

// ── serial → stdout + web log ring ──────────────────────────────────────
static std::string log_partial;
static void log_feed(const char *s, size_t n) {
    fwrite(s, 1, n, stdout);
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\n') {
            sim.log.push_back(log_partial);
            sim.log_seq++;
            if (sim.log.size() > 400) sim.log.pop_front();
            log_partial.clear();
        } else if (s[i] != '\r') {
            log_partial += s[i];
        }
    }
}
void sim_log(const char *line) { std::string l = std::string(line) + "\n"; log_feed(l.data(), l.size()); }
void   SimSerial::flush() { fflush(stdout); }
size_t SimSerial::print(const char *s) { log_feed(s, strlen(s)); return strlen(s); }
size_t SimSerial::print(int v) { char b[24]; int n = snprintf(b, sizeof b, "%d", v); log_feed(b, n); return n; }
size_t SimSerial::println(const char *s) { size_t n = strlen(s); log_feed(s, n); log_feed("\n", 1); return n + 1; }
size_t SimSerial::println(const String &s) { return println(s.c_str()); }
size_t SimSerial::println(int v) { char b[24]; snprintf(b, sizeof b, "%d", v); return println(b); }
size_t SimSerial::printf(const char *fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if (n >= (int)sizeof buf) n = sizeof buf - 1;
    log_feed(buf, n);
    return n;
}

// ── time ────────────────────────────────────────────────────────────────
static uint64_t now_ns() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec; }
static const uint64_t start_ns = now_ns();
unsigned long millis() { return (unsigned long)((now_ns() - start_ns) / 1000000ull); }
unsigned long micros() { return (unsigned long)((now_ns() - start_ns) / 1000ull); }
void delay(unsigned long ms) {
    unsigned long t0 = millis();
    for (;;) {
        sim_pump();
        if (millis() - t0 >= ms) break;
        usleep(500);
    }
}
void delayMicroseconds(unsigned int us) { usleep(us); }

// ESP32's RTC-backed system clock keeps counting through deep sleep; here that
// is "real clock + offset", with the offset carried across the restart in a file.
static double rtc_offset_s = 0;
int sim_gettimeofday(struct timeval *tv, void *) {
    struct timeval real; ::gettimeofday(&real, NULL);
    double t = real.tv_sec + real.tv_usec / 1e6 + rtc_offset_s;
    tv->tv_sec = (time_t)t;
    tv->tv_usec = (suseconds_t)((t - (double)(time_t)t) * 1e6);
    return 0;
}
int sim_settimeofday(const struct timeval *tv, const void *) {
    struct timeval real; ::gettimeofday(&real, NULL);
    rtc_offset_s = (tv->tv_sec + tv->tv_usec / 1e6) - (real.tv_sec + real.tv_usec / 1e6);
    FILE *f = fopen((sim_state_dir() + "/rtc_offset.txt").c_str(), "w");
    if (f) { fprintf(f, "%.6f\n", rtc_offset_s); fclose(f); }
    return 0;
}

// ── GPIO / PWM / ADC ────────────────────────────────────────────────────
static void set_motor(bool level) {
    if (level == sim.motor_level) return;
    sim.motor_level = level;
    sim.edges.push_back({++sim.motor_seq, (uint32_t)millis(), level ? 1 : 0});
    if (sim.edges.size() > 64) sim.edges.pop_front();
    char b[64]; snprintf(b, sizeof b, "[MOTOR] GPIO%d %s", sim.motor_pin, level ? "ON" : "off");
    sim_log(b);
}
void pinMode(uint8_t pin, uint8_t mode) {
    if (mode == OUTPUT && pin != TFT_BL) sim.motor_pin = pin;   // the only other output
}
void digitalWrite(uint8_t pin, uint8_t v) {
    if (pin == TFT_BL) { if (!sim.bl_pwm) sim.bl_duty = v ? 255 : 0; return; }
    if ((int)pin == sim.motor_pin) set_motor(v != 0);
}
int digitalRead(uint8_t) { return 0; }
void analogReadResolution(uint8_t) {}
void analogSetPinAttenuation(uint8_t, int) {}
uint32_t analogReadMilliVolts(uint8_t) {
    // inverse of getBatteryPercent(): v_bat = 3.5 + 0.65*pct, ADC sees v_bat/3
    float v_bat = 3.5f + 0.65f * (sim.battery_pct / 100.0f);
    return (uint32_t)ceilf(v_bat / 3.0f * 1000.0f);   // ceil: getBatteryPercent() truncates
}
bool ledcAttach(uint8_t pin, uint32_t, uint8_t) { if (pin == TFT_BL) sim.bl_pwm = true; return true; }
bool ledcWrite(uint8_t pin, uint32_t duty) { if (pin == TFT_BL && sim.bl_pwm) sim.bl_duty = (int)duty; return true; }
bool ledcDetach(uint8_t pin) { if (pin == TFT_BL) sim.bl_pwm = false; return true; }
bool setCpuFrequencyMhz(uint32_t) { return true; }
void attachInterrupt(uint8_t, void (*)(void), int) {}
void detachInterrupt(uint8_t) {}
esp_err_t gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return ESP_OK; }
esp_err_t gpio_hold_en(gpio_num_t) { return ESP_OK; }
esp_err_t gpio_hold_dis(gpio_num_t) { return ESP_OK; }
void gpio_deep_sleep_hold_en(void) {}
void gpio_deep_sleep_hold_dis(void) {}

// ── NVS (Preferences) ───────────────────────────────────────────────────
static std::map<std::string, std::string> nvs;
static bool nvs_loaded = false;
static std::string nvs_path() { return sim_state_dir() + "/prefs.txt"; }
static std::string esc(const std::string &s) {
    std::string r;
    for (char c : s) { if (c == '\\') r += "\\\\"; else if (c == '\n') r += "\\n"; else if (c == '\t') r += "\\t"; else r += c; }
    return r;
}
static std::string unesc(const std::string &s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) { char n = s[++i]; r += (n == 'n') ? '\n' : (n == 't') ? '\t' : n; }
        else r += s[i];
    }
    return r;
}
static void nvs_load() {
    if (nvs_loaded) return;
    nvs_loaded = true;
    FILE *f = fopen(nvs_path().c_str(), "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        std::string l(line);
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        size_t t = l.find('\t');
        if (t != std::string::npos) nvs[unesc(l.substr(0, t))] = unesc(l.substr(t + 1));
    }
    fclose(f);
}
static void nvs_save() {
    FILE *f = fopen(nvs_path().c_str(), "w");
    if (!f) return;
    for (auto &kv : nvs) fprintf(f, "%s\t%s\n", esc(kv.first).c_str(), esc(kv.second).c_str());
    fclose(f);
}
static void nvs_put(const char *k, const std::string &v) { nvs_load(); nvs[k] = v; nvs_save(); }
static bool nvs_get(const char *k, std::string &out) { nvs_load(); auto it = nvs.find(k); if (it == nvs.end()) return false; out = it->second; return true; }

bool Preferences::begin(const char *, bool) { nvs_load(); return true; }
bool Preferences::clear() { nvs_load(); nvs.clear(); nvs_save(); return true; }
bool Preferences::isKey(const char *k) { std::string s; return nvs_get(k, s); }
size_t Preferences::putInt(const char *k, int32_t v) { nvs_put(k, std::to_string(v)); return 4; }
int32_t Preferences::getInt(const char *k, int32_t d) { std::string s; return nvs_get(k, s) ? (int32_t)strtol(s.c_str(), 0, 10) : d; }
size_t Preferences::putUInt(const char *k, uint32_t v) { nvs_put(k, std::to_string(v)); return 4; }
uint32_t Preferences::getUInt(const char *k, uint32_t d) { std::string s; return nvs_get(k, s) ? (uint32_t)strtoul(s.c_str(), 0, 10) : d; }
size_t Preferences::putBool(const char *k, bool v) { nvs_put(k, v ? "1" : "0"); return 1; }
bool Preferences::getBool(const char *k, bool d) { std::string s; return nvs_get(k, s) ? s == "1" : d; }
size_t Preferences::putUChar(const char *k, uint8_t v) { nvs_put(k, std::to_string((int)v)); return 1; }
uint8_t Preferences::getUChar(const char *k, uint8_t d) { std::string s; return nvs_get(k, s) ? (uint8_t)strtol(s.c_str(), 0, 10) : d; }
size_t Preferences::putString(const char *k, const char *v) { nvs_put(k, v ? v : ""); return v ? strlen(v) : 0; }
String Preferences::getString(const char *k, const String &d) { std::string s; return nvs_get(k, s) ? String(s.c_str()) : d; }

// ── display: TFT_eSPI + GC9A01A ─────────────────────────────────────────
void TFT_eSPI::begin() { sim.panel_slpin = false; sim.panel_dispoff = false; }
void TFT_eSPI::setRotation(uint8_t r) { sim.tft_rot = r & 3; }
void TFT_eSPI::fillScreen(uint32_t c) {
    for (int i = 0; i < 240 * 240; i++) sim.fb[i] = (uint16_t)c;
    sim.frame++;
}
void TFT_eSPI::writecommand(uint8_t cmd) {
    switch (cmd) {
        case 0x10: sim.panel_slpin = true;   break;   // SLPIN
        case 0x11: sim.panel_slpin = false;  break;   // SLPOUT
        case 0x28: sim.panel_dispoff = true; break;   // DISPOFF
        case 0x29: sim.panel_dispoff = false; break;  // DISPON
    }
    sim.frame++;
}
void TFT_eSPI::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) {
    sim.win_x = x; sim.win_y = y; sim.win_w = w; sim.win_h = h;
}
// TFT_eSPI writes in the panel's memory frame; MADCTL (set by setRotation)
// then maps memory -> what you see. Same table as GC9A01_Rotation.h.
static inline void mem_to_screen(int c, int r, int &x, int &y) {
    bool mv = false, mx = false, my = false;
    switch (sim.tft_rot & 3) {
        case 1: mx = true; mv = true; break;
        case 2: mx = true; my = true; break;
        case 3: mv = true; my = true; break;
    }
    x = c; y = r;
    if (mv) { x = r; y = c; }
    if (mx) x = 239 - x;
    if (my) y = 239 - y;
    // Calibration: LVGL pre-rotates its render buffer (lv_display_set_rotation
    // 270) and the panel applies MADCTL on top; on the real hardware those two
    // cancel to an upright picture. This extra quarter-turn makes the mock match
    // that observed result (verified against the README layout).
    int cx = 239 - y, cy = x;
    x = cx; y = cy;
}
void TFT_eSPI::pushColors(uint16_t *data, uint32_t, bool) {
    for (int r = 0; r < sim.win_h; r++) {
        for (int c = 0; c < sim.win_w; c++) {
            int x, y;
            mem_to_screen(sim.win_x + c, sim.win_y + r, x, y);
            if ((unsigned)x < 240 && (unsigned)y < 240)
                sim.fb[y * 240 + x] = data[r * sim.win_w + c];
        }
    }
    sim.frame++;
}

// ── touch ───────────────────────────────────────────────────────────────
bool CST816S::available() {
    if (!sim.touch_down) return false;
    // Raw touch axes are in the panel's physical orientation. The firmware does
    // raw' = 240 - raw, then LVGL's indev applies the 270-degree display rotation
    // (lv_indev.c indev_pointer_proc): logical = (raw'y, 239 - raw'x).
    // Inverting that chain for a wanted screen point (touch_x, touch_y):
    data.x = sim.touch_y + 1;
    data.y = 240 - sim.touch_x;
    data.event = 2; data.points = 1;
    return true;
}

// ── sleep ───────────────────────────────────────────────────────────────
const char *esp_err_to_name(esp_err_t e) { return e == ESP_OK ? "ESP_OK" : "ESP_ERR"; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us) { sim.timer_us = us; return ESP_OK; }
esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t, int) { sim.ext0_armed = true; return ESP_OK; }
esp_err_t esp_sleep_enable_gpio_wakeup(void) { sim.gpio_wake_armed = true; return ESP_OK; }
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) { return (esp_sleep_wakeup_cause_t)sim.last_wake; }
esp_reset_reason_t esp_reset_reason(void) { return sim.from_deep ? ESP_RST_DEEPSLEEP : ESP_RST_POWERON; }

esp_err_t esp_light_sleep_start(void) {
    sim.mode = SimState::LIGHT;
    sim_log("[SIM] light sleep (CPU halted, RAM kept)");
    unsigned long t0 = millis();
    int cause = 0;
    for (;;) {
        sim_pump();
        sim_apply_actions();
        if (sim.wake_request == 1) { cause = ESP_SLEEP_WAKEUP_TIMER; break; }
        if (sim.wake_request == 2 || (sim.gpio_wake_armed && sim.touch_down)) { cause = ESP_SLEEP_WAKEUP_GPIO; break; }
        if (sim.timer_us && (uint64_t)(millis() - t0) * 1000ull >= sim.timer_us) { cause = ESP_SLEEP_WAKEUP_TIMER; break; }
        usleep(2000);
    }
    sim.wake_request = 0;
    sim.last_wake = cause;
    sim.mode = SimState::AWAKE;
    return ESP_OK;
}

// RTC memory: the section RTC_DATA_ATTR variables live in. Saved across the
// process restart that stands in for a deep-sleep reboot, wiped on power cycle.
extern "C" { extern char __start_rtc_data[] __attribute__((weak)); extern char __stop_rtc_data[] __attribute__((weak)); }
static std::string rtc_path() { return sim_state_dir() + "/rtc.bin"; }
static void rtc_save() {
    if (!__start_rtc_data) return;
    FILE *f = fopen(rtc_path().c_str(), "wb");
    if (f) { fwrite(__start_rtc_data, 1, __stop_rtc_data - __start_rtc_data, f); fclose(f); }
}
static void rtc_load() {
    if (!__start_rtc_data) return;
    FILE *f = fopen(rtc_path().c_str(), "rb");
    if (!f) return;
    size_t n = __stop_rtc_data - __start_rtc_data;
    if (fread(__start_rtc_data, 1, n, f) != n) { /* size mismatch: keep defaults */ }
    fclose(f);
}

static int    saved_argc; static char **saved_argv;
static std::string exe_path;   // real path of this binary, resolved once at startup
[[noreturn]] void sim_restart(int wake_cause, bool deep_sleep, bool clear_rtc) {
    fflush(stdout);
    if (clear_rtc) { unlink(rtc_path().c_str()); unlink((sim_state_dir() + "/rtc_offset.txt").c_str()); }
    else if (deep_sleep) rtc_save();
    if (deep_sleep) setenv("SIM_DEEPWAKE", std::to_string(wake_cause).c_str(), 1); else unsetenv("SIM_DEEPWAKE");
    setenv("SIM_BOOT_ID", std::to_string(sim.boot_id + 1).c_str(), 1);
    setenv("SIM_BATTERY", std::to_string(sim.battery_pct).c_str(), 1);
    execv(exe_path.c_str(), saved_argv);
    perror("execv");
    exit(1);
}

[[noreturn]] void esp_deep_sleep_start(void) {
    sim.mode = SimState::DEEP;
    sim_log("[SIM] DEEP SLEEP — RAM lost; wake = full reboot through setup()");
    unsigned long t0 = millis();
    int cause = 0;
    for (;;) {
        sim_pump();
        sim_apply_actions();
        if (sim.wake_request == 1) { cause = ESP_SLEEP_WAKEUP_TIMER; break; }
        if (sim.wake_request == 2 || (sim.ext0_armed && sim.touch_down)) { cause = ESP_SLEEP_WAKEUP_EXT0; break; }
        if (sim.timer_us && (uint64_t)(millis() - t0) * 1000ull >= sim.timer_us) { cause = ESP_SLEEP_WAKEUP_TIMER; break; }
        usleep(2000);
    }
    sim_restart(cause, true, false);
}

// ── init ────────────────────────────────────────────────────────────────
void sim_init(int argc, char **argv) {
    saved_argc = argc; saved_argv = argv;
    { char b[PATH_MAX]; ssize_t n = readlink("/proc/self/exe", b, sizeof b - 1); exe_path = n > 0 ? std::string(b, n) : std::string(argv[0]); }
    setvbuf(stdout, NULL, _IOLBF, 0);
    memset(sim.fb, 0, sizeof sim.fb);
    if (const char *b = getenv("SIM_BATTERY")) sim.battery_pct = atoi(b);
    if (const char *b = getenv("SIM_BOOT_ID")) sim.boot_id = atoi(b);
    if (const char *w = getenv("SIM_DEEPWAKE")) {
        sim.from_deep = true;
        sim.env_wake = atoi(w);
        sim.last_wake = sim.env_wake;
        rtc_load();
        FILE *f = fopen((sim_state_dir() + "/rtc_offset.txt").c_str(), "r");
        if (f) { if (fscanf(f, "%lf", &rtc_offset_s) != 1) rtc_offset_s = 0; fclose(f); }
    }
    (void)saved_argc;
}
