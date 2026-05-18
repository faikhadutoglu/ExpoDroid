/**
 * webController.cpp
 *
 * - Spannt ESP32 Access Point auf
 * - Liefert HTML-Steueroberflaeche (PROGMEM)
 * - Empfaengt Steuerwerte via WebSocket bei /ws
 *
 * Protokoll (vom Handy zum ESP):
 *   "c:<throttle>,<steering>,<head>"   z.B. "c:50,-30,90"
 *   "w:r"  -> Wave Rechts
 *   "w:b"  -> Wave Beide
 *
 * Sendet das Handy 500 ms keine Nachricht -> Failsafe (Werte auf neutral).
 */

#include "webController.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/******************************************************************************
 *  Konfiguration
 ******************************************************************************/
#define AP_SSID      "EXPODROID-Bot"
#define AP_PASSWORD  "expodroid"   // mind. 8 Zeichen, sonst startet AP nicht!
#define AP_CHANNEL   6

/******************************************************************************
 *  Interne Variablen
 ******************************************************************************/
static AsyncWebServer    server(80);
static AsyncWebSocket    ws("/ws");

static volatile int      gThrottle      = 0;     // -100..100
static volatile int      gSteering      = 0;     // -100..100
static volatile int      gHeadAngle     = 90;    // 0..180

static volatile uint32_t gLastMessage_ms = 0;
static volatile bool     gClientConnected = false;

static volatile bool     gWaveRightFlag = false;
static volatile bool     gWaveBothFlag  = false;

/******************************************************************************
 *  HTML-Steueroberflaeche
 ******************************************************************************/
