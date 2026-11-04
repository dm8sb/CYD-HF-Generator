# TODO – Code- & Design-Review CYD HF Generator

Stand: 2026-11-03 · Firmware 1.3.0 · Einsatzziel: Experimente mit HF-Mischern und Amateurfunkausbildung; wichtig sind geringes Phasenrauschen und genauer IQ-Betrieb.

Prioritäten: **P0** = sofort / Voraussetzung · **P1** = Fehler / sichtbares Problem · **P2** = deutliche Verbesserung · **P3** = Aufräumen / nice-to-have
Status: `[ ]` offen · `[~]` teilweise · `[?]` wartet auf Rückfrage (Abschnitt 9) · `[M]` wartet auf Messung ([MESSPLAN.md](MESSPLAN.md))

Erledigte Punkte sind entfernt; sie stehen in der Git-Historie dieser Datei. Seit dem Review (1.2.0) umgesetzt: Host-Unit-Tests, SDA/SCL-Erkennung (Messplan J), Display-Erkennung mit einer Firmware für beide Boards (Messplan I) und in 1.3.0 die CONFIG-Optionen Tuning / Spread / Off state, scrollbare CONFIG-Seite mit Info-Karte, Backlight in 10-%-Schritten, Anzeige der tatsächlichen Frequenz, Aufräumen und der Umbau von Spezifikation und Handbüchern (Messplan K). Die Entscheidungen dazu stehen in der Spezifikation.

Arbeitsreihenfolge, Zusammenfassung und Aufgabenliste mit je einem Commit pro Schritt: [ARBEITSPLAN.md](ARBEITSPLAN.md). Messungen am Gerät: [MESSPLAN.md](MESSPLAN.md).

Audit 2026-11 ([AUDIT_2026-11.md](AUDIT_2026-11.md)): Code, Security, Bedienung und Updates bewertet; die Befunde stehen mit ihrer ID (C-n Code, S-n Security, U-n Bedienung, P-n Plattform) in den Abschnitten unten, die Reihenfolge der Arbeit in Abschnitt 0a. Geplantes Feature WEB-HMI: [KONZEPT_WEB_HMI.md](KONZEPT_WEB_HMI.md) (nach dem Audit überarbeitet). **Umgesetzt ist davon noch nichts.**

---

## 0. Ergebnis des Reviews (Kurzfassung)

Die Firmware ist klar geschichtet (Treiber / AppController / GUI / Services). Die Fehlerpfade (NO HW, ERR, Wiederverbindung, NVS) sind durchdacht. Der eigene SI5351-Treiber schreibt nur geänderte Register und setzt die PLL nur bei Bedarf zurück; im Bereich 1–100 MHz ist er genauer als dokumentiert (Host-Test: ≤ 0,375 Hz Rechenfehler). Für Mischerversuche und Ausbildung fehlen vor allem Messdaten zu Nebenlinien, Phasenrauschen und IQ-Phasenfehler; die Betriebsarten „Low noise“ und „Integer“ (1.3.0) sind dafür vorbereitet (Messplan K5).

| # | Befund | Abschnitt |
|---|---|---|
| 1 | IQ-Abstimmen setzt bei **jedem** Schritt die PLL zurück, beide Ausgänge setzen kurz aus. Mit festgehaltenem Teiler ginge es ohne Reset und ohne Phasenverlust. | 2.2 |
| 2 | 54 % der IQ-Teiler sind ungerade (14,2 MHz → 63, 99 MHz → 9). Ob 180°/270° per Invertierung dort stimmen, hängt vom Tastverhältnis ab und ist ungeprüft. | 2.2, E4 |
| 3 | Bereichserweiterung: Um 112,5 MHz weicht der Ausgang bei Korrektur 0 um bis zu 5,1 Hz ab. Fix ist ausgearbeitet und per Host-Test belegt. | 2.4 |
| 4 | Mögliche Störquellen der Plattform sind ungemessen: Backlight-PWM 5 kHz, I2C-Überwachung alle 2 s, Display-SPI 40 MHz, weiterlaufende MultiSynths abgeschalteter Kanäle. | 2.1, F |
| 5 | Audit: Der Header zeigt die Anforderung statt des Ist-Zustands, signalverändernde Einstellungen sind auf den Arbeitsseiten unsichtbar, mehrere Touch-Ziele sind klein oder liegen zu dicht (U-1 … U-7). | 3 |
| 6 | Audit: Plattform ohne Support (ESP-IDF 4.4, mbedTLS 2.28, LVGL 8.3); das offizielle `espressif32@8.0.0` bringt Arduino 3.3.12 / IDF 5.5.5. | 1 |
| 7 | Audit: Fast der gesamte Messplan ist noch nie am Gerät gelaufen; mehrere Entscheidungen warten auf Messungen. | [MESSPLAN.md](MESSPLAN.md) |

