# 🌙 Nattduellen – fristående webbapp över internet (HTTPS)

Ett Robinson-inspirerat spel: fem lysdioder tänds, och efter en slumpmässig
tid slocknar en av dem. Spelet styrs från en **fristående webbapp** som körs
på internet (GitHub Pages över HTTPS) — appen söker efter din enhet, kopplar
upp sig och kommunicerar med den över MQTT.

## Arkitektur

```
 Webbapp (statisk, GitHub Pages)          ESP32 + 5 lysdioder
 https://hsjolin.github.io/nattduellen/    hemma bakom din router
        │                                         │
        │ WSS (8884)                              │ MQTT/TLS (8883)
        └──────────▶ MQTT-broker ◀────────────────┘
              broker.hivemq.com   (publik, TLS)
```

- **ESP32:n kopplar utåt** till en publik MQTT-broker över TLS. Därför
  fungerar det bakom vilken router/NAT som helst — ingen portöppning,
  ingen fast IP behövs.
- **Webbappen** är en helt statisk HTML/JS-fil (`index.html`) som hostas
  gratis på **GitHub Pages** med automatisk HTTPS.
- **Sök/anslut:** ESP32:n publicerar `nattduellen/<kod>/online` (retained).
  Appen kan söka på `nattduellen/+/online` och lista enheter som är online,
  eller så anger du enhetskoden (6 tecken) direkt.
- **Kommunikation:** appen publicerar `start`/`stop` på
  `nattduellen/<kod>/cmd`; ESP32:n publicerar sitt tillstånd som JSON på
  `nattduellen/<kod>/state`.

### MQTT-topics

| Topic | Riktning | Innehåll |
|---|---|---|
| `nattduellen/<kod>/cmd` | app → ESP32 | `start` eller `stop` |
| `nattduellen/<kod>/state` | ESP32 → app | `{"state":"idle\|armed\|done","darkLed":0-4,"elapsedMs":ms}` (retained) |
| `nattduellen/<kod>/online` | ESP32 → app | `1` (retained), LWT `0` vid frånkoppling |

---

## 1. Vilket kort ska du köpa?

**Rekommendation: ESP32 DevKit v1 (WROOM-32)** – ca 60–120 kr. Programmeras i
Arduino IDE, har inbyggd WiFi och Bluetooth. Sök efter "ESP32 DevKit v1" /
"ESP32-WROOM-32 development board".

| Alternativ | Pris ca | Kommentar |
|---|---|---|
| **ESP32 DevKit v1 (WROOM-32)** | 60–120 kr | **Rekommenderas.** GPIO 25/26/27/32/33 för dioderna. |
| Arduino Nano ESP32 | 250–300 kr | Officiellt Arduino-märke; samma kod men andra pin-nummer. |
| Raspberry Pi Pico W | ~80 kr | Fungerar med MicroPython + MQTT-bibliotek, men mindre väldokumenterat. |

## 2. Komponentlista

| Komponent | Antal | Kommentar | Pris ca |
|---|---|---|---|
| ESP32 DevKit v1 (WROOM-32) | 1 | Huvudkortet | 60–120 kr |
| Lysdioder (5 mm, valfria färger) | 5 | | ~20 kr/paket |
| Motstånd 220 Ω | 5 | Ett per lysdiod | några kr |
| Kopplingsdäck (breadboard) | 1 | För testning | 30–50 kr |
| Kopplingskablar | ~10 | | 20–40 kr |
| USB-kabel (data, ej bara laddkabel) | 1 | micro-USB/USB-C beroende på kort | medföljer ofta |
| USB-väggadapter 5 V ≥ 1 A | 1 | Fristående drift | har du säkert hemma |
| Projektlåda (valfritt) | 1 | Slutmontering | 50–100 kr |

## 3. Koppling

Lysdiodens **anod (långa benet)** till GPIO, **katod (korta benet)** via
220 Ω-motstånd till **GND**:

| Lysdiod | ESP32 GPIO | Motstånd | Till |
|---|---|---|---|
| Lampa 1 | GPIO 25 | 220 Ω | GND |
| Lampa 2 | GPIO 26 | 220 Ω | GND |
| Lampa 3 | GPIO 27 | 220 Ω | GND |
| Lampa 4 | GPIO 32 | 220 Ω | GND |
| Lampa 5 | GPIO 33 | 220 Ω | GND |

## 4. Steg för steg

### Steg 1: Publicera webbappen på internet (HTTPS)
1. Gå till repot på GitHub → **Settings → Pages**.
2. Under *Build and deployment*: Source = **Deploy from a branch**,
   Branch = **main**, Folder = **/ (root)** → **Save**.
3. Efter någon minut är appen nåbar på:
   **https://hsjolin.github.io/nattduellen/** (HTTPS automatiskt).

### Steg 2: Förbered Arduino IDE
1. Ladda ner **Arduino IDE 2.x** (arduino.cc) och installera.
2. **Fil → Inställningar** → *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Verktyg → Board → Boards Manager** → sök `esp32` → installera
   **"esp32" by Espressif Systems**.
4. **Verktyg → Board** → välj **ESP32 Dev Module**. Anslut kortet via USB och
   välj rätt port under **Verktyg → Port** (saknas porten? installera
   CH340/CP2102-drivrutin).