static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#0a0d12">
<title>EXPODROID</title>
<style>
  *,*::before,*::after{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}
  :root{
    --bg:#0a0d12;
    --surface:#11161e;
    --surface-2:#161c26;
    --border:#1f2733;
    --text:#e6edf3;
    --muted:#5b6573;
    --accent:#00e0c8;
    --accent-glow:rgba(0,224,200,.45);
    --danger:#ff4d6d;
    --warn:#ffb454;
    --mono:'SF Mono',ui-monospace,Menlo,Consolas,monospace;
  }
  html,body{height:100%;width:100%;overflow:hidden;background:var(--bg);color:var(--text);
    font-family:-apple-system,BlinkMacSystemFont,'SF Pro Text','Helvetica Neue',sans-serif;
    overscroll-behavior:none;touch-action:none;-webkit-touch-callout:none;
  }
  body{
    display:flex;flex-direction:column;
    background:
      radial-gradient(1200px 600px at 80% -20%, rgba(0,224,200,.06), transparent 60%),
      radial-gradient(900px 500px at -10% 110%, rgba(255,77,109,.04), transparent 60%),
      var(--bg);
    padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left);
  }

  /* Header */
  header{
    flex:0 0 auto;
    display:flex;justify-content:space-between;align-items:center;
    padding:14px 20px 12px;
    border-bottom:1px solid var(--border);
  }
  .brand{
    display:flex;align-items:baseline;gap:10px;
    font-family:var(--mono);font-size:13px;letter-spacing:.28em;
    color:var(--text);font-weight:600;
  }
  .brand-mark{color:var(--accent);font-size:11px}
  .brand-sub{color:var(--muted);font-size:10px;letter-spacing:.32em}

  .status{
    display:flex;align-items:center;gap:8px;
    font-family:var(--mono);font-size:11px;letter-spacing:.18em;
    color:var(--muted);text-transform:uppercase;
  }
  .dot{
    width:8px;height:8px;border-radius:50%;
    background:var(--danger);box-shadow:0 0 10px var(--danger);
    transition:background .2s,box-shadow .2s;
  }
  .status.ok .dot{background:var(--accent);box-shadow:0 0 12px var(--accent-glow)}
  .status.ok{color:var(--accent)}

  /* Main */
  main{
    flex:1;min-height:0;
    display:grid;grid-template-rows:minmax(0,1fr) auto auto;
    gap:14px;padding:14px;
  }

  .grid-top{
    display:grid;grid-template-columns:1fr 110px;gap:14px;min-height:0;
  }

  .panel{
    display:flex;flex-direction:column;
    background:var(--surface);
    border:1px solid var(--border);
    border-radius:14px;
    padding:12px;
    min-height:0;
    position:relative;
    overflow:hidden;
  }
  .panel::before{
    content:'';position:absolute;inset:0;
    background:linear-gradient(180deg,rgba(255,255,255,.02),transparent 40%);
    pointer-events:none;
  }

  .panel-head{
    display:flex;justify-content:space-between;align-items:center;
    font-family:var(--mono);font-size:10px;letter-spacing:.3em;
    color:var(--muted);text-transform:uppercase;
    padding-bottom:10px;
  }
  .panel-value{
    color:var(--accent);font-weight:600;letter-spacing:.06em;
    font-feature-settings:'tnum' 1,'zero' 1;font-size:13px;
  }

  /* Slider */
  .slider{
    flex:1;
    background:var(--surface-2);
    border:1px solid var(--border);
    border-radius:10px;
    position:relative;touch-action:none;
    overflow:hidden;min-height:80px;
  }
  .center-line{position:absolute;background:rgba(255,255,255,.06);pointer-events:none}
  .slider.h .center-line{top:50%;left:14px;right:14px;height:1px;transform:translateY(-.5px)}
  .slider.v .center-line{left:50%;top:14px;bottom:14px;width:1px;transform:translateX(-.5px)}

  .ticks{position:absolute;inset:14px;pointer-events:none;opacity:.18}
  .slider.h .ticks{background-image:repeating-linear-gradient(90deg,var(--text) 0,var(--text) 1px,transparent 1px,transparent 12.5%)}
  .slider.v .ticks{background-image:repeating-linear-gradient(0deg,var(--text) 0,var(--text) 1px,transparent 1px,transparent 12.5%)}

  .fill{
    position:absolute;pointer-events:none;
    background:linear-gradient(90deg,rgba(0,224,200,.18),rgba(0,224,200,.04));
    border-radius:6px;
  }
  .slider.v .fill{
    background:linear-gradient(0deg,rgba(0,224,200,.18),rgba(0,224,200,.04));
  }

  .thumb{
    position:absolute;background:var(--accent);
    box-shadow:0 0 18px var(--accent-glow),0 0 4px var(--accent-glow);
    pointer-events:none;border-radius:3px;
  }
  .slider.h .thumb{top:10px;bottom:10px;width:6px;left:50%;transform:translateX(-50%)}
  .slider.v .thumb{left:10px;right:10px;height:6px;top:50%;transform:translateY(-50%)}

  .slider-labels{
    position:absolute;inset:0;pointer-events:none;
    font-family:var(--mono);font-size:9px;letter-spacing:.2em;color:var(--muted);
  }
  .slider-labels span{position:absolute;text-transform:uppercase}
  .slider.h .slider-labels .lo{left:14px;top:50%;transform:translateY(-50%)}
  .slider.h .slider-labels .hi{right:14px;top:50%;transform:translateY(-50%)}
  .slider.v .slider-labels .lo{bottom:10px;left:50%;transform:translateX(-50%)}
  .slider.v .slider-labels .hi{top:10px;left:50%;transform:translateX(-50%)}

  .head-area .slider{min-height:78px}

  /* Buttons */
  .actions{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .btn{
    background:var(--surface);
    border:1px solid var(--border);
    color:var(--text);
    padding:14px;border-radius:12px;
    font-family:var(--mono);font-size:11px;letter-spacing:.25em;
    font-weight:600;text-transform:uppercase;
    transition:transform .08s ease,background .12s ease,border-color .12s ease,color .12s ease;
  }
  .btn:active{
    background:var(--accent);color:var(--bg);border-color:var(--accent);
    transform:translateY(1px) scale(.99);
  }

  /* Disconnected overlay */
  .overlay{
    position:fixed;inset:0;z-index:50;
    background:rgba(10,13,18,.92);
    backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);
    display:flex;align-items:center;justify-content:center;flex-direction:column;
    gap:10px;opacity:0;pointer-events:none;
    transition:opacity .25s ease;
  }
  .overlay.show{opacity:1;pointer-events:auto}
  .ov-title{
    font-family:var(--mono);font-size:13px;letter-spacing:.32em;
    color:var(--danger);text-transform:uppercase;font-weight:700;
  }
  .ov-sub{
    font-family:var(--mono);font-size:11px;letter-spacing:.18em;color:var(--muted);
  }
  .pulse{
    width:42px;height:42px;border-radius:50%;
    border:2px solid var(--danger);position:relative;margin-bottom:10px;
  }
  .pulse::after{
    content:'';position:absolute;inset:-2px;border-radius:50%;
    border:2px solid var(--danger);animation:pulse 1.4s ease-out infinite;
  }
  @keyframes pulse{0%{transform:scale(1);opacity:.8}100%{transform:scale(1.6);opacity:0}}
</style>
</head>
<body>

<header>
  <div class="brand">
    <span class="brand-mark">[ ●  ]</span>
    EXPODROID
    <span class="brand-sub">RC // V1</span>
  </div>
  <div class="status" id="status">
    <span class="dot"></span>
    <span id="status-text">Verbinde</span>
  </div>
</header>

