# 🌙 Nattduellen – portabelt spel med lokal Bluetooth-uppkoppling

Ett Mästarnas Mästare-inspirerat spel: fem lysdioder tänds, och efter en slumpmässig
tid slocknar en av dem. Du bär med dig hela spelet (ESP32 + dioder + USB-ström)
och styr det från en webbapp i telefonen — **utan WiFi, router eller internet
för själva uppkopplingen**.

## Arkitektur

```
  Webbapp (HTTPS, GitHub Pages)          ESP32 + 5 lysdioder + USB-ström
  https://hsjolin.github.io/nattduellen/      "Nattduellen-XK7Q2M"
        │                                            │
        └────── Bluetooth LE (Web Bluetooth) ───────┘
              lokalt, punkt-till-punkt, var som helst
```

- ESP32:n sänder ut sig över **Bluetooth LE** med namnet `Nattduellen-<kod>`
  (6-teckens kod som skapas vid första uppstart och sparas i flash).
- I webbappen trycker du **Sök & anslut** – telefonens inbyggda väljare visar
  närliggande Nattduellen-enheter och du väljer din.
- All kommunikation efter det går **lokalt mellan telefon och ESP32** över BLE
  (ett enkelt JSON-API, se nedan). Ingen WiFi behövs alls.

### BLE-API (Nordic UART Service)

| Riktning | Kanal | Innehåll |
|---|---|---|
| app → ESP32 | write till RX (`…0002`) | `{"cmd":"start"}` / `{"cmd":"stop"}` |
| ESP32 → app | notify på TX (`…0003`) | `{"type":"hello","id":"XK7Q2M"}` |
| ESP32 → app | notify på TX (`…0003`) | `{"type":"state","state":"idle\|armed\|done","darkLed":0-4,"elapsedMs":ms}` |

Service-UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`

---

## 1. Vilket kort ska du köpa?

**Rekommendation: ESP32 DevKit v1 (WROOM-32)** – ca 60–120 kr. Programmeras i
Arduino IDE och har inbyggd **Bluetooth LE** (BLE-biblioteket följer med
ESP32-kärnan, ingen extra bibliotek behövs). Sök efter "ESP32 DevKit v1" /
"ESP32-WROOM-32 development board".

| Alternativ | Pris ca | Kommentar |
|---|---|---|
| **ESP32 DevKit v1 (WROOM-32)** | 60–120 kr | **Rekommenderas om du köper bara kortet.** BLE inbyggt. GPIO 25/26/27/32/33 till dioderna. |
| **Arduino UNO R4 WiFi** (enskilt kort eller Starter Kit R4, Kjell p88445) | kort ~350–400 kr, kit ~1 400 kr | **Fungerar också!** BLE via ESP32-S3-coprocessorn (biblioteket ArduinoBLE). Kitet innehåller breadboard, kablar, motstånd, lysdioder, sensorer m.m. — men är betydligt dyrare än ESP32 + lösa delar. Se avsnitt 3b och kodvarianten i `arduino-uno-r4-wifi/`. |
| Arduino Nano ESP32 | 250–300 kr | Officiellt Arduino-märke, också BLE; andra pin-nummer. |
| Raspberry Pi Pico W | ~80 kr | Har BLE men Web Bluetooth mot Pico kräver annan firmware/profil. |

## 2. Komponentlista

| Komponent | Antal | Kommentar | Pris ca |
|---|---|---|---|
| ESP32 DevKit v1 (WROOM-32) | 1 | Huvudkortet | 60–120 kr |
| Lysdioder (5 mm, valfria färger) | 5 | | ~20 kr/paket |
| Motstånd 220 Ω | 5 | Ett per lysdiod | några kr |
| Kopplingsdäck (breadboard) | 1 | För testning | 30–50 kr |
| Kopplingskablar | ~10 | | 20–40 kr |
| USB-kabel (data, ej bara laddkabel) | 1 | Till datorn vid flashning | medföljer ofta |
| USB-väggadapter eller powerbank | 1 | Ström när spelet är portabelt | har du säkert |
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

## 3b. Väljer du Arduino UNO R4 WiFi (Starter Kit R4)?

UNO R4 WiFi **fungerar utmärkt** för detta projekt — kortet har Bluetooth LE
(inbyggt via en ESP32-S3-coprocessor) och webbappen är oförändrad. Skillnaderna
mot ESP32-varianten:

- **Kod:** använd `arduino-uno-r4-wifi/arduino-uno-r4-wifi.ino` i detta repo
  (samma BLE-API och JSON-protokoll, men skrivet mot biblioteket
  **ArduinoBLE** istället för ESP32:s BLEDevice).
- **Pinnar:** lysdioderna kopplas till Arduino-pin **D2–D6** (undvik D0/D1 som
  är serieporten och D13 som är inbyggd LED):

  | Lysdiod | UNO R4 pin | Motstånd | Till |
  |---|---|---|---|
  | Lampa 1 | D2 | 220 Ω | GND |
  | Lampa 2 | D3 | 220 Ω | GND |
  | Lampa 3 | D4 | 220 Ω | GND |
  | Lampa 4 | D5 | 220 Ω | GND |
  | Lampa 5 | D6 | 220 Ω | GND |

- **Arduino IDE:** installera board-paketet **"Arduino UNO R4 Boards"** (av
  Arduino), välj bräda **"Arduino UNO R4 WiFi"**, och installera biblioteket
  **"ArduinoBLE"** (senaste versionen) via Library Manager. Porten visas som
  USB-C-port (t.ex. COMx).
- **Enhetskod:** slumpas vid varje uppstart (RA4M1 har ingen enkel
  flash-lagring som ESP32) — läs den i Serial Monitor eller välj enheten i
  appens söklista.
- **Ström:** USB-C, samma som innan (powerbank/mobilladdare ≥1 A).
- Kitet innehåller redan breadboard, kablar, motstånd och lysdioder, så
  tillbehörslistan i avsnitt 2 är i princip täckt av kitet.

## 4. Steg för steg

### Steg 1: Publicera webbappen (HTTPS)
1. GitHub → repot → **Settings → Pages** → Source: **Deploy from a branch**,
   Branch **main**, Folder **/ (root)** → Save.
2. Appen nås sedan på **https://hsjolin.github.io/nattduellen/** (HTTPS krävs
   för Web Bluetooth).
3. Tips för "var som helst": öppna appen en gång när du har internet — en
   service worker (`sw.js`) cachar den, så appen fungerar även helt offline
   (eller "Lägg till på hemskärmen" för en riktig app-ikon).

### Steg 2: Förbered Arduino IDE
1. Ladda ner **Arduino IDE 2.x** (arduino.cc).
2. **Fil → Inställningar** → *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Verktyg → Board → Boards Manager** → sök `esp32` → installera
   **"esp32" by Espressif Systems**.
4. Välj **Verktyg → Board → ESP32 Dev Module**, anslut kortet via USB och välj
   rätt **Verktyg → Port** (saknas porten? installera CH340/CP2102-drivrutin).

### Steg 3: Testa kortet
1. **Fil → Exempel → 01.Basics → Blink** → ladda upp → inbyggda dioden blinkar.
2. Koppla en extern diod till GPIO 25 och kör:
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

### Steg 4: Flasha spelet
1. Öppna `nattduellen.ino`, välj bräda + port, ladda upp.
   **Inga WiFi-uppgifter behövs längre** – BLE kräver ingen konfiguration.
2. Öppna **Serial Monitor (115200 baud)**:
   ```
   Enhetskod: XK7Q2M
   BLE sänder som "Nattduellen-XK7Q2M" - sök efter detta namn i webbappen
   ```

### Steg 5: Använd från telefonen (var som helst)
1. Ge spelet ström (USB-adapter/powerbank).
2. Öppna **https://hsjolin.github.io/nattduellen/** i **Chrome eller Edge**
   på Android-telefonen (eller på datorn).
3. Tryck **🔍 Sök & anslut** och välj din `Nattduellen-XXXXXX` i listan.
4. Tryck **▶ STARTA** – alla fem dioder tänds. Efter 3–10 s (slumpmässigt)
   slocknar en slumpmässigt vald diod; appen visar vilken och efter hur lång tid.
5. **■ STOPPA** släcker alla dioder och nollställer. Nya rundor startas med STARTA.

### Steg 6: Slutmontering
1. Skruva fast ESP32-kortet i en projektlåda, montera dioderna i fronten
   (ev. pingisbollar/akryl som diffusorer).
2. Ström via powerbank eller USB-adapter — klart att bära med.

## 5. Viktiga begränsningar att känna till

- **Web Bluetooth kräver Chrome eller Edge** (Android, Windows, macOS,
  ChromeOS). **iPhone/iPad (Safari) stöds inte av Apple.** Vill du använda en
  iPhone: använd istället lokalläget i commit `f93eb6a` (ESP32 som eget
  WiFi-AP + webbserver på `http://192.168.4.1`).