---

## 0a. Reihenfolge (Audit 2026-11, Abschnitt 5; Messkampagne nach Entscheidung vom 2026-11-04 ans Ende verschoben)

**Entschieden 2026-11-04 (Audit D9):** Erst Qualität, Bedienung und WEB-HMI (Phasen 0–6), dann die Messkampagne (Phase 7, [MESSPLAN.md](MESSPLAN.md)). Zwischen den Phasen läuft nur der Smoke-Test (MESSPLAN 4.3). Die Referenz 1.3.0 wird als Tag und Binärdatei gesichert, damit R0 später nachgemessen werden kann. Folgen der Messungen kommen als Version 1.4.x bzw. 1.5.0.

Jede Phase endet mit den Gates aus Audit 5.8: Host-Tests in `native-win` und `native` grün, 0 Warnungen mit `-Wall -Wextra` in `src/`, Review (bei Netz-Code gegen die Sicherheitsanforderungen W-S), Smoke-Test auf beiden Boards, Handbuch DE + EN im selben Commit.

| Phase | Inhalt | Abschnitte hier | Aufwand |
|---|---|---|---|
| 0 | Hygiene und Basis: PlatformIO 6.1.5 aus dem PATH, CI, `-Wall -Wextra`, Abhängigkeiten auf Commit, Heap-Debugausgabe; Firmware 1.3.0 als Tag und Binärdatei sichern (Referenz R0) | 1, 4 | 0,5 d |
| 1 | Robustheit 1.3.1: Fehler C-1 … C-13, Assert-Handler und Loop-WDT, Parser, Doku-Abgleich, Tests T-1 … T-9 | 2.3, 4, 5, 7 | ≈ 2 d |
| 2 | Bedienung 1.3.2: U-1 … U-10, U-12, U-14, U-15, U-17 … U-19, Handbuch U-23, GUI-Logik testbar (T-10 … T-14); Test-Builds für B8 … B12 | 3, 5, 7 | ≈ 3–4 d |
| 3 | Laborbetrieb 1.3.3: Sperre, Grundzustand, Kalibrieren, CH3 mit Masterfrequenz (U-11, U-13, U-16, U-20, U-22); optional Bereichserweiterung 2.4 | 2, 3 | ≈ 2–3 d |
| 4 | Migration Core 3.x (Konzept WEB-HMI Abschnitt 4) | 1 | ≈ 2,5–3 d |
| 5 | WEB-HMI-Vorarbeiten V-1 … V-6, V-11 (Konzept Abschnitt 5) | 6 | ≈ 4 d |
| 6 | WEB-HMI (Konzept Abschnitt 11, Schritte 0–9) mit W-S1 … W-S25; Test-Builds für Q4, Q5 | 6 | ≈ 15–16 d |
| 7 | **Messkampagne** ([MESSPLAN.md](MESSPLAN.md)): Vorbereitung, R0, R1 (A bis K, L, L-S, Q), Bedientests mit Testpersonen, Auswertung und Entscheidungen | MESSPLAN.md | ≈ 9 Arbeitstage + Auswertung |
| 8 | später und Folgearbeiten aus den Messungen: DMA, IQ ohne Aussetzer, Phasen-Feinabgleich, Sticky-Bit, Störquellen-Abhilfen, LVGL 8.4, Unity 2.7, Arduino 3.3.13, Erweiterungen aus 6 | 2, 4, 6 | – |

## 1. Build, Tests & Hardware-Varianten

