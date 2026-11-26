/*
  ============================================================================
  NATTDUELLEN - ett Robinson-inspirerat reaktionsspel
  ----------------------------------------------------------------------------
  Hårdvara : ESP32 DevKit (WROOM-32) + 5 lysdioder + 5 motstånd (220 ohm)
  Mjukvara : Arduino IDE 2.x + ESP32-kärnan från Espressif
  Kommunikation: WiFi. Kortet startar sitt eget WiFi-nätverk (accesspunkt)
             och kör en webbserver. Anslut till nätverket "Nattduellen" och
             öppna http://192.168.4.1 (eller http://nattduellen.local)
             i valfri webbläsare på telefon/dator.
  Spelidé  : Alla 5 dioder tänds när spelet startas från webbappen. Efter en
             slumpmässig tid (3-10 s) slocknar en slumpmässigt vald diod.
             STOPPA släcker alla dioder och nollställer spelet.
  ============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ------------------------- Konfiguration -----------------------------------
const char* AP_SSID = "Nattduellen";      // WiFi-namn som kortet sänder ut
const char* AP_PASS = "nattduellen123";   // WiFi-lösenord (minst 8 tecken)

// GPIO-pinnar för lysdioderna (säkra pinnar på ESP32 DevKit / WROOM-32)
const int LED_PINS[5] = {25, 26, 27, 32, 33};
const int NUM_LEDS    = 5;

// Slumpmässig väntetid innan en lampa slocknar (millisekunder)
const unsigned long MIN_WAIT_MS = 3000;   // minst 3 sekunder
const unsigned long MAX_WAIT_MS = 10000;  // högst 10 sekunder

// ------------------------- Webbsidan ---------------------------------------
const char PAGE[] = R"HTML(
<!DOCTYPE html>
<html lang="sv">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Nattduellen</title>
<style>
  body{font-family:system-ui,sans-serif;background:#0b1020;color:#e8ecff;
       display:flex;flex-direction:column;align-items:center;min-height:100vh;
       margin:0;padding:24px;box-sizing:border-box}
  h1{font-size:2rem;margin:8px 0 16px}
  #status{font-size:1.3rem;min-height:2.2em;text-align:center;margin-bottom:16px}
  #lamps{display:flex;gap:14px;margin-bottom:28px}
  .lamp{width:42px;height:42px;border-radius:50%;background:#2a3357;
        border:2px solid #46538a;transition:all .15s}
  .lamp.on{background:#ffd94d;border-color:#ffe89a;
           box-shadow:0 0 18px 4px rgba(255,217,77,.55)}
  button{font-size:1.4rem;font-weight:bold;border:none;border-radius:12px;
         padding:16px 40px;margin:8px;cursor:pointer;width:min(320px,80vw)}
  #start{background:#2ecc71;color:#06210f}
  #start:disabled{background:#1d5c3e;color:#9fc7ae;cursor:default}
  #stop{background:#e74c3c;color:#2b0703}
</style>
</head>
<body>
<h1>&#127769; Nattduellen</h1>
<div id="status">Redo &ndash; tryck p&aring; STARTA</div>
<div id="lamps">
  <div class="lamp"></div><div class="lamp"></div><div class="lamp"></div>
  <div class="lamp"></div><div class="lamp"></div>
</div>
<button id="start">&#9654; STARTA</button>
<button id="stop">&#9632; STOPPA</button>
<script>
const statusEl = document.getElementById('status');
const startBtn = document.getElementById('start');
const lamps = document.getElementById('lamps').children;
document.getElementById('start').addEventListener('click', () => fetch('/start'));
document.getElementById('stop').addEventListener('click', () => fetch('/stop'));
async function poll(){
  try{
    const s = await (await fetch('/state')).json();
    if(s.state === 'idle'){
      statusEl.textContent = 'Redo \u2013 tryck p\u00e5 STARTA';
    } else if(s.state === 'armed'){
      statusEl.textContent = 'Alla lampor t\u00e4nda\u2026 h\u00e5ll utkik! (' +
                            (s.elapsedMs/1000).toFixed(1) + ' s)';
    } else {
      statusEl.textContent = '\u{1F4A5} Lampa ' + (s.darkLed + 1) +
                            ' slocknade efter ' + (s.elapsedMs/1000).toFixed(2) + ' s!';
    }
    startBtn.disabled = (s.state === 'armed');
    for(let i = 0; i < lamps.length; i++){
      const on = (s.state === 'armed') || (s.state === 'done' && i !== s.darkLed);
      lamps[i].classList.toggle('on', on);
    }
  }catch(e){
    statusEl.textContent = 'Ingen kontakt med spelet\u2026';
  }
}
poll();
setInterval(poll, 200);
</script>
</body>
</html>
)HTML";

WebServer server(80);

// ------------------------- Speltillstånd -----------------------------------
enum State { IDLE, ARMED, DONE };   // IDLE=nollställd, ARMED=alla lyser, DONE=en släckt
State state = IDLE;
int darkLed = -1;                   // vilken diod som slocknade (-1 = ingen)
unsigned long armedAtMs = 0;        // när spelet startades
unsigned long waitMs = 0;           // slumpmässig väntetid denna runda
unsigned long doneAtMs = 0;         // när dioden slocknade

void setAllLeds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

// ------------------------- HTTP-handlers -----------------------------------
void handleRoot() {
  server.send(200, "text/html", PAGE);
}

void handleStart() {
  if (state == ARMED) {                       // ett spel är redan igång
    server.send(409, "application/json", "{\"error\":\"redan startad\"}");
    return;
  }
  randomSeed(esp_random());                   // äkta slump från ESP32:ns hårdvara
  darkLed = random(NUM_LEDS);                 // välj vilken diod som ska slockna
  waitMs  = random(MIN_WAIT_MS, MAX_WAIT_MS + 1);
  setAllLeds(true);
  armedAtMs = millis();
  state = ARMED;
  Serial.printf("Start: lampa %d slocknar om %lu ms\n", darkLed + 1, waitMs);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  state = IDLE;
  darkLed = -1;
  setAllLeds(false);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleState() {
  unsigned long elapsed = 0;
  if (state == ARMED) elapsed = millis() - armedAtMs;
  if (state == DONE)  elapsed = doneAtMs - armedAtMs;
  const char* name = (state == IDLE) ? "idle" : (state == ARMED) ? "armed" : "done";
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"darkLed\":%d,\"elapsedMs\":%lu}",
           name, darkLed, elapsed);
  server.send(200, "application/json", buf);
}

// ------------------------- Setup & loop ------------------------------------
void setup() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(115200);
  delay(500);

  // Starta eget WiFi-nätverk (accesspunkt) - ingen router behövs
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("Accesspunkt startad: "); Serial.println(AP_SSID);
  Serial.print("Öppna i webbläsaren: http://"); Serial.println(ip);

  if (MDNS.begin("nattduellen")) {
    Serial.println("Eller: http://nattduellen.local");
  }

  /*
  // --- Alternativ: anslut till ditt eget WiFi istället för eget AP ---
  // const char* STA_SSID = "ditt-wifi-namn";
  // const char* STA_PASS = "ditt-wifi-losenord";
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(STA_SSID, STA_PASS);
  // while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  // Serial.print("Ansluten, IP: "); Serial.println(WiFi.localIP());
  */

  server.on("/", HTTP_GET, handleRoot);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop",  HTTP_GET, handleStop);
  server.on("/state", HTTP_GET, handleState);
  server.onNotFound([]() { server.send(404, "text/plain", "Hittades inte"); });
  server.begin();
  Serial.println("Webbservern är igång.");
}

void loop() {
  // När väntetiden gått ut: släck den valda dioden
  if (state == ARMED && (millis() - armedAtMs) >= waitMs) {
    digitalWrite(LED_PINS[darkLed], LOW);
    doneAtMs = millis();
    state = DONE;
    Serial.printf("Lampa %d slocknade efter %lu ms\n",
                  darkLed + 1, doneAtMs - armedAtMs);
  }
  server.handleClient();
}
