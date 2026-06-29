#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

// WebServer::stop() only closes the socket — handlers persist.
// This helper exposes a method to clear the handler linked list.
struct ServerHelper : public WebServer {
    using WebServer::WebServer;
    void clearAllHandlers() {
        RequestHandler *h = _firstHandler;
        while (h) {
            RequestHandler *next = h->next();
            delete h;
            h = next;
        }
        _firstHandler = nullptr;
        _lastHandler = nullptr;
    }
};

// ── Types ─────────────────────────────────────────────────
struct Reminder {
    int  hour, minute;
    char label[32];
    bool enabled;
};

// ── Globals from main.cpp ─────────────────────────────────
#define BAT_ADC 1
#define MAX_REMINDERS 10
extern Preferences prefs;
extern int hour_, minute_, day_, month_, year_;
extern uint32_t tasbeehCount;
extern uint32_t istighfarCount;
extern uint32_t totalTasbeeh;
extern uint32_t totalIstighfar;
extern int tasbeeh_phrase_idx;
extern Reminder reminders[MAX_REMINDERS];
extern bool reminderFired[];

// ── WiFi scan globals ─────────────────────────────────────
extern int scan_count;
extern String scan_ssids[];
extern int scan_rssi[];
extern int scan_enc[];

// ── Functions from main.cpp ───────────────────────────────
extern void saveTime(void);
extern void saveReminders(void);
extern void wifi_ap_save_credentials(String ssid, String pass);

static int getBatteryPercentWeb() {
    long sum = 0;
    for (int i = 0; i < 16; i++) { sum += analogReadMilliVolts(BAT_ADC); delay(1); }
    float v_adc = sum / 16.0f / 1000.0f;
    float v_bat = v_adc * 3.0f;
    int pct = (int)((v_bat - 3.5f) / (4.15f - 3.5f) * 100.0f);
    return constrain(pct, 0, 100);
}

// ── Dashboard HTML ────────────────────────────────────────────
static const char DASHBOARD_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="ar" dir="rtl"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>لوحة التحكم</title>
<style>
:root{
  --bg:#0b1410;--card:#0e2820;--teal:#33cc55;--gold:#d4af37;
  --red:#ff4444;--grey:#7a6e56;--white:#f6e6b3;--cream:#e9d9a8;
  --border:#1f3a2a;--input-bg:#1a2e24;
}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent;}
body{background:var(--bg);color:var(--white);font-family:'Segoe UI',Tahoma,sans-serif;
     min-height:100vh;font-size:16px;}
header{background:var(--card);padding:12px 16px;
       border-bottom:2px solid var(--teal);
       display:flex;justify-content:space-between;align-items:center;
       position:sticky;top:0;z-index:100;}
header h1{color:var(--gold);font-size:1rem;}
#lv{color:var(--teal);font-size:1rem;font-weight:700;letter-spacing:2px;}
.c{max-width:520px;margin:0 auto;padding:12px 14px 30px;}
.card{background:var(--card);border-radius:14px;padding:16px;
      margin-bottom:14px;border:1px solid var(--border);}
.card h2{font-size:.85rem;color:var(--gold);margin-bottom:12px;
         letter-spacing:.5px;}
.clk{text-align:center;font-size:2.6rem;font-weight:700;
     color:var(--teal);letter-spacing:4px;font-variant-numeric:tabular-nums;}
.dte{text-align:center;color:var(--grey);font-size:.82rem;margin:4px 0;}
.bat{text-align:center;color:var(--grey);font-size:.75rem;margin-bottom:6px;}
.div{border:none;border-top:1px solid var(--border);margin:10px 0;}
.stats{display:flex;justify-content:space-around;padding:6px 0;}
.stat{text-align:center;}
.sv{font-size:1.9rem;font-weight:700;color:var(--teal);
    font-variant-numeric:tabular-nums;}
.sl{font-size:.72rem;color:var(--grey);margin-top:2px;}
label{display:block;font-size:.78rem;color:var(--grey);margin:8px 0 3px;}