<main>
  <div class="grid-top">

    <div class="panel">
      <div class="panel-head">
        <span>Lenkung</span>
        <span class="panel-value" id="steering-value">+000</span>
      </div>
      <div class="slider h" id="steering">
        <div class="ticks"></div>
        <div class="center-line"></div>
        <div class="fill"></div>
        <div class="thumb"></div>
        <div class="slider-labels"><span class="lo">L</span><span class="hi">R</span></div>
      </div>
    </div>

    <div class="panel">
      <div class="panel-head">
        <span>Gas</span>
        <span class="panel-value" id="throttle-value">+000</span>
      </div>
      <div class="slider v" id="throttle">
        <div class="ticks"></div>
        <div class="center-line"></div>
        <div class="fill"></div>
        <div class="thumb"></div>
        <div class="slider-labels"><span class="hi">FWD</span><span class="lo">REV</span></div>
      </div>
    </div>

  </div>

  <div class="panel head-area">
    <div class="panel-head">
      <span>Kopfdrehung</span>
      <span class="panel-value" id="head-value">090&deg;</span>
    </div>
    <div class="slider h" id="head">
      <div class="ticks"></div>
      <div class="center-line"></div>
      <div class="fill"></div>
      <div class="thumb"></div>
      <div class="slider-labels"><span class="lo">0</span><span class="hi">180</span></div>
    </div>
  </div>

  <div class="actions">
    <button class="btn" id="wave-r">Wink Rechts</button>
    <button class="btn" id="wave-b">Wink Beide</button>
  </div>
</main>

<div class="overlay" id="overlay">
  <div class="pulse"></div>
  <div class="ov-title">Verbindung verloren</div>
  <div class="ov-sub">Verbinde erneut...</div>
</div>

<script>
class Slider{
  constructor(id,opts){
    this.el=document.getElementById(id);
    this.fill=this.el.querySelector('.fill');
    this.thumb=this.el.querySelector('.thumb');
    this.vertical=!!opts.vertical;
    this.snap=opts.snap!==false;       // returns to center on release
    this.min=opts.min!==undefined?opts.min:-100;
    this.max=opts.max!==undefined?opts.max:100;
    this.center=opts.center!==undefined?opts.center:(this.snap?0:(this.min+this.max)/2);
    this.value=this.center;
    this.activeId=null;
    this.onChange=opts.onChange||(()=>{});
    const ev=['pointerdown','pointermove','pointerup','pointercancel','pointerleave'];
    ev.forEach(t=>this.el.addEventListener(t,e=>this._handle(t,e)));
    this.render();
  }
  _handle(type,e){
    if(type==='pointerdown'){
      if(this.activeId!==null) return;
      this.activeId=e.pointerId;
      try{this.el.setPointerCapture(e.pointerId)}catch(_){}
      this._update(e);
    } else if(type==='pointermove'){
      if(e.pointerId!==this.activeId) return;
      this._update(e);
    } else { // up/cancel/leave
      if(e.pointerId!==this.activeId) return;
      this.activeId=null;
      if(this.snap){this.value=this.center;this.render();this.onChange(this.value)}
    }
  }
  _update(e){
    const r=this.el.getBoundingClientRect();
    const pad=10;
    let p;
    if(this.vertical){
      const h=r.height-2*pad;
      p=1-(e.clientY-r.top-pad)/h;
    } else {
      const w=r.width-2*pad;
      p=(e.clientX-r.left-pad)/w;
    }
    p=Math.max(0,Math.min(1,p));
    this.value=this.min+p*(this.max-this.min);
    this.render();
    this.onChange(this.value);
  }
  render(){
    const range=this.max-this.min;
    const p=(this.value-this.min)/range;
    const cp=(this.center-this.min)/range;
    const lo=Math.min(p,cp), hi=Math.max(p,cp);
    if(this.vertical){
      this.fill.style.left='10px';this.fill.style.right='10px';
      this.fill.style.bottom=(lo*100)+'%';
      this.fill.style.top=((1-hi)*100)+'%';
      this.thumb.style.top=((1-p)*100)+'%';
    } else {
      this.fill.style.top='10px';this.fill.style.bottom='10px';
      this.fill.style.left=(lo*100)+'%';
      this.fill.style.right=((1-hi)*100)+'%';
      this.thumb.style.left=(p*100)+'%';
    }
  }
}

const fmt=v=>{const n=Math.round(v);const s=n>=0?'+':'-';return s+String(Math.abs(n)).padStart(3,'0')};

let throttle,steering,head;
let ws=null,connected=false;

const setStatus=(ok)=>{
  connected=ok;
  const el=document.getElementById('status');
  const t=document.getElementById('status-text');
  const ov=document.getElementById('overlay');
  if(ok){el.classList.add('ok');t.textContent='Verbunden';ov.classList.remove('show')}
  else  {el.classList.remove('ok');t.textContent='Getrennt';ov.classList.add('show')}
};

