/*
  ============================================================================
  NATTDUELLEN - variant för Arduino UNO R3 + HM-10 (BLE 4.0-modul)
  ----------------------------------------------------------------------------
  UNO R3 har ingen egen trådlös kommunikation, så lägger vi till en HM-10
  (BLE 4.0). Webbappen i telefonen (Web Bluetooth) kopplar upp mot HM-10 och
  pratar samma JSON-API som tidigare - appen växlar automatiskt mellan
  NUS-UUIDer (ESP32 / UNO R4 WiFi) och HM-10:s FFE0/FFE1.

  Koppling HM-10 -> UNO R3:
    HM-10 VCC -> 5V (om breakout-kortet har inbyggd regulator), annars 3.3V
    HM-10 GND -> GND
    HM-10 TXD -> D10          (UNO RX, SoftwareSerial)
    HM-10 RXD <- D11 via spänningsdelare (HM-10 har 3.3V-logik):
                 D11 --[1k]--+--> HM-10 RXD
                             +--[2k]--> GND
  Lysdioder:  D2..D6 -> anod, katod via 220 ohm till GND.

  Bibliotek: SoftwareSerial (inbyggt i Arduino IDE). HM-10 default baud: 9600.
  ============================================================================
*/

#include <Arduino.h>
#include <SoftwareSerial.h>

SoftwareSerial bleSerial(10, 11);   // RX = D10, TX = D11

// ------------------------- Konfiguration -----------------------------------
const int LED_PINS[5] = {2, 3, 4, 5, 6};   // UNO R3: D2-D6
const int NUM_LEDS    = 5;

const unsigned long MIN_WAIT_MS = 3000;
const unsigned long MAX_WAIT_MS = 10000;

// ------------------------- Enhet --------------------------------------------
char deviceId[7];

// ------------------------- Speltillstånd -----------------------------------
enum State { IDLE, ARMED, DONE };
State state = IDLE;
int darkLed = -1;
unsigned long armedAtMs = 0;
unsigned long waitMs = 0;
unsigned long doneAtMs = 0;
unsigned long lastStatePub = 0;

String rxBuf;

void setAllLeds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void sendJson(const char* json) {
  Serial.println(json);
  bleSerial.println(json);   // HM-10 skickar vidare över BLE till appen
}

void publishState() {
  unsigned long elapsed = 0;
  if (state == ARMED) elapsed = millis() - armedAtMs;
  if (state == DONE)  elapsed = doneAtMs - armedAtMs;
  const char* name = (state == IDLE) ? "idle" : (state == ARMED) ? "armed" : "done";
  char buf[96];
  snprintf(buf, sizeof(buf),
           "{\"type\":\"state\",\"state\":\"%s\",\"darkLed\":%d,\"elapsedMs\":%lu}",
           name, darkLed, elapsed);
  sendJson(buf);
}

void startGame() {
  if (state == ARMED) return;
  darkLed = random(NUM_LEDS);
  waitMs  = random(MIN_WAIT_MS, MAX_WAIT_MS + 1);
  setAllLeds(true);
  armedAtMs = millis();
  state = ARMED;
  Serial.printf("Start: lampa %d slocknar om %lu ms\n", darkLed + 1, waitMs);
  publishState();
}

void stopGame() {
  state = IDLE;
  darkLed = -1;
  setAllLeds(false);
  publishState();
}

void handleIncoming() {
  while (bleSerial.available()) {
    char c = (char)bleSerial.read();
    rxBuf += c;
    if (rxBuf.indexOf("\"start\"") >= 0) { startGame(); rxBuf = ""; }
    else if (rxBuf.indexOf("\"stop\"") >= 0) { stopGame(); rxBuf = ""; }
    if (rxBuf.length() > 64) rxBuf = "";   // skydda mot skräp
  }
}

// ------------------------- Setup & loop -------------------------------------
void setup() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(115200);
  bleSerial.begin(9600);   // HM-10 standardbaud

  // Slumpa enhetskod från brus på oansluten analog pin (A0)
  randomSeed(analogRead(A0));
  const char ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // utan I/L/O/0/1
  for (int i = 0; i < 6; i++) deviceId[i] = ALPHABET[random(32)];
  deviceId[6] = '\0';

  // Sätt BLE-namnet så appen kan söka efter "Nattduellen-XXXXXX"
  bleSerial.print("AT+NAMENattduellen-");
  bleSerial.println(deviceId);
  delay(800);                            // vänta på OK+Set...
  while (bleSerial.available()) bleSerial.read();  // töm svarsbufferten

  Serial.printf("Enhetskod: %s\n", deviceId);
  Serial.printf("HM-10 sänder nu som \"Nattduellen-%s\"\n", deviceId);

  publishState();
}

void loop() {
  handleIncoming();

  // När väntetiden gått ut: släck den valda dioden
  if (state == ARMED && (millis() - armedAtMs) >= waitMs) {
    digitalWrite(LED_PINS[darkLed], LOW);
    doneAtMs = millis();
    state = DONE;
    Serial.printf("Lampa %d slocknade efter %lu ms\n", darkLed + 1, doneAtMs - armedAtMs);
    publishState();
  }

  // Skicka läge regelbundet - då får appen state även direkt efter uppkoppling
  // (HM-10 har inget "retained"-läge som MQTT, så vi skickar löpande)
  if (millis() - lastStatePub > 500) {
    lastStatePub = millis();
    publishState();
  }

  delay(10);
}