/* ── Custom time picker ── */
.time-picker{display:flex;align-items:center;gap:4px;
             background:var(--input-bg);border:1px solid #374151;
             border-radius:10px;padding:6px 10px;transition:border-color .2s;}
.time-picker:focus-within{border-color:var(--teal);}
.time-picker select{background:transparent;border:none;color:var(--white);
  font-size:1.1rem;font-weight:700;outline:none;cursor:pointer;
  -webkit-appearance:none;appearance:none;text-align:center;
  width:44px;padding:4px 2px;font-variant-numeric:tabular-nums;}
.time-picker select option{background:#1a2e24;color:var(--white);}
.time-sep{color:var(--teal);font-size:1.2rem;font-weight:700;line-height:1;
          user-select:none;}

input[type=text],input[type=password]{
  width:100%;padding:9px 12px;
  background:var(--input-bg);border:1px solid #374151;border-radius:10px;
  color:var(--white);font-size:.95rem;outline:none;
  transition:border-color .2s;}
input:focus{border-color:var(--teal);}
input::placeholder{color:#4a5568;}

.row{display:flex;gap:10px;flex-wrap:wrap;}
.row>.col{flex:1;min-width:130px;}
.row>.col-time{flex:0 0 160px;}

.rc{display:flex;align-items:center;gap:8px;margin-top:8px;}
.rc input[type=checkbox]{
  width:18px;height:18px;accent-color:var(--teal);
  flex-shrink:0;cursor:pointer;}
.rc label{margin:0;color:var(--white);font-size:.88rem;cursor:pointer;}

.rem{background:#0a1f17;border-radius:10px;padding:12px;
     margin-bottom:8px;border:1px solid var(--border);}
.rn{color:var(--gold);font-size:.72rem;font-weight:700;margin-bottom:8px;
    opacity:.8;}
.btn{display:block;width:100%;padding:12px;border:none;
     border-radius:10px;font-size:.95rem;cursor:pointer;
     font-weight:700;margin-top:10px;letter-spacing:.3px;
     transition:opacity .15s,transform .1s;}
.btn:active{opacity:.75;transform:scale(.98);}
.bt{background:var(--teal);color:#0b1410;}
.bg{background:var(--gold);color:#0b1410;}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);
       padding:10px 24px;border-radius:20px;font-weight:700;
       display:none;z-index:999;font-size:.9rem;
       box-shadow:0 4px 20px rgba(0,0,0,.5);white-space:nowrap;}
</style></head><body>
<header>
  <h1>ساعة التسبيح</h1>
  <span id="lv">--:--</span>
</header>
<div class="c">
  <!-- Status card -->
  <div class="card">
    <div class="clk" id="clk">--:--</div>
    <div class="dte" id="dte">--/--/----</div>
    <div class="bat" id="bat">-- %</div>
    <div class="div"></div>
    <div class="stats">
      <div class="stat">
        <div class="sv" id="tv">0</div>
        <div class="sl">تسبيح</div>
      </div>
      <div class="stat">
        <div class="sv" id="iv">0</div>
        <div class="sl">استغفار</div>
      </div>
    </div>
    <div class="stats" style="margin-top:2px;border-top:1px solid var(--border);padding-top:8px;">
      <div class="stat">
        <div class="sv" id="ttv" style="font-size:1.2rem;">0</div>
        <div class="sl">إجمالي التسبيح</div>
      </div>
      <div class="stat">
        <div class="sv" id="tiv" style="font-size:1.2rem;">0</div>
        <div class="sl">إجمالي الاستغفار</div>
      </div>
    </div>
  </div>

  <!-- Time set card -->
  <div class="card">
    <h2>ضبط الوقت</h2>
    <div class="row">
      <div class="col-time">
        <label>الوقت</label>
        <div class="time-picker">
          <select id="mt-m" aria-label="دقيقة"></select>
          <span class="time-sep">:</span>
          <select id="mt-h" aria-label="ساعة"></select>
        </div>
      </div>
      <div class="col">
        <label>التاريخ (YYYY-MM-DD)</label>
        <input type="text" id="md" placeholder="2026-06-28"
               pattern="\d{4}-\d{2}-\d{2}" inputmode="numeric">
      </div>
    </div>
    <button class="btn bg" onclick="setTime()">حفظ الوقت</button>
  </div>

  <!-- Reminders card -->
  <div class="card">
    <h2>التذكيرات</h2>
    <div id="rem-list"></div>
    <button class="btn bt" onclick="saveRem()">حفظ التذكيرات</button>
  </div>
</div>
<div class="toast" id="toast"></div>
<script>
/* ── Build hour/minute selects ── */
(function(){
  var h=document.getElementById('mt-h');
  var m=document.getElementById('mt-m');
  for(var i=0;i<24;i++){
    var o=document.createElement('option');
    o.value=i;o.text=String(i).padStart(2,'0');h.appendChild(o);
  }
  for(var i=0;i<60;i++){
    var o=document.createElement('option');
    o.value=i;o.text=String(i).padStart(2,'0');m.appendChild(o);
  }
})();

function toast(msg,ok){
  var t=document.getElementById('toast');
  t.innerText=msg;
  t.style.background=ok?'var(--teal)':'var(--red)';
  t.style.color=ok?'#0b1410':'#fff';
  t.style.display='block';
  clearTimeout(t._tid);
  t._tid=setTimeout(function(){t.style.display='none';},2500);
}

/* ── Build reminder row HTML ── */
function buildRemRow(i,r){
  var hh=String(r.hour||8).padStart(2,'0');
  var mm=String(r.minute||0).padStart(2,'0');
  // Build hour options
  var hOpts='',mOpts='';
  for(var x=0;x<24;x++)
    hOpts+='<option value="'+x+'"'+(r.hour===x?' selected':'')+'>'+String(x).padStart(2,'0')+'</option>';
  for(var x=0;x<60;x++)
    mOpts+='<option value="'+x+'"'+(r.minute===x?' selected':'')+'>'+String(x).padStart(2,'0')+'</option>';
  return '<div class="rem">'+
    '<div class="rn">تذكير '+(i+1)+'</div>'+
    '<div class="row">'+
      '<div class="col-time"><label>الوقت</label>'+
        '<div class="time-picker">'+
          '<select id="rm'+i+'" aria-label="دقيقة">'+mOpts+'</select>'+
          '<span class="time-sep">:</span>'+
          '<select id="rh'+i+'" aria-label="ساعة">'+hOpts+'</select>'+
        '</div>'+
      '</div>'+
      '<div class="col"><label>النص</label>'+
        '<input type="text" id="rl'+i+'" value="'+(r.label||'')+'" placeholder="نص التذكير...">'+
      '</div>'+
    '</div>'+
    '<div class="rc">'+
      '<input type="checkbox" id="re'+i+'"'+(r.enabled?' checked':'')+'>'+
      '<label for="re'+i+'">تفعيل</label>'+
    '</div>'+
  '</div>';
}

/* ── Initial reminder list build ── */
var remindersLoaded=false;
function buildRemList(rems){
  if(remindersLoaded) return; // don't overwrite user edits on refresh
  var html='';
  for(var i=0;i<10;i++){
    var r=rems[i]||{hour:8,minute:0,label:'',enabled:false};
    html+=buildRemRow(i,r);
  }
  document.getElementById('rem-list').innerHTML=html;
  remindersLoaded=true;
}

/* ── Load state from device ── */
var loadInFlight=false;
async function load(){
  if(loadInFlight) return;
  loadInFlight=true;
  try{
    var resp=await fetch('/api/state');
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    var d=await resp.json();
    var h=String(d.hour||0).padStart(2,'0');
    var m=String(d.minute||0).padStart(2,'0');
    document.getElementById('clk').innerText=h+':'+m;
    document.getElementById('lv').innerText=h+':'+m;
    document.getElementById('dte').innerText=
      String(d.day||1).padStart(2,'0')+'/'+
      String(d.month||1).padStart(2,'0')+'/'+(d.year||2026);
    document.getElementById('bat').innerText=(d.battery||0)+' %';
    document.getElementById('tv').innerText=d.tasbeeh||0;
    document.getElementById('iv').innerText=d.isteghfar||0;
    document.getElementById('ttv').innerText=d.totaltasbeeh||0;
    document.getElementById('tiv').innerText=d.totalisteghf||0;
    buildRemList(d.reminders||[]);
  }catch(e){
    console.error('load error',e);
    toast('خطأ في الاتصال',false);
  }finally{
    loadInFlight=false;
  }
}

/* ── Save reminders ── */
var saveBusy=false;
async function saveRem(){
  if(saveBusy) return;
  saveBusy=true;
  var rems=[];
  for(var i=0;i<10;i++){
    var hEl=document.getElementById('rh'+i);
    var mEl=document.getElementById('rm'+i);
    if(!hEl||!mEl) continue;
    rems.push({
      hour:parseInt(hEl.value)||0,
      minute:parseInt(mEl.value)||0,
      label:(document.getElementById('rl'+i).value||'').trim(),
      enabled:document.getElementById('re'+i).checked
    });
  }
  try{
    var resp=await fetch('/api/reminders',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({reminders:rems})});
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    toast('تم حفظ التذكيرات!',true);
    remindersLoaded=false;  // allow next load() to rebuild fresh HTML
  }catch(e){toast('خطأ في الحفظ',false);}
  saveBusy=false;
}

/* ── Set time ── */
async function setTime(){
  var hh=parseInt(document.getElementById('mt-h').value);
  var mm=parseInt(document.getElementById('mt-m').value);
  var dv=document.getElementById('md').value.trim();
  var dd=1,mo=1,yy=2026;
  if(dv){
    var p=dv.split('-');
    if(p.length===3&&p[0].length===4){
      yy=parseInt(p[0]);mo=parseInt(p[1]);dd=parseInt(p[2]);
    }else{toast('تنسيق التاريخ: YYYY-MM-DD',false);return;}
  }
  try{
    var resp=await fetch('/api/time',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({hour:hh,minute:mm,day:dd,month:mo,year:yy})});
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    toast('تم حفظ الوقت!',true);
    // Refresh display
    document.getElementById('clk').innerText=String(hh).padStart(2,'0')+':'+String(mm).padStart(2,'0');
  }catch(e){toast('خطأ في الحفظ',false);}
}

/* ── Live clock (browser local time shown in header) ── */
setInterval(function(){
  var n=new Date();
  document.getElementById('lv').innerText=
    String(n.getHours()).padStart(2,'0')+':'+
    String(n.getMinutes()).padStart(2,'0');
},1000);

load();
setInterval(load, 3000);
</script>
</body></html>
)rawhtml";

