# 🌙 Nattduellen – bygginstruktion

Ett Robinson-inspirerat spel: fem lysdioder tänds, och efter en slumpmässig
tid slocknar en av dem. Spelet styrs (start/stopp) från en webbapp i
webbläsaren på telefon eller dator.

**Arkitektur:** ESP32-kortet kör både spelets logik och webbservern över
WiFi (eget nätverk). Lysdioderna kopplas till ESP32-kortet. Webbappen öppnas
i valfri webbläsare på en enhet som är ansluten till kortets WiFi.

```
Telefon/dator ──WiFi──▶ ESP32 (webbserver + spellogik) ──▶ 5 × lysdiod
   (webbapp)              (192.168.4.1)                      GPIO 25,26,27,32,33
```

---

## 1. Vilket kort ska du köpa?

**Rekommendation: ESP32 DevKit v1 (WROOM-32)** – ca 60–120 kr. Den
programmeras i Arduino IDE (så den kör "Arduino-kod"), har inbyggd WiFi och
Bluetooth, och är den mest väldokumenterade ESP32-varianten. Sök efter
"ESP32 DevKit v1" eller "ESP32-WROOM-32 development board" hos t.ex. Electrokit,
Kina-sajter eller Amazon.

| Alternativ | Pris ca | WiFi | Bluetooth | Kommentar |
|---|---|---|---|---|
| **ESP32 DevKit v1 (WROOM-32)** | 60–120 kr | ✅ | ✅ BLE + klassisk | **Rekommenderas.** Samma kod som här, GPIO 25/26/27/32/33. |
| Arduino Nano ESP32 | 250–300 kr | ✅ | ✅ | Officiellt Arduino-märke, kompakt. Samma kod fungerar, men pin-nummer skiljer – använd kortets pinout-tabell. |
| Raspberry Pi Pico W | ~80 kr | ✅ | ✅ BLE | Programmeras med MicroPython (eller Arduino-core). Billig, men mindre bibliotek. |
| Arduino Uno + HC-05/HC-06 | ~100 kr totalt | ❌ | ✅ | Webbläsaren kan inte prata klassisk Bluetooth-seriell; kräver Web Bluetooth (fungerar bara i Chrome/Edge). Krångligare. |

**Varför WiFi och inte Bluetooth?** En webbapp i webbläsaren kan inte
prata med en klassisk Bluetooth-seriellmodul (HC-05) – Web Bluetooth finns
men stöds bara i Chrome/Edge och kräver parkoppling. Med WiFi låter vi
ESP32:n skapa sitt eget nätverk och köra en webbserver: då ansluter du bara
telefonen till nätverket "Nattduellen" och öppnar `http://192.168.4.1`.
Ingen app behöver installeras.

---

## 2. Komponentlista (tillbehör)

| Komponent | Antal | Kommentar | Pris ca |
|---|---|---|---|
| ESP32 DevKit v1 (WROOM-32) | 1 | Huvudkortet | 60–120 kr |
| Lysdioder (valfria färger, 5 mm) | 5 | T.ex. 2 röda, 2 gröna, 1 gul | 10–20 kr för ett paket |
| Motstånd 220 Ω (eller 330 Ω) | 5 | Ett per lysdiod | några kr (köp ett motståndssortiment) |
| Kopplingsdäck (breadboard) | 1 | För testning | 30–50 kr |
| Kopplingskablar (hona/hane eller hane/hane) | ~10 | För kopplingsdäcket | 20–40 kr |
| USB-kabel (micro-USB eller USB-C, beroende på kort) | 1 | **Viktigt:** en riktig datakabel, inte bara laddkabel | medföljer ofta |
| USB-väggadapter 5 V, minst 1 A | 1 | T.ex. gammal mobilladdare – ger ström när spelet ska vara fristående | har du säkert hemma |
| Projektlåda + ev. strömbrytare (valfritt) | 1 | För slutmontering | 50–100 kr |

**Tips:** Vill du ha snyggare lampor i slutändan kan du limma fast
halvtransparenta pingisbollar eller bitar av akryl över dioderna som
diffusorer.