- [ ] **P1 CI** (Phase 0): GitHub Action mit `pio test -e native` und `pio run` (eine Firmware-Umgebung); `env:native` (Linux, gcc) ist noch nie gelaufen. Actions per SHA gepinnt, `permissions: contents: read`, kein `pull_request_target` mit PR-Checkout (S-27). Später Fuzzing-Job (Konzept WEB-HMI W-S25).
- [ ] **P2 Abhängigkeiten prüfen** (S-20): Plattform-Prüfsumme in der CI; Advisory-Check je Release (XPT2046 ist seit S05 auf den Commit gepinnt).
- [ ] **P2 Migration Arduino-Core 3.x** (Phase 4, [Konzept WEB-HMI Abschnitt 4](KONZEPT_WEB_HMI.md)): Ziel offizielles `platformio/espressif32@8.0.0` (Arduino 3.3.12, IDF 5.5.5; Spike gegen pioarduino 55.03.312-1). Grund ist vor allem Sicherheit: IDF 4.4 und mbedTLS 2.28 sind ohne Support (S-19, P-4). Teilaufgaben: LEDC auf `ledcAttach()` (auch im Fake), Wire auf dem neuen `i2c_master`-Treiber mit gebündelten Transfers in `I2CBus` (C-14), TFT_eSPI 2.5.43 früh bauen und testen, A/B-Umgebungen bis zur Abnahme, GCC-14-Warnungen beheben (`src/` baut seit S05 mit `-Wall -Wextra`), Host-Test-Umgebung mit `-std=gnu++17` (V-9); Messplan A, B, I, J gegen die Basis aus Phase 0 wiederholen.
- [ ] **P4 Updates später** (P-6, P-8): LVGL 8.4.0 (nur Fixes, optional), Unity 2.7.0 sobald in der Registry, Arduino 3.3.13 / IDF 5.5.6 sobald erschienen. Nicht empfohlen: LVGL 9, espressif32 6.13.0, LovyanGFX außer als Fallback.

## 2. SI5351 / HF-Funktion

### 2.1 Phasenrauschen und Nebenlinien

- [M] **P2 MultiSynth abgeschalteter Kanäle abschalten**: CH1/CH2 werden im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter. CLK2 bekommt bei CH3 aus `CLK2_PDN`, aber nur bei Off state „Low“; bei „High“/„High-Z“ muss der Ausgangstreiber den Pegel halten. Welchen Pegel ein Ausgang mit `CLKx_PDN` hat, zeigt Messplan K2; ist er hochohmig, bei „High-Z“ alle abgeschalteten Ausgänge mit `CLKx_PDN` abschalten. Messplan F7 zeigt, ob es sich lohnt.
- [M] **P2 Störquellen der Plattform**: Die Backlight-PWM mit 5 kHz ([backlight.cpp:7](src/drivers/backlight.cpp#L7)) kann über die Versorgung Seitenlinien bei ±5 kHz erzeugen. Abhilfe: PWM ≥ 20 kHz (LEDC schafft bei 8 bit bis 312 kHz) oder 100 % ohne PWM. Die Überwachung liest alle 2 s Register über I2C ([app_controller.cpp:32](src/core/app_controller.cpp#L32)). Sind Störungen im 2-s-Takt sichtbar, hilft ein „Ruhemodus“ (Überwachung nur bei Bedienung). Die CONFIG-Seite schreibt alle 300 ms dieselben Labels neu und erzeugt damit SPI-Verkehr; nur bei Änderung schreiben. Messplan F6.
- [ ] **P3 Hardware-Hinweise** (Handbuch, ggf. Aufbau): SI5351 über einen eigenen LDO mit Ferrit aus 5 V versorgen, nicht aus dem 3,3-V-Netz des ESP32; kurze Masse; Abschirmung; optional 25-MHz-TCXO statt Quarz (Entscheidung nach Messplan C7).

### 2.2 IQ-Betrieb

- [M] **P2 IQ-Abstimmen ohne Aussetzer**: Jede Frequenzänderung im IQ-Betrieb schaltet beide Ausgänge ab, setzt die PLL zurück und schaltet sie wieder ein (`setupIQPair()` in [si5351_manager.cpp](src/drivers/si5351_manager.cpp)); im Host-Test ergeben 1000 Schritte à 100 Hz 1000 PLL-Resets. Die Phasenlage ist aber ein Versatz in VCO-Takten zwischen zwei MultiSynths an derselben PLL. Ändert sich nur die PLL, bei gleichem Teiler und Offset, bleibt sie erhalten. Vorschlag: Teiler mit Hysterese festhalten (bei 10 MHz deckt Teiler 90 den Bereich 6,67–10 MHz ab) und dann nur die PLL ohne Reset schreiben, wie es „Low noise“ im Einzelbetrieb schon tut (`planIQ()` in `si5351_plan.*`). Am Gerät prüfen (Messplan D3, D5/K6, E9). Damit entfällt auch der Aussetzer bei jeder Korrekturänderung.
- [M] **P2 180°/270° bei ungeraden Teilern**: Die Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°. Der IQ-Teiler ist `min(127, ⌊900 MHz / f⌋)` (`planIQ()` in [si5351_plan.cpp](src/drivers/si5351_plan.cpp)) und in 54 % der Fälle ungerade (4,73–5 MHz: 127, 14,2 MHz: 63, 99 MHz: 9). Zeigt Messplan E4 eine Abweichung: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, darüber bleibt nur 9. Alternativ 180° über den Phasen-Offset (2 × Teiler ≤ 127) statt über die Invertierung.
- [ ] **P2 Phasen-Feinabgleich und freie Phase**: Der Offset hat eine Auflösung von T_VCO/4, also 90° / Teiler (10 MHz: 1°, 30 MHz: 3°, 99 MHz: 10°). Mit Offset auf Master oder Folger (je 0–127) und der Invertierung ist jede Phase in diesem Raster einstellbar. Nutzen: Ausgangs-Skew und Kabellaufzeit ausgleichen (Messplan E3) und Ausbildungsversuche zu Spiegelunterdrückung gegen Phasenfehler. Die CH3-Seite zeigt dann die erreichbare Auflösung.

### 2.3 Ausgänge und Robustheit

- [ ] **P3 SI5351 läuft beim Flashen und nach ESP32-Reset weiter**: Der SI5351 hat keinen Reset-Eingang. Nach Reset-Taste oder Upload behält er die letzte Einstellung, bis `begin()` die Ausgänge abschaltet, beim Flashen also einige Sekunden lang. Im Handbuch erwähnen (Verstärker oder Mischer angeschlossen), Messplan A10.
- [ ] **P3 Spannungseinbruch über Sticky-Bit erkennen**: `checkAlive()` erkennt einen Neustart des SI5351 daran, dass die Steuerregister vom Cache abweichen. Das funktioniert, solange mindestens ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen. Dasselbe Register meldet `LOL_A` / `LOL_B` (PLL nicht eingerastet); als Zeile „PLL locked“ in der Info-Karte auf CONFIG anzeigen.
- [ ] **P3 VCO bei positiver Korrektur leicht über 900 MHz** (Tuning „Fractional“): Die feste PLL 36 × Quarz ergibt bei +50 ppm 900,045 MHz. Praktisch unkritisch. (Integer hält 600–900 MHz seit S14 auch bei ±50 ppm ein, C-7.)
- [ ] **P2 Heap auf der Info-Karte** (V-8, Rest von C-3): freier/minimaler Heap und LVGL-Heap auf der CONFIG-Info-Karte; kommt mit der Info-Karten-Erweiterung der WEB-HMI (Arbeitsplan S53). Assert-Handler und Loop-Watchdog sind seit S09 umgesetzt.
- [ ] **P2 Dirty je Schlüssel** (Rest von C-5, V-4): Bei einer echten Änderung schreibt das Autosave weiterhin alle 11 Schlüssel; nur geänderte schreiben (Arbeitsplan S45). Ein/Aus und unveränderte Werte speichern seit S11 nicht mehr.

### 2.4 Frequenzbereich CH1/CH2 auf 8 kHz – 160 MHz erweitern (Entscheidung 2026-10-27)

- [ ] **P2 Bereichserweiterung**. Der Treiber kann den Bereich schon (`OUT_MIN_HZ` / `OUT_MAX_HZ`, unter 1 MHz mit R-Teiler). Teilaufgaben:
  - **Zuerst Treiberfehler um 112,5 MHz beheben** (Tuning „Fractional“, `planStandalone()` in [si5351_plan.cpp](src/drivers/si5351_plan.cpp)): Bei Korrektur 0 rundet `approxRatio()` den MultiSynth auf genau 8/1. Dann greift `p.ms.a < 8` nicht, der Weg „gerader Integer-Teiler + fraktionale PLL“ wird nicht versucht, und z. B. 112 500 004 Hz liegt 5,1 Hz daneben. Zusätzlich setzt der Zweig „aktuelle PLL behalten“ `bestErr` nicht. Erprobter Fix (Stand 1.2.0, mit den damaligen Namen; seit 1.3.0 heißen sie `p.pll`, `p.ms`, `p.rLog2`, `synthesizedHz()`, und die VCO-Grenzen kommen aus `range` mit Spread):

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
  - **CH3 bleibt bei 4,73 – 100 MHz**: `iqConfigured_()`, `iqMaxHz()` und `Ch3Status::FreqTooHigh` mit „WAIT … > … MHz“ gibt es seit 1.3.0 (Spread); nur einen Testfall mit Master > 100 MHz ergänzen.
  - Numpad: maximale Länge 9 → 10 Zeichen (z. B. „150.000001“ MHz).
  - Anzeige unter 1 MHz in kHz (z. B. „455.000 kHz“) statt „0.455 000 MHz“ ([frequency_utils.cpp:16-29](src/utils/frequency_utils.cpp#L16-L29)); `digitExponent()` und die Tests in `test_frequency_utils` anpassen.
  - Hinweis auf der Kanalseite: über 100 MHz geringerer Pegel, unter 1 MHz starke Oberwellen (Tiefpass nötig).
  - Handbuch DE (und Übersetzung EN), Spezifikation (Anforderungen A-…, Entscheidung E-16) und README nachziehen. Messplan H1–H4.

## 3. Bedienung (UX)

Befunde und Rechnungen (Pixel, mm, Kontraste, Tippzahlen) stehen im Audit, Abschnitt 2.3. Reihenfolge: Phase 2 (schnelle Verbesserungen), Phase 3 (Laborbetrieb).

- [ ] **P2 Bildschirmsperre und Grundzustand** (U-13, war P3): Sperre durch 2 s Halten mit Anzeige „LOCK“ (unterdrückt auch Neuzeichnen, 2.1); „Reset to defaults (keep calibration)“ mit Bestätigung; **Werksreset** (NVS löschen inkl. künftigem `hfnet`) als weitere Zeile auf der CONFIG-Seite.
- [ ] **P2 Kalibrieren** (U-11): heute Kopfrechnen und ≈ 20 Tipps → Wert antippen öffnet das Numpad (ppm mit Vorzeichen); Assistent siehe 6.
- [ ] **P3 Masterfrequenz und Master-Schalter auf der CH3-Seite** (U-16), Kanalwahl im Header (CH1 → CH2 heute 2 Tipps).
- [ ] **P3 Wiederholung beim Halten** (U-20): beschleunigen, Druckaussetzer bis 60–80 ms überbrücken. (Die übrigen kleineren Punkte U-12, U-15, U-17, U-21 sind mit S26 erledigt.)

## 4. Code-Qualität & Wartbarkeit

- [ ] **P3 Feste Koordinaten in den Pages**: Header (158/214), CH3 (114/134/186) usw. in [gui_layout.h](src/gui/gui_layout.h) bzw. als lokale Konstanten zusammenfassen, wie in [page_main.cpp](src/gui/pages/page_main.cpp) schon geschehen.
- [ ] **P2 Display-Ausgabe mit DMA und Doppelpuffer** (Entscheidung 2026-10-28: ausgearbeitet, in 1.3.0 noch nicht umgesetzt). Heute: ein Puffer 320 × 24 px, LVGL rendert und `DisplayPanel::flush()` sendet abwechselnd mit `pushColors(…, swap = true)`; ein ganzer Bildschirm braucht allein auf dem Bus ≈ 31 ms. Plan:
  - Zwei Puffer à 320 × 40 px (je 25,6 KB, statisch, `DMA_ATTR`; `pushPixelsDMA()` erlaubt < 32 767 Pixel), `LV_COLOR_16_SWAP` = 1 und `setSwapBytes(false)`, damit die CPU die Bytes nicht mehr tauscht.
  - In `DisplayPanel::begin()` erst Erkennung, `tft_.init()`, ST7789-Init, Rotation, `fillScreen()` (alles noch mit rohem Buszugriff), dann `tft_.initDMA()` (TFT_eSPI 2.5.43: `HSPI_HOST`, DMA-Kanal 1) und einmal `tft_.startWrite()`; CS bleibt LOW, `endWrite()` nie aufrufen (würde CS während einer DMA-Übertragung hochziehen). Danach kein `SpiPanelBus`-Zugriff und kein direktes Zeichnen ohne `dmaWait()`.
  - `flush()`: `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`; `lv_disp_flush_ready()` sofort (LVGL rendert in den anderen Puffer; der erste wird erst nach dem nächsten `flush()` mit seinem `dmaWait()` wieder beschrieben).
  - Erwartung ≈ max(Rendern, Senden) statt Summe, bis ≈ 30 Bilder/s; +36 KB RAM. Messung vorher/nachher mit `LV_USE_PERF_MONITOR` beim Scrollen auf CONFIG, `+` halten und Seitenwechsel; 30 min Bedienung ohne Pixelfehler.

## 5. Dokumentation

- [ ] **P2 Anwendungskapitel „Mischer-Experimente“** in beiden Handbüchern: Ausgangspegel je Drive-Stärke in dBm an 50 Ω (aus Messplan F2); Oberwellen eines Rechtecks (3. ≈ −9,5 dBc, 5. ≈ −14 dBc) und passende Tiefpässe; LO-Pegel für Diodenringmischer (+7 dBm) bzw. Dämpfung für NE612/SA612; IQ direkt an FST3253/74CBT3253 (Tayloe-Detektor); gleich lange Kabel bei IQ (1° bei 100 MHz ≈ 28 ps ≈ 5,6 mm Koax); Zusammenhang Phasenfehler ↔ Spiegelunterdrückung (1° ≈ 41 dB, 2° ≈ 35 dB, 5° ≈ 27 dB); Rechtecksignal nie ungefiltert an eine Antenne.
- [M] **P2 „50 % Tastverhältnis“** (Handbücher 1) ist nicht belegt. Nach Messplan E5 präzisieren.
- [ ] **P2 Handbuch ausbauen** (U-23): Inhaltsverzeichnis, einseitige Kurzanleitung für den Laborplatz, Seitenbilder, Glossar „Anzeige am Gerät → Bedeutung“ (NO HW, ERR, Corr., SSC, Out, Off state …), Symptomtabelle („kHz-Abweichung → Integer“, „breites Spektrum → Spread“, „DC bei OFF → Off state“); dass die Schrittweite für CH1 und CH2 gemeinsam gilt. Hinweise „physischer Zugriff = Vollzugriff“ (S-22) und „ein Fremdgerät an 0x60 bekommt einen Schreibzugriff“ (S-24).

## 6. Erweiterungen für Experimente und Ausbildung

- [ ] **P2 WEB-HMI** ([KONZEPT_WEB_HMI.md](KONZEPT_WEB_HMI.md), Zielversion 1.4.0, ≈ 20–21 d): WLAN (Client / Access Point mit Zufallspasswort und QR, Captive-DNS), Weboberfläche mit Tastatur, REST, SCPI über TCP 5025 und USB-Seriell (ersetzt die frühere Idee „Fernsteuerung über USB-Seriell“), Statusleiste auf MAIN, Option „Remote outputs: allowed / local only“, Identify. Voraussetzungen: Phase 4 (Migration) und Phase 5:
  - **V-1** Ergebniscodes statt stillem Klemmen, Kanalindex strikt (S-12);
  - **V-2** Änderungszähler `revision()`;
  - **V-3** Ereignisse statt `ErrorCallback`;
  - **V-4** Speichern je Quelle (5 s / 30 s) und je Schlüssel, `saveNow()`, `*RST`-Semantik (C-17);
  - **V-5** Logger umschaltbar, weil SCPI über USB dieselbe Schnittstelle nutzt (C-16);
  - **V-6** Ausführer und Schnappschuss;
  - **V-11** gehärteter Parser.
  Sicherheitsanforderungen W-S1 … W-S25 (Konzept Abschnitt 10), Tests Abschnitt 12, Messplan L und L-S.
- [?] **P2 Info-Seite „Synthesizer“**: VCO, PLL a + b/c, MultiSynth a + b/c, R-Teiler, Integer/Fraktional, Phasenregister, PLL-Lock. Zeigt, wie der Synthesizer rechnet, und erklärt beobachtete Nebenlinien.
- [?] **P2 Freie Phase in 90°/Teiler-Schritten** (2.2).
- [?] **P2 Gekoppelter Betrieb CH2 = CH1 ± Offset**: Demonstration Überlagerungsempfänger (LO folgt der Empfangsfrequenz mit fester ZF, z. B. 455 kHz / 10,7 MHz).
- [?] **P3 Speicherplätze** mit Vorgaben für Amateurfunkbänder und übliche ZF (Rückfrage 9.1 a).
- [?] **P3 Wobbeln** mit Start per Taster oder GPIO35 (nur Eingang) (Rückfrage 9.1 b).
- [ ] **P3 Kalibrierassistent** (U-11): gemessene Frequenz eingeben, Korrektur wird berechnet (6 statt 20 Tipps, ohne Kopfrechnen).
- [ ] **P3 Modulationsarten** (aus der alten Spezifikation, Abschnitt „Future Improvement Ideas“): z. B. AM/FM-Testsignale über den PLL-Bruchteil.
- [ ] **P3 Externe Referenz**: SI5351C mit `CLKIN` für 10 MHz vom Messplatz oder GPSDO, alternativ TCXO (2.1).

## 7. Unit-Tests

Umgesetzt: `pio test -e native-win` (Windows) bzw. `pio test -e native` (gcc). Stand 2026-11-03 (1.3.0): 161 Tests, 160 bestanden, 1 übersprungen (bekannter Fehler 2.4). Laufzeit ≈ 15 s. Gegenproben mit absichtlich eingebauten Fehlern (Vorzeichen der Korrektur, Phasen-Offset −1, Erkennungsregeln, Fremdgerät-Vermerk) ließen die betroffenen Tests fehlschlagen.

| Suite | Tests | Inhalt |
|---|---|---|
| `test_frequency_utils` | 20 | Formatierung (MHz, Schritt, ppm, Zeile „Out …“, Spread-Bezeichnung), Parser (Einheiten, Grenzen, Rundung, ungültige Eingaben), Schrittweiten, Ziffer ↔ Schritt |
| `test_si5351` | 53 | Init, Identitätsprüfung und Überwachung; Einzelbetrieb in allen drei Tuning-Modi (Genauigkeit 1–100 MHz mit Korrektur 0 / +9,7 / ±50 ppm, Schwachstellen, R-Teiler, Hysterese ohne PLL-Reset in Low noise, Rasterpunkte und `FBx_INT` in Integer); Spread Spectrum (Register 149–161 zurückgerechnet, PLL B unberührt, VCO mit Modulation in 600–900 MHz); Off state (Register 24, Power-down-Strategie); IQ in allen Modi; Registergrenzen nach AN619 |
| `test_si5351_plan` | 19 | Planungsfunktionen ohne I2C: feste PLLs, Hysterese, Raster, VCO-Fenster mit Spread, IQ-Bereich `iqMinHz()` / `iqMaxHz()` |
| `test_app_controller` | 49 | Bootzustand, Schalten, Abstimmen, Grenzen, Drive, Korrektur, Backlight-Stufen; CH3 (Warten, Nachführen, Masterwechsel, IQ-Bereich mit Spread); CONFIG-Optionen (Standard, Speichern, ungültige NVS-Werte, Integer sperrt Spread); SDA/SCL-Erkennung (gerade, vertauscht, Hot-Plug, Fremdgerät, Leitung low, 400 kHz zu schwach); Autosave und NVS |
| `test_display_detect` | 20 | Erkennung gegen ein Panel-Modell mit dem gemessenen Leseverhalten, Entscheidung (Build-Flag, Erkennung, Standard), ST7789-Init-Tabelle, Löschen des alten NVS-Schlüssels `disp_panel` |

Aufbau: [test/fakes](test/fakes) ersetzt Arduino-Core, `Wire` (SI5351-Registermodell mit Schreibprotokoll, Fehlerinjektion, Pinbelegung, Fremdgerät) und `Preferences`. [test/helpers/si5351_model.h](test/helpers/si5351_model.h) dekodiert die geschriebenen Register unabhängig vom Treiber zurück in Frequenzen, Spread-Parameter und Off state und prüft die Grenzen aus AN619; [test/helpers/panel_model.h](test/helpers/panel_model.h) bildet die Display-Controller nach.

Weitere Vorschläge:

- [ ] **P2** Mit IQ ohne Reset (2.2): Test „PLL-Änderung ohne Reset, Phasen-Offset und Teiler unverändert“.
- [ ] **P1** Weitere Tests aus dem Audit (Phase 1): Bootreihenfolge/Änderungszähler (T-2), `millis()`-Überlauf (T-3), Parser-Sonderfälle `nan`, `inf`, `1e7`, `0x…`, `1..2`, `.`, leer (T-4), NVS-Fehler beim Teilspeichern und Retry (T-5), Fehlerinjektion `Wire.end()`/`begin()` (T-6), nur ON/OFF → kein NVS-Schreiben (T-7), Neuverbindung mit Spread (T-8), Integer-VCO bei ±50 ppm (T-9).
- [ ] **P2** GUI-Logik testbar machen (Phase 2, T-10 … T-15; 45 % der `src`-Zeilen haben heute keinen Host-Test): `NumpadInput` (ein Dezimalpunkt, Länge, ⌫/C, Einheiten, Grenzen), Ziffer-Antippen (`charIndexAt`, `stepMarkup`), Message-Queue, Textbausteine (`formatIqLimit`, Speicherzustand, Dropdown-Index ↔ Enum mit `static_assert`), Touch-Filter und Schwelle der Boot-Geste, Display-Erkennung mit Wiederholungen – jeweils als reine Funktionen.
- [ ] **P2** Fakes realistischer: `endTransmission(false)` erst mit `requestFrom` ausführen, `Wire.end()` kann fehlschlagen, Preferences mit mehreren Namespaces und Strings (für `hfnet`), Serial-Fake mit allen Zeilen (Log/SCPI-Kollision testbar), LEDC-API von Core 3.x.
- [ ] **P3** CI prüft, dass das Firmware-ELF den aktuellen Commit enthält (T-16).
- [ ] **P3** Bei Änderungen am NVS-Schema (z. B. Drive je Kanal): Migrationstest mit alten Schlüsseln.

## 8. Entschieden, nicht umsetzen

- RGB-LED als Ausgangsanzeige: Die LED zeigt weiter die aktive Seite (Spezifikation E-3).
- Doppelte Statusanzeige im Header: Sie bleibt, Spezifikation E-13 verlangt die Textanzeige ON/OFF.

---

## 9. Rückfragen

Beantwortet am 2026-10-27: Alle Ausgänge starten aus. Am 2026-10-28: eine Firmware für beide Boards (Display-Erkennung); Low-Noise-Modus und Zustand abgeschalteter Ausgänge als CONFIG-Optionen (Standard „Fractional“ bzw. „Low“). Am 2026-11-03 (WEB-HMI, nach dem Audit): AP mit Zufallspasswort und QR, Passwörter nie über einen offenen AP, kein automatisch offener Rückfall-AP; Option „Remote outputs“ (Standard erlaubt); Captive-DNS im AP-Modus; Statusleiste auf MAIN statt der Tasten CH1–CH3; Fernsteuerung über SCPI (TCP und USB) statt eines eigenen Textprotokolls; Migration auf das offizielle `espressif32@8.0.0`.

1. **Neue Funktionen**: Welche sind gewünscht?
   a) Speicherplätze (z. B. 5 Frequenzen je Kanal),
   b) Wobbeln (Start / Stopp / Schrittweite / Verweilzeit),
   c) Drive-Strength je Kanal statt global,
   d) Info-Seite „Synthesizer“,
   e) freie Phase / Phasen-Feinabgleich,
   f) gekoppelter Betrieb CH2 = CH1 ± Offset?
2. **Standard-Tuning**: Bleibt „Fractional“ Standard, oder wird „Low noise“ Standard, falls Messplan K5/K6 einen klaren Vorteil zeigt?
3. ~~Reihenfolge (Audit D9)~~ **beantwortet 2026-11-04:** erst Qualität, Bedienung und WEB-HMI, danach die Messkampagne (Abschnitt 0a).

---

## 10. Messplan

Der vollständige Messplan mit Einzelschritten, Messgeräten, Sollwerten, Aufbauten und Bewertung steht in [MESSPLAN.md](MESSPLAN.md) (150 Messungen A … K, L, L-S, Q). Die Nummern A1 … K12 sind unverändert, die Ergebnisse von I0 bis I2 (2026-10-28) stehen dort in Gruppe I. Die Messungen laufen als Phase 7 nach der WEB-HMI (Abschnitt 0a); Befunde (M-n) und die Entscheidungen aus Abschnitt 1.4 des Messplans kommen als Aufgaben zurück in diese Datei.
