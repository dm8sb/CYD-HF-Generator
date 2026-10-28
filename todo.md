# TODO – Code- & Design-Review CYD HF Generator

Stand: 2026-10-27 · Review-Basis: Commit `ae034f8` (Firmware 1.2.0) · Einsatzziel: Experimente mit HF-Mischern und Amateurfunkausbildung; wichtig sind geringes Phasenrauschen und genauer IQ-Betrieb.

Prioritäten: **P1** = Fehler / sichtbares Problem · **P2** = deutliche Verbesserung · **P3** = Aufräumen / nice-to-have
Status: `[ ]` offen · `[~]` teilweise · `[?]` wartet auf Rückfrage (Abschnitt 9) · `[M]` wartet auf Messung (Abschnitt 10)

Erledigte Punkte sind entfernt; sie stehen in der Git-Historie dieser Datei. In diesem Review umgesetzt: Host-Unit-Tests (Abschnitt 7), `platformio.ini` mit `[esp32]` + `extends` und den Testumgebungen `native` / `native-win`, `constants.h` / `types.h` / `frequency_utils.h` ohne `Arduino.h`, README-Abschnitt „Unit Tests“, Spezifikation 33.1. Beide Firmware-Varianten bauen weiterhin.

Nachtrag 2026-10-28: Automatische SDA/SCL-Erkennung umgesetzt (Spezifikation 42). Dabei behoben: Boot-Hänger nach fehlgeschlagenem `Wire.begin()` (I2C-Scan auf nicht gestartetem Bus). Host-Tests grün, beide Varianten bauen; die Hardwaretests stehen in Abschnitt 10 J.

Nachtrag 2026-10-28: Display-Controller-Erkennung umgesetzt (Spezifikation 43): eine Firmware für beide Boards, Boot-Geste als Rückfallebene, Umgebung `esp32-2432S028R-usbc` entfernt. Voruntersuchung I0 am Gerät erledigt; die Hardwaretests stehen in Abschnitt 10 I.

---

## 0. Ergebnis des Reviews (Kurzfassung)

Die Firmware ist klar geschichtet (Treiber / AppController / GUI / Services). Die Fehlerpfade (NO HW, ERR, Wiederverbindung, NVS) sind durchdacht. Der eigene SI5351-Treiber schreibt nur geänderte Register und setzt die PLL nur bei Bedarf zurück; im Bereich 1–100 MHz ist er genauer als dokumentiert (Host-Test: ≤ 0,375 Hz Rechenfehler). Für Mischerversuche und Ausbildung fehlen vor allem Messdaten zu Nebenlinien, Phasenrauschen und IQ-Phasenfehler sowie eine Betriebsart, die auf geringes Phasenrauschen ausgelegt ist.

| # | Befund | Abschnitt |
|---|---|---|
| 1 | Im Einzelbetrieb teilt der MultiSynth fast immer fraktional (Stichprobe 1–100 MHz: 0 % Integer-Mode; mit Korrektur ≠ 0 auch bei 10, 25 und 50 MHz). Fraktionale Teiler erzeugen Nebenlinien nahe am Träger. | 2.1 |
| 2 | IQ-Abstimmen setzt bei **jedem** Schritt die PLL zurück, beide Ausgänge setzen kurz aus. Mit festgehaltenem Teiler ginge es ohne Reset und ohne Phasenverlust. | 2.2 |
| 3 | 54 % der IQ-Teiler sind ungerade (14,2 MHz → 63, 99 MHz → 9). Ob 180°/270° per Invertierung dort stimmen, hängt vom Tastverhältnis ab und ist ungeprüft. | 2.2, E4 |
| 4 | Bereichserweiterung: Um 112,5 MHz weicht der Ausgang bei Korrektur 0 um bis zu 5,1 Hz ab. Fix ist ausgearbeitet und per Host-Test belegt. | 2.4 |
| 5 | IQ-Frequenzfehler bis 1,32 Hz, die Spezifikation nennt ≤ 1 Hz. | 2.2, 5 |
| 6 | Abgeschaltete Ausgänge liegen auf LOW (Register 24 nie geschrieben), README verspricht „High-Z“. | 2.3, 5 |
| 7 | Mögliche Störquellen der Plattform sind ungemessen: Backlight-PWM 5 kHz, I2C-Überwachung alle 2 s, Display-SPI 40 MHz, weiterlaufende MultiSynths abgeschalteter Kanäle. | 2.1, F |

---

## 1. Build, Tests & Hardware-Varianten

- [ ] **P2 CI**: GitHub Action mit `pio test -e native` und `pio run` für beide Firmware-Umgebungen. Die Host-Tests brauchen ≈ 10 s und keine Hardware. `env:native` (Linux, gcc) ist noch nicht ausgeführt worden, nur `env:native-win`.
- [ ] **P3 TFT_eSPI exakt pinnen** (`bodmer/TFT_eSPI@2.5.43` statt `^2.5.43`), wie die anderen Bibliotheken auch.
- [ ] **P3 Arduino-Core 3.x**: Bei einem Update `ledcSetup` / `ledcAttachPin` umstellen (auch im Fake `test/fakes/Arduino.h`).

## 2. SI5351 / HF-Funktion

### 2.1 Phasenrauschen und Nebenlinien

- [M] **P2 Low-Noise-Modus: gerader Integer-MultiSynth, fraktionale PLL.** Heute bleibt die PLL fest (900 bzw. 887,5 MHz) und der MultiSynth teilt fraktional ([si5351_manager.cpp:205-221](src/drivers/si5351_manager.cpp#L205-L221)). Das erlaubt Abstimmen ohne PLL-Änderung, aber fraktionale Teiler erzeugen deterministischen Jitter und Nebenlinien. Host-Analyse mit 20 000 Zufallsfrequenzen je Teilbereich: 0 % laufen im Integer-Mode; bei Korrektur ≠ 0 auch 10 MHz nicht mehr (900 MHz / 10 MHz ist dann nicht mehr ganzzahlig). Vorschlag nach dem Verfahren der QRP-Labs- und Etherkit-VFOs: MultiSynth = größter gerader Teiler mit VCO ≤ 900 MHz und `MS_INT` = 1, die PLL nimmt Frequenz und Korrektur auf. Abstimmen ändert nur die PLL, ohne Reset, solange der Teiler bleibt (Hysterese über 600–900 MHz VCO); Reset nur beim Teilerwechsel. Beide Kanäle haben eigene PLLs, im IQ-Betrieb arbeitet der Treiber schon so. Entscheidung nach Messplan F3/F5 (Nebenlinien, Phasenrauschen) und D5 (Einschwingen ohne Reset), als CONFIG-Option „Tuning: Clean / Fast“ oder als neuer Standard (Rückfrage 9.2).
- [ ] **P2 Integer-PLL kennzeichnen**: Bei ganzzahligem PLL-Verhältnis (900 MHz = 36 × 25 MHz) das Bit `FBA_INT` / `FBB_INT` setzen (Register 22/23, Bit 6, AN619); `begin()` schreibt dort heute `0x80` ([si5351_manager.cpp:146](src/drivers/si5351_manager.cpp#L146)). Laut AN619 senkt das den Jitter. Registerbelegung vor der Umsetzung im Datenblatt prüfen.
- [ ] **P2 Spread Spectrum explizit aus**: Register 149, Bit 7 (`SSC_EN`) in `begin()` auf 0 schreiben. Bei unprogrammierten Bausteinen ist es aus, Module mit vorprogrammiertem NVM sind aber nicht ausgeschlossen. Ein aktives Spread Spectrum würde das Spektrum stark verbreitern.
- [ ] **P2 MultiSynth abgeschalteter Kanäle abschalten**: CH1/CH2 werden im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter ([app_controller.cpp:97-98](src/core/app_controller.cpp#L97-L98)). Für CLK2 ist das bereits gelöst („An unused MultiSynth would only add spurs“). Gleiches für CH1/CH2: bei OFF `CLKx_PDN` setzen, beim Einschalten das Steuerregister zurückschreiben (der Cache hat alle Werte). Messplan F7 zeigt, ob es sich lohnt.
- [M] **P2 Störquellen der Plattform**: Die Backlight-PWM mit 5 kHz ([backlight.cpp:7](src/drivers/backlight.cpp#L7)) kann über die Versorgung Seitenlinien bei ±5 kHz erzeugen. Abhilfe: PWM ≥ 20 kHz (LEDC schafft bei 8 bit bis 312 kHz) oder 100 % ohne PWM. Die Überwachung liest alle 2 s Register über I2C ([app_controller.cpp:32](src/core/app_controller.cpp#L32)). Sind Störungen im 2-s-Takt sichtbar, hilft ein „Ruhemodus“ (Überwachung nur bei Bedienung). Die CONFIG-Seite schreibt alle 300 ms dasselbe Label neu ([gui.cpp:257](src/gui/gui.cpp#L257)) und erzeugt damit SPI-Verkehr; nur bei Änderung schreiben. Messplan F6.
- [ ] **P3 Hardware-Hinweise** (Handbuch, ggf. Aufbau): SI5351 über einen eigenen LDO mit Ferrit aus 5 V versorgen, nicht aus dem 3,3-V-Netz des ESP32; kurze Masse; Abschirmung; optional 25-MHz-TCXO statt Quarz (Entscheidung nach Messplan C7).

### 2.2 IQ-Betrieb

- [M] **P2 IQ-Abstimmen ohne Aussetzer**: Jede Frequenzänderung im IQ-Betrieb schaltet beide Ausgänge ab, setzt die PLL zurück und schaltet sie wieder ein ([si5351_manager.cpp:273-284](src/drivers/si5351_manager.cpp#L273-L284)); im Host-Test ergeben 1000 Schritte à 100 Hz 1000 PLL-Resets. Die Phasenlage ist aber ein Versatz in VCO-Takten zwischen zwei MultiSynths an derselben PLL. Ändert sich nur die PLL, bei gleichem Teiler und Offset, bleibt sie erhalten. Vorschlag: Teiler mit Hysterese festhalten (bei 10 MHz deckt Teiler 90 den Bereich 6,67–10 MHz ab) und dann nur die PLL ohne Reset schreiben. Am Gerät prüfen (Messplan D3, E9). Damit entfällt auch der Aussetzer bei jeder Korrekturänderung.
- [M] **P2 180°/270° bei ungeraden Teilern**: Die Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°. Der IQ-Teiler ist `min(127, ⌊900 MHz / f⌋)` ([si5351_manager.cpp:254-255](src/drivers/si5351_manager.cpp#L254-L255)) und in 54 % der Fälle ungerade (4,73–5 MHz: 127, 14,2 MHz: 63, 99 MHz: 9). Zeigt Messplan E4 eine Abweichung: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, darüber bleibt nur 9. Alternativ 180° über den Phasen-Offset (2 × Teiler ≤ 127) statt über die Invertierung.
- [ ] **P2 Phasen-Feinabgleich und freie Phase**: Der Offset hat eine Auflösung von T_VCO/4, also 90° / Teiler (10 MHz: 1°, 30 MHz: 3°, 99 MHz: 10°). Mit Offset auf Master oder Folger (je 0–127) und der Invertierung ist jede Phase in diesem Raster einstellbar. Nutzen: Ausgangs-Skew und Kabellaufzeit ausgleichen (Messplan E3) und Ausbildungsversuche zu Spiegelunterdrückung gegen Phasenfehler. Die CH3-Seite zeigt dann die erreichbare Auflösung.
- [ ] **P3 IQ-Frequenzfehler bis 1,32 Hz** (z. B. 91 666 668 Hz: Der PLL-Bruchteil liegt knapp über 1/(2·2^20)). Die Spezifikation nennt ≤ 1 Hz (10.5, 35). Entweder Doku auf ≤ 1,5 Hz ändern oder unter den gültigen Teilern den mit dem kleinsten Fehler wählen; zwischen ≈ 4,8 und 90 MHz gibt es immer mindestens zwei. `test_iq_accuracy_at_weak_spots` prüft heute ≤ 1,35 Hz.

### 2.3 Ausgänge und Robustheit

- [?] **P2 Definierter Zustand im OFF**: Register 24 (`CLKx_DIS_STATE`) wird nie geschrieben. Mit dem Reset-Wert 0 liegen abgeschaltete Ausgänge auf LOW. Das passt für DC-gekoppelte Mischer (Diodenring, FST3253), README und Spezifikation sprechen aber von „High-Z“. Register in `begin()` explizit schreiben und Doku angleichen (Rückfrage 9.3, Messplan A9).
- [ ] **P3 SI5351 läuft beim Flashen und nach ESP32-Reset weiter**: Der SI5351 hat keinen Reset-Eingang. Nach Reset-Taste oder Upload behält er die letzte Einstellung, bis `begin()` die Ausgänge abschaltet, beim Flashen also einige Sekunden lang. Im Handbuch erwähnen (Verstärker oder Mischer angeschlossen), Messplan A10.
- [ ] **P3 Spannungseinbruch über Sticky-Bit erkennen**: `checkAlive()` erkennt einen Neustart des SI5351 daran, dass die Steuerregister vom Cache abweichen ([si5351_manager.cpp:160](src/drivers/si5351_manager.cpp#L160)). Das funktioniert, solange mindestens ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen. Dasselbe Register meldet `LOL_A` / `LOL_B` (PLL nicht eingerastet), nützlich für die Info-Zeile (3).
- [ ] **P3 VCO bei positiver Korrektur leicht über 900 MHz**: Die feste PLL 36 × Quarz ergibt bei +50 ppm 900,045 MHz. Praktisch unkritisch; mit dem Low-Noise-Modus entfällt es.

### 2.4 Frequenzbereich CH1/CH2 auf 8 kHz – 160 MHz erweitern (Entscheidung 2026-10-27)

- [ ] **P2 Bereichserweiterung**. Der Treiber kann den Bereich schon (`OUT_MIN_HZ` / `OUT_MAX_HZ`, unter 1 MHz mit R-Teiler). Teilaufgaben:
  - **Zuerst Treiberfehler um 112,5 MHz beheben**: Bei Korrektur 0 rundet `approxRatio()` den MultiSynth auf genau 8/1. Dann greift `msR.a < 8` nicht ([si5351_manager.cpp:223](src/drivers/si5351_manager.cpp#L223)), der Weg „gerader Integer-Teiler + fraktionale PLL“ wird nicht versucht, und z. B. 112 500 004 Hz liegt 5,1 Hz daneben. Zusätzlich setzt der Zweig „aktuelle PLL behalten“ `bestErr` nicht ([si5351_manager.cpp:211-215](src/drivers/si5351_manager.cpp#L211-L215)). Erprobter Fix (alle 62 Host-Tests bestanden, 1–100 MHz unverändert):

    ```cpp
    // im Zweig "aktuelle PLL behalten" zusätzlich: bestErr = err;
    if (msR.a < 8 || bestErr > FIXED_PLL_KEEP_ERR_HZ) {
        for (uint32_t div : {8u, 6u, 4u}) {   // größter gültiger gerader Teiler
            if (div * msHz > VCO_MAX_HZ || div * msHz < VCO_MIN_HZ) continue;
            const Ratio p = approxRatio(4 * div * msHz, xtal4);
            const Ratio m {div, 0, 1};
            const double err = fabs(synthesizedHz_(p, m, rLog2) - (double)freq);
            if (msR.a < 8 || err < bestErr) { bestErr = err; pllR = p; msR = m; divBy4 = div == 4; }
            break;
        }
    }
    ```
    Danach in `test_standalone_above_100_mhz` das `TEST_IGNORE_MESSAGE` entfernen. Erreichbare Genauigkeit (begrenzt durch den 20-bit-Nenner der PLL bei Teiler 8/6/4): ≤ 0,5 Hz bis 100 MHz, ≤ 1,5 Hz bis 112,5 MHz, ≤ 2 Hz bis 150 MHz, ≤ 3 Hz bis 160 MHz.
  - Über 112,5 MHz ändert jeder Schritt die PLL und löst einen PLL-Reset aus (Host-Test: 100 Schritte bei 120 MHz → 100 Resets). Prüfen, ob kleine PLL-Änderungen ohne Reset auskommen (wie 2.1 / 2.2, Messplan D5).
  - `FREQ_MIN_HZ` / `FREQ_MAX_HZ` in [constants.h](include/constants.h) anpassen; Numpad-Fehlertext („1 - 100 MHz“) und Eingabeprüfung folgen den Konstanten. Die Host-Tests leiten ihre Grenzen ebenfalls aus den Konstanten ab.
  - **CH3 bleibt bei 4,73 – 100 MHz**: `setupIQPair()` lehnt Frequenzen über `IQ_MAX_HZ` ab. `IQ_MAX_HZ` zusätzlich in `iqConfigured_()` prüfen, neuer `Ch3Status::FreqTooHigh` mit Anzeige „WAIT … > 100 MHz“ auf MAIN und CH3. Testfall in `test_app_controller` ergänzen.
  - Numpad: maximale Länge 9 → 10 Zeichen (z. B. „150.000001“ MHz).
  - Anzeige unter 1 MHz in kHz (z. B. „455.000 kHz“) statt „0.455 000 MHz“ ([frequency_utils.cpp:16-29](src/utils/frequency_utils.cpp#L16-L29)); `digitExponent()` und die Tests in `test_frequency_utils` anpassen.
  - Hinweis auf der Kanalseite: über 100 MHz geringerer Pegel, unter 1 MHz starke Oberwellen (Tiefpass nötig).
  - Handbücher DE/EN, Spezifikation (Abschnitt „Approved Deviations“) und README nachziehen. Messplan H1–H4.

## 3. Bedienung (UX)

- [ ] **P3 Info-Zeile in CONFIG**: Firmwareversion, SI5351-Status (gefunden / fehlt, I2C-Pins, PLL eingerastet) und Display-Variante. Erleichtert die Fehlersuche ohne serielle Konsole. Für das Display vorhanden: `DisplayPanel::panel()` / `probeResult()`. Für I2C vorhanden: `AppController::linkState()`, `I2CBus::sdaPin()` / `sclPin()` / `map()` (vertauscht), `SI5351Manager::revId()` / `idUnverified()`.
- [ ] **P3 Werksreset** (NVS löschen) in CONFIG, mit Bestätigung.
- [ ] **P3 Masterfrequenz auf der CH3-Seite verstellen** (`+` / `−` mit der globalen Schrittweite), damit IQ-Versuche ohne Seitenwechsel auskommen.
- [ ] **P3 Bildschirmsperre** für Messungen: verhindert versehentliche Eingaben und unterdrückt Neuzeichnen (siehe Störquellen 2.1).
- [ ] **P3 Lesbarkeit im Unterricht**: Status- und Detailzeilen auf MAIN nutzen 12 px; wo Platz ist, 14 px.

## 4. Code-Qualität & Wartbarkeit

- [ ] **P3 Teilerwahl als reine Funktion**: Die Berechnung in `setupStandalone()` / `setupIQPair()` (Kandidaten, Fehler, Teiler) von den I2C-Schreibzugriffen trennen (z. B. `planStandalone()` / `planIQ()` mit Ergebnis-Struktur). Low-Noise-Modus, IQ-Hysterese und der 112,5-MHz-Fix werden damit einfacher und direkt testbar; die Tests über das Registermodell bleiben gültig.
- [ ] **P3 IQ-Grenze hart kodiert**: „4.730 MHz“ bzw. „4.73 MHz“ stehen als Text in [page_ch3.cpp:26](src/gui/pages/page_ch3.cpp#L26), [page_ch3.cpp:57](src/gui/pages/page_ch3.cpp#L57) und [page_main.cpp:77](src/gui/pages/page_main.cpp#L77). Aus `IQ_MIN_HZ` formatieren.
- [ ] **P3 Redundanter Drive-Zustand**: `drive_strength` steht in `Channel`, `Channel3` und `SystemSettings`, ist aber immer gleich (globale Einstellung). Nur `sys.default_drive_strength` behalten.
- [ ] **P3 Ungenutzter Code**: `BacklightDriver::percent()`, `Storage::end()`, `AppController::ch1()/ch2()/state()`, `Logger::debug()`, `PIN_FREE_INPUT_ONLY` und `LV_HOR_RES_MAX` (ein LVGL-7-Rest in [lv_conf.h](include/lv_conf.h)).
- [ ] **P3 Feste Koordinaten in den Pages**: Header (158/214), CH3 (114/134/186), CONFIG (92/140/180/236) usw. in [gui_layout.h](src/gui/gui_layout.h) bzw. als lokale Konstanten zusammenfassen, wie in [page_main.cpp](src/gui/pages/page_main.cpp) schon geschehen.
- [~] **P3 Logging im Normalbetrieb**: `valueU64` bei jeder Frequenzänderung hinter `LOG_DEBUG` legen. Erledigt: I2C-Scan nur mit `LOG_DEBUG`, `CORE_DEBUG_LEVEL=1`.
- [ ] **P3 Mehrere Anweisungen pro Zeile** in [touch.cpp:22-23](src/drivers/touch.cpp#L22-L23).
- [ ] **P3 Display-Flush mit DMA und Doppelpuffer.**

## 5. Dokumentation

- [ ] **P2 Anwendungskapitel „Mischer-Experimente“** in beiden Handbüchern: Ausgangspegel je Drive-Stärke in dBm an 50 Ω (aus Messplan F2); Oberwellen eines Rechtecks (3. ≈ −9,5 dBc, 5. ≈ −14 dBc) und passende Tiefpässe; LO-Pegel für Diodenringmischer (+7 dBm) bzw. Dämpfung für NE612/SA612; IQ direkt an FST3253/74CBT3253 (Tayloe-Detektor); gleich lange Kabel bei IQ (1° bei 100 MHz ≈ 28 ps ≈ 5,6 mm Koax); Zusammenhang Phasenfehler ↔ Spiegelunterdrückung (1° ≈ 41 dB, 2° ≈ 35 dB, 5° ≈ 27 dB); Rechtecksignal nie ungefiltert an eine Antenne.
- [ ] **P2 „High-Z“ ist falsch**: README („Independent output enable/High-Z control“) und Log-Beispiele; abgeschaltete Ausgänge liegen auf LOW (2.3).
- [M] **P2 „50 % Tastverhältnis“** (Spezifikation 1 / 9.1, Handbücher 1) ist nicht belegt. Nach Messplan E5 präzisieren.
- [ ] **P3 IQ-Genauigkeit „≤ 1 Hz“** (Spezifikation 10.5, 35) → tatsächlich ≤ 1,32 Hz (2.2).
- [ ] **P3 Veraltete Logzeilen**: „Init OK; all outputs High-Z“ (Code: „all outputs off“) und „Correction (0.01 ppm): …“ (gibt es nicht) in den Handbüchern Abschnitt 12 und in der Spezifikation Abschnitt 31.
- [ ] **P3 README „Notes and Limitations“** nennt `si5351_CalcIQ()`; die Funktion gibt es seit dem eigenen Treiber nicht mehr. „Libraries“ nennt `paulstoffregen/XPT2046_Touchscreen@1.4`, eingebunden wird die Git-URL `#v1.4`.
- [ ] **P3 CH3-Statustabelle** in den Handbüchern (Abschnitt 7): Zeile „IQ setup failed, retrying“ fehlt.
- [ ] **P3 Doppelte Beschreibung**: Spezifikation (≈ 1350 Zeilen) und Handbücher DE/EN beschreiben dasselbe Verhalten; dieses Review fand mehrere Abweichungen. Vorschlag: Spezifikation auf Anforderungen und Entscheidungen beschränken, Verhalten nur im Handbuch; das EN-Handbuch als Übersetzung des DE-Handbuchs pflegen.
- [ ] **P3 Handbücher/Spezifikation** auf die Punkte aus 1–3 nachziehen, sobald sie umgesetzt sind.

## 6. Erweiterungen für Experimente und Ausbildung

- [?] **P2 Fernsteuerung über USB-Seriell**: einfaches Textprotokoll (z. B. `F1 7100000`, `ON 1`, `PH 90`, `CORR 970`, `?` für den Status). Ermöglicht automatisierte Messreihen mit Python und Laborplätze ohne Touch-Bedienung.
- [?] **P2 Info-Seite „Synthesizer“**: VCO, PLL a + b/c, MultiSynth a + b/c, R-Teiler, Integer/Fraktional, Phasenregister, PLL-Lock. Zeigt, wie der Synthesizer rechnet, und erklärt beobachtete Nebenlinien.
- [?] **P2 Freie Phase in 90°/Teiler-Schritten** (2.2).
- [?] **P2 Gekoppelter Betrieb CH2 = CH1 ± Offset**: Demonstration Überlagerungsempfänger (LO folgt der Empfangsfrequenz mit fester ZF, z. B. 455 kHz / 10,7 MHz).
- [?] **P3 Speicherplätze** mit Vorgaben für Amateurfunkbänder und übliche ZF (Rückfrage 9.1 a).
- [?] **P3 Wobbeln** mit Start per Taster oder GPIO35 (nur Eingang) (Rückfrage 9.1 b).
- [ ] **P3 Kalibrierassistent**: gemessene Frequenz eingeben, Korrektur wird berechnet (Spezifikation 36).
- [ ] **P3 Externe Referenz**: SI5351C mit `CLKIN` für 10 MHz vom Messplatz oder GPSDO, alternativ TCXO (2.1).

## 7. Unit-Tests

Umgesetzt: `pio test -e native-win` (Windows) bzw. `pio test -e native` (gcc). Stand 2026-10-27: 62 Tests, 61 bestanden, 1 übersprungen (bekannter Fehler 2.4). Laufzeit ≈ 10 s. Eine Gegenprobe mit absichtlich eingebauten Fehlern (Vorzeichen der Korrektur, Phasen-Offset −1) ließ die betroffenen Tests fehlschlagen.

| Suite | Tests | Inhalt |
|---|---|---|
| `test_frequency_utils` | 14 | Formatierung (MHz, Schritt, ppm), Parser (Einheiten, Grenzen, Rundung, ungültige Eingaben, alle kHz-Werte 1–100 MHz), Schrittweiten, Ziffer ↔ Schritt |
| `test_si5351` | 25 | Init und Überwachung (fehlendes Gerät, Spannungseinbruch, Busfehler, Wiederverbindung); Einzelbetrieb: Genauigkeit 1–100 MHz mit Korrektur 0 / +9,7 / ±50 ppm, Schwachstellen bei 900/n und 887,5/n MHz, R-Teiler 8 kHz–1 MHz, > 100 MHz (übersprungen), Integer-Mode, Abstimmen ohne PLL-Reset und ohne Output-Disable, Korrektur ohne Reset, Register-Cache, PLL-Trennung CH1/CH2; Registergrenzen nach AN619; IQ: Phasenregister und Invertierung, PLL B, Genauigkeit und Schwachstellen, Realignment, Drive/Invertierung ohne Reset, VCO an den IQ-Grenzen, ungültige Argumente |
| `test_app_controller` | 23 | Bootzustand (alle aus), Schalten, Abstimmen ohne Aussetzer, Bereichsgrenzen, Drive, Korrektur, Backlight; CH3: Warten auf Master, Nachführen, unter 4,73 MHz warten und automatisch starten, Masterwechsel, Phasenwechsel, Master aus, CH3 aus; Hot-Plug, Spannungseinbruch (ein Dialog, alles wiederhergestellt), Busfehler (NO HW statt ERR), keine Schreibzugriffe im Normalbetrieb; Autosave, Speicherfehler, NVS fehlt, Neustart mit gespeicherten Werten, Bereinigung ungültiger NVS-Werte |

Aufbau: [test/fakes](test/fakes) ersetzt Arduino-Core, `Wire` (SI5351-Registermodell mit Schreibprotokoll und Fehlerinjektion) und `Preferences`. [test/helpers/si5351_model.h](test/helpers/si5351_model.h) dekodiert die geschriebenen Register unabhängig vom Treiber zurück in Frequenzen und prüft die Grenzen aus AN619.

Weitere Vorschläge:

- [ ] **P2** Mit dem Low-Noise-Modus (2.1): Test „jede Frequenz läuft mit `MS_INT`“ und „kein PLL-Reset, solange der Teiler bleibt“.
- [ ] **P2** Mit IQ ohne Reset (2.2): Test „PLL-Änderung ohne Reset, Phasen-Offset und Teiler unverändert“.
- [ ] **P3** Umgebung `native-fault` mit `-DTEST_FAULT_IQ=1`: Zustand ERR, stilles Wiederholen ohne weitere Dialoge, Rückkehr nach Phasenwechsel.
- [ ] **P3** Eingabelogik der GUI testbar machen: Numpad (ein Dezimalpunkt, Länge, Einheiten) und Ziffer-Antippen (x-Position → Zeichen) als reine Funktionen aus `page_numpad.cpp` / `page_channel.cpp` herauslösen.
- [ ] **P3** Bei Änderungen am NVS-Schema (z. B. Drive je Kanal): Migrationstest mit alten Schlüsseln.

## 8. Entschieden, nicht umsetzen

- RGB-LED als Ausgangsanzeige: Die LED zeigt weiter die aktive Seite (Spezifikation 8.1).
- Doppelte Statusanzeige im Header: Sie bleibt, Spezifikation 9.3 verlangt die Textanzeige ON/OFF.

---

## 9. Rückfragen

Beantwortet am 2026-10-27: Alle Ausgänge starten aus. Die USB-C-Firmware läuft am Gerät. Vorerst bleibt es bei zwei Build-Varianten.

1. **Neue Funktionen**: Welche sind gewünscht?
   a) Speicherplätze (z. B. 5 Frequenzen je Kanal),
   b) Wobbeln (Start / Stopp / Schrittweite / Verweilzeit),
   c) Drive-Strength je Kanal statt global,
   d) Fernsteuerung über USB-Seriell,
   e) Info-Seite „Synthesizer“,
   f) freie Phase / Phasen-Feinabgleich,
   g) gekoppelter Betrieb CH2 = CH1 ± Offset?
2. **Low-Noise-Modus** (2.1): als Option in CONFIG oder, falls Messplan F3/F5 einen klaren Vorteil zeigt, als Standard?
3. **Zustand abgeschalteter Ausgänge** (2.3): LOW (heute, Reset-Wert) oder hochohmig?

---

## 10. Messplan für die Hardware-Verifikation

**Geräte**: Frequenzzähler mit externer 10-MHz-Referenz (für C und E besser als 0,1 ppm); Oszilloskop mit 2 Kanälen (für Phasenmessungen bis 100 MHz ≥ 500 MHz Bandbreite und ≥ 2 GS/s); Spektrumanalysator (RBW ≤ 10 Hz, für F5 mit Rauschmarker); 50-Ω-Abschlüsse; Dämpfungsglieder 10/20/30 dB; zwei gleich lange Koaxkabel. Für G zusätzlich ein Diodenringmischer (z. B. SBL-1, ADE-1) und eine PC-Soundkarte oder ein SDR.

**Bedingungen**, wenn nicht anders angegeben: Ausgang mit 50 Ω abgeschlossen (am Spektrumanalysator über 20 dB Dämpfung), Drive 8 mA, Korrektur nach C2 kalibriert, 15 min Aufwärmzeit für C, E und F.

**Abhaken**: `[ ]` durch `[x]` ersetzen und den Messwert eintragen. „(alt n)“ ist die Nummer im Messplan vom 2026-10-26; keiner dieser Tests ist bisher am Gerät gelaufen.

### A. Inbetriebnahme und Robustheit

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | A1 | Booten ohne SI5351 (oder mit getrenntem SDA) (alt 1) | genau **ein** Dialog „SI5351 not found … SDA/SCL may be in either order …“, „OK“ schließt ihn, Karten zeigen „NO HW“ | |
| [ ] | A2 | Danach SDA verbinden (alt 1a) | nach ≤ 2 s verschwindet „NO HW“, Kanäle schaltbar; ein noch offener SI5351-Dialog schließt sich | |
| [ ] | A3 | CH1 an, SDA kurz trennen und wieder verbinden (alt 1b) | Dialog „Connection lost. Check the cable; SDA/SCL may be in either order …“, „NO HW“, danach läuft CH1 mit gleicher Frequenz, Dialog schließt sich | |
| [ ] | A4 | CH1 und CH3 an (IQ, 270°), SI5351-Versorgung kurz unterbrechen (alt 1c) | wie A3; CH3 läuft danach wieder mit 270° | |
| [ ] | A5 | Einstellung ändern, > 5 s warten („Saved“), aus- und einschalten (alt 14) | Einstellung erhalten, alle Ausgänge aus | |
| [ ] | A6 | Einschalten und Display beobachten (alt 14a) | kein weißes oder verrauschtes Bild, Hauptseite erscheint direkt | |
| [ ] | A7 | Beide Boards mit `pio run -t upload` (ohne `-e`) flashen (alt 17) | nur `esp32-2432S028R` wird gebaut, Display läuft auf beiden | |
| [ ] | A8 | Build mit `-DTEST_FAULT_IQ=1`: CH1 an, CH3 an, Phase 270° (alt 18b) | CH3 „ERR“ (MAIN, Header, Infozeile), ein Dialog, CH1 läuft weiter; Phase 90° → nach ≤ 2 s „ON“ | |
| [ ] | A9 | CH1 aus: Pegel an CLK0 DC-gekoppelt, einmal an 1 MΩ, einmal an 50 Ω | dokumentieren; erwartet LOW, nicht hochohmig (2.3) | |
| [ ] | A10 | CH1 an, ESP32-Reset-Taste bzw. Upload; Scope im Single-Trigger an CLK0 | Ausgang läuft bis zur Initialisierung weiter, dann aus; Dauer notieren (2.3) | |

### B. Bedienung

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | B1 | Helligkeits-Slider ganz nach links (alt 2) | Display bleibt bei 5 % sichtbar | |
| [ ] | B2 | `+` gedrückt halten (alt 12) | erster Schritt nach 0,4 s, dann alle 0,1 s, ohne Flackern | |
| [ ] | B3 | Numpad: „500“ + MHz, „0.5“ + MHz, „10.7“ + MHz (alt 13) | Fehler, Fehler, 10,700 000 MHz | |
| [ ] | B4 | Numpad: Eingabe mit „Hz“ bzw. „kHz“ bestätigen, Finger kurz liegen lassen (alt 13a) | Kanalseite erscheint, Numpad öffnet **nicht** erneut, Schrittweite unverändert | |
| [ ] | B5 | Kanalseite: auf die Ziffer „1 kHz“ tippen, dann auf „MHz“ (alt 18a) | Schritt 1 kHz und Ziffer farbig markiert; NUMPAD öffnet sich | |
| [ ] | B6 | Sichtprüfung aller Seiten, beide Varianten (alt 15) | nichts abgeschnitten, keine Scrollbalken, kein Überlappen | |
| [ ] | B7 | Elemente an den Bildrändern (Chips „1“ und „1M“, MAIN, CONFIG) je 10× antippen, beide Varianten | jeder Tipp trifft (feste Touch-Kalibrierung) | |

### C. Frequenzgenauigkeit und Kalibrierung

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | C1 | Korrektur 0, CH1 = 10 MHz, Zähler an CLK0 (alt 3) | Abweichung ≈ angezeigte „Deviation“ + Quarzfehler | |
| [ ] | C2 | Kalibrieren: Abweichung bei 10 MHz in Hz ÷ 10 = ppm eintragen (alt 4) | danach < 1 Hz Abweichung bei 10 MHz | |
| [ ] | C3 | Nach C2: CH1 = 1 / 3,5 / 7,1 / 14,2 / 28,4 / 50 / 99,999 999 MHz | jeweils ≤ 1 Hz + Zählerfehler (Host-Test: Rechenfehler ≤ 0,375 Hz) | |
| [ ] | C4 | Korrektur +50 ppm, CH1 bei 1,9 MHz und 10,9 MHz (alt 19) | Abweichung ≈ „Deviation“ + Quarzfehler, bei beiden Frequenzen gleich gut | |
| [ ] | C5 | Korrektur um +1 ppm ändern, CH1 = 10 MHz (alt 11) | Frequenz sinkt um ≈ 10 Hz, ohne Aussetzer | |
| [ ] | C6 | CH1 = 80 MHz, Schrittweite 1 Hz, 10× `+` (alt 5) | Zähler steigt um ≈ 10 Hz und bleibt nicht hängen | |
| [ ] | C7 | Aufwärmdrift: Kaltstart, CH1 = 10 MHz, 60 min lang alle 5 min Zähler ablesen | Drift in ppm → Aufwärmzeit fürs Handbuch, Entscheidung TCXO | |
| [ ] | C8 | Kurzzeitstabilität: 10 MHz, Torzeit 1 s, 100 Werte | Standardabweichung dokumentieren | |

### D. Abstimmen und Umschalten

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | D1 | Scope auf CH1 (Trigger CH1, Nachleuchten), CH2 mehrfach verstellen (alt 6) | **kein** Aussetzer an CH1 | |
| [ ] | D2 | CH1 = 10 MHz und 99 MHz, Schrittweite 1 Hz bzw. 1 MHz, `+` halten, Nachleuchten (alt 18) | keine Lücken im Signal | |
| [ ] | D3 | CH3 an (IQ), Master in 1-kHz-Schritten verstellen, Scope im Single-Trigger auf fehlende Flanke | Aussetzer beider Ausgänge bei jedem Schritt (erwartet, Realignment); Dauer notieren (2.2) | |
| [ ] | D4 | wie D3, aber Korrektur ändern | wie D3 | |
| [ ] | D5 | Nur mit Testbuild „PLL ohne Reset“ (2.1, 2.2): Sprung 10,000 → 10,001 MHz, SDR-Wasserfall oder Zähler mit kurzer Torzeit | Einschwingzeit und Überschwingen dokumentieren; IQ-Phase danach unverändert | |

### E. IQ-Betrieb

Messmethode Phase: beide Ausgänge über gleich lange Kabel an zwei Scope-Kanäle mit 50 Ω, Zeitversatz der Flanken mit Mittelung messen, φ = Δt · f · 360°. Den Eigenversatz von Kabeln und Kanälen vorher bestimmen (dasselbe Signal über ein T-Stück auf beide Kanäle oder Kabel tauschen und mitteln).

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | E1 | Master CH1 = 10 MHz, Phase 0/90/180/270, CLK0 gegen CLK2 (alt 7) | 0°, 90°, 180°, 270° ± 1° | |
| [ ] | E2 | wie E1 bei 4,73 / 5 / 14,2 / 30 / 99 MHz (alt 8, erweitert) | alle vier Phasen; Abweichung je Frequenz eintragen | |
| [ ] | E3 | Phase 0°, 5–99 MHz: Zeitversatz CLK0 → CLK2 | konstanter Versatz in ps (Ausgangs-Skew); Phasenfehler = Δt · f · 360° | |
| [ ] | E4 | 180° per Invertierung bei ungeradem Teiler: 14,2 MHz (Teiler 63) und 99 MHz (9) gegen 10 MHz (90) | gleich genau wie bei geradem Teiler; sonst 2.2 „ungerade Teiler“ umsetzen | |
| [ ] | E5 | Tastverhältnis: CLK0 bei 7,1 MHz (fraktional) und 10 MHz (ganzzahlig), CLK2 bei 14,2 MHz (IQ, Teiler 63) | dokumentieren; die Doku verspricht 50 % | |
| [ ] | E6 | Master auf 4,72 MHz, dann 4,73 MHz (alt 9) | CH3 erst „WAIT … < 4.73 MHz“, danach automatisch aktiv | |
| [ ] | E7 | Korrektur −1 ppm, CH3 an, Master = 100 MHz (alt 13b) | Master und CH3 laufen (keine Meldung „CH3 IQ setup failed“) | |
| [ ] | E8 | 10 MHz, 90°, 30 min beobachten (Phase mit Mittelung) | Drift < 0,5° | |
| [ ] | E9 | Master 20× um 1 kHz hin und her verstellen, danach jeweils Phase messen | nach jedem Realignment wieder 90°, Streuung < 0,5° | |
| [ ] | E10 | Ausbildungsaufbau: IQ an QSD (FST3253) oder zwei Mischer mit 90°-NF-Netzwerk; Testsignal oberhalb und unterhalb des LO | Seitenbandunterdrückung messen; passt zu E1–E3 (1° ≈ 41 dB, 2° ≈ 35 dB) | |

### F. Spektrale Reinheit und Phasenrauschen

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | F1 | CH1 = 10 MHz, Drive 2/4/6/8 mA: Grundwelle und 2.–5. Harmonische | Tabelle; 3. ≈ −9,5 dBc, 2. klein bei 50 % Tastverhältnis | |
| [ ] | F2 | Ausgangspegel an 50 Ω bei 1 / 10 / 30 / 100 MHz, Drive 2/4/6/8 mA | dBm-Tabelle fürs Handbuch (LO-Pegel für Mischer) | |
| [ ] | F3 | Span 200 kHz, RBW 100 Hz: 10,000 000 MHz bei Korrektur 0 (ganzzahliger Teiler) / 7,100 000 MHz (fraktional) / 7,100 000 MHz mit CH3 an (IQ: ganzzahliger Teiler, fraktionale PLL) | Nebenlinien in dBc dokumentieren → Entscheidung Low-Noise-Modus (2.1) | |
| [ ] | F4 | wie F3 bei 10 MHz, Korrektur 0 gegen +0,01 ppm | zeigt den Wechsel ganzzahlig → fraktional | |
| [ ] | F5 | Phasenrauschen L(f) bei 10 und 14,2 MHz an 100 Hz / 1 kHz / 10 kHz / 100 kHz Ablage (Rauschmarker) | Tabelle; Eigenrauschen des Analysators daneben notieren | |
| [ ] | F6 | 10 MHz, Span 20 kHz, RBW 10 Hz: a) Backlight 100 / 80 / 5 %; b) Neuzeichnen (`+` halten); c) Zero-Span auf dem Träger: Störung im 2-s-Takt (I2C-Überwachung)?; d) Versorgung PC-USB gegen Powerbank | Seitenlinien bei ±5 kHz (PWM) und periodische Störungen dokumentieren (2.1) | |
| [ ] | F7 | CH1 = 10 MHz an; CH2 = 10,1 MHz aus, dann an; Spektrum an CLK0 | Übersprechen bei 10,1 MHz in dBc, mit CH2 aus und an (2.1) | |
| [ ] | F8 | Spektrum CLK2 gegen CLK0 bei 4,73 MHz und 10 MHz (alt 10) | keine zusätzlichen Nebenlinien an CLK2 | |
| [ ] | F9 | Nahfeldsonde über Display und ESP32, 1–160 MHz | Störlinien der Plattform (z. B. 40-MHz-SPI-Takt und Vielfache) dokumentieren; Abstand für Empfängerversuche | |

### G. Mischer-Versuche (Ausbildung)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | G1 | Diodenringmischer: LO = CH1 10 MHz (Pegel nach F2 auf +7 dBm), RF = CH2 10,455 MHz über 30 dB Dämpfung | ZF bei 455 kHz und 20,455 MHz, Konversionsverlust ≈ 6–7 dB; LO-Durchschlag dokumentieren | |
| [ ] | G2 | CH1 = 10,000 MHz und CH2 = 10,001 MHz an den Mischer, ZF an Soundkarte (FFT) | sauberer 1-kHz-Ton; Seitenlinien und Rauschsockel dokumentieren (Phasenrauschen beider Kanäle) | |

### H. Nach der Bereichserweiterung (2.4)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | H1 | CH1 = 8 kHz, 455 kHz, 999,999 kHz, 1 MHz, Zähler (alt 20) | Frequenz stimmt, Anzeige in kHz bzw. MHz | |
| [ ] | H2 | CH1 = 112,499 996 / 112,500 004 / 120 / 160 MHz, Zähler und Spektrum (alt 21) | Abweichung ≤ 1,5 / 2 / 2 / 3 Hz (2.4), Ausgang vorhanden (Pegel geringer) | |
| [ ] | H3 | CH3 an, Master von 100 MHz auf 100,000001 MHz stellen (alt 22) | CH3 wechselt auf „WAIT … > 100 MHz“, CLK2 aus, Master läuft weiter | |
| [ ] | H4 | CH1 = 455 kHz, Spektrum bis 5 MHz | Oberwellen dokumentieren → Tiefpass-Hinweis im Handbuch | |

### I. Display-Erkennung (eine Firmware, beide Boards)

I0 (Diagnose-Sketch, 2026-10-28, beide Boards, je 2 EN-Resets mit 3 Software-Neustarts, 50 Wiederholungen je Lesetakt) ist erledigt; die Messwerte stehen in Spezifikation 43. Nach dem Flashen der Einheitsfirmware: 5/5 EN-Resets je Board richtig erkannt (COM13 Original-CYD: ILI9341, ID 000000; COM7 CYD2USB: ST7789, ID 8181B3).

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [x] | I0 | Diagnose: MADCTL-Rücklesen, RDDID, Pull an GPIO12, SPI-Modus 0/3, Schreiben mit 40 MHz | Rücklesen auf beiden Boards ohne Pull; IDs 000000 / 8181B3; MODE0 fehlerfrei | siehe Spez. 43 |
| [x] | I1 | Original-CYD einschalten | Log „ILI9341 (detected)“; Bild, Farben, Ausrichtung und Touch-Ecken wie 1.2.0 | 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B7 |
| [x] | I2 | CYD2USB einschalten | Log „ST7789 (detected)“; Bild, Farben, Ausrichtung und Touch-Ecken wie mit der früheren `usbc`-Firmware | 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B7 |
| [ ] | I3 | Je Board 20× Kaltstart (Stecker) und 20× EN-Taste | 40/40 richtig erkannt, nie Rauschen | |
| [ ] | I4 | Boot-Geste auf dem CYD2USB | Beleuchtung blinkt 2×, Rauschen (ILI9341), Log „boot gesture“; zweite Geste: wieder ST7789, `disp_panel` = 0 (Log „detected“) | |
| [ ] | I5 | Boot-Geste länger als 3 s halten, dann loslassen; außerdem nur 1 s berühren und weiter halten, bis die Hauptseite erscheint | kein Tipp landet in der GUI | |
| [ ] | I6 | Kurz berühren beim Einschalten (< 1,5 s) | keine Umschaltung | |
| [ ] | I7 | CYD2USB, 30 min Bedienung (Seitenwechsel, Numpad, `+` halten) | keine Pixelfehler (SPI-Modus 0 mit 40 MHz) | |
| [ ] | I8 | Testplan A und B sowie Messplan F6 mit der Einheitsfirmware auf beiden Boards | unverändert | |

### J. SDA/SCL-Erkennung

Die Identitätsprüfung (Register 0, 183, Rücklesen Register 3) folgt AN619; J7 prüft sie am eigenen Modul und, wenn möglich, an einem Klon (MS5351M). Lehnt J7 ein echtes Modul ab, die harten Prüfungen in `SI5351Manager::begin()` lockern.

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | J1 | Kabel gerade (SDA→22) | verbunden, Log „SDA=22 SCL=27“, kein Dialog | |
| [ ] | J2 | Kabel vertauscht | verbunden, Log „(swapped)“, 10 MHz an CH1 am Zähler | |
| [ ] | J3 | Boot ohne Modul, dann gerade anstecken; danach Boot ohne Modul, vertauscht anstecken | Dialog schließt sich, Ausgänge ≤ 2,5 s nach dem Anstecken | |
| [ ] | J4 | CH1 + CH3 IQ 270°, im Betrieb abziehen und vertauscht wieder anstecken | ein Dialog „Connection lost“, danach Phase an 2 Scope-Kanälen korrekt | |
| [ ] | J5 | Wackelkontakt 30 s | keine Fehlprogrammierung, eine Logzeile pro Zustandswechsel | |
| [ ] | J6 | SDA nach GND, dann SCL nach GND; Logikanalysator, Rückgabecode im Debug-Log | erwartet Code 5 → Dialog „I2C line stays low“; GUI stockt < 100 ms; nach Entfernen verbunden. Liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen | |
| [ ] | J7 | Registerdump 0, 3, 183 nach dem Einschalten (eigenes Modul, wenn möglich auch MS5351M) | Werte notieren; Modul wird angenommen, Log ohne „Unexpected reg183“ (sonst Klon-Warnung) | |
| [ ] | J8 | Modul ohne Pull-ups | Erkennung bei 100 kHz; Betrieb mit 400 kHz stabil oder Log „I2C fails at 400 kHz; staying at 100 kHz“ und Betrieb mit 100 kHz stabil | |
| [ ] | J9 | 10 min ohne Modul, Heap im Debug-Log | freier Heap konstant (ein `Wire.end()`/`begin()` pro 2 s), keine wiederholten Logzeilen | |
| [ ] | J10 | CYD2USB: CN1-Belegung prüfen, J1/J2 wiederholen | CN1 führt GPIO22/27, wie J1/J2 | |
| [ ] | J11 | Logikanalysator bei vertauschtem Kabel | am Chip nur START/STOP und Fragmente, keine Registeränderung | |
| [ ] | J12 | 3,3 V offen, GND verbunden | „not found“, „line stays low“ oder „not an SI5351“, kein Teilschreiben; nach Verbinden von 3,3 V verbunden (ohne Abziehen) | |
| [ ] | J13 | GND offen, 3,3 V verbunden | wie J12 | |
| [ ] | J14 | Debug-Build mit Dauerlesen; während der Lesezugriffe 20× EN-Reset | nach jedem Boot verbunden (Leitungsfreigabe beim Start greift) | |
| [ ] | J15 | Fremdes I2C-Gerät mit Adresse 0x60 (z. B. anderer Sensor) an CN1 | Dialog „not an SI5351“, höchstens ein Schreibzugriff (Register 3), keine weiteren Dialoge | |