---

## 3. Koppling

Varje lysdiod: **anod (långa benet)** direkt till en GPIO-pin, **katod
(korta benet)** via ett 220 Ω-motstånd till **GND** (jord) på ESP32-kortet.

| Lysdiod | ESP32 GPIO | Motstånd | Till |
|---|---|---|---|
| Lampa 1 | GPIO 25 | 220 Ω | GND |
| Lampa 2 | GPIO 26 | 220 Ω | GND |
| Lampa 3 | GPIO 27 | 220 Ω | GND |
| Lampa 4 | GPIO 32 | 220 Ω | GND |
| Lampa 5 | GPIO 33 | 220 Ω | GND |

```
ESP32 DevKit          Breadboard
GPIO25 ─────────────►|──[220Ω]──┐
GPIO26 ─────────────►|──[220Ω]──┤   (▶| = lysdiod, långt ben mot GPIO)
GPIO27 ─────────────►|──[220Ω]──┼── alla till samma GND-rad
GPIO32 ─────────────►|──[220Ω]──┤
GPIO33 ─────────────►|──[220Ω]──┘
GND ──────────────────────────────┘
```

Se till att alla katoder hamnar på samma GND-rad på kopplingsdäcket, och
att du kopplar rätt håll på lysdioderna (långt ben = plus).

---

## 4. Steg för steg – från tomt kort till färdigt spel

### Steg 0: Installera Arduino IDE
1. Ladda ner **Arduino IDE 2.x** från https://www.arduino.cc/en/software och installera.
2. Öppna IDE:n. Gå till **Fil → Inställningar** och klistra in denna URL i
   fältet *Additional boards manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Gå till **Verktyg → Board → Boards Manager**, sök på `esp32`, installera
   paketet **"esp32" by Espressif Systems** (tar några minuter).
4. Välj bräda: **Verktyg → Board → ESP32 Arduino → ESP32 Dev Module**.
5. Anslut ESP32-kortet med USB-kabeln. Välj rätt port under
   **Verktyg → Port** (visas som t.ex. `COM3 (USB)`). Saknas porten?
   Installera drivrutin för CH340 eller CP2102 (vanliga USB-chip på
   billiga ESP32-kort) – kolla Enhetshanteraren efter gul markering.

### Steg 1: Testa att kortet svarar (Blink)
1. **Fil → Exempel → 01.Basics → Blink**, klicka på **Upload** (→).
2. Den lilla inbyggda lysdioden på kortet ska börja blinka. Fungerar det är
   IDE + drivrutin + kort OK.

### Steg 2: Testa en extern lysdiod
Koppla en lysdiod enligt tabellen ovan (GPIO 25). Skapa en ny sketch och
klistra in:
```cpp
const int LED = 25;
void setup() { pinMode(LED, OUTPUT); }
void loop() {
  digitalWrite(LED, HIGH); delay(500);
  digitalWrite(LED, LOW);  delay(500);
}
```
Ladda upp – dioden ska blinka. Fungerar inte den? Kontrollera diodens riktning och att motståndet sitter på katodsidan.

### Steg 3: Testa alla fem dioderna
Koppla in alla fem enligt tabellen. Testa med denna sekvens-sketch:
```cpp
const int LEDS[5] = {25, 26, 27, 32, 33};
void setup() {
  for (int i = 0; i < 5; i++) pinMode(LEDS[i], OUTPUT);
}
void loop() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LEDS[i], HIGH); delay(200);
    digitalWrite(LEDS[i], LOW);
  }
}
```
Lamporna ska tändas en i taget i följd. Gör de det är kopplingen klar.

### Steg 4: Flasha spelet
1. Öppna filen **`nattduellen.ino`** i Arduino IDE (Fil → Öppna, eller dubbelklicka på filen – IDE:n skapar då en mapp med samma namn; lägg filen i `nattduellen/nattduellen.ino` så är strukturen rätt).
2. Kontrollera att bräda (**ESP32 Dev Module**) och port är rätt.
3. Klicka **Upload**. Öppna **Verktyg → Serial Monitor** (115200 baud) – du ska se:
   ```
   Accesspunkt startad: Nattduellen
   Öppna i webbläsaren: http://192.168.4.1
   Webbservern är igång.
   ```

