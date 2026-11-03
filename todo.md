# TODO – Code- & Design-Review CYD HF Generator

Stand: 2026-11-03 · Firmware 1.3.0 · Einsatzziel: Experimente mit HF-Mischern und Amateurfunkausbildung; wichtig sind geringes Phasenrauschen und genauer IQ-Betrieb.

Prioritäten: **P0** = sofort / Voraussetzung · **P1** = Fehler / sichtbares Problem · **P2** = deutliche Verbesserung · **P3** = Aufräumen / nice-to-have
Status: `[ ]` offen · `[~]` teilweise · `[?]` wartet auf Rückfrage (Abschnitt 9) · `[M]` wartet auf Messung (Abschnitt 10)

Erledigte Punkte sind entfernt; sie stehen in der Git-Historie dieser Datei. Seit dem Review (1.2.0) umgesetzt: Host-Unit-Tests, SDA/SCL-Erkennung (Messplan J), Display-Erkennung mit einer Firmware für beide Boards (Messplan I) und in 1.3.0 die CONFIG-Optionen Tuning / Spread / Off state, scrollbare CONFIG-Seite mit Info-Karte, Backlight in 10-%-Schritten, Anzeige der tatsächlichen Frequenz, Aufräumen und der Umbau von Spezifikation und Handbüchern (Messplan K). Die Entscheidungen dazu stehen in der Spezifikation.

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
| 7 | Audit: Fast der gesamte Messplan ist noch nie am Gerät gelaufen; mehrere Entscheidungen warten auf Messungen. | 10 |

---

## 0a. Reihenfolge (Audit 2026-11, Abschnitt 5)

Jede Phase endet mit den Gates aus Audit 5.8: Host-Tests in `native-win` und `native` grün, 0 Warnungen mit `-Wall -Wextra` in `src/`, Review (bei Netz-Code gegen die Sicherheitsanforderungen W-S), Smoke-Test auf beiden Boards, Handbuch DE + EN im selben Commit.

| Phase | Inhalt | Abschnitte hier | Aufwand |
|---|---|---|---|
| 0 | Hygiene und Basis: PlatformIO 6.1.5 aus dem PATH, CI, `-Wall -Wextra`, Abhängigkeiten auf Commit, Heap-Debugausgabe; **Basismessung am Gerät** (A, B, I, J, K1/K2) | 1, 4, 10 | 0,5 d + Messung |
| 1 | Robustheit 1.3.1: Fehler C-1 … C-13, Assert-Handler und Loop-WDT, Parser, Doku-Abgleich, Tests T-1 … T-9 | 2.3, 4, 5, 7 | ≈ 2 d |
| 2 | Bedienung 1.3.2: U-1 … U-10, U-12, U-14, U-15, U-17 … U-19, Handbuch U-23, GUI-Logik testbar (T-10 … T-14), Bedientests B8 … B15 | 3, 5, 7, 10 B | ≈ 3–4 d |
| 3 | Laborbetrieb 1.3.3: Sperre, Grundzustand, Kalibrieren, CH3 mit Masterfrequenz (U-11, U-13, U-16, U-20, U-22); **HF-Messkampagne** (C, D, E, F, K) und die Entscheidungen daraus; optional Bereichserweiterung 2.4; Bedientest mit Testpersonen B16 … B20 | 2, 3, 10 | ≈ 2–3 d + Messung |
| 4 | Migration Core 3.x (Konzept WEB-HMI Abschnitt 4) | 1 | ≈ 2,5–3 d |
| 5 | WEB-HMI-Vorarbeiten V-1 … V-6, V-11 (Konzept Abschnitt 5) | 6 | ≈ 4 d |
| 6 | WEB-HMI (Konzept Abschnitt 11, Schritte 0–9) mit W-S1 … W-S25, Messplan L und L-S | 6, 10 L | ≈ 15–16 d |
| 7 | später: DMA, IQ ohne Aussetzer, Phasen-Feinabgleich, Sticky-Bit, Störquellen-Abhilfen, LVGL 8.4, Unity 2.7, Arduino 3.3.13, Erweiterungen aus 6 | 2, 4, 6 | – |

## 1. Build, Tests & Hardware-Varianten