- Telefonen behöver internet **en gång** för att ladda appen från GitHub
  Pages; efter det är den cache:ad och fungerar offline (service worker).
- Räckvidden är Bluetooth-räckvidden (~10 m), vilket passar ett spel på bordet.

## 6. Felsökning

| Problem | Lösning |
|---|---|
| "Web Bluetooth stöds inte" | Byt till Chrome/Edge. Kontrollera att sidan öppnas över **https** (ej http). |
| Enheten syns inte i söklistan | Se till att ESP32:n har ström; stå nära; tryck på sök igen. På Android: kontrollera att Bluetooth är på och att appen fått BT-behörighet (plats/Bluetooth i Android-inställningar). |
| Anslutningen bryts | ESP32 börjar sända igen automatiskt efter frånkoppling — tryck "Sök & anslut" på nytt. |
| Upload failar "Failed to connect" | Håll in **BOOT**-knappen vid "Connecting…". |
| Ingen COM-port | Byt kabel (måste vara datakabel); installera CH340/CP2102-drivrutin. |

## 7. Ändra beteendet

| Vad | Var |
|---|---|
| Diodernas pinnar | `LED_PINS[5]` i `nattduellen.ino` |
| Väntetid innan släckning | `MIN_WAIT_MS` / `MAX_WAIT_MS` i `nattduellen.ino` |
| BLE-namn | `deviceName` i `setup()` (prefix måste matcha `namePrefix` i `index.html`) |
| Appens utseende/logik | `index.html` (vanlig HTML/JS, Web Bluetooth) |

## 8. Vill du kunna styra över internet också?

Se commit `a359c8f`: då låg webbappen på GitHub Pages och pratade MQTT över
TLS via en publik broker — då krävs internet på båda sidor, men ingen lokal
kanal. BLE-varianten i denna version är helt lokal och portabel.

---

*Koden är skriven för Arduino IDE 2.x + ESP32-kärnan och Web Bluetooth
(Chrome/Edge), men är inte kompilerad/testad på riktig hårdvara från denna
miljö — följ steg 3–5 för att verifiera.*