### Steg 5: Testa spelet från webbappen
1. På telefonen eller datorn: anslut till WiFi-nätverket **"Nattduellen"**
   (lösenord `nattduellen123` – ändra i koden om du vill).
2. Öppna webbläsaren och gå till **http://192.168.4.1**
   (eller **http://nattduellen.local** på enheter som stödjer det).
3. Tryck **▶ STARTA**: alla fem dioder tänds. Efter 3–10 sekunder (slumpmässigt)
   slocknar en slumpmässigt vald diod, och webbsidan visar vilken och efter hur
   lång tid.
4. Tryck **■ STOPPA**: alla dioder släcks och spelet nollställs. Starta igen
   så körs en ny runda. Upprepa några gånger och kontrollera att både tid och
   vald lampa varierar.

### Steg 6: Slutmontering
1. Skruva eller tejpa fast ESP32-kortet i en projektlåda.
2. Borra fem hål för lysdioderna (eller limma fast diffusorer över dem).
3. Koppla USB-kabeln från kortet till väggadaptern (minst 1 A) – då kör spelet
   fristående utan dator. Sätt ev. en strömbrytare på USB-kabeln.
4. Klart! Deltagarna ansluter till WiFi "Nattduellen" och öppnar webbsidan
   för att starta/stoppa duellen.

---

## 5. Felsökning

| Problem | Lösning |
|---|---|
| Ingen COM-port syns | Byt USB-kabel (vissa är bara laddkablar). Installera CH340/CP2102-drivrutin. |
| Upload failar med "Failed to connect" | Håll in **BOOT**-knappen på kortet när "Connecting…" visas, släpp när upload börjar. |
| Telefonen hittar inte WiFi "Nattduellen" | Vänta 5–10 s efter upload. Kontrollera Serial Monitor. Ändra `AP_SSID`/`AP_PASS` i koden vid behov. |
| Samma väntetid varje runda | Osannolikt – koden använder `esp_random()`. Starta om kortet om det ändå händer. |
| En diod lyser svagt/inte alls | Kontrollera riktning (långt ben mot GPIO), motstånd och att katoden når GND. |
| Webbappen laddar men knapparna gör inget | Kontrollera att du är ansluten till kortets WiFi (inte ditt eget). Testa att öppna `http://192.168.4.1/state` – ska ge JSON. |

---

## 6. Ändra beteendet i koden (`nattduellen.ino`)

| Vad du vill ändra | Var i koden |
|---|---|
| WiFi-namn / lösenord | `AP_SSID`, `AP_PASS` (minst 8 tecken) |
| Vilka pinnar dioderna sitter på | `LED_PINS[5] = {...}` |
| Hur länge lamporna lyser | `MIN_WAIT_MS` / `MAX_WAIT_MS` (millisekunder) |
| Ansluta till ditt eget WiFi i stället | Avkommentera STA-blocket i `setup()` |

---

## 7. Kodöversikt

- `setup()`: initierar pinnar, startar WiFi-accesspunkt + webbserver, registrerar `/start`, `/stop`, `/state` och webbsidan.
- `handleStart()`: väljer slumpmässig diod + slumpmässig väntetid (3–10 s), tänder alla dioder, sätter tillståndet `ARMED`.
- `loop()`: när väntetiden gått ut släcks den valda dioden, tillståndet blir `DONE`.
- `handleStop()`: släcker alla dioder, tillståndet blir `IDLE`.
- Webbappen poll:ar `/state` var 200:e ms och visar status + vilka lampor som lyser.

**Nästa steg (idéer):** summer (buzzer) när lampan slocknar, knappar för
reaktionstid per spelare, eller flera rundor med poängräkning i webbappen.

---

*Koden i `nattduellen.ino` är granskad men inte kompilerad på riktig hårdvara
från denna miljö – följ steg 1–3 ovan så verifierar du att allt fungerar innan
du flashar huvudsketchen.*