- [ ] **P0 PlatformIO 6.1.5 aus dem PATH entfernen** (P-1): Das alte `pio` (Python-3.10-User-Site) bricht ESP32-Builds; nur `~/.platformio/penv` (6.2.0) verwenden oder `pip install -U platformio`.
- [ ] **P1 CI** (Phase 0): GitHub Action mit `pio test -e native` und `pio run` (eine Firmware-Umgebung); `env:native` (Linux, gcc) ist noch nie gelaufen. Actions per SHA gepinnt, `permissions: contents: read`, kein `pull_request_target` mit PR-Checkout (S-27). Später Fuzzing-Job (Konzept WEB-HMI W-S25).
- [ ] **P1 Warnungen**: `build_src_flags = -Wall -Wextra` für `src/` (heute 0 Warnungen, auch damit); bei der Migration zusätzlich eine Host-Test-Umgebung mit `-std=gnu++17` (V-9).
- [ ] **P1 Abhängigkeiten auf Commit pinnen** (S-20): `XPT2046_Touchscreen` von `#v1.4` auf den Commit `d57f64c8b5f2bc5b8d10d121550806eeff7b06d9`; Plattform-Prüfsumme in der CI; Advisory-Check je Release.
- [ ] **P2 Migration Arduino-Core 3.x** (Phase 4, [Konzept WEB-HMI Abschnitt 4](KONZEPT_WEB_HMI.md)): Ziel offizielles `platformio/espressif32@8.0.0` (Arduino 3.3.12, IDF 5.5.5; Spike gegen pioarduino 55.03.312-1). Grund ist vor allem Sicherheit: IDF 4.4 und mbedTLS 2.28 sind ohne Support (S-19, P-4). Teilaufgaben: LEDC auf `ledcAttach()` (auch im Fake), Wire auf dem neuen `i2c_master`-Treiber mit gebündelten Transfers in `I2CBus` (C-14), TFT_eSPI 2.5.43 früh bauen und testen, A/B-Umgebungen bis zur Abnahme; Messplan A, B, I, J gegen die Basis aus Phase 0 wiederholen.
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
- [ ] **P3 VCO bei positiver Korrektur leicht über 900 MHz** (Tuning „Fractional“): Die feste PLL 36 × Quarz ergibt bei +50 ppm 900,045 MHz. Praktisch unkritisch. In „Integer“ ebenso (C-7): `aMin/aMax` kommen aus dem Nennquarz, bei ±50 ppm ergibt das 599,970 bzw. 900,045 MHz → Grenzen aus `xtal4` rechnen oder in A-13 aufnehmen.
- [ ] **P1 CLK2 im Fehlerzustand eingeschaltet** (C-1, [app_controller.cpp:265](src/core/app_controller.cpp#L265)): `setChannelEnabled()` gibt CLK2 frei, sobald `iqConfigured_()` gilt, auch wenn CH3 auf ERR steht; mit Off state High/High-Z liegen dann bis zum Retry 25 MHz bzw. die alte Frequenz an CLK2. Bedingung `iqConfigured_() && !fault_[CLK_CH3]`; Test in `native-fault` (7).
- [ ] **P1 Einfrieren bei vollem LVGL-Heap** (C-3, S-23): `LV_USE_ASSERT_MALLOC` endet in `while(1);`, `loop()` hat keinen Watchdog → Ausgänge laufen unbedienbar weiter. Eigener `LV_ASSERT_HANDLER` (Log + `esp_restart()`), `enableLoopWDT()`, `lv_mem_monitor()` und freier/minimaler Heap ins Debug-Log und auf die Info-Karte (V-8).
- [ ] **P2 Wiederverbinden im selben Zyklus** (C-12, [app_controller.cpp:37-48](src/core/app_controller.cpp#L37-L48)): Nach erkanntem Verlust erst im nächsten 2-s-Zyklus `connect_()` → bis zu 4 s ohne Wiederherstellung.
- [ ] **P2 Speichern wiederholen** (C-13, [settings.cpp:78](src/services/settings.cpp#L78)): `dirty_` wird vor dem Speichern gelöscht; ein einmaliger NVS-Fehler verliert die Änderung. `dirty_` behalten, Retry mit Backoff 30 s, Dialog nur einmal.
- [ ] **P2 Unnötige Flash-Schreibvorgänge** (C-5): ON/OFF und eine unveränderte Schrittweite markieren „dirty“ und schreiben nach 5 s alle 11 Schlüssel. `markChanged_()` nur bei geänderten gespeicherten Feldern; dauerhaft Dirty je Schlüssel (V-4).

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

- [ ] **P1 Header zeigt den Ist-Zustand** (U-1): Heute folgt der Text nur der Anforderung („ON“ bei WAIT oder NO HW). `setHeaderState_()` bekommt den Status (ON / OFF / WAIT / ERR / NO HW wie auf MAIN); der Schalter zeigt weiter die Anforderung. Handbuch §11 „Kanal steht auf ON?“ → „MAIN zeigt ON“.
- [ ] **P1 Sondermodi sichtbar** (U-2): Off state High/High-Z, Spread, Integer, Low noise und Drive sind nur in CONFIG sichtbar und überleben Neustarts; MAIN zeigt „OFF“, obwohl am Ausgang HIGH/DC liegt. Detailzeile der MAIN-Karten und der Kanalseite (z. B. „SSC -0.5 % · INT · 8 mA“, „OFF = HIGH“ in Warnfarbe); Off state „High“ einmal bestätigen.
- [ ] **P1 Kartenschalter abgrenzen** (U-3): Trefferzone des Schalters endet 0,5 mm neben „100.000 000 MHz“ – ein Tipp auf „MHz“ kann den Ausgang einschalten. Schalter in eigene Spalte, ≥ 12 px Abstand.
- [ ] **P2 Dialog- und Randtasten** (U-4, U-5): OK ≈ 80 × 23 px mit 1,10:1 Flächenkontrast → ≥ 120 × 40 px, ≥ 3:1. MAIN-Taste 30 px hoch in der Ecke, Ecken-Tipps werden auf (319, 0) geklemmt und verfehlen sie → Randtasten bis an den Bildrand (`ext_click_area`), MAIN 36 px.
- [ ] **P2 Phasen-Chips** (U-6): 2 px Abstand, ein Fehltipp vertauscht das Seitenband → 6 px.
- [ ] **P2 Kontraste** (U-7, U-19): Weiß auf CH1/CH2/CH3 nur 3,66 / 2,86 / 3,19:1 → Textfarbe nach Kontrast (dunkel); Schalter-Spur und Chip-Rand ≥ 3:1; Zustände zusätzlich über Form („!“ bei ERR, gefülltes ON-Kennzeichen) für Farbsehschwäche.
- [ ] **P2 Lesbarkeit im Unterricht** (U-8): ON/OFF auf MAIN in 12 px ist nur bis ≈ 33 cm lesbar → gefülltes Kennzeichen 16 px, Status- und Detailzeilen 14 px. Die Navigationstasten CH1–CH3 auf MAIN werden durch die Statusleiste ersetzt (entschieden 2026-11-03, Konzept WEB-HMI W-E26; kann vor WEB-HMI kommen).
- [ ] **P2 Numpad** (U-9): Fehleingabe löscht die Eingabe; Hinweis nur als Platzhalter mit 2,29:1; 10. Zeichen still ignoriert → Eingabe stehen lassen, Fehlerlabel, aktuelle Frequenz als feste Zeile.
- [ ] **P2 Boot-Geste absichern** (U-10): Versehentliche Geste speichert den falschen Controller → Dialog „Display OK?“, ohne Tipp nach 10 s zurück und nichts speichern; Hinweis auch im Handbuch §11.
- [ ] **P2 Bildschirmsperre und Grundzustand** (U-13, war P3): Sperre durch 2 s Halten mit Anzeige „LOCK“ (unterdrückt auch Neuzeichnen, 2.1); „Reset to defaults (keep calibration)“ mit Bestätigung; **Werksreset** (NVS löschen inkl. künftigem `hfnet`) als weitere Zeile auf der CONFIG-Seite.
- [ ] **P2 Kalibrieren** (U-11): heute Kopfrechnen und ≈ 20 Tipps → Wert antippen öffnet das Numpad (ppm mit Vorzeichen); Assistent siehe 6.
- [ ] **P3 Masterfrequenz und Master-Schalter auf der CH3-Seite** (U-16), Kanalwahl im Header (CH1 → CH2 heute 2 Tipps).
- [ ] **P3 CONFIG-Reihenfolge** (U-14): Drive, Off state, Tuning, Spread, Corr., Backlight; Zeile „Step“ streichen (doppelt die Chips; entschieden 2026-11-03); 20 px der nächsten Zeile zeigen. CONFIG-Tasten 40 px, Hinweis „(Integer: off)“ bei gesperrtem Spread (U-18).
- [ ] **P3 Kleinere Punkte**: „°“ statt „deg“ (U-12, der Font hat das Zeichen); Ziffer-Antippen als Komfortfunktion beschreiben, Trennzeichen der Nachbarziffer zuordnen (U-15); Kanalseite bei OFF „Off \| …“ und Hinweis „IQ master of CH3“ (U-17); Wiederholung beim Halten beschleunigen, Druckaussetzer bis 60–80 ms überbrücken (U-20); Meldungen mit Kanal und Abhilfe, z. B. „CH1: frequency setup failed – retrying every 2 s“ (U-21); Speicherzustand in der Statusleiste (U-22).

## 4. Code-Qualität & Wartbarkeit

- [ ] **P1 Frequenzparser härten** (C-4, S-21): `parseFrequency("nan")` liefert true, `1e7` und `0x989680` werden angenommen (in der GUI unerreichbar, für REST/SCPI kritisch). `isfinite()`, nur Ziffern und ein Punkt; für das Netz ein eigener Parser (V-11).
- [ ] **P2 Unerreichbare Dialoge** (C-6): „Output switch failed“, „Off state setup failed“, „Spread spectrum setup failed“ können praktisch nicht auftreten, stehen aber im Handbuch → entfernen bzw. über Ergebniscodes (V-1), Handbuch DE/EN.
- [ ] **P2 „~“ auf MAIN nach dem Boot** (C-2): MAIN wird vor `applyStoredChannelSettings()` gezeichnet → nach Schritt 9 neu zeichnen; dauerhaft über den Änderungszähler (V-2).
- [ ] **P3 Toter Code** (C-10): `SI5351Manager::revId()`, `DisplayPanel::probeResult()` (ggf. für die Info-Karte nutzen), unerreichbares `p.z < 150` in `touch.cpp`, nie ausgewertete Rückgabewerte von `readLvgl()` und `Gui::begin()`.
- [ ] **P3 Doppelte Formatierung und Literale** (C-11): IQ-Grenze dreimal mit unterschiedlicher Rundung formatiert → `FrequencyUtils::formatIqLimit()`; Zeiten (300 ms, 35 ms, 75 × 20 ms, 10 s) nach `constants.h`.
- [ ] **P3 Feste Koordinaten in den Pages**: Header (158/214), CH3 (114/134/186) usw. in [gui_layout.h](src/gui/gui_layout.h) bzw. als lokale Konstanten zusammenfassen, wie in [page_main.cpp](src/gui/pages/page_main.cpp) schon geschehen.
- [ ] **P2 Display-Ausgabe mit DMA und Doppelpuffer** (Entscheidung 2026-10-28: ausgearbeitet, in 1.3.0 noch nicht umgesetzt). Heute: ein Puffer 320 × 24 px, LVGL rendert und `DisplayPanel::flush()` sendet abwechselnd mit `pushColors(…, swap = true)`; ein ganzer Bildschirm braucht allein auf dem Bus ≈ 31 ms. Plan:
  - Zwei Puffer à 320 × 40 px (je 25,6 KB, statisch, `DMA_ATTR`; `pushPixelsDMA()` erlaubt < 32 767 Pixel), `LV_COLOR_16_SWAP` = 1 und `setSwapBytes(false)`, damit die CPU die Bytes nicht mehr tauscht.
  - In `DisplayPanel::begin()` erst Erkennung, `tft_.init()`, ST7789-Init, Rotation, `fillScreen()` (alles noch mit rohem Buszugriff), dann `tft_.initDMA()` (TFT_eSPI 2.5.43: `HSPI_HOST`, DMA-Kanal 1) und einmal `tft_.startWrite()`; CS bleibt LOW, `endWrite()` nie aufrufen (würde CS während einer DMA-Übertragung hochziehen). Danach kein `SpiPanelBus`-Zugriff und kein direktes Zeichnen ohne `dmaWait()`.
  - `flush()`: `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`; `lv_disp_flush_ready()` sofort (LVGL rendert in den anderen Puffer; der erste wird erst nach dem nächsten `flush()` mit seinem `dmaWait()` wieder beschrieben).
  - Erwartung ≈ max(Rendern, Senden) statt Summe, bis ≈ 30 Bilder/s; +36 KB RAM. Messung vorher/nachher mit `LV_USE_PERF_MONITOR` beim Scrollen auf CONFIG, `+` halten und Seitenwechsel; 30 min Bedienung ohne Pixelfehler.

## 5. Dokumentation

- [ ] **P2 Anwendungskapitel „Mischer-Experimente“** in beiden Handbüchern: Ausgangspegel je Drive-Stärke in dBm an 50 Ω (aus Messplan F2); Oberwellen eines Rechtecks (3. ≈ −9,5 dBc, 5. ≈ −14 dBc) und passende Tiefpässe; LO-Pegel für Diodenringmischer (+7 dBm) bzw. Dämpfung für NE612/SA612; IQ direkt an FST3253/74CBT3253 (Tayloe-Detektor); gleich lange Kabel bei IQ (1° bei 100 MHz ≈ 28 ps ≈ 5,6 mm Koax); Zusammenhang Phasenfehler ↔ Spiegelunterdrückung (1° ≈ 41 dB, 2° ≈ 35 dB, 5° ≈ 27 dB); Rechtecksignal nie ungefiltert an eine Antenne.
- [M] **P2 „50 % Tastverhältnis“** (Handbücher 1) ist nicht belegt. Nach Messplan E5 präzisieren.
- [ ] **P1 Doku-Abgleich** (C-8): Low noise „≈ 1,4 Hz“ (A-12, Handbuch) vs. Test 1,5 Hz (gemessen max. 0,49 Hz); WAIT-Gründe ohne die Spread-Grenzen (Handbuch §2/§11, MAIN-Tabelle); veralteter Kommentar in `pins.h` („one environment per display controller“); Kommentar „begin() writes spread“.
- [ ] **P2 Handbuch ausbauen** (U-23): Inhaltsverzeichnis, einseitige Kurzanleitung für den Laborplatz, Seitenbilder, Glossar „Anzeige am Gerät → Bedeutung“ (NO HW, ERR, Corr., SSC, Out, Off state …), Symptomtabelle („kHz-Abweichung → Integer“, „breites Spektrum → Spread“, „DC bei OFF → Off state“); dass die Schrittweite für CH1 und CH2 gemeinsam gilt; Boot-Geste auch in §11. Hinweise „physischer Zugriff = Vollzugriff“ (S-22) und „ein Fremdgerät an 0x60 bekommt einen Schreibzugriff“ (S-24).

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
| `test_display_detect` | 20 | Erkennung gegen ein Panel-Modell mit dem gemessenen Leseverhalten, Entscheidung (Build-Flag, gespeicherte Wahl, Boot-Geste), ST7789-Init-Tabelle, NVS-Schlüssel `disp_panel` |

Aufbau: [test/fakes](test/fakes) ersetzt Arduino-Core, `Wire` (SI5351-Registermodell mit Schreibprotokoll, Fehlerinjektion, Pinbelegung, Fremdgerät) und `Preferences`. [test/helpers/si5351_model.h](test/helpers/si5351_model.h) dekodiert die geschriebenen Register unabhängig vom Treiber zurück in Frequenzen, Spread-Parameter und Off state und prüft die Grenzen aus AN619; [test/helpers/panel_model.h](test/helpers/panel_model.h) bildet die Display-Controller nach.

Weitere Vorschläge:

- [ ] **P2** Mit IQ ohne Reset (2.2): Test „PLL-Änderung ohne Reset, Phasen-Offset und Teiler unverändert“.
- [ ] **P1** Umgebung `native-fault` mit `-DTEST_FAULT_IQ=1` (T-1): Zustand ERR, stilles Wiederholen ohne weitere Dialoge, Rückkehr nach Phasenwechsel; Off state High + Master ein → CLK2 bleibt aus (C-1).
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
3. **Reihenfolge** (Audit D9): Phasen 1–3 (Robustheit, Bedienung, Laborbetrieb und Messungen) vor der Migration und WEB-HMI? Vorschlag: ja.

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
| [ ] | A9 | CH1 aus mit Off state „Low“ (Standard): Pegel an CLK0 DC-gekoppelt, einmal an 1 MΩ, einmal an 50 Ω | LOW; die anderen Zustände prüft K1 | |
| [ ] | A10 | CH1 an, ESP32-Reset-Taste bzw. Upload; Scope im Single-Trigger an CLK0 | Ausgang läuft bis zur Initialisierung weiter, dann aus; Dauer notieren (2.3) | |

### B. Bedienung

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | B2 | `+` gedrückt halten (alt 12) | erster Schritt nach 0,4 s, dann alle 0,1 s, ohne Flackern | |
| [ ] | B3 | Numpad: „500“ + MHz, „0.5“ + MHz, „10.7“ + MHz (alt 13) | Fehler, Fehler, 10,700 000 MHz | |
| [ ] | B4 | Numpad: Eingabe mit „Hz“ bzw. „kHz“ bestätigen, Finger kurz liegen lassen (alt 13a) | Kanalseite erscheint, Numpad öffnet **nicht** erneut, Schrittweite unverändert | |
| [ ] | B5 | Kanalseite: auf die Ziffer „1 kHz“ tippen, dann auf „MHz“ (alt 18a) | Schritt 1 kHz und Ziffer farbig markiert; NUMPAD öffnet sich | |
| [ ] | B6 | Sichtprüfung aller Seiten, beide Varianten (alt 15) | nichts abgeschnitten, keine Scrollbalken, kein Überlappen | |
| [ ] | B7 | Elemente an den Bildrändern (Chips „1“ und „1M“, MAIN, CONFIG) je 10× antippen, beide Varianten | jeder Tipp trifft (feste Touch-Kalibrierung) | |
| [ ] | B8 | Trefferkarte (Test-Build mit Touch-Log und 20 Zielkreuzen inkl. Ecken und 4-px-Rand): 3 Personen × 5 Tipps, Finger und Stift, beide Boards (U-5) | mittlere und maximale Abweichung je Bereich in px → Mindestgröße, Randabstand, Grundlage für E-1 | |
| [ ] | B9 | Trefferquote echter Ziele: Dialog-OK, MAIN-Ecke, Phase 90/180, Chips „1“/„1M“, Corr. −/+, Listeneinträge, Numpad ◀ und C; je 20 Tipps × 5 Personen (U-4, U-6) | ≤ 2 % Fehltipps; 0 Treffer auf dem Nachbarziel bei Phase und Schaltern | |
| [ ] | B10 | 20× Kanal über die Karte öffnen (Tipp auf Frequenz bzw. „MHz“) (U-3) | 0 ungewollte Schaltvorgänge | |
| [ ] | B11 | Jede Ziffer von 10.000 000 und 100.000 000 MHz je 10× antippen (U-15) | ≥ 95 % richtige Schrittwahl, sonst nur als Komfortfunktion beschreiben | |
| [ ] | B12 | CONFIG wischen mit leichtem und festem Druck; Tipp während der Einrast-Animation | 0 ungewollte Auslösungen; Druckaussetzer > 35 ms im Touch-Log (ergänzt K10) | |
| [ ] | B13 | `+` 5 s halten, leicht und fest (U-20) | 47 Schritte erwartet; Unterbrechungen zählen (ergänzt B2) | |
| [ ] | B14 | Lesbarkeit von MAIN mit zufälligen Zuständen aus 0,4 / 0,7 / 1,0 / 1,5 m; Blickwinkel 0° / 30° / 45°; Beleuchtung 30 / 80 / 100 %; ≈ 500 lx; beide Boards (U-8) | Erkennungsrate je Abstand | |
| [ ] | B15 | Rot-Grün-Schwäche: Testperson oder Foto mit Simulationsfilter (U-7, U-19) | CH1/CH2, ON/ERR, markierte Ziffer, gewählter Chip unterscheidbar | |
| [ ] | B16 | Aufgabentest mit lautem Denken: 5 Lernende, 2–3 Lehrende; Aufgaben aus Audit 2.3 (Frequenz, Einschalten, IQ 90°, Kalibrieren, Tuning, Off state, Stromausfall) erst ohne, dann mit Handbuch | Erfolg, Zeit, Tipps (Touch-Log), Fehler; SUS ≥ 70; WAIT, NO HW, Out, Corr., Off state, Tuning verstanden | |
| [ ] | B17 | 20× Einschalten in natürlicher Haltung (Daumen auf dem Display, USB einstecken) (U-10) | ungewollte Boot-Gesten zählen; Zeit bis zur Behebung durch Novizen | |
| [ ] | B18 | A1/A3 mit Testperson | Meldung verstanden, OK beim ersten Versuch getroffen | |
| [ ] | B19 | B8/B9 mit Eingabestift wiederholen | Entscheidung über einen Stift am Laborplatz | |
| [ ] | B20 | Stecker ziehen während einer Messung | Wann merkt die Testperson, dass alle Ausgänge aus sind? Geht eine Änderung < 5 s verloren? | |

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
| [ ] | F3 | Span 200 kHz, RBW 100 Hz: 10,000 000 MHz bei Korrektur 0 (ganzzahliger Teiler) / 7,100 000 MHz (fraktional) / 7,100 000 MHz mit CH3 an (IQ: ganzzahliger Teiler, fraktionale PLL) | Nebenlinien in dBc dokumentieren; Vergleich der Tuning-Modi in K5 | |
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

I0 (Diagnose-Sketch, 2026-10-28, beide Boards, je 2 EN-Resets mit 3 Software-Neustarts, 50 Wiederholungen je Lesetakt) ist erledigt; die Messwerte stehen in Spezifikation E-18. Nach dem Flashen der Einheitsfirmware: 5/5 EN-Resets je Board richtig erkannt (COM13 Original-CYD: ILI9341, ID 000000; COM7 CYD2USB: ST7789, ID 8181B3).

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [x] | I0 | Diagnose: MADCTL-Rücklesen, RDDID, Pull an GPIO12, SPI-Modus 0/3, Schreiben mit 40 MHz | Rücklesen auf beiden Boards ohne Pull; IDs 000000 / 8181B3; MODE0 fehlerfrei | siehe Spez. E-18 |
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

### K. CONFIG-Optionen (1.3.0)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | K1 | CH1 aus, Off state Low / High / High-Z: Pegel an CLK0 an 1 MΩ und 50 Ω | LOW / HIGH / hochohmig | |
| [ ] | K2 | CH3 aus bei Off state „Low“ (CLK2 mit `CLK2_PDN`): Pegel an CLK2 | Pegel notieren; entscheidet, ob „High-Z“ alle abgeschalteten Ausgänge mit `CLKx_PDN` abschalten darf (2.1) | |
| [ ] | K3 | Spread −0,5 % bei 10 MHz, Spektrum Span 200 kHz | Band 9,95–10,00 MHz, Modulation ≈ 31,5 kHz; CH2 unverändert; Kanalseite „Out 9.975 000 MHz mean, SSC -0.5 %“ | |
| [ ] | K4 | Spread ±0,5 % mit IQ (Master CH1), Phase CH1/CH3 | Phase bleibt, beide gleich verbreitert | |
| [ ] | K5 | 7,1 MHz in Fractional gegen Low noise gegen Integer, Span 200 kHz, RBW 100 Hz | Nebenlinien vergleichen (wie F3); Grundlage für den Standard-Modus (Rückfrage 9.2) | |
| [ ] | K6 | Low noise: `+` in 1-kHz-Schritten bei 10 MHz, Scope mit Nachleuchten | Aussetzer nur beim Teilerwechsel (wie D5) | |
| [ ] | K7 | Integer: CH1 = 7,1 MHz, Zähler | Zähler = angezeigte tatsächliche Frequenz „Out 7.102 273 MHz“ (± Quarzfehler) | |
| [ ] | K8 | CONFIG: alle Dropdowns öffnen, auch ganz unten gescrollt; Info-Karte | Listen vollständig sichtbar; Firmware, Branch, Commit, Build-Zeit stimmen mit `git log` überein | |
| [ ] | K9 | Optionen ändern, 5 s warten („Saved“ im Header), aus- und einschalten | Optionen erhalten, Ausgänge aus | |
| [ ] | K10 | CONFIG scrollen, beide Boards: auf freier Fläche, auf Tasten (Corr. `+` mit Wiederholung) und auf Dropdowns wischen; danach einzelne Tipps | rastet an Zeilenkanten ein; Wischen löst keine Aktion aus; Tipps treffen; Scrollposition nach MAIN → CONFIG erhalten | |
| [ ] | K11 | Tuning „Integer“ wählen | „Spread CH1“ steht auf Off und ist gesperrt; MAIN zeigt „~“ vor abweichenden Frequenzen | |
| [ ] | K12 | Backlight `−` / `+` (ersetzt B1) | Stufen 10 … 100 %, bei 10 % noch lesbar; Wert bleibt nach Neustart | |

### L. WEB-HMI (nach der Umsetzung, Konzept Abschnitt 12)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | L1 | WLAN-Modi Off / Client / Access point; Router aus | Rückfall-AP (WPA2) nach 60 s, Suche läuft weiter; mit Verbindung AP aus | |
| [ ] | L2 | zwei gespeicherte WLANs mit unterschiedlicher Signalstärke | das stärkere gewinnt, bei Misserfolg das andere | |
| [ ] | L3 | AP mit Zufallspasswort per QR beitreten; AP auf Wunsch offen | QR funktioniert (Android, iOS); offen: Warnung „AP open“, Aus nach 15 min ohne Station | |
| [ ] | L4 | statische IP, auch ungültige (Gateway, Broadcast) | gültige wird übernommen, ungültige abgelehnt | |
| [ ] | L5 | `<name>.local` und Captive-DNS (Android, iOS, Windows, Schulnetz) | Einrichtungsseite öffnet sich; Name auflösbar | |
| [ ] | L6 | SCPI per PyVISA über USB (DTR/RTS aus) und TCP 5025 | kein Neustart beim Öffnen; 1000 × `*IDN?` ohne fremde Zeilen | |
| [ ] | L7 | zweite SCPI-Verbindung; Leerlauf 10 min | zweite abgelehnt (Info-Karte); Leerlauf getrennt | |
| [ ] | L8 | Web auf Handy und PC, 3 Clients synchron, vierter Client | Änderungen live überall; vierter bekommt die Fehlerseite | |
| [ ] | L9 | Tastaturbedienung im Browser | Belegung wie Konzept 9.1, `Space` schaltet nichts | |
| [ ] | L10 | Netzänderung ohne Bestätigung | nach 120 s Rückfall auf die alten Einstellungen | |
| [ ] | L11 | Speichern nach 5 s (Touch/Web) bzw. 30 s (SCPI); nur Ein/Aus | Zeiten stimmen; Ein/Aus schreibt nichts | |
| [ ] | L12 | ohne SI5351 | Web und SCPI funktionieren, Kanäle `NO HW` | |
| [ ] | L13 | WLAN an: Spektrum an CLK0 bei 10 MHz (wie Schritt 0) | Nebenlinien dokumentiert | |
| [ ] | L14 | 24-h-Dauertest mit 3 Browsern und SCPI-Skript (10 Befehle/s) | minimaler Heap sinkt nicht stetig, Stack-Reserve ≥ 1 KB | |
| [ ] | L15 | WLAN aus | Spektrum wie 1.3.0 | |
| [ ] | L16 | Einrichtung durch Novizen, mit und ohne QR | Erfolg und Zeit; mobile Daten abschalten nötig? | |
| [ ] | L17 | 16-stelliges Passwort mit der Bildschirmtastatur, Finger und Stift, 5 Personen | Zeit, Fehler, Abbrüche | |
| [ ] | L18 | drei Browser und Gerät ändern gegenseitig Einstellungen | fremde Änderungen werden bemerkt (Hervorhebung, `REMOTE`) | |
| [ ] | L19 | Tastaturbedienung ohne Maus, Aufgaben aus B16 | Fehlbedienungen zählen | |
| [ ] | L20 | Identify mit mindestens 3 Geräten im Raum | richtiges Gerät gefunden | |
| [ ] | L21 | Großansicht am Beamer aus 5 m | lesbar | |

### L-S. WEB-HMI Sicherheit (Konzept Abschnitt 10/12)

| OK | Nr. | Test | Erwartung | Messwert |
|---|---|---|---|---|
| [ ] | L-S1 | `curl`-Matrix: `Host: evil.example`; `text/plain`; ohne Token; `Origin: http://evil.example`; Body 100 KB; Passwort über offenen AP | 421; 415; 403; 403; 413; 403 | |
| [ ] | L-S2 | Testseite unter `python -m http.server` mit `fetch no-cors`, `<form enctype=text/plain>` und WebSocket von fremdem Origin | Zustand unverändert | |
| [ ] | L-S3 | `curl -X POST --data-binary $'OUTP1 ON\n' http://IP:5025/`; 20 000 × `A` + `OUTP1 ON` über `nc` | CH1 bleibt aus | |
| [ ] | L-S4 | Test-AP mit SSIDs `<svg onload=alert(1)>`, `a"b\c`, ungültigem UTF-8; 200 Beacons (eigene Umgebung) | keine Skriptausführung, Gerät bleibt bedienbar | |
| [ ] | L-S5 | Slowloris mit 30 Sockets, WebSocket-Flut, Multipart-PoCs aus den Advisories | Touch reagiert, Heap erholt sich, kein Absturz | |
| [ ] | L-S6 | Handy am AP; Laptop mit statischer Route über 192.168.4.1 | kein Gateway per DHCP; kein Host im Heimnetz erreichbar | |
| [ ] | L-S7 | `esptool read_flash` der NVS-Partition, dann `strings` | keine WLAN- oder Web-Passphrase im Klartext | |
| [ ] | L-S8 | `Remote outputs: local only`, dann `OUTP1 ON` per SCPI und REST | abgelehnt (−221 / 403); Touch schaltet weiter | |