5. **Sketch → Include Library → Manage Libraries** → sök `PubSubClient`
   → installera (av Nick O'Leary).

### Steg 3: Testa kortet
1. **Fil → Exempel → 01.Basics → Blink** → ladda upp → inbyggda dioden blinkar.
2. Koppla en extern diod till GPIO 25 enligt tabellen och kör:
   ```cpp
   const int LED = 25;
   void setup() { pinMode(LED, OUTPUT); }
   void loop() { digitalWrite(LED, HIGH); delay(500); digitalWrite(LED, LOW); delay(500); }
   ```
3. Koppla alla fem dioder och kör sekvensen:
   ```cpp
   const int LEDS[5] = {25, 26, 27, 32, 33};
   void setup() { for (int i = 0; i < 5; i++) pinMode(LEDS[i], OUTPUT); }
   void loop() {
     for (int i = 0; i < 5; i++) { digitalWrite(LEDS[i], HIGH); delay(200); digitalWrite(LEDS[i], LOW); }
   }
   ```

### Steg 4: Konfigurera och flasha spelet
1. Öppna `nattduellen.ino` i Arduino IDE.
2. Ändra längst upp i koden:
   ```cpp
   const char* WIFI_SSID = "ditt-wifi-namn";
   const char* WIFI_PASS = "ditt-wifi-losenord";
   ```
3. Ladda upp. Öppna **Verktyg → Serial Monitor (115200 baud)**. Du ska se:
   ```
   Enhetskod: XK7Q2M   (används i webbappen)
   WiFi ansluten, IP: 192.168.x.x
   MQTT-broker ansluten
   ```
   **Spara enhetskoden** — den är konstant mellan omstarter.

### Steg 5: Använd webbappen
1. Öppna **https://hsjolin.github.io/nattduellen/** på telefon eller dator.
2. Klicka **🔍 Sök enheter** – efter några sekunder listas enheter som är
   online (klicka på din kod), eller skriv in koden manuellt → **Anslut**.
3. Tryck **▶ STARTA** – alla fem dioder tänds. Efter 3–10 s (slumpmässigt)
   slocknar en slumpmässigt vald diod; appen visar vilken och efter hur lång tid.
4. **■ STOPPA** släcker alla dioder och nollställer. Starta nya rundor hur
   många gånger som helst.

### Steg 6: Slutmontering
1. Skruva fast ESP32-kortet i en projektlåda, montera dioderna i fronten
   (ev. med pingisbollar/akryl som diffusorer).
2. Ström via USB-väggadapter (≥1 A) — då kör spelet helt fristående; det enda
   som behövs är WiFi. Användaren öppnar webbappen och söker/ansluter.

## 5. Säkerhet (viktigt att känna till)

Publika MQTT-brokern har ingen inloggning — **enhetskoden är hemligheten**.
Alla som känner till koden (6 tecken) kan styra spelet. För ett spel hemma är
det okej. Vill du låsa ner det på riktigt:

- Kör en **egen broker** (t.ex. Mosquitto på en Raspberry Pi eller Azure-VM)
  med TLS + användarnamn/lösenord, och ändra `MQTT_HOST`/`MQTT_PORT` i koden
  och `BROKER_URL` i `index.html`.
- Eller sätt en hemlig delad nyckel i topic-namnet.

## 6. Felsökning

| Problem | Lösning |
|---|---|
| "WiFi ansluten" men aldrig "MQTT-broker ansluten" | Kontrollera stavning av WiFi-uppgifter; vissa nät (företagsnät/gästnät) blockerar port 8883 – testa telefonens hotspot. |
| Enheten syns inte i sökningen | Vänta 10 s efter uppstart; kontrollera att ESP32:n är online (Serial Monitor); brandväggar kan blockera 8883. |
| Appen hittar enheten men inget händer vid STARTA | Kontrollera att du anslutit till rätt kod; kolla `nattduellen/<kod>/cmd` i en MQTT-explorer (t.ex. MQTT Explorer). |
| Upload failar "Failed to connect" | Håll in **BOOT**-knappen vid "Connecting…". |
| Ingen COM-port | Byt kabel (måste vara datakabel); installera CH340/CP2102-drivrutin. |

## 7. Ändra beteendet

| Vad | Var |
|---|---|
| WiFi-uppgifter | `WIFI_SSID` / `WIFI_PASS` i `nattduellen.ino` |
| Broker | `MQTT_HOST` / `MQTT_PORT` i `.ino`, `BROKER_URL` i `index.html` |
| Diodernas pinnar | `LED_PINS[5]` i `.ino` |
| Väntetid innan släckning | `MIN_WAIT_MS` / `MAX_WAIT_MS` i `.ino` |
| Appens utseende/logik | `index.html` (vanlig HTML/JS) |

## 8. Vill du köra helt lokalt utan internet?

Se commit `f93eb6a` i detta repo: då körde ESP32:n eget WiFi-AP och webbserver
(`http://192.168.4.1`) utan extern broker — samma spel, men bara i lokala
nätverket.

---

*Koden är skriven för Arduino IDE 2.x + ESP32-kärnan och MQTT.js v5, men är
inte kompilerad/testad på riktig hårdvara från denna miljö — följ steg 3–5
för att verifiera.*
