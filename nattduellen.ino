/*
  ============================================================================
  NATTDUELLEN - lokal uppkoppling via Bluetooth LE (BLE)
  ----------------------------------------------------------------------------
  Hårdvara : ESP32 DevKit (WROOM-32) + 5 lysdioder + 5 motstånd (220 ohm)
  Mjukvara : Arduino IDE 2.x + ESP32-kärnan (BLE-biblioteket är inbyggt)
  Kommunikation: Webbappen (HTTPS, t.ex. GitHub Pages) öppnas i Chrome/Edge
  på telefonen, söker efter enheten "Nattduellen-XXXXXX" via Web Bluetooth
  och kopplar upp över BLE. All trafik går lokalt telefon <-> ESP32 -
  ingen WiFi, router eller internet behövs för själva spelet.

  BLE-API (Nordic UART Service, enkel text/JSON-kanal):
    Telefon skriver till RX:   {"cmd":"start"} eller {"cmd":"stop"}
    ESP32 notifierar på TX:   {"type":"hello","id":"XK7Q2M"}
                              {"type":"state","state":"idle|armed|done",
                               "darkLed":0-4,"elapsedMs":ms}
  ============================================================================
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include <Preferences.h>

// Nordic UART Service (NUS) - standard-UUIDer för enkel BLE-seriekanal
#define NUS_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // telefonen skriver hit
#define NUS_TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // telefonen får notify här

// ------------------------- Konfiguration -----------------------------------
// GPIO-pinnar för lysdioderna (säkra pinnar på ESP32 DevKit / WROOM-32)
const int LED_PINS[5] = {25, 26, 27, 32, 33};
const int NUM_LEDS    = 5;

// Slumpmässig väntetid innan en lampa slocknar (millisekunder)
const unsigned long MIN_WAIT_MS = 3000;
const unsigned long MAX_WAIT_MS = 10000;

// ------------------------- Enhet --------------------------------------------
char deviceId[7];        // 6 tecken + null (sparas i flash)
char deviceName[24];     // BLE-namn, t.ex. "Nattduellen-XK7Q2M"

Preferences prefs;

// ------------------------- Speltillstånd -----------------------------------
enum State { IDLE, ARMED, DONE };
State state = IDLE;
int darkLed = -1;
unsigned long armedAtMs = 0;
unsigned long waitMs = 0;
unsigned long doneAtMs = 0;
unsigned long lastStatePub = 0;

BLEServer* bleServer = nullptr;
BLECharacteristic* txChar = nullptr;
volatile bool deviceConnected = false;
volatile bool helloPending = false;

void setAllLeds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void sendJson(const char* json) {
  Serial.println(json);
  if (deviceConnected && txChar) {
    txChar->setValue((const uint8_t*)json, strlen(json));
    txChar->notify();
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
class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string v = c->getValue();
    if (v.find("\"start\"") != std::string::npos) startGame();
    else if (v.find("\"stop\"") != std::string::npos) stopGame();
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    deviceConnected = true;
    helloPending = true;
  }
  void onDisconnect(BLEServer* s) override {
    deviceConnected = false;
    s->startAdvertising();   // börja sända igen så appen kan koppla upp på nytt
  }
};

void setupBLE() {
  BLEDevice::init(deviceName);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = bleServer->createService(NUS_SERVICE_UUID);

  txChar = svc->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());   // krävs för att klienten ska få notify

  BLECharacteristic* rxChar = svc->createCharacteristic(
      NUS_RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->setScanResponse(true);   // namnet ligger i scan response (får ej plats i annonsen)
  BLEDevice::startAdvertising();
  Serial.printf("BLE sänder som \"%s\" - sök efter detta namn i webbappen\n", deviceName);
}

// ------------------------- Setup & loop -------------------------------------
void setup() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  Serial.begin(115200);
  delay(300);
  randomSeed(esp_random());

  // Hämta eller skapa enhetskod (sparas i flash, överlever omstart)
  prefs.begin("nattduellen", false);
  String saved = prefs.getString("id", "");
  if (saved.length() == 6) {
    saved.toCharArray(deviceId, sizeof(deviceId));
  } else {
    const char ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"; // utan I/L/O/0/1
    for (int i = 0; i < 6; i++) deviceId[i] = ALPHABET[esp_random() % 32];
    deviceId[6] = '\0';
    prefs.putString("id", deviceId);
  }
  prefs.end();

  snprintf(deviceName, sizeof(deviceName), "Nattduellen-%s", deviceId);
  Serial.printf("Enhetskod: %s\n", deviceId);

  setupBLE();
}

void loop() {
  // Skicka hälsning + aktuellt läge så fort någon kopplar upp
  if (helloPending && deviceConnected) {
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
  if (state == ARMED && deviceConnected && (millis() - lastStatePub) > 300) {
    lastStatePub = millis();
    publishState();
  }

  delay(10);
}