// ── WiFi Captive Portal HTML ──────────────────────────────────
static const char WIFI_CONFIG_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="ar" dir="rtl"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>إعداد الواي فاي</title>
<style>
:root{--bg:#0b1410;--card:#0e2820;--teal:#33cc55;--gold:#d4af37;
      --grey:#7a6e56;--white:#f6e6b3;--red:#ff4444;--border:#1f3a2a;}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent;}
body{background:var(--bg);color:var(--white);
     font-family:'Segoe UI',Tahoma,sans-serif;
     padding:16px;min-height:100vh;}
.card{background:var(--card);border-radius:14px;padding:18px;
      max-width:440px;margin:0 auto;border:1px solid var(--border);}
h1{color:var(--gold);font-size:1.05rem;text-align:center;margin-bottom:14px;}
label{display:block;font-size:.8rem;color:var(--grey);margin:10px 0 4px;}
input{width:100%;padding:11px 13px;background:#1a2e24;
      border:1px solid #374151;border-radius:10px;color:var(--white);
      font-size:.95rem;outline:none;transition:border-color .2s;}
input:focus{border-color:var(--teal);}
input::placeholder{color:#4a5568;}
.btn{display:block;width:100%;padding:13px;border:none;border-radius:10px;
     font-size:.95rem;cursor:pointer;font-weight:700;margin-top:14px;
     letter-spacing:.3px;transition:opacity .15s,transform .1s;}
.btn:active{opacity:.75;transform:scale(.98);}
.bt{background:var(--teal);color:#0b1410;}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);
       padding:10px 24px;border-radius:20px;font-weight:700;
       display:none;z-index:999;font-size:.9rem;
       box-shadow:0 4px 20px rgba(0,0,0,.5);white-space:nowrap;}
.scan-title{color:var(--gold);font-size:.85rem;margin:14px 0 8px;font-weight:600;}
.net-list{max-height:220px;overflow-y:auto;margin-bottom:10px;
          scrollbar-width:thin;scrollbar-color:var(--border) transparent;}
.net-list::-webkit-scrollbar{width:4px;}
.net-list::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px;}
.net-item{display:flex;align-items:center;padding:11px 12px;
           border:1px solid var(--border);border-radius:10px;margin-bottom:6px;
           cursor:pointer;background:#0a1f17;transition:border-color .2s,background .2s;
           gap:8px;}
.net-item:hover,.net-item.sel{border-color:var(--teal);background:#0d2b1f;}
.net-item:active{background:#0f3324;}
.net-lock{font-size:.85rem;flex-shrink:0;}
.net-ssid{flex:1;color:var(--white);font-size:.9rem;
          overflow:hidden;text-overflow:ellipsis;white-space:nowrap;
          min-width:0;}
.net-rssi{color:var(--grey);font-size:.72rem;flex-shrink:0;}
/* Signal bars */
.sig{display:flex;gap:2px;align-items:flex-end;flex-shrink:0;}
.sig span{width:4px;border-radius:1px;background:#2a3a2a;}
.sig span:nth-child(1){height:4px;}
.sig span:nth-child(2){height:7px;}
.sig span:nth-child(3){height:10px;}
.sig span:nth-child(4){height:14px;}
.sig.s4 span{background:var(--teal);}
.sig.s3 span:nth-child(-n+3){background:var(--teal);}
.sig.s2 span:nth-child(-n+2){background:var(--teal);}
.sig.s1 span:nth-child(-n+1){background:var(--teal);}
.scanning{text-align:center;color:var(--grey);padding:22px;font-size:.85rem;}
.status-msg{text-align:center;padding:12px;color:var(--teal);
            font-size:.88rem;display:none;}
</style></head><body>
<div class="card">
  <h1>إعداد شبكة الواي فاي</h1>
  <div class="scan-title">الشبكات المتاحة:</div>
  <div class="net-list" id="netlist">
    <div class="scanning">جاري البحث عن الشبكات...</div>
  </div>
  <label>اسم الشبكة</label>
  <input type="text" id="ssid" placeholder="اختر من القائمة أو اكتب الاسم..."
         autocomplete="off" autocorrect="off" autocapitalize="none" spellcheck="false">
  <label>كلمة المرور</label>
  <input type="password" id="pass" placeholder="كلمة المرور..."
         autocomplete="current-password">
  <div class="status-msg" id="smsg">جاري الاتصال... يرجى الانتظار</div>
  <button class="btn bt" id="sbtn" onclick="save()">اتصال</button>
</div>
<div class="toast" id="toast"></div>
<script>
function sigClass(r){
  if(r>-55)return's4';
  if(r>-65)return's3';
  if(r>-75)return's2';
  return's1';
}
function sigBars(r){
  return'<div class="sig '+sigClass(r)+'"><span></span><span></span><span></span><span></span></div>';
}
function escHtml(s){
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
          .replace(/"/g,'&quot;').replace(/'/g,'&#39;');
}

async function loadNets(retry){
  retry=retry||0;
  try{
    var resp=await fetch('/api/scan');
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    var nets=await resp.json();
    var h='';
    if(!nets||!nets.length){
      if(retry<8){
        document.getElementById('netlist').innerHTML=
          '<div class="scanning">جاري البحث... ('+(retry+1)+'/8)</div>';
        setTimeout(function(){loadNets(retry+1);},2000);
        return;
      }
      h='<div class="scanning">لم يتم العثور على شبكات</div>';
    }else{
      for(var i=0;i<nets.length;i++){
        var n=nets[i];
        var safe=escHtml(n.ssid);
        h+='<div class="net-item" data-ssid="'+safe+'">'
          +'<span class="net-lock">'+(n.open?'':'🔒')+'</span>'
          +'<span class="net-ssid">'+safe+'</span>'
          +sigBars(n.rssi)
          +'<span class="net-rssi">'+n.rssi+' dBm</span>'
          +'</div>';
      }
    }
    document.getElementById('netlist').innerHTML=h;
  }catch(e){
    document.getElementById('netlist').innerHTML=
      '<div class="scanning">تعذر البحث عن الشبكات</div>';
  }
}

// Event delegation for network list — avoids XSS in onclick
document.getElementById('netlist').addEventListener('click',function(e){
  var el=e.target.closest('.net-item');
  if(!el) return;
  var ssid=el.getAttribute('data-ssid');
  if(!ssid) return;
  document.querySelectorAll('.net-item').forEach(function(x){x.classList.remove('sel');});
  el.classList.add('sel');
  document.getElementById('ssid').value=ssid;
  document.getElementById('pass').focus();
});

function toast(msg,ok){
  var t=document.getElementById('toast');
  t.innerText=msg;
  t.style.background=ok?'var(--teal)':'var(--red)';
  t.style.color=ok?'#0b1410':'#fff';
  t.style.display='block';
  clearTimeout(t._tid);
  t._tid=setTimeout(function(){t.style.display='none';},3000);
}

async function save(){
  var ssid=document.getElementById('ssid').value.trim();
  var pass=document.getElementById('pass').value;
  if(!ssid){toast('أدخل اسم الشبكة',false);return;}
  var btn=document.getElementById('sbtn');
  btn.disabled=true;btn.innerText='جاري الاتصال...';
  document.getElementById('smsg').style.display='block';
  try{
    var resp=await fetch('/api/wifi-save',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({ssid:ssid,password:pass})});
    if(!resp.ok) throw new Error('HTTP '+resp.status);
    toast('تم الحفظ! جاري إعادة التشغيل...',true);
    // Don't call window.close() — just show message and disable UI
  }catch(e){
    toast('خطأ في الحفظ',false);
    btn.disabled=false;btn.innerText='اتصال';
    document.getElementById('smsg').style.display='none';
  }
}
loadNets();
</script>
</body></html>
)rawhtml";

// ── API: Dashboard state ──────────────────────────────────────
static void handle_api_state(WebServer *srv) {
    StaticJsonDocument<4096> doc;
    doc["hour"]   = hour_;    doc["minute"] = minute_;
    doc["day"]    = day_;     doc["month"]  = month_;
    doc["year"]   = year_;    doc["second"] = 0;
    doc["tasbeeh"]      = tasbeehCount;
    doc["isteghfar"]    = istighfarCount;
    doc["totaltasbeeh"] = totalTasbeeh;
    doc["totalisteghf"] = totalIstighfar;
    doc["battery"]    = getBatteryPercentWeb();
    JsonArray arr = doc.createNestedArray("reminders");
    for (int i = 0; i < MAX_REMINDERS; i++) {
        JsonObject o = arr.createNestedObject();
        o["hour"]    = reminders[i].hour;
        o["minute"]  = reminders[i].minute;
        o["label"]   = reminders[i].label;
        o["enabled"] = reminders[i].enabled;
    }
    String out; serializeJson(doc, out);
    srv->send(200, "application/json", out);
}

// ── API: Set time ─────────────────────────────────────────────
static void handle_api_time(WebServer *srv) {
    if (!srv->hasArg("plain")) {
        srv->send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, srv->arg("plain"));
    if (err) {
        srv->send(500, "application/json", "{\"ok\":false,\"error\":\"json\"}");
        return;
    }
    hour_   = doc["hour"]   | hour_;
    minute_ = doc["minute"] | minute_;
    day_    = doc["day"]    | day_;
    month_  = doc["month"]  | month_;
    year_   = doc["year"]   | year_;
    saveTime();
    srv->send(200, "application/json", "{\"ok\":true}");
}

// ── API: Set reminders ────────────────────────────────────────
static void handle_api_reminders(WebServer *srv) {
    if (!srv->hasArg("plain")) {
        srv->send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, srv->arg("plain"));
    if (err) {
        Serial.printf("[API] reminders JSON error: %s\n", err.c_str());
        srv->send(500, "application/json", "{\"ok\":false,\"error\":\"json\"}");
        return;
    }
    JsonArray arr = doc["reminders"].as<JsonArray>();
    if (arr.isNull()) {
        srv->send(400, "application/json", "{\"ok\":false,\"error\":\"missing reminders\"}");
        return;
    }
    for (int i = 0; i < MAX_REMINDERS && i < (int)arr.size(); i++) {
        reminders[i].hour    = arr[i]["hour"]    | reminders[i].hour;
        reminders[i].minute  = arr[i]["minute"]  | reminders[i].minute;
        reminders[i].enabled = arr[i]["enabled"] | reminders[i].enabled;
        const char *lbl = arr[i]["label"];
        if (lbl) {
            strncpy(reminders[i].label, lbl, 31);
            reminders[i].label[31] = '\0';
        }
        reminderFired[i] = false;
    }
    saveReminders();
    srv->send(200, "application/json", "{\"ok\":true}");
}

// ── Setup dashboard server ────────────────────────────────────
static void setup_dashboard_server(WebServer *srv) {
    srv->on("/", HTTP_GET, [srv]() {
        srv->send(200, "text/html", DASHBOARD_HTML);
    });
    srv->on("/api/state", HTTP_GET, [srv]() {
        handle_api_state(srv);
    });
    srv->on("/api/time", HTTP_POST, [srv]() {
        handle_api_time(srv);
    });
    srv->on("/api/reminders", HTTP_POST, [srv]() {
        handle_api_reminders(srv);
    });
    srv->begin();
}

// ── Setup WiFi captive portal ─────────────────────────────────
static void setup_wifi_portal(WebServer *srv) {
    srv->on("/", HTTP_GET, [srv]() {
        srv->send(200, "text/html", WIFI_CONFIG_HTML);
    });
    srv->on("/api/scan", HTTP_GET, [srv]() {
        StaticJsonDocument<4096> doc;
        JsonArray arr = doc.to<JsonArray>();
        for (int i = 0; i < scan_count && i < 30; i++) {
            JsonObject o = arr.createNestedObject();
            o["ssid"]  = scan_ssids[i];
            o["rssi"]  = scan_rssi[i];
            o["open"]  = (scan_enc[i] == WIFI_AUTH_OPEN);
        }
        String out; serializeJson(doc, out);
        srv->send(200, "application/json", out);
    });
    srv->on("/api/wifi-save", HTTP_POST, [srv]() {
        if (srv->hasArg("plain")) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, srv->arg("plain"));
            if (!err) {
                String ssid = doc["ssid"].as<String>();
                String pass = doc["password"].as<String>();
                wifi_ap_save_credentials(ssid, pass);
            }
        }
        srv->send(200, "application/json", "{\"ok\":true}");
    });
    srv->begin();
}