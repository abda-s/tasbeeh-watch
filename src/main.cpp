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
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

#include "config/CST816S_pin_config.h"
#include "ui/styles.h"
#include "ui/screens.h"

#define BAT_ADC       1
#define NTP_SERVER    "pool.ntp.org"
#define TZ_OFFSET     (3 * 3600)
#define DST_OFFSET    0
#define MAX_REMINDERS 10

struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
};

CST816S     touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);
Preferences prefs;
WebServer   server(80);

int   hour_ = 8, minute_ = 0, second_ = 0;
int   day_  = 1, month_  = 1, year_ = 2026;

uint32_t tasbeehCount   = 0;
uint32_t isteghfarCount = 0;

Reminder reminders[MAX_REMINDERS];
bool     reminderFired[MAX_REMINDERS];
int      popupRemIdx = 0;
bool     reminderActive = false;
lv_obj_t *reminder_mbox = NULL;

String deviceIP = "";
bool   wifiOK   = false;

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

static void syncFromNTP() {
    configTime(TZ_OFFSET, DST_OFFSET, NTP_SERVER);
    struct tm t;
    int tries = 0;
    while (!getLocalTime(&t) && tries < 20) { delay(500); tries++; }
    if (getLocalTime(&t)) {
        hour_ = t.tm_hour; minute_ = t.tm_min; second_ = t.tm_sec;
        day_ = t.tm_mday; month_ = t.tm_mon + 1; year_ = t.tm_year + 1900;
        saveTime();
    }
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

static String buildHTML() {
    return R"rawhtml(
<!DOCTYPE html><html lang="ar" dir="rtl"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ساعة التسبيح</title>
<style>
:root{--bg:#0a0e1a;--card:#111827;--teal:#00c8a0;--gold:#fea020;
      --green:#00e000;--red:#ff4040;--grey:#6b7280;--white:#f9fafb;}
*{box-sizing:border-box;margin:0;padding:0;}
body{background:var(--bg);color:var(--white);font-family:'Segoe UI',sans-serif;}
header{background:var(--card);padding:14px 18px;
       border-bottom:2px solid var(--teal);
       display:flex;justify-content:space-between;align-items:center;}
header h1{color:var(--teal);font-size:1.05rem;}
.c{max-width:580px;margin:0 auto;padding:14px;}
.card{background:var(--card);border-radius:14px;padding:16px;
      margin-bottom:16px;border:1px solid #1f2937;}
.card h2{font-size:.9rem;color:var(--gold);margin-bottom:12px;}
.clk{text-align:center;font-size:2.8rem;font-weight:700;
     color:var(--teal);letter-spacing:4px;}
.dte{text-align:center;color:var(--grey);font-size:.85rem;margin-bottom:8px;}
.bat{text-align:center;color:var(--grey);font-size:.8rem;margin-bottom:4px;}
.stats{display:flex;justify-content:space-around;padding:10px 0;}
.stat{text-align:center;}
.sv{font-size:2rem;font-weight:700;color:var(--teal);}
.sl{font-size:.75rem;color:var(--grey);}
label{display:block;font-size:.8rem;color:var(--grey);margin:8px 0 3px;}
input[type=text],input[type=time]{width:100%;padding:9px 11px;
  background:#1f2937;border:1px solid #374151;border-radius:9px;
  color:var(--white);font-size:.95rem;outline:none;}
input:focus{border-color:var(--teal);}
.row{display:flex;gap:10px;}.row>div{flex:1;}
.rc{display:flex;align-items:center;gap:8px;margin-top:7px;}
.rc input[type=checkbox]{width:17px;height:17px;accent-color:var(--teal);}
.rc label{margin:0;color:var(--white);font-size:.9rem;}
.rem{background:#1a2235;border-radius:10px;padding:11px;
     margin-bottom:9px;border:1px solid #2d3748;}
.rn{color:var(--gold);font-size:.78rem;font-weight:700;margin-bottom:5px;}
.btn{display:block;width:100%;padding:11px;border:none;
     border-radius:10px;font-size:.95rem;cursor:pointer;
     font-weight:700;margin-top:11px;}
.btn:active{opacity:.75;}
.bt{background:var(--teal);color:#000;}
.bg{background:var(--gold);color:#000;}
.div{border:none;border-top:1px solid #1f2937;margin:12px 0;}
.toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);
       padding:9px 22px;border-radius:18px;font-weight:700;
       display:none;z-index:999;}
</style></head><body>
<header><h1>⌚ ساعة التسبيح</h1><span id="lv" style="color:#6b7280">--:--</span></header>
<div class="c">
  <div class="card">
    <div class="clk" id="clk">--:--</div>
    <div class="dte" id="dte">--/--/----</div>
    <div class="bat" id="bat">🔋 --%</div>
    <div class="div"></div>
    <div class="stats">
      <div class="stat"><div class="sv" id="tv">0</div><div class="sl">📿 تسبيح</div></div>
      <div class="stat"><div class="sv" id="iv">0</div><div class="sl">🤲 استغفار</div></div>
    </div>
  </div>
  <div class="card">
    <h2>🕐 ضبط الوقت</h2>
    <div class="row">
      <div><label>الوقت</label><input type="time" id="mt"></div>
      <div><label>التاريخ (YYYY-MM-DD)</label>
           <input type="text" id="md" placeholder="2026-06-16"></div>
    </div>
    <button class="btn bg" onclick="setTime()">💾 حفظ الوقت</button>
  </div>
  <div class="card">
    <h2>🔔 التذكيرات</h2>
    <div id="rl"></div>
    <button class="btn bt" onclick="saveRem()">💾 حفظ التذكيرات</button>
  </div>
</div>
<div class="toast" id="toast"></div>
<script>
function toast(m,ok=true){
  const t=document.getElementById('toast');
  t.innerText=m;t.style.background=ok?'#00e000':'#ff4040';
  t.style.color=ok?'#000':'#fff';t.style.display='block';
  setTimeout(()=>t.style.display='none',2500);
}
async function load(){
  try{
    const d=await(await fetch('/api/state')).json();
    const h=String(d.hour||0).padStart(2,'0');
    const m=String(d.minute||0).padStart(2,'0');
    document.getElementById('clk').innerText=h+':'+m;
    document.getElementById('lv').innerText=h+':'+m;
    document.getElementById('dte').innerText=
      String(d.day||1).padStart(2,'0')+'/'+
      String(d.month||1).padStart(2,'0')+'/'+(d.year||2026);
    document.getElementById('bat').innerText='🔋 '+(d.battery||0)+'%';
    document.getElementById('tv').innerText=d.tasbeeh||0;
    document.getElementById('iv').innerText=d.isteghfar||0;
    const rems=d.reminders||[];
    let html='';
    for(let i=0;i<10;i++){
      const r=rems[i]||{hour:8,minute:0,label:'',enabled:false};
      const hh=String(r.hour).padStart(2,'0');
      const mm=String(r.minute).padStart(2,'0');
      html+=`<div class="rem"><div class="rn">🔔 ${i+1}</div>
        <div class="row">
          <div style="flex:0 0 105px"><label>الوقت</label>
            <input type="time" id="rt${i}" value="${hh}:${mm}"></div>
          <div><label>النص</label>
            <input type="text" id="rl${i}" value="${r.label}"
              placeholder="صلاة الفجر..."></div>
        </div>
        <div class="rc">
          <input type="checkbox" id="re${i}" ${r.enabled?'checked':''}>
          <label for="re${i}">تفعيل</label>
        </div></div>`;
    }
    document.getElementById('rl').innerHTML=html;
  }catch(e){toast('❌ Connection error',false);}
}
async function saveRem(){
  const rems=[];
  for(let i=0;i<10;i++){
    const t=document.getElementById('rt'+i).value||'08:00';
    const[hh,mm]=t.split(':').map(Number);
    rems.push({hour:hh,minute:mm,
      label:document.getElementById('rl'+i).value.trim(),
      enabled:document.getElementById('re'+i).checked});
  }
  try{
    await fetch('/api/reminders',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({reminders:rems})});
    toast('✅ Saved!');
  }catch(e){toast('❌ Error',false);}
}
async function setTime(){
  const tv=document.getElementById('mt').value;
  if(!tv){toast('⚠️ Enter time',false);return;}
  const[hh,mm]=tv.split(':').map(Number);
  const dv=document.getElementById('md').value;
  let dd=1,mo=1,yy=2026;
  if(dv){const p=dv.split('-');if(p.length===3){yy=+p[0];mo=+p[1];dd=+p[2];}}
  try{
    await fetch('/api/time',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({hour:hh,minute:mm,day:dd,month:mo,year:yy})});
    toast('✅ Time saved!');load();
  }catch(e){toast('❌ Error',false);}
}
setInterval(()=>{
  const n=new Date();
  document.getElementById('lv').innerText=
    String(n.getHours()).padStart(2,'0')+':'+
    String(n.getMinutes()).padStart(2,'0');
},1000);
load();
</script>
</body></html>
)rawhtml";
}

static void setupWebServer() {
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", buildHTML());
    });
    server.on("/api/state", HTTP_GET, []() {
        StaticJsonDocument<2048> doc;
        doc["hour"] = hour_; doc["minute"] = minute_;
        doc["second"] = second_;
        doc["day"] = day_; doc["month"] = month_; doc["year"] = year_;
        doc["tasbeeh"] = tasbeehCount;
        doc["isteghfar"] = isteghfarCount;
        doc["battery"] = getBatteryPercent();
        JsonArray arr = doc.createNestedArray("reminders");
        for (int i = 0; i < MAX_REMINDERS; i++) {
            JsonObject o = arr.createNestedObject();
            o["hour"] = reminders[i].hour;
            o["minute"] = reminders[i].minute;
            o["label"] = reminders[i].label;
            o["enabled"] = reminders[i].enabled;
        }
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });
    server.on("/api/time", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            StaticJsonDocument<256> doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                hour_ = doc["hour"] | hour_;
                minute_ = doc["minute"] | minute_;
                day_ = doc["day"] | day_;
                month_ = doc["month"] | month_;
                year_ = doc["year"] | year_;
                second_ = 0;
                saveTime();
                update_home_clock();
            }
        }
        server.send(200, "application/json", "{\"ok\":true}");
    });
    server.on("/api/reminders", HTTP_POST, []() {
        if (server.hasArg("plain")) {
            StaticJsonDocument<4096> doc;
            if (!deserializeJson(doc, server.arg("plain"))) {
                JsonArray arr = doc["reminders"].as<JsonArray>();
                for (int i = 0; i < MAX_REMINDERS && i < (int)arr.size(); i++) {
                    reminders[i].hour = arr[i]["hour"] | reminders[i].hour;
                    reminders[i].minute = arr[i]["minute"] | reminders[i].minute;
                    reminders[i].enabled = arr[i]["enabled"] | reminders[i].enabled;
                    const char *lbl = arr[i]["label"];
                    if (lbl) { strncpy(reminders[i].label, lbl, 31); reminders[i].label[31] = '\0'; }
                    reminderFired[i] = false;
                }
                saveReminders();
            }
        }
        server.send(200, "application/json", "{\"ok\":true}");
    });
    server.begin();
    Serial.println("Server: http://" + deviceIP);
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

    // WiFi deferred to loop
    wifiOK = false;

    Serial.println("=== SETUP DONE ===\n");
}

void loop() {
    static bool wifi_done = false;
    lv_timer_handler();

    if (!wifi_done && millis() > 3000) {
        wifi_done = true;
        Serial.println("[loop] starting WiFi...");
        WiFiManager wm;
        wm.setConfigPortalTimeout(30);
        wifiOK = wm.autoConnect("TasbeehWatch");
        if (wifiOK) {
            deviceIP = WiFi.localIP().toString();
            Serial.println("IP: " + deviceIP);
            syncFromNTP();
            update_home_clock();
            setupWebServer();
            if (settings_ip_label) {
                lv_label_set_text_fmt(settings_ip_label, "http://%s", deviceIP.c_str());
            }
        }
        if (settings_ip_label && !wifiOK) {
            lv_label_set_text(settings_ip_label, "Offline");
        }
    }

    server.handleClient();
    delay(5);
}