const connect=()=>{
  try{ws=new WebSocket('ws://'+location.host+'/ws')}catch(e){setTimeout(connect,800);return}
  ws.onopen   =()=>setStatus(true);
  ws.onclose  =()=>{setStatus(false);setTimeout(connect,800)};
  ws.onerror  =()=>{try{ws.close()}catch(_){}}
};
const send=msg=>{if(ws&&ws.readyState===1)ws.send(msg)};

window.addEventListener('DOMContentLoaded',()=>{
  throttle=new Slider('throttle',{vertical:true,  snap:true,
    onChange:v=>document.getElementById('throttle-value').textContent=fmt(v)});
  steering=new Slider('steering',{vertical:false, snap:true,
    onChange:v=>document.getElementById('steering-value').textContent=fmt(v)});
  head    =new Slider('head',    {vertical:false, snap:false, min:0, max:180, center:90,
    onChange:v=>document.getElementById('head-value').textContent=String(Math.round(v)).padStart(3,'0')+'\u00b0'});

  document.getElementById('wave-r').addEventListener('click',()=>send('w:r'));
  document.getElementById('wave-b').addEventListener('click',()=>send('w:b'));

  // Steuerwerte mit 20 Hz schicken
  setInterval(()=>{
    const t=Math.round(throttle.value);
    const s=Math.round(steering.value);
    const h=Math.round(head.value);
    send('c:'+t+','+s+','+h);
  },50);

  connect();
});

// Gesten/Zoom unterdruecken (iOS)
document.addEventListener('gesturestart',e=>e.preventDefault());
document.addEventListener('contextmenu',e=>e.preventDefault());
document.addEventListener('dblclick',e=>e.preventDefault());
window.addEventListener('beforeunload',()=>{try{ws&&ws.close()}catch(_){}})
</script>
</body>
</html>)HTML";

/******************************************************************************
 *  WebSocket-Eventhandler
 ******************************************************************************/
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch(type){
    case WS_EVT_CONNECT:
      gClientConnected = true;
      gLastMessage_ms  = millis();
      Serial.printf("[WS] Client #%u verbunden, IP %s\n",
                    client->id(), client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u getrennt\n", client->id());
      // Falls keine weiteren Clients mehr da sind: alles auf neutral
      gClientConnected = (server->count() > 0);
      if(!gClientConnected){
        gThrottle = 0;
        gSteering = 0;
      }
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if(!(info->final && info->index == 0 && info->len == len)) return;
      if(info->opcode != WS_TEXT) return;
      if(len == 0 || len > 64) return;

      char buf[72];
      memcpy(buf, data, len);
      buf[len] = '\0';

      if(buf[0] == 'c' && buf[1] == ':'){
        int t=0, s=0, h=90;
        if(sscanf(buf+2, "%d,%d,%d", &t, &s, &h) == 3){
          gThrottle  = constrain(t,  -100, 100);
          gSteering  = constrain(s,  -100, 100);
          gHeadAngle = constrain(h,    0, 180);
          gLastMessage_ms = millis();
        }
      } else if(buf[0] == 'w' && buf[1] == ':'){
        if     (buf[2] == 'r') gWaveRightFlag = true;
        else if(buf[2] == 'b') gWaveBothFlag  = true;
        gLastMessage_ms = millis();
      }
      break;
    }

    case WS_EVT_PONG:
    case WS_EVT_ERROR:
    default:
      break;
  }
}

/******************************************************************************
 *  Public API
 ******************************************************************************/
void webControllerInit(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
  WiFi.setSleep(false);   // wichtig fuer Latenz!

  Serial.print(F("[WIFI] AP gestartet. SSID: "));
  Serial.print(AP_SSID);
  Serial.print(F("  IP: "));
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *r = request->beginResponse_P(200, "text/html", INDEX_HTML);
    r->addHeader("Cache-Control", "no-store");
    request->send(r);
  });

  // Captive-Portal-Style: jede Anfrage auf 192.168.4.1 leiten
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.begin();
  Serial.println(F("[HTTP] Server laeuft auf Port 80"));
}

void webControllerLoop(){
  ws.cleanupClients();
}

int      webControllerGetThrottle()             { return gThrottle; }
int      webControllerGetSteering()             { return gSteering; }
int      webControllerGetHeadAngle()            { return gHeadAngle; }
bool     webControllerHasClient()               { return gClientConnected; }
uint32_t webControllerGetLastMessageTime_ms()   { return gLastMessage_ms; }

bool webControllerConsumeWaveRight(){
  noInterrupts(); bool f = gWaveRightFlag; gWaveRightFlag = false; interrupts();
  return f;
}
bool webControllerConsumeWaveBoth(){
  noInterrupts(); bool f = gWaveBothFlag;  gWaveBothFlag  = false; interrupts();
  return f;
}