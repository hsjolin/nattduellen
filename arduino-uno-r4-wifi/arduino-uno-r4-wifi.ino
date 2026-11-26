/*
  ============================================================================
  NATTDUELLEN - variant för Arduino UNO R4 WiFi (t.ex. Starter Kit R4)
  ----------------------------------------------------------------------------
  Samma funktion och samma BLE-API/webbapp som ESP32-varianten, men anpassad
  för UNO R4 WiFi:
    - BLE via ESP32-S3-coprocessorn och biblioteket ArduinoBLE
    - lysdioder på Arduino-pin D2..D6 (undvik D0/D1 = serieport, D13 = LED)
    - enhetskoden slumpas vid varje uppstart (ingen flash-lagring)
  ============================================================================
*/

#include <Arduino.h>
#include <ArduinoBLE.h>

// Nordic UART Service - samma UUIDer som webbappen använder
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // telefonen skriver hit
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // telefonen får notify här

// ------------------------- Konfiguration -----------------------------------
const int LED_PINS[5] = {2, 3, 4, 5, 6};   // UNO R4: D2-D6
const int NUM_LEDS    = 5;

const unsigned long MIN_WAIT_MS = 3000;   // minst 3 sekunder
const unsigned long MAX_WAIT_MS = 10000;  // högst 10 sekunder

// ------------------------- Enhet --------------------------------------------
char deviceId[7];
char deviceName[24];

// ------------------------- Speltillstånd -----------------------------------
enum State { IDLE, ARMED, DONE };
State state = IDLE;
int darkLed = -1;
unsigned long armedAtMs = 0;
unsigned long waitMs = 0;
unsigned long doneAtMs = 0;
unsigned long lastStatePub = 0;
bool helloPending = false;

BLEService nusService(NUS_SERVICE_UUID);
BLECharacteristic rxChar(NUS_RX_UUID, BLEWrite | BLEWriteWithoutResponse, 64);
BLECharacteristic txChar(NUS_TX_UUID, BLENotify, 128);

void setAllLeds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void sendJson(const char* json) {
  Serial.println(json);
  if (BLE.connected()) {
    txChar.writeValue((const uint8_t*)json, strlen(json));  // notifierar appen
  }
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

// ------------------------- BLE-callbacks ------------------------------------
void onRxWritten(BLEDevice central, BLECharacteristic ch) {
  String s;
  const uint8_t* v = ch.value();
  for (int i = 0; i < ch.valueLength(); i++) s += (char)v[i];
  if (s.indexOf("\"start\"") >= 0) startGame();
  else if (s.indexOf("\"stop\"") >= 0) stopGame();
}

void onConnect(BLEDevice central) { helloPending = true; }
void onDisconnect(BLEDevice central) { BLE.advertise(); }  // sänd igen efter frånkoppling

// ------------------------- Setup & loop -------------------------------------
void setup() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(115200);

  // Slumpa enhetskod från brus på en oansluten analog pin (A0)
  randomSeed(analogRead(A0));
  const char ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // utan I/L/O/0/1
  for (int i = 0; i < 6; i++) deviceId[i] = ALPHABET[random(32)];
  deviceId[6] = '\0';
  snprintf(deviceName, sizeof(deviceName), "Nattduellen-%s", deviceId);

  if (!BLE.begin()) {
    Serial.println("BLE kunde inte startas - kontrollera att kortet är UNO R4 WiFi");
    while (true) { delay(1000); }
  }

  BLE.setLocalName(deviceName);
  BLE.setAdvertisedService(nusService);
  nusService.addCharacteristic(rxChar);
  nusService.addCharacteristic(txChar);
  BLE.addService(nusService);
  rxChar.setEventHandler(BLEWritten, onRxWritten);
  BLE.setEventHandler(BLEConnected, onConnect);
  BLE.setEventHandler(BLEDisconnected, onDisconnect);
  BLE.advertise();

  Serial.printf("Enhetskod: %s\n", deviceId);
  Serial.printf("BLE sänder som \"%s\" - sök efter detta namn i webbappen\n", deviceName);
}

void loop() {
  BLE.poll();

  // Skicka hälsning + aktuellt läge så fort någon kopplar upp
  if (helloPending && BLE.connected()) {
    helloPending = false;
    char hello[48];
    snprintf(hello, sizeof(hello), "{\"type\":\"hello\",\"id\":\"%s\"}", deviceId);
    sendJson(hello);
    publishState();
  }

  // När väntetiden gått ut: släck den valda dioden
  if (state == ARMED && (millis() - armedAtMs) >= waitMs) {
    digitalWrite(LED_PINS[darkLed], LOW);
    doneAtMs = millis();
    state = DONE;
    Serial.printf("Lampa %d slocknade efter %lu ms\n", darkLed + 1, doneAtMs - armedAtMs);
    publishState();
  }

  // Skicka uppdaterat läge löpande medan spelet är igång (för timern i appen)
  if (state == ARMED && BLE.connected() && (millis() - lastStatePub) > 300) {
    lastStatePub = millis();
    publishState();
  }

  delay(10);
}
