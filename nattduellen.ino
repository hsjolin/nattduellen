/*
  ============================================================================
  NATTDUELLEN - fristående webbapp över internet (MQTT över TLS)
  ----------------------------------------------------------------------------
  Hårdvara : ESP32 DevKit (WROOM-32) + 5 lysdioder + 5 motstånd (220 ohm)
  Mjukvara : Arduino IDE 2.x + ESP32-kärnan + biblioteket PubSubClient
  Kommunikation: ESP32 ansluter till ditt WiFi och vidare ut till en publik
             MQTT-broker över TLS (broker.hivemq.com:8883). Webbappen
             (https://hsjolin.github.io/nattduellen/) pratar WSS mot samma
             broker. Ingen portöppning i router behövs.
  Enhet     : Vid första boot skapas en slumpmässig enhetskod (6 tecken) som
             sparas i flash och skrivs till Serial Monitor. Koden används i
             webbappen för att hitta/ansluta till just din enhet.
  Spelidé  : Alla 5 dioder tänds vid "start". Efter en slumpmässig tid
             (3-10 s) slocknar en slumpmässigt vald diod. "stop" nollställer.
  ============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>

// ------------------------- Konfiguration -----------------------------------
const char* WIFI_SSID = "ditt-wifi-namn";      // ditt eget WiFi (med internet)
const char* WIFI_PASS = "ditt-wifi-losenord";

const char* MQTT_HOST = "broker.hivemq.com";   // publik broker
const uint16_t MQTT_PORT = 8883;               // MQTT över TLS

// GPIO-pinnar för lysdioderna (säkra pinnar på ESP32 DevKit / WROOM-32)
const int LED_PINS[5] = {25, 26, 27, 32, 33};
const int NUM_LEDS    = 5;

// Slumpmässig väntetid innan en lampa slocknar (millisekunder)
const unsigned long MIN_WAIT_MS = 3000;
const unsigned long MAX_WAIT_MS = 10000;

// ------------------------- Enhet & MQTT-topics ------------------------------
char deviceId[7];          // 6 tecken + nullterminering
char topicCmd[48];         // nattduellen/<kod>/cmd     <- start/stop från appen
char topicState[48];       // nattduellen/<kod>/state   -> speltillstånd till appen
char topicOnline[48];      // nattduellen/<kod>/online  -> retained "1"/LWT "0"

WiFiClientSecure net;
PubSubClient mqtt(net);
Preferences prefs;

// ------------------------- Speltillstånd -----------------------------------
enum State { IDLE, ARMED, DONE };
State state = IDLE;
int darkLed = -1;
unsigned long armedAtMs = 0;
unsigned long waitMs = 0;
unsigned long doneAtMs = 0;
unsigned long lastStatePub = 0;

void setAllLeds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], on ? HIGH : LOW);
  }
}

void publishState() {
  unsigned long elapsed = 0;
  if (state == ARMED) elapsed = millis() - armedAtMs;
  if (state == DONE)  elapsed = doneAtMs - armedAtMs;
  const char* name = (state == IDLE) ? "idle" : (state == ARMED) ? "armed" : "done";
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"darkLed\":%d,\"elapsedMs\":%lu}",
           name, darkLed, elapsed);
  mqtt.publish(topicState, buf, true);   // retained så nya lyssnare ser läget direkt
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

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  char msg[16];
  unsigned int n = min(len, (unsigned int)(sizeof(msg) - 1));
  memcpy(msg, payload, n);
  msg[n] = '\0';
  if (strcmp(topic, topicCmd) == 0) {
    if (strcmp(msg, "start") == 0) startGame();
    else if (strcmp(msg, "stop") == 0) stopGame();
  }
}

bool mqttConnect() {
  String clientId = String("nattduellen-") + deviceId + "-" +
                    String(esp_random() & 0xFFFF, HEX);
  // Last Will: "0" på online-topic om anslutningen dör oväntat
  boolean ok = mqtt.connect(clientId.c_str(), topicOnline, 0, true, "0");
  if (ok) {
    mqtt.publish(topicOnline, "1", true);
    mqtt.subscribe(topicCmd);
    Serial.println("MQTT-broker ansluten");
  } else {
    Serial.printf("MQTT misslyckades, rc=%d\n", mqtt.state());
  }
  return ok;
}

// ------------------------- Setup & loop ------------------------------------
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

  snprintf(topicCmd,    sizeof(topicCmd),    "nattduellen/%s/cmd",    deviceId);
  snprintf(topicState,  sizeof(topicState),  "nattduellen/%s/state",  deviceId);
  snprintf(topicOnline, sizeof(topicOnline), "nattduellen/%s/online", deviceId);

  Serial.printf("Enhetskod: %s   (används i webbappen)\n", deviceId);

  // Anslut till WiFi (internet krävs för att nå brokern)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Ansluter till WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("WiFi ansluten, IP: "); Serial.println(WiFi.localIP());

  net.setInsecure();   // hoppar över certifikatvalidering (OK för hobbybruk)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
}

unsigned long lastReconnectAttempt = 0;

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }

  if (!mqtt.connected()) {
    if (millis() - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = millis();
      mqttConnect();
    }
  } else {
    mqtt.loop();

    // När väntetiden gått ut: släck den valda dioden
    if (state == ARMED && (millis() - armedAtMs) >= waitMs) {
      digitalWrite(LED_PINS[darkLed], LOW);
      doneAtMs = millis();
      state = DONE;
      Serial.printf("Lampa %d slocknade efter %lu ms\n",
                    darkLed + 1, doneAtMs - armedAtMs);
      publishState();
    }

    // Skicka uppdaterat läge löpande medan spelet är igång (för timern i appen)
    if (state == ARMED && (millis() - lastStatePub) > 300) {
      lastStatePub = millis();
      publishState();
    }
  }
}
