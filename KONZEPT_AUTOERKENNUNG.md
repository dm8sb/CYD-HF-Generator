# Konzept: Automatische Hardware-Erkennung

Stand: 2026-10-28 · Basis: Commit `10f1a34` (Firmware 1.2.0)

## Eine Firmware für beide CYD-Varianten: Display-Controller zur Laufzeit erkennen

### Ziel

- Es gibt nur noch eine Firmware und eine PlatformIO-Umgebung für beide Boards: den Original-CYD mit ILI9341 und den CYD2USB mit ST7789. Der Nutzer muss beim Flashen keine Variante mehr wählen.
- Auf dem Original-CYD bleibt die Init-Byte-Folge genau wie in 1.2.0. Der Start dauert dort höchstens etwa 20 ms länger.
- Erkennt die Firmware den Controller nicht sicher, kommt der Nutzer ohne PC weiter: mit einer Geste beim Einschalten, die in jedem Bildzustand funktioniert.
- Die Lösung bleibt klein: TFT_eSPI bleibt, es kommt keine neue Bibliothek dazu.

### Ausgangslage

- Der Controller wird heute über zwei Umgebungen gewählt: [platformio.ini](platformio.ini) mit `[env:esp32-2432S028R]` (`ILI9341_DRIVER`) und `[env:esp32-2432S028R-usbc]` (`ST7789_DRIVER`, `TFT_RGB_ORDER=TFT_BGR`, `TFT_INVERSION_OFF`). Wer die falsche Variante flasht, sieht nur Rauschen ([README.md:87](README.md#L87)).
- TFT_eSPI ist nicht exakt gepinnt (`^2.5.43`). Der Treiber wird beim Kompilieren festgelegt. Die treiberabhängigen Teile sind:
  - die Init-Folge,
  - `setRotation` (MADCTL),
  - `readPixel`/`readRect`.
  - Alles andere (CASET, RASET, RAMWR, COLMOD 0x55) ist bei beiden Controllern gleich.
- Der SPI-Modus hängt am Treiber: MODE3 nur bei ST7789, sonst MODE0 (`TFT_eSPI.h:131-136`).
- `Gui` besitzt `TFT_eSPI tft_` ([gui.h:99](src/gui/gui.h#L99)). Die Init steht in [gui.cpp:19-21](src/gui/gui.cpp#L19-L21), der Flush in [gui.cpp:50-56](src/gui/gui.cpp#L50-L56). `StatusLed::begin()` wird erst in [gui.cpp:16](src/gui/gui.cpp#L16) aufgerufen.
- RST des Displays hängt an EN (`TFT_RST=-1`). Nach Panic, WDT oder Brownout bekommt das Panel keinen Hardware-Reset. `init()` sendet dann nur SWRST und wartet 150 ms (`TFT_eSPI.cpp:686-695`).
- Auf dem CYD2USB landet die ILI9341-Folge bei jedem Start auf dem ST7789. Dort bedeuten zwei ihrer Befehle etwas anderes:
  - `C1 10` ist beim ST7789 IDSET ([ILI9341_Init.h:48](.pio/libdeps/esp32-2432S028R/TFT_eSPI/TFT_Drivers/ILI9341_Init.h), `ST7789_Defines.h:150`).
  - `C0 23` ist beim ST7789 LCMCTRL, die Bits XMV, XBGR und XGS werden dann per XOR umgekehrt.
- Der Ansatz in todo.md Abschnitt 1 (P3: `0xD3` → `0x9341`) braucht den undokumentierten 0xD9-Index-Trick aus `readcommand8` (`TFT_eSPI.cpp:1081-1094`). Dieser setzt 0xD9 danach nicht zurück und ist für die Unterscheidung nicht nötig. Er entfällt.
- `SPIClass::begin` kehrt sofort zurück, wenn der Bus schon läuft. Deshalb kann man `TFT_eSPI::getSPIinstance()` (HSPI) **vor** `tft_.init()` mit denselben Pins starten und damit lesen.
- Die MADCTL-Werte je Rotation sind bestätigt: ILI9341 `{48, 28, 88, E8}`, ST7789 (BGR) `{08, 68, C8, A8}`. Es gilt immer XOR 0x40, aber nur mit LCMCTRL = 0x0C aus der ST7789-Init.
- NVS: `Storage::getU8/putU8` ([storage.cpp:23-24](src/services/storage.cpp#L23-L24)), Namespace `hfgen`. Der Fake `Preferences` zählt die Schreibzugriffe (`writeCount`).

### Erkennungsverfahren

`DisplayDetect::probe(PanelBus&)` läuft **vor** `tft_.init()`. Eigenschaften:

- Hardware-HSPI mit 1 MHz, MSB first, im selben Modus wie `TFT_SPI_MODE` (Standard MODE0, siehe I0).
- CS15 und DC2 werden von Hand gesteuert.
- Jeder Zugriff wird einzeln mit `beginTransaction`/`endTransaction` geklammert. Sonst blockiert später der Parameter-Mutex des SPI-Treibers in `tft_.init()`.
- Es wird kein 0xD9 und kein 0xD3 gesendet.

Ablauf:

1. **SWRESET:** `01`, danach 10 ms warten. Laut beiden Datenblättern reichen 5 ms vor dem nächsten Befehl. Die 120 ms gelten nur vor SLPOUT, und das sendet erst `init()`. Das Panel ist danach in einem bekannten Zustand, auch nach einem Warmstart.
2. **Prüfen, ob der Rückkanal funktioniert:**
   - `36 A8` schreiben, dann `0B` mit 3 Bytes lesen → `ra`.
   - `36 54` schreiben, dann `0B` mit 3 Bytes lesen → `rb`.
   - `x = ra ^ rb` bilden. Bestanden, wenn für eine Verschiebung s ∈ {0, 1, 8, 9} gilt: `((x << s) >> 16) & 0xFC == 0xFC`.
   - Warum XOR: Die XOR-Wirkung von LCMCTRL auf dem ST7789 ist in beiden Lesungen gleich und hebt sich auf. Eine schwebende oder feste Leitung ergibt x = 0 und fällt durch.
   - Die erlaubten Verschiebungen decken Dummy-Bit und Dummy-Byte ab, die im 4-Draht-SPI ohne 0xD9 nicht belegt sind.
   - Scheitert die Prüfung, folgt sofort Schritt 5. Weitere Lesebefehle gehen dann nicht an einen unbekannten Chip.
3. **ID lesen:** `04` (RDDID) zweimal mit je 4 Bytes lesen. Beide Lesungen müssen gleich sein, sonst ist das Ergebnis `None`.
4. **Einordnen:**

   | Bedingung | Ergebnis |
   |---|---|
   | Rückkanal fehlt oder Lesungen ungleich | `None` |
   | RDDID enthält `85 52` (ID2/ID3) bei Verschiebung 0–9 | `ST7789` |
   | Rückkanal ok, keine ST-Signatur | `ILI9341` (plausibel, nicht bewiesen) |

   - Die Erkennung prüft nur ID2/ID3, nicht ID1. Ein IDSET aus einem früheren Lauf (`C1 10` ändert nur ID1) kann sie deshalb nicht verfälschen. Ob SWRESET IDSET zurücksetzt, spielt dann keine Rolle.
   - Liefert I0 auf dem ILI9341 eine eindeutige RDDID, kommt eine positive ILI-Regel dazu. Ein lesbarer, aber fremder Controller ergibt dann `Unknown`, das wie `None` behandelt wird.
5. **Aufräumen:** `36 00` schreiben, CS auf high. Die endgültige Konfiguration schreibt danach `tft_.init()`.

Der gesamte Lauf dauert etwa 15 ms. Die Logzeile enthält immer die Rohwerte, zum Beispiel `DISP 0B=a8..^54.. s=1 04=10855200 -> ST7789; stored=0 -> ST7789 [detected]`. Damit lässt sich später ein drittes Panel ergänzen, und es braucht keinen eigenen Verbose-Schalter.

### Architektur und betroffene Dateien

**Neue Dateien:**

```
src/drivers/display_detect.{h,cpp}  rein (<stdint.h>), host-testbar: PanelBus, probe, decide,
                                    ST7789_INIT[], postInit, MADCTL-Tabellen, name()
src/drivers/display_panel.{h,cpp}   nur ESP32: besitzt TFT_eSPI und SpiPanelBus
test/test_display_detect/           Host-Tests
test/helpers/panel_model.h          Panel-Modell analog zu si5351_model.h
```

**`display_detect.h`:**

```cpp
enum class PanelType : uint8_t { Unknown = 0, ILI9341 = 1, ST7789 = 2 };   // = NVS-Wert "disp_panel"
enum class PanelSource : uint8_t { Build, Detected, Stored, Probe, Default };

class PanelBus {                       // Fake auf dem Host
public:
    virtual ~PanelBus() {}
    virtual void write(uint8_t cmd, const uint8_t* d, uint8_t n) = 0;
    virtual void read(uint8_t cmd, uint8_t* buf, uint8_t n) = 0;    // Rohbytes direkt nach dem Befehl
    virtual void delayMs(uint32_t ms) = 0;
};
struct ProbeResult  { bool readbackOk; uint8_t madA[3], madB[3], rddid[4]; PanelType panel; };
struct PanelDecision { PanelType panel; PanelSource source; bool gestureApplied; bool save; PanelType saveValue; };

namespace DisplayDetect {
    ProbeResult   probe(PanelBus& bus);
    PanelDecision decide(const ProbeResult& p, PanelType stored, bool gesture, uint8_t buildForce);
    void          postInit(PanelBus& bus, PanelType p);    // ILI9341: nichts; ST7789: 01, 120 ms, ST7789_INIT
    struct InitCmd { uint8_t cmd, len; uint8_t data[14]; uint16_t delayMs; };
    extern const InitCmd ST7789_INIT[]; extern const uint8_t ST7789_INIT_LEN;
    constexpr uint8_t MADCTL_ILI9341[4] = {0x48, 0x28, 0x88, 0xE8};
    constexpr uint8_t MADCTL_ST7789[4]  = {0x08, 0x68, 0xC8, 0xA8};
    const char* name(PanelType t);
}
```

- `ST7789_INIT` übernimmt die Byte-Folge aus [ST7789_Init.h](.pio/libdeps/esp32-2432S028R/TFT_eSPI/TFT_Drivers/ST7789_Init.h) von 2.5.43 mit drei Änderungen:
  - ohne `fillScreen(TFT_RED)`;
  - INVOFF statt INVON (entspricht `TFT_INVERSION_OFF`);
  - MADCTL 0x08.
- Wartezeiten: 120 ms nach SLPOUT, 10 ms nach COLMOD, 120 ms vor DISPON. Die 120 ms nach DISPON entfallen, sie sind nicht vorgeschrieben.

**`display_panel.h`:**

```cpp
class DisplayPanel {
public:
    PanelDecision begin(PanelType stored, bool gesture);   // probe, decide, applyPanel; scheitert nie
    void applyPanel(PanelType p);    // tft_.init(); postInit(bus_, p); setRotation(1); fillScreen(BLACK)
    void setRotation(uint8_t r);     // tft_.setRotation(r); bei ST7789 zusätzlich 36 MADCTL_ST7789[r]
    void flush(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* px);
    PanelType panel() const;
    const ProbeResult& probe() const;
private:
    class SpiPanelBus;               // getSPIinstance().begin(14,12,13,-1), SPISettings(1 MHz, MSBFIRST, TFT_SPI_MODE)
    TFT_eSPI tft_; SpiPanelBus* bus_; PanelType panel_ = PanelType::ILI9341; ProbeResult probe_ {};
};
```

- In `display_panel.cpp` stellen `static_assert`s sicher, dass `PIN_TFT_*` aus `pins.h` und die `TFT_*`-Build-Flags gleich sind.
- Hinweis im Header: `setRotation`, `readPixel` und `readRect` nie direkt auf `tft_` aufrufen. TFT_eSPI hält das Panel immer für einen ILI9341.

**Änderungen an bestehenden Dateien:**

| Datei | Änderung |
|---|---|
| [gui.h](src/gui/gui.h) / [gui.cpp](src/gui/gui.cpp) | `TFT_eSPI tft_` → `DisplayPanel* display_`. Neue Signatur `begin(AppController&, TouchDriver&, DisplayPanel&)`. Die Zeilen [gui.cpp:16](src/gui/gui.cpp#L16) und [gui.cpp:19-21](src/gui/gui.cpp#L19-L21) entfallen. `flushCb_` ruft `display_->flush(...)` auf. `gui.h` bindet `TFT_eSPI.h` nicht mehr ein. |
| [touch.h](src/drivers/touch.h) / [touch.cpp](src/drivers/touch.cpp) | Neu: `bool pressed()` (= `touch_.touched()`, das schon Z ≥ 400 prüft). |
| [settings.h](src/services/settings.h) / [settings.cpp](src/services/settings.cpp) | Neu: `PanelType loadDisplayPanel()` (ungültig → `Unknown`) und `bool saveDisplayPanel(PanelType)`. Schreibt sofort und nur bei Änderung, außerhalb von `PersistentState` und Autosave. |
| [pins.h](include/pins.h) | Neu: `PIN_TFT_MOSI=13, MISO=12, SCLK=14, CS=15, DC=2`. Der Kommentar „one environment per display controller“ entfällt. |
| [main.cpp](src/main.cpp) | Neuer Startablauf, siehe unten. Neues Objekt `static DisplayPanel display`. |
| [platformio.ini](platformio.ini) | `bodmer/TFT_eSPI@2.5.43` exakt (erledigt den P3-Punkt in Abschnitt 1). Eine Umgebung mit `-DILI9341_DRIVER=1` in `[esp32]`. Die `usbc`-Umgebung wird entfernt. `-DDISPLAY_FORCE_PANEL=0\|1\|2` als Notausgang für Entwickler. `-DTFT_SPI_MODE=SPI_MODE3` nur, wenn I0 es verlangt. In `[native_test] build_src_filter` kommt `+<drivers/display_detect.cpp>` dazu. |

Bewusst **nicht** in diesem Umbau: Die Font-Flags (`LOAD_FONT2/6/7/8`, `SMOOTH_FONT`) und `SPI_READ_FREQUENCY` werden höchstens in einem eigenen Aufräum-Commit angefasst.

### Startablauf

1. Logger
2. I2C
3. `backlight.begin()` (dunkel)
4. **`StatusLed::begin()`**, vorgezogen aus [gui.cpp:16](src/gui/gui.cpp#L16)
5. Settings laden, dazu **`stored = settings.loadDisplayPanel()`**
6. SI5351 (unverändert)
7. `touch.begin`
8. **Boot-Geste prüfen:**
   - Ist der Touch beim Start gedrückt, 1,5 s lang alle 20 ms abtasten.
   - Sind mindestens 90 % der Abtastungen gedrückt, ist `gesture = true`.
9. **`decision = display.begin(stored, gesture)`:** `probe`, dann `decide`, dann `applyPanel`.
10. Ist `decision.save` gesetzt, `settings.saveDisplayPanel(decision.saveValue)` aufrufen. Ein normaler Start schreibt nichts in den Flash.
11. **Rückmeldung, wenn eine Geste erkannt wurde:**
    - Geste übernommen: Backlight blinkt zweimal, LED leuchtet weiß.
    - Geste ignoriert (ST7789 sicher erkannt): Backlight blinkt einmal.
    - Danach bis zu 10 s warten, bis der Finger losgelassen ist. Sonst erhält LVGL den Druck als ersten Tipp.
12. Logzeile ausgeben, dann `gui.begin(app, touch, display)`, `lv_refr_now`, Backlight auf den gespeicherten Wert, wie bisher.

Die Bootzeit geht mit dunklem Backlight in die Messwerte ein:

| Board | Wie lange | Grund |
|---|---|---|
| Original-CYD | etwa +15 ms | nur die Erkennung |
| CYD2USB | etwa +0,1 s (550 → 650 ms) | Erkennung 15 ms, dazu `init()` mit ILI-Folge 270 ms, SWRESET 120 ms und eigene Tabelle 250 ms. Dafür entfallen `fillScreen` und die 120 ms nach DISPON. |

### Rückfallebenen und Bedienung

`decide()`: Die erste zutreffende Regel gilt.

| # | Bedingung | Panel | Boot-Geste | Speichern |
|---|---|---|---|---|
| 1 | `DISPLAY_FORCE_PANEL` = 1 oder 2 | gemäß Flag | ignoriert | nein |
| 2 | Erkennung `ST7789` (Signatur) | ST7789 | ignoriert (einmal blinken) | `disp_panel=ST7789`, wenn abweichend |
| 3 | `stored` gesetzt | `stored` | schaltet um | ja, siehe unten |
| 4 | Erkennung `ILI9341` (Rückkanal ok) | ILI9341 | schaltet um | ja |
| 5 | `None` | ILI9341 (Standard) | schaltet um | ja |

Die Boot-Geste schaltet auf das jeweils andere Panel um:

- Neues Panel = Ergebnis der Erkennung: gespeichert wird `Unknown` (0), also wieder automatisch.
- Sonst wird das neue Panel gespeichert.

Ergebnis: Sobald die Erkennung funktioniert, kann die Geste den Nutzer nie aussperren. Eine falsche Wahl macht eine zweite Geste rückgängig. Ein dauerhaft falsch gespeicherter Wert wie ein „verwaistes“ `disp_mode` kann nicht entstehen.

Für das Handbuch (DE/EN):

> Zeigt das Display nach dem Einschalten nur Rauschen oder ein gespiegeltes Bild: Display berühren, Gerät einschalten und gedrückt halten, bis die Beleuchtung zweimal blinkt. Dann loslassen.

Die Bedienung hängt nicht von der Touch-Position ab, also auch nicht von der Touch-Ausrichtung.

**Ausbaustufe Prüfbild**, nur wenn I0 auf einem Board ein unzuverlässiges Lesen zeigt:

- **Wann:** Beim ersten Start mit `None` und leerem NVS erscheint ein TFT_eSPI-Prüfbild ohne LVGL. Es startet mit ILI9341.
- **Ziele:** zwei benachbarte Ziele nacheinander, oben rechts und dann unten rechts. Begründung: Mit dem falschen Panel ist das Bild nicht nur waagerecht gespiegelt (MADCTL XOR 0x40). Auf dem ST7789 setzt die ILI-Folge LCMCTRL auf 0x23, also XMV und XGS. Dabei treten Transposition und vertikale Spiegelung tatsächlich auf.
- **Eingaben:**
  - Das Prüfbild nimmt Eingaben erst an, wenn der Finger mindestens 300 ms losgelassen war.
  - Ein Tipp zählt nur, wenn er neu beginnt und stabil ist (3 Abtastungen innerhalb von 30 px). Ausgewertet wird beim Loslassen.
  - Ein Tipp neben das Ziel wechselt zum anderen Panel.
- **Zeitablauf:**
  - Je Kandidat 10 s, insgesamt 2 Runden (40 s).
  - Danach Start mit dem Ergebnis der Erkennung; es wird nichts gespeichert.
- **Aufwand:** etwa +0,5 Tage.

### Persistenz

- Ein Schlüssel `disp_panel` (U8) im Namespace `hfgen`: 0 = automatisch, 1 = ILI9341, 2 = ST7789.
- Der Standardwert 0 ist kompatibel zu NVS-Daten aus 1.2.0.
- Geschrieben wird nur in zwei Fällen: bei der ST7789-Signatur mit abweichendem gespeichertem Wert und nach einer übernommenen Geste. Jeweils sofort und nur bei Änderung.
- Ein Werksreset (todo Abschnitt 3) löscht den Wert mit. Danach erkennt die Firmware wieder automatisch.
- Die Info-Zeile (todo Abschnitt 3) zeigt zum Beispiel „ST7789 · detected (85 52)“ oder „ILI9341 · stored“.

### Tests

**Host-Tests** (`test/test_display_detect`):

Das Panel-Modell `test/helpers/panel_model.h` bildet ab:

- ST7789 mit RDDID `85 85 52`;
- IDSET ändert ID1 und bleibt über SWRESET erhalten (pessimistischer Fall);
- LCMCTRL-XOR auf 0x0B;
- ILI9341 mit RDDID gemäß I0;
- Dummy-Bits 0, 1, 8 und 9;
- schwebende Leitung (0xFF/0x00), fest auf 0x00 oder 0xFF;
- gestörte Lesungen, die zwischen den Läufen verschieden ausfallen.

| # | Prüfung |
|---|---|
| T1 | `probe`: Der erste Befehl ist `01`, danach mindestens 5 ms. Es werden nur `01`, `36`, `0B` und `04` gesendet, nie `D9` oder `D3`. Am Ende steht `36 00`. Scheitert der Rücklesetest, folgt kein `04`. |
| T2 | Erkennung: ST7789 mit und ohne IDSET-Fehler, mit LCMCTRL 0x23 und 0x2C und allen Verschiebungen → `ST7789`. ILI9341 → `ILI9341`. Schwebend, fest und gestört → `None`. |
| T3 | `decide`: volle Matrix aus Build-Flag × Erkennung × `stored` × Geste. Gespeichert wird nur bei Signatur mit Änderung oder bei Geste. Die Geste speichert 0, wenn das Ziel dem Ergebnis der Erkennung entspricht. |
| T4 | Tabellen: `MADCTL_ST7789[r] == MADCTL_ILI9341[r] ^ 0x40`. `ST7789_INIT` entspricht der festen Byte-Liste aus 2.5.43, enthält LCMCTRL 0x0C, INVOFF, COLMOD 0x55, CASET 0..239 und RASET 0..319. SLPOUT steht mindestens 120 ms vor DISPON. Keiner der Befehle 0x01, 0x21, 0xD9, 0xEC, 0xFA, 0xFC oder 0xFE kommt vor. |
| T5 | `postInit`: ILI9341 → keine Schreibzugriffe (Regressionsschutz für den Original-CYD). ST7789 → `01`, mindestens 120 ms, dann die Tabelle. |
| T6 | Settings (`writeCount`): Rundlauf von `disp_panel`. Ein ungültiger Wert ergibt 0. Gleicher Wert → kein Schreibzugriff. Standardwert 0. |

**Hardware-Testfälle**, neue Gruppe in todo.md Abschnitt 10. Die Gruppe D ist schon vergeben, deshalb Gruppe I. A6 und A7 werden an die Einheitsfirmware angepasst.

### I. Display-Erkennung (eine Firmware, beide Boards)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | I0 | **Vor der Umsetzung:** Diagnose-Sketch (etwa 50 Zeilen, HSPI 1 MHz) auf beiden Boards, jeweils MODE0 und MODE3. Gelesen werden roh `0B` (nach `36 A8`/`54`) und `04`, mit Pull-up, mit Pull-down und ohne Pull an GPIO12. Fälle: Kaltstart; ILI-Folge, dann SWRESET, dann Lesen; nach `esp_restart`. Zusätzlich den `usbc`-Build mit `-DTFT_SPI_MODE=SPI_MODE0` flashen. | Rücklesen klappt auf beiden Boards. Notieren: Verschiebung, RDDID des ILI9341 (positive Signatur?), ID1 nach IDSET und SWRESET, ob MODE0 auf dem ST7789 ein korrektes Bild gibt | |
| [ ] | I1 | Original-CYD einschalten | `ILI9341 [detected]`. Bild, Farben, Ausrichtung und Touch-Ecken wie 1.2.0. Bootzeit höchstens +0,05 s | |
| [ ] | I2 | CYD2USB einschalten | `ST7789 [detected]`. Bild und **Touch-Ecken** wie im 1.2.0-`usbc`-Build. Bootzeit höchstens +0,3 s | |
| [ ] | I3 | Je Board 20× Kaltstart, 20× EN-Taste und 20× Warmstart (Build `-DDISPLAY_TEST_WARMSTART=20`: `esp_restart` mit RTC-Zähler) | 60/60 richtig. Im Log beim Warmstart auf dem CYD2USB: ID1 ≠ 0x85 schadet nicht | |
| [ ] | I4 | NVS löschen, auf beiden Boards starten | richtige Erkennung. `disp_panel` ist auf dem CYD2USB danach 2, auf dem Original-CYD bleibt es 0 | |
| [ ] | I5 | CYD2USB mit `-DDISPLAY_FORCE_PANEL=1` (simuliert `None` und falsches Panel), danach normale Firmware mit leerem NVS und gestörter Erkennung (Build-Hook) | Rauschen. Die Geste schaltet auf ST7789 um und blinkt 2×; der nächste Start kommt ohne Geste aus | |
| [ ] | I6 | Boot-Geste auf beiden Boards bei funktionierender Erkennung | CYD2USB: 1× blinken, Panel bleibt. Original-CYD: Umschalten auf ST7789 (Rauschen); eine zweite Geste stellt ILI9341 wieder her, und `disp_panel` ist dann 0 | |
| [ ] | I7 | Geste länger als 3 s halten, dann loslassen | kein Tipp landet in der GUI | |
| [ ] | I8 | CYD2USB, 30 min GUI-Betrieb im gewählten SPI-Modus | keine Pixelfehler | |
| [ ] | I9 | Testplan A und B sowie Messplan F6 mit der Einheitsfirmware | unverändert | |

### Risiken und offene Punkte

| Risiko | Gegenmaßnahme |
|---|---|
| ST7789 im MODE0 statt heute MODE3 | I0 prüft das vorab mit dem `usbc`-Build in MODE0. Beide Controller übernehmen Daten an der steigenden Flanke, und CS ist vorhanden. Klappt es nicht: `TFT_SPI_MODE=SPI_MODE3` für beide Boards und I1 wiederholen. |
| MISO am CYD2USB nicht verdrahtet | Ergebnis `None`. Der erste Start zeigt Rauschen, nach einer Geste ist der Wert dauerhaft gespeichert. Optional die Ausbaustufe Prüfbild. |
| 3-Draht-Panel (Interface I): Der Controller treibt beim Lesen SDA (GPIO13) gegen MOSI | Nur wenn I0 das zeigt: GPIO13 für die Lesephase vom SPI-Ausgang lösen und HSPI-MISO per `pinMatrixInAttach(13, HSPIQ_IN_IDX, false)` auf GPIO13 legen. Kein Bit-Bang. |
| Dummy-Bits bei `0B`/`04` im 4-Draht-SPI ohne 0xD9 nicht belegt | Die Verschiebungen 0, 1, 8 und 9 sind erlaubt. I0 protokolliert die Rohwerte. |
| LCMCTRL-XOR verfälscht die MADCTL-Rücklesung auf dem ST7789 | Der XOR-Vergleich zweier Lesungen hebt den Effekt auf (T2). |
| IDSET aus der ILI-Folge bleibt über SWRESET erhalten | Die Erkennung nutzt nur ID2/ID3 (T2, I3). |
| Die ILI-Folge erreicht bei jedem Start den ST7789 | Wie heute beim falschen Build. Sie enthält keinen NVM-Befehl des ST7789 (EC/FA/FC/FE). Das Backlight ist dunkel, danach folgen SWRESET und die eigene Tabelle. |
| Die ST7789-Tabelle erreicht den ILI9341 (nur per Geste oder Flag) | `D0 A4 A1` ist beim ILI9341 NVMWR. Ohne den Schlüssel `D1 55 AA 66` sollte es wirkungslos sein; das stammt aus dem Gedächtnis und ist **am Datenblatt zu prüfen**. Ein Hardware-Reset (EN) stellt alles andere wieder her. |
| Ein fremder, lesbarer Controller wird als ILI9341 eingestuft | Die Geste hilft, die Rohwerte stehen im Log. Positive ILI-Regel nach I0. |
| Varianten mit gleicher ID, aber anderer Inversion oder RGB/BGR-Reihenfolge | Werden nicht erkannt und nicht unterstützt. Im README so dokumentieren. |
| Der SPI-Mutex hängt, wenn `SpiPanelBus` eine Transaktion offen lässt | Jeder Zugriff ist einzeln geklammert; vor `tft_.init()` ist nichts mehr offen. |
| Ein TFT_eSPI-Update ändert Init-Dateien oder Interna | Version exakt gepinnt; T4 hält die ST7789-Tabelle fest. |
| Die Status-LED sitzt auf der Rückseite | Rückmeldung zur Geste vor allem über das Backlight-Blinken; die LED nur zusätzlich. |

### Umsetzungsschritte

| # | Schritt | Tage |
|---|---|---|
| 1 | I0: Diagnose-Sketch auf beiden Boards, dazu der `usbc`-Build mit MODE0. Danach die offenen Werte festlegen: SPI-Modus, Verschiebung, ILI-Signatur, Rückkanal am CYD2USB. | 0,25 |
| 2 | `display_detect` (`probe`, `decide`, `postInit`, Tabellen) mit Panel-Modell und T1–T5 | 0,5 |
| 3 | `DisplayPanel`/`SpiPanelBus`, Umbau von gui, main, touch und pins, TFT_eSPI pinnen. Die `usbc`-Umgebung bleibt vorübergehend als `-DDISPLAY_FORCE_PANEL=2`. Tests I1–I4. | 0,35 |
| 4 | Settings-Schlüssel (T6), Boot-Geste, Rückmeldung, Warten auf Loslassen, Build-Hooks. Tests I5–I7. | 0,2 |
| 5 | `usbc`-Umgebung entfernen. Doku: README-Tabelle und Hinweis „noise“, USER_MANUAL_DE/EN (Geste), CLAUDE.md, todo.md (Abschnitt 1 P3 erledigt, A6/A7 anpassen, Gruppe I). Tests I8–I9. | 0,3 |
| | **Summe** | **etwa 1,6** |
| opt. | Ausbaustufe Prüfbild mit zwei Zielen (nur wenn I0 ein unzuverlässiges Lesen zeigt) | +0,5 |

Gegenüber dem ersten Entwurf entfallen:

- der 0xD9-/0xD3-Weg;
- die Pull-Diagnose in der Firmware (sie steht nur im I0-Sketch);
- der zweite Probe-Lauf mit SWRESET;
- die Stufen Strong/Weak;
- `disp_mode` neben `disp_last`;
- die Schalter `PROBE_VERBOSE` und `PROBE_SIMULATE`;
- die festen Build-Flags `TFT_SPI_MODE` und `SPI_READ_FREQUENCY`;
- das Prüfbild als Pflicht.

---

## Konzept: Automatische SDA/SCL-Erkennung (Kabel beliebig gepolt)

> **Stand 2026-10-28: umgesetzt** (Spezifikation 42, Hardwaretests todo.md 10 J). Abweichungen vom Konzept:
> - `foreignOn_` wird nach jeder erfolgreichen Verbindung gelöscht. Sonst bliebe nach einem Schein-ACK (schwache Pull-ups) die richtige Belegung nach dem Umstecken dauerhaft als „fremdes Gerät“ gesperrt.
> - Der Fehlertyp heißt `SI5351Manager::Error` (verschachtelt wie `Pll`), nicht `Si5351Error`.
> - `Gui::dismissMessages_()` ist privat und wird nur in `Gui::update()` beim Wechsel auf „bereit“ aufgerufen.
> - `AppController::i2cSwapped()` entfällt; die Info-Zeile kann `I2CBus::map()` direkt lesen.
> - Hardwaretests als Gruppe J statt I (I gehört zur Display-Erkennung).
> - Nach dem Code-Review ergänzt: Fehlertyp `NoReadback` getrennt von `NotSi5351`. Ein nur per Lesen abgelehntes Gerät wird jeden Zyklus erneut nur gelesen und bei bestandener Prüfung initialisiert (SI5351 im Hochlauf). 400 kHz werden nach der Initialisierung geprüft, sonst bleibt der Bus bei 100 kHz. Die GUI schließt Verbindungsdialoge anhand von `connectionCount()` und `isLinkText()`, Ausgabefehler-Dialoge bleiben. Busfreigabe mit internem Pull-up. Ein fehlgeschlagenes `Wire.end()` gilt als geschlossener Bus.

### Ziel

- Das SI5351-Modul an CN1 funktioniert unabhängig davon, welche Ader auf GPIO22 und welche auf GPIO27 liegt. Es gibt keine Build-Option, keinen Eingriff in `pins.h` und keinen Dialog, wenn die Adern vertauscht sind.
- Erkannt wird beim Start und beim Wiederverbinden während des Betriebs (Hot-Plug, Umstecken). Solange der SI5351 bereit ist, ändern sich die Pins nie.
- Ein Dialog erscheint nur bei echten Fehlern, deren Text die Ursache nennt und den Hinweis „SDA/SCL beliebig“ enthält. Nach dem automatischen Wiederverbinden schließt sich der Dialog von selbst.
- Ein fremdes Gerät auf 0x60 wird nicht programmiert (höchstens ein Byte, siehe unten).

### Ausgangslage

- **Feste Rollen:** [pins.h:17-19](include/pins.h#L17-L19) legt `PIN_I2C_SDA = 22` und `PIN_I2C_SCL = 27` fest. Der Kommentar sagt: „swap these back“, wer nach der Original-Spezifikation verdrahtet.
- **Bus-Init:** [i2c_bus.cpp:9](src/hal/i2c_bus.cpp#L9) ruft `Wire.begin` mit den festen Pins auf. Der Scan in [i2c_bus.cpp:14-23](src/hal/i2c_bus.cpp#L14-L23) läuft bei jedem Start, auch nach einem fehlgeschlagenen `begin`. In dem Fall hängt der Boot bei der zweiten Adresse (Lock-Leck, siehe unten).
- **Treiber:**
  - `probe_()` akzeptiert jedes ACK auf 0x60 ([si5351_manager.cpp:364-367](src/drivers/si5351_manager.cpp#L364-L367)).
  - Die SYS_INIT-Schleife scheitert nie ([si5351_manager.cpp:139-143](src/drivers/si5351_manager.cpp#L139-L143)).
  - `ok &= writeReg_(…)` schreibt nach einem Fehler weiter ([si5351_manager.cpp:145-148](src/drivers/si5351_manager.cpp#L145-L148)). Das sind 11 Transfers, bei Timeouts bis ≈ 220 ms.
  - Die Primitive ([si5351_manager.cpp:364-387](src/drivers/si5351_manager.cpp#L364-L387)) prüfen nicht, ob der Bus offen ist.
- **Überwachung:** Im Zustand „nicht bereit“ ruft [app_controller.cpp:42](src/core/app_controller.cpp#L42) alle 2 s `si5351_.begin()` auf (`HW_CHECK_INTERVAL_MS`, [constants.h:36](include/constants.h#L36)). Dort setzt die Erkennung an.
- **Boot:** [main.cpp:47](src/main.cpp#L47) ruft `si5351.begin()` direkt auf. Der Text in [main.cpp:61-62](src/main.cpp#L61-L62) lautet „Check SDA/SCL pins“.
- **Arduino-ESP32 2.0.16** (`framework-arduinoespressif32@3.20016.0`), geprüft:
  - **Lock-Leck nach `end()` oder fehlgeschlagenem `begin()`:**
    - `beginTransmission` nimmt den Lock (Wire.cpp:421).
    - `endTransmission` gibt bei `txBuffer == NULL` 4 zurück, ohne den Lock freizugeben (Wire.cpp:447-450). Dasselbe gilt für `requestFrom` (Wire.cpp:480-482).
    - Der nächste `beginTransmission` oder `end()` blockiert dann für immer.
  - **Vor dem ersten `begin()`:** Der Lock ist NULL, `end()` ist ein No-op (Wire.cpp:329).
  - **`begin()` auf einem laufenden Bus:** Er ignoriert neue Pins (`log_w`, return true; Wire.cpp:300-303).
  - **`i2cInit`:** Er schaltet die internen Pull-ups ein und loggt `log_i` (esp32-hal-i2c.c:75-82). Der Hardware-Timeout beträgt 0xFFFFF APB-Takte ≈ 13,1 ms.
  - **`i2cSetClock`:** Er ändert den Takt am laufenden Bus ohne Re-Init (esp32-hal-i2c.c:247 ff.).
  - **Logging:** Ein Adress-NACK in `i2cWrite` erzeugt keine Logzeile. Ein fehlgeschlagenes `requestFrom` loggt `log_e` (Wire.cpp:498-500).
- **Physik der Vertauschung:**
  - Der ESP liest das ACK auf dem Pin, der am SCL-Eingang des Chips hängt. Diesen treibt der Chip nie, das Ergebnis ist also immer NACK.
  - Der Chip sieht nur START/STOP und wenige Flanken, also nie einen gültigen Adressrahmen.
  - Bei vertauschten Adern kann der Chip also weder antworten noch falsch programmiert werden.
- **Tests:**
  - `TwoWire::begin` im Fake ignoriert die Pins, `end()` fehlt ([Wire.h:89](test/fakes/Wire.h#L89)).
  - `setUp` in [test_si5351](test/test_si5351/test_main.cpp#L23-L26) und `Device` in [test_app_controller](test/test_app_controller/test_main.cpp#L40) rufen `si.begin()` ohne geöffneten Bus auf.
  - Der `[native_test]`-Filter enthält `hal/` nicht ([platformio.ini:75-82](platformio.ini#L75-L82)).
  - `CORE_DEBUG_LEVEL=3` ([platformio.ini:23](platformio.ini#L23)).

### Erkennungsverfahren

Der Ablauf steckt in `AppController::connect_()` und läuft beim Start sowie in jedem „nicht bereit“-Zyklus der Überwachung.

1. **Reihenfolge:** zuerst die gerade offene Belegung (beim Start `Default` = SDA 22 / SCL 27), dann die andere. Nach einem kurzen Aussetzer liegt der Bus noch auf der funktionierenden Belegung. Es gibt also kein `Wire.end()`, und die Belegung kippt nicht hin und her.
2. **`I2CBus::open(m)`:**
   - Sind die Pins schon so gesetzt, wird nur der Takt auf `I2C_DETECT_HZ` (100 kHz) gesetzt.
   - Sonst `Wire.end()`, dann `Wire.begin(sda, scl, 100 kHz)` und `setTimeOut(20)`.
   - Scheitert `begin`, wird diese Belegung übersprungen, und `Wire` wird nicht mehr angefasst.
3. **`I2CBus::probe(0x60)`** ist ein reiner Adress-Write:
   - `5` (Timeout): Eine Leitung hängt. Die Belegung wird beim ersten Timeout abgebrochen.
   - `2` (NACK): weiter mit der nächsten Belegung. Ist für diese Belegung ein fremdes Gerät vermerkt, wird der Vermerk gelöscht (Gerät entfernt).
   - `0` (ACK): Ist für diese Belegung ein fremdes Gerät vermerkt, wird `begin()` übersprungen. Das Gerät wird also nicht alle 2 s neu beschrieben, und es entstehen keine Core-Logzeilen. Sonst folgt Schritt 4.
4. **`si5351_.begin(corr)`** mit Identitätsprüfung (siehe Architektur):
   - Bei Erfolg `I2CBus::setRunClock()` (400 kHz) und `LinkState::Connected`.
   - Bei einem Fehler wird die **andere Belegung trotzdem versucht**. Damit bleibt die Erkennung nicht hängen, falls über schwache Pull-ups ein falsches ACK auf der falschen Belegung entsteht.
   - Bei `NotSi5351` wird das fremde Gerät für diese Belegung vermerkt.
5. **Ergebnis ohne Erfolg:**
   - fremdes Gerät gesehen → `UnknownDevice`
   - sonst Timeout gesehen → `Blocked`
   - sonst → `NotFound`
   - Der Bus bleibt auf der zuletzt versuchten Belegung offen. Solange kein Modul angeschlossen ist, wechselt die Startbelegung also von Zyklus zu Zyklus. Das ist funktional egal und kostet ein Re-Init pro Zyklus.

Es gibt keine „Ghost-Probe“ auf 0x7F. Eine dauerhaft LOW liegende SDA führt am ESP32 beim ersten 1-Bit zum Arbitrierungsverlust, eine LOW liegende SCL zum 13,1-ms-Hardware-Timeout. Beides kommt als `5` zurück, nicht als ACK (IDF 4.4, Quelle vorkompiliert, Prüfung über J6). Ein „alles ACKt“-Gerät fängt die Identitätsprüfung ab.

**Blockierzeit pro „nicht bereit“-Zyklus** (blockiert `loop()` und LVGL):

| Fall | Dauer |
|---|---|
| Verbindung gesund | 0, die Erkennung läuft nicht |
| Kein Modul | ≈ 1–2 ms (1 Re-Init, 2 Probes à 100 kHz) |
| Vertauscht und vorhanden | ≈ 2 ms + `begin()`, direkt nach dem Einschalten bis zu 20 ms SYS_INIT-Wartezeit |
| Leitung LOW (beide Belegungen Timeout) | ≤ 2 × 20 ms = 40 ms alle 2 s |
| Schlechtester Fall (ACK, dann Timeouts in `begin()`, andere Belegung Timeout) | ≤ 20 (SYS_INIT) + 2 × 20 + 20 ≈ 80 ms alle 2 s, weil `begin()` beim ersten Fehler abbricht |

### Architektur & betroffene Dateien

**[include/pins.h](include/pins.h)**: `PIN_I2C_SDA` und `PIN_I2C_SCL` entfallen, weil die Namen eine feste Rolle suggerieren. Ebenso entfällt der „swap back“-Kommentar.
```cpp
// SI5351 on CN1. Firmware detects which pin is SDA; the cable may be swapped.
static constexpr int PIN_I2C_A = 22;              // tried first as SDA (verified wiring)
static constexpr int PIN_I2C_B = 27;
static constexpr uint32_t I2C_DETECT_HZ = 100000; // detection tolerates weak pull-ups
static constexpr uint32_t I2C_FREQ_HZ = 400000;   // after successful init
static constexpr uint16_t I2C_TIMEOUT_MS = 20;    // > 13.1 ms ESP32 HW timeout
// GPIO21 (backlight, also on P3) is never an I2C candidate.
```

**[src/hal/i2c_bus.h/.cpp](src/hal/i2c_bus.cpp)**: Neu geschrieben, etwa 90 Zeilen. Die Datei baut auch gegen die Fakes, deshalb `Logger` statt `Serial.printf`.
```cpp
enum class I2CMap : uint8_t { Default, Swapped };   // Default: SDA=PIN_I2C_A, SCL=PIN_I2C_B
namespace I2CBus {
bool begin();                 // recoverLines_() if a line reads LOW, open_=false, open(Default); scan only with LOG_DEBUG
bool open(I2CMap m);          // same pins: setClock(I2C_DETECT_HZ); else end()+begin(sda,scl,I2C_DETECT_HZ)+setTimeOut
void setRunClock();           // Wire.setClock(I2C_FREQ_HZ)
bool isOpen();
I2CMap map();
int sdaPin(); int sclPin();
uint8_t probe(uint8_t addr);  // 0 ACK, 2 NACK, 5 timeout; 4 WITHOUT touching Wire if closed
}
```
- `open()` setzt `open_` aus dem Rückgabewert von `Wire.begin`. `begin()` ruft `Wire.end()` nur einmal auf, und zwar über `open()`.
- **Freigabe hängender Leitungen nur beim Start** (`recoverLines_()`, etwa 30 Zeilen, ≈ 0,2 ms).
  - Wann: nur wenn GPIO22 oder GPIO27 vor `Wire.begin` LOW liest.
  - Ablauf: Ein Pin wird als SCL angenommen und erhält 9 Open-Drain-Takte, danach ein STOP. Anschließend dasselbe mit dem anderen Pin.
  - Warum harmlos: Takte auf der echten SDA erzeugen für den Chip nur START/STOP.
  - Anlass: Wird der ESP während eines Lesezugriffs zurückgesetzt (EN-Taste, Upload mit Auto-Reset, Brownout), bleibt der SI5351 an 3,3 V und hält SDA. Er hat keinen I2C-Timeout.
  - Zur Laufzeit genügt die Bus-Freigabe der IDF beim Timeout. Sie taktet den gerade als SCL konfigurierten Pin, und weil jeder Zyklus beide Belegungen versucht, trifft sie auch den richtigen.

**[src/drivers/si5351_manager.h/.cpp](src/drivers/si5351_manager.cpp)**
- `static constexpr uint8_t I2C_ADDR = 0x60;` wandert in den Header und ersetzt `SI5351_ADDR` ([si5351_manager.cpp:11](src/drivers/si5351_manager.cpp#L11)).
- `probe_`, `readRegs_` und `writeRegs_` beginnen mit `if (!I2CBus::isOpen()) return false;`. `probe_` nutzt `I2CBus::probe()`. Damit ist das Deadlock-Risiko im Code ausgeschlossen, nicht nur per Konvention.
- Neu: `enum class Si5351Error : uint8_t { None, NoAck, NotSi5351, Io };` mit den Gettern `lastError()`, `idUnverified()` und `revId()`.
- `begin(corr)` in dieser Reihenfolge:
  1. Probe, sonst `NoAck`.
  2. Register 0 lesen. **0xFF → `NotSi5351`**. Das wird vor der Poll-Schleife geprüft, weil 0xFF Bit 7 gesetzt hat und die Schleife sonst nur in den Timeout läuft.
  3. SYS_INIT bis zu 20 ms pollen. Bleibt es gesetzt, gibt es nur eine Warnung im Log (wegen Klonen); den Ausschlag gibt der OE-Readback in Schritt 5.
  4. Register 183 lesen, nur Lesezugriffe:
     - **0x00, 0xFF oder XTAL_CL (Bit 7:6) = 00** (laut AN619 reserviert, Reset-Wert 11) → `NotSi5351`.
     - `(r183 & 0x3F) != 0x12` setzt nur `idUnverified_` und erzeugt eine Warnung mit dem Wert (Klone wie MS5351M).
  5. Register 3 = 0xFF schreiben und zurücklesen. Ist das Ergebnis ≠ 0xFF → `NotSi5351`. Ein fremdes Gerät erhält damit höchstens dieses eine Byte. Die harte Prüfung ist keine Verschärfung: `checkAlive()` verlangt dieselbe Rücklesung schon heute ([si5351_manager.cpp:166](src/drivers/si5351_manager.cpp#L166)).
  6. Restliche Init-Schreibzugriffe mit **Abbruch beim ersten Fehler** (`ok = ok && …`) → `Io`.
  7. `ready_ = true`, `revId_` speichern (Register 0, Bit 1:0).
- Die treiberinternen Fehlerlogs („Init write failed“) sinken auf Debug-Level, die Zustandsmeldung übernimmt der AppController.

**[src/core/app_controller.h/.cpp](src/core/app_controller.cpp)**
```cpp
enum class LinkState : uint8_t { NotFound, Blocked, UnknownDevice, Connected };
bool connectHardware();                       // boot; = connect_()
LinkState linkState() const;
bool i2cSwapped() const;                      // I2CBus::map() == I2CMap::Swapped
static const char* linkText(LinkState s);     // dialog texts, < 160 chars
```
- Privat: `connect_()` (Ablauf siehe oben, etwa 30 Zeilen), `setLinkState_()` und `int8_t unknownOn_ = -1` (Belegung mit vermerktem fremdem Gerät).
- In `superviseHardware_()` bleibt der Bereit-Zweig unverändert. Im Zweig „nicht bereit“ wird `si5351_.begin(...)` ([app_controller.cpp:42](src/core/app_controller.cpp#L42)) zu `connect_()`. Bei Erfolg folgt wie heute `reconfigureOutputs_()`.
- `setLinkState_()`:
  - Eine Logzeile pro Zustandswechsel. Bei Connected: `"SI5351 at 0x60, SDA=%d SCL=%d (swapped)"`.
  - Ein Dialog über `reportError_` nur beim Übergang nach `Blocked` oder `UnknownDevice`.
  - `NotFound` bleibt zur Laufzeit still.
- `MSG_HW_LOST` → `"Connection lost. Check the cable; SDA/SCL may be in either order. Reconnecting automatically."`

**[src/main.cpp](src/main.cpp)**
- Schritt 2 `I2CBus::begin()` bleibt.
- Schritt 5 wird zu `app.connectHardware()`.
- In Schritt 7 zeigt `gui.showMessage("SI5351", AppController::linkText(s))` nur für `s != Connected` einen Dialog an.

**[src/gui/gui.h/.cpp](src/gui/gui.cpp)**
- Neu: `void dismissMessages(const char* title)`. Sie schließt den offenen Dialog mit diesem Titel (dafür kommt `msgTitle_[24]` hinzu) und entfernt passende Einträge aus der Warteschlange.
- `Gui::update()` ruft sie auf, wenn `hardwareReady()` von 0 auf 1 wechselt ([gui.cpp:253](src/gui/gui.cpp#L253)).
- `hardwareState_()` bleibt unverändert, weil die Karten für jeden nicht verbundenen Zustand „NO HW“ zeigen.

**[platformio.ini](platformio.ini)**
- `[native_test]` erhält `+<hal/i2c_bus.cpp>`.
- `CORE_DEBUG_LEVEL=1` (siehe [todo.md](todo.md) 4, Logging). Das blendet die `log_i`-Zeile von `i2cInit` bei jedem Re-Init aus.

### Startablauf

1. Logger
2. `I2CBus::begin()`: Leitungen bei Bedarf freigeben, dann `open(Default)` mit 100 kHz und Timeout 20 ms. Der Scan läuft nur mit `LOG_DEBUG` und nur bei offenem Bus.
3. Backlight (dunkel)
4. Settings, dann `app.begin(loaded)`
5. `app.connectHardware()`: gerade ≈ 1 ms, vertauscht ≈ 2 ms
6. Touch
7. GUI, dann ein Dialog nur bei `NotFound`, `Blocked` oder `UnknownDevice`
8. Backlight-Pegel, dann `applyStoredChannelSettings()`

### Fallback & Benutzerführung

| Situation | Verhalten |
|---|---|
| Bereit | `checkAlive` alle 2 s wie heute, die Pins werden nie angefasst |
| Gerade oder vertauscht angeschlossen | Kein Dialog, eine Logzeile mit Pins und „swapped“ |
| Kabel gezogen | Einmal „Connection lost“, die Karten zeigen NO HW, `connect_()` alle 2 s |
| Wieder gesteckt, gleich oder umgekehrt | Erkennung innerhalb eines Zyklus; `reconfigureOutputs_()` stellt Frequenzen, ON-Zustände und die CH3-Phase wieder her; SI5351-Dialoge schließen sich |
| `NotFound` | Stille Wiederholung, eine Logzeile pro Wechsel |
| `Blocked` / `UnknownDevice` | Ein Dialog pro Übergang; ein fremdes Gerät wird bis zum Abziehen nicht erneut angesprochen |
| Kabel draußen, SI5351 versorgt | Der Chip behält seine Ausgänge; beim Wiederverbinden schaltet OE = 0xFF sie für einige ms ab (wie heute, im Handbuch erwähnen) |

**Texte** (englisch wie die übrige UI, Titel „SI5351“, jeweils < 160 Zeichen, `msgText_[160]` aus [gui.h:119](src/gui/gui.h#L119)):

| Zustand | Text |
|---|---|
| NotFound | "SI5351 not found. Check the cable at CN1, 3.3 V and GND. SDA/SCL may be in either order. Searching automatically." |
| Blocked | "I2C line stays low: short, missing 3.3 V or defective module. Check the cable at CN1. Searching automatically." |
| UnknownDevice | "The device at I2C address 0x60 is not an SI5351. It is not programmed. Check the module." |

**Logging:**
- Eine Zeile pro Zustandswechsel, Probes erzeugen keine Logzeilen.
- Ausnahme: Antwortet ein Modul mit ACK, schlagen aber die Lesezugriffe fehl (Wackelkontakt), loggt der Core alle 2 s `i2cWriteReadNonStop returned Error`. Das wird akzeptiert und im Handbuch erwähnt.

**Info-Anzeige** (nicht Teil dieses Konzepts): Pins und „swapped“ gehören in die Info-Zeile aus [todo.md](todo.md) 3, zusammen mit der Display-Erkennung. Diese nutzt `i2cSwapped()`, `I2CBus::sdaPin()` / `sclPin()`, `revId()` und `idUnverified()`.

### Persistenz

- Nur im RAM, kein NVS-Schlüssel und kein Flash-Schreibzugriff.
- Beim Start wird die verifizierte Belegung SDA 22 / SCL 27 zuerst versucht. Eine vertauschte Installation kostet ≈ 1 ms mehr pro Boot.
- Zur Laufzeit gilt die gerade offene Belegung als „zuletzt gut“.

### Tests

**Fakes**
- `TwoWire` ([test/fakes/Wire.h](test/fakes/Wire.h)):
  - speichert `begun_`, `sda_`, `scl_`, `clockHz_` und `timeoutMs_`;
  - neu: `end()`, `setTimeOut()`, die Zähler `beginCalls` / `endCalls` / `setClockCalls`;
  - `begin()` bei laufendem Bus behält die alten Pins und liefert true (wie die echte Bibliothek);
  - `begin(x, x, …)` scheitert;
  - jeder Transfer bei geschlossenem Bus setzt `Fake::wireMisuse() = true` und liefert 4.
- `Si5351Chip`:
  - `wiredSda = PIN_I2C_A`, `wiredScl = PIN_I2C_B`; ein ACK gibt es nur bei passender Belegung, sonst NACK;
  - `lineLow {None, Sda, Scl}` → Rückgabe 5, bei `Scl` zusätzlich `advanceMs(13)`;
  - `ackAll` (falsches ACK auf jeder Belegung und Adresse, Lesen ergibt 0xFF);
  - `regs[183] = 0xD2` in `powerOn()`; `reset()` stellt die gerade Verdrahtung wieder her.
- `Fake::OtherDevice` auf 0x60 mit festem Lesewert (0x00 oder 0xFF), das Schreibzugriffe protokolliert.
- `Arduino.h`: `pinMode`, `digitalRead` (HIGH), `digitalWrite` und `delayMicroseconds` als Stubs.
- Harness:
  - `setUp` in `test_si5351` ruft `I2CBus::begin()` vor `si.begin(0)` auf.
  - `Device` in `test_app_controller` ruft `I2CBus::begin()` und dann `app.connectHardware()` auf, wie `main.cpp`.

**Neue Host-Tests**

`test_si5351`:
- `begin()` bei geschlossenem Bus liefert false, ohne Writes, `!wireMisuse`.
- Der Reset-Zustand wird akzeptiert, `idUnverified() == false`.
- Eine Abweichung in Register 183 (Bit 5:0) wird akzeptiert, mit `idUnverified()`.
- Fremdgerät liest 0x00 → `NotSi5351`, `writes.empty()`.
- Fremdgerät liest 0xFF → `NotSi5351` ohne Wartezeit, `writes.empty()`.
- Gerät speichert Register 3 nicht → `NotSi5351`, genau 1 Write.
- SYS_INIT bleibt gesetzt → nur Warnung, das Ergebnis entscheidet der OE-Readback.
- `failAfter` während der Init → `Io`, kein Transfer nach dem ersten Fehler.

`test_app_controller` (jeder Test prüft zusätzlich `!wireMisuse`):
- **Boot gerade:** Connected; `beginCalls == 1`, `endCalls == 1`; danach Takt 400 kHz.
- **Boot vertauscht:** Connected, `i2cSwapped()`, Frequenzen per `assertHz`.
- **Boot ohne Modul:** NotFound, `writes.empty()`, `errorCount == 0`; über 10 Zyklen ≤ 1 Re-Init pro Zyklus.
- **Hot-Plug:** nach Boot ohne Modul gerade, dann vertauscht anstecken; jeweils Connected nach ≤ 1 Zyklus.
- **Umstecken im Betrieb mit CH1 + CH3 IQ 270°:** genau ein „Connection lost“, alles wiederhergestellt inklusive Phase.
- **Aussetzer** (`failAfter`) im Connected-Zustand: Wiederverbinden auf derselben Belegung, `endCalls` unverändert.
- **Gesunder Betrieb:** 100 Zyklen ohne `begin`, `end` oder `setClock`.
- **Falsches ACK auf der falschen Belegung** (`ackAll` nur auf Swapped), Chip gerade verdrahtet: Connected innerhalb von 1 Zyklus.
- **Fremdgerät 0x00:** UnknownDevice, über 10 Zyklen höchstens 1 Write und genau 1 Dialog; nach Abziehen und Anstecken des SI5351 Connected.
- **`lineLow` Sda bzw. Scl:** Blocked, 1 Dialog, Zyklusdauer ≤ 2 × `I2C_TIMEOUT_MS` (`Fake::clockMs()`); nach Freigabe Connected.
- **Schlechtester Zyklus** (ACK, danach Timeouts): ≤ 4 × `I2C_TIMEOUT_MS`.
- Alle `linkText()`-Texte und `MSG_HW_LOST` sind < 160 Zeichen.

**Hardware-Testfälle** (neuer Abschnitt im Messplan 10. Gruppe J, weil H für die Bereichserweiterung und I für die Display-Erkennung vergeben sind.)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | J1 | Kabel gerade (SDA→22) | Connected, Log „SDA=22 SCL=27“, kein Dialog | |
| [ ] | J2 | Kabel vertauscht | Connected, Log „swapped“, 10 MHz an CH1 am Zähler | |
| [ ] | J3 | Boot ohne Modul, dann gerade anstecken; danach Boot ohne Modul, vertauscht anstecken | Dialog schließt sich, Ausgänge ≤ 2,5 s nach dem Anstecken | |
| [ ] | J4 | CH1 + CH3 IQ 270°, im Betrieb abziehen und vertauscht wieder anstecken | ein Dialog, Phase an 2 Scope-Kanälen korrekt | |
| [ ] | J5 | Wackelkontakt 30 s | keine Fehlprogrammierung, eine Logzeile pro Zustandswechsel | |
| [ ] | J6 | SDA nach GND, dann SCL nach GND; Logikanalysator, Rückgabecode im Debug-Log | Code notieren (0/2/5), erwartet 5 → Blocked; GUI stockt < 100 ms; nach Entfernen Connected | |
| [ ] | J7 | Registerdump 0, 3, 183 nach dem Einschalten (eigenes Modul, wenn möglich auch MS5351M); **vor Schritt 5** | Werte notieren; entscheidet, welche Prüfungen hart sind | |
| [ ] | J8 | Modul ohne Pull-ups | Erkennung bei 100 kHz, Betrieb mit 400 kHz stabil; sonst `I2C_FREQ_HZ` senken | |
| [ ] | J9 | 10 min ohne Modul, Heap im Debug-Log | freier Heap konstant, keine wiederholten Logzeilen | |
| [ ] | J10 | CYD2USB: CN1-Belegung prüfen, J1/J2 wiederholen | CN1 führt GPIO22/27, wie J1/J2 | |
| [ ] | J11 | Logikanalysator bei vertauschtem Kabel | am Chip nur START/STOP und Fragmente, keine Registeränderung | |
| [ ] | J12 | 3,3 V offen, GND verbunden | NotFound oder Blocked, kein Teilschreiben; nach Verbinden Connected | |
| [ ] | J13 | GND offen, 3,3 V verbunden | wie J12 | |
| [ ] | J14 | Debug-Build mit Dauerlesen; während der Lesezugriffe 20× EN-Reset | nach jedem Boot Connected (Leitungsfreigabe greift) | |

Außerdem in A1–A3 „SDA trennen/verbinden“ beibehalten, aber als Erwartung den neuen Text angeben.

### Risiken & offene Punkte

1. **IDF-Verhalten bei LOW-Leitung** (Timeout, NACK über Arbitrierungsverlust oder ACK) lässt sich nicht aus den Quellen prüfen, weil die IDF nur als `libdriver.a` vorliegt. Davon hängt nur die Unterscheidung zwischen `Blocked` und `NotFound` ab. J6 entscheidet; liefert J6 nicht 5, werden die beiden Zustände zusammengelegt (nur der Text ändert sich).
2. **Klone (MS5351M):** Hart geprüft werden nur 0x00/0xFF, XTAL_CL = 00 und der OE-Readback (der heute schon Pflicht ist). Register 183 Bit 5:0 und SYS_INIT ergeben nur Warnungen. J7 läuft vor der Umsetzung von Schritt 5.
3. **Wire-Deadlock:** ausgeschlossen durch den Guard in `I2CBus::probe` und den drei Primitiven; `wireMisuse` wird in jedem Host-Test geprüft.
4. **Falsches ACK auf der falschen Belegung** (schwache Pull-ups): abgedeckt, weil ein fehlgeschlagenes `begin()` zur anderen Belegung weiterführt; die Erkennung mit 100 kHz verringert das Risiko.
5. **Heap-Fragmentierung** durch `i2c_driver_install`/`delete` (1 Re-Init pro 2 s ohne Modul): Blöcke fester Größe; J9.
6. **Core-Logzeile alle 2 s** im Fall „ACK, aber Lesen scheitert“: akzeptiert. `CORE_DEBUG_LEVEL=0` würde nützliche Fehlermeldungen verbergen.
7. **Leitungsfreigabe beim Boot** pulst GPIO22/27 nur, wenn eine Leitung LOW liest; das ist für den Chip unkritisch. Zur Laufzeit gibt es keine eigene Freigabe; reicht die IDF-Freigabe nicht (J6, J14), kommt der Aufruf auch beim Übergang nach `Blocked` hinzu.
8. **5-V-Modul, Hot-Plug, GPIO21 auf P3:** keine neuen Risiken, Hinweise ins Handbuch.
9. **arduino-esp32 3.x:** Das Wire-Verhalten ändert sich, ist aber vollständig in `i2c_bus.cpp` gekapselt.

Bewusst nicht enthalten:
- Ghost-Probe auf 0x7F: auf dem ESP32 praktisch nicht erreichbar, siehe oben.
- Speicherung der Belegung in NVS.
- Build-Option für feste Pins: Sie würde genau die Einstellung zurückbringen, die wegfallen soll.
- 10-s-Backoff nach Timeouts.
- LinkState-Bits in `hardwareState_()`.

### Umsetzungsschritte mit Aufwand

| # | Schritt (je ein Commit) | Aufwand |
|---|---|---|
| 1 | Boot-Hang beheben: Scan nur nach erfolgreichem `begin` und nur mit `LOG_DEBUG`; `CORE_DEBUG_LEVEL=1` | 0,1 d |
| 2 | Fakes (Pins, `end()`, `wireMisuse`, `lineLow`, `ackAll`, `OtherDevice`, GPIO-Stubs), `+<hal/i2c_bus.cpp>`, beide Harnesses; bestehende 62 Tests grün | 0,5 d |
| 3 | `I2CBus` (`open` / `probe` / `setRunClock` / `recoverLines_`), `pins.h`, Guard im Treiber | 0,4 d |
| 4 | J7 am Gerät (Registerdump) | 0,1 d |
| 5 | Treiber: Identitätsprüfung, OE-Readback nach dem ersten Write, Abbruch beim ersten Fehler, `lastError()`; Tests | 0,3 d |
| 6 | AppController `connect_()`, `LinkState`, Fremdgerät-Vermerk, Texte, `main.cpp`, `Gui::dismissMessages`; Tests | 0,6 d |
| 7 | Doku: README, `PROJECT_SPECIFICATION.md`, `USER_MANUAL_DE.md` / `USER_MANUAL_EN.md` („SDA/SCL beliebig“), `CLAUDE.md`, `todo.md` (A1–A3, Messplan J) | 0,25 d |
| 8 | Hardwaretests J1–J14 | 0,5 d |
| | **Summe** | **≈ 2,75 d** |

Optional und getrennt: Info-Zeile in CONFIG ([todo.md](todo.md) 3) zusammen mit der Display-Erkennung, ≈ 0,25 d.
