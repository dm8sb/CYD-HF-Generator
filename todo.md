# TODO – Code- & Design-Review CYD HF Generator

Stand: 2026-10-27 · Review-Basis: Commit `058e44d` (Firmware 1.1.0 + Build-Umgebung für CYD2USB) · Umsetzung: Firmware 1.2.0 (noch nicht committet)

Prioritäten: **P1** = Fehler / sichtbares Problem · **P2** = deutliche Verbesserung · **P3** = Aufräumen / nice-to-have
Status: `[ ]` offen · `[~]` teilweise · `[?]` wartet auf Rückfrage (Abschnitt 7)

Erledigte Punkte sind entfernt; sie stehen in der Git-Historie dieser Datei. In 1.2.0 umgesetzt: eigener SI5351-Treiber (exakte Korrektur, kein PLL-Reset beim Abstimmen, Powerdown CLK2, Wiederverbindung), Ausgänge starten aus, Backlight erst nach dem ersten Frame, NVS-Dialoge, Stelle direkt antippen, `PIN_TFT_*` entfernt. Danach: USB-C-Variante in Handbüchern und Spezifikation dokumentiert, `gGui` und globales `GUI` entfernt (Instanz über `user_data` der LVGL-Treiber).

---

## 1. Build & Hardware-Varianten

- [ ] **P3 Eine Firmware für beide Varianten**: Den Display-Controller beim Start erkennen, statt zwei Build-Umgebungen zu pflegen (Entscheidung 2026-10-27: vorerst zwei Builds). Ansatz: Das ID-Register per SPI lesen (MISO GPIO 12 ist angeschlossen): ILI9341 liefert über `0xD3` die ID `0x9341`, ST7789 über `0x04` die ID `0x85 0x85 0x52`. TFT_eSPI legt den Treiber beim Kompilieren fest, deshalb ist ein Wechsel zu einer Bibliothek mit Laufzeit-Auswahl nötig (z. B. LovyanGFX) oder eine eigene Init-Sequenz je Controller. Danach entfallen die zweite Umgebung und das Risiko, die falsche Firmware zu flashen.
- [ ] **P3 TFT_eSPI exakt pinnen** (`bodmer/TFT_eSPI@2.5.43` statt `^2.5.43`), wie die anderen Bibliotheken auch.
- [ ] **P3 CI**: GitHub Action, die beide Umgebungen baut (später auch `pio test -e native`).
- [ ] **P3 Arduino-Core 3.x**: Bei einem Update `ledcSetup` / `ledcAttachPin` umstellen.
- [ ] **P3 Unit-Tests** (`pio test -e native`) für `FrequencyUtils`. Voraussetzung: [constants.h](include/constants.h) darf `Arduino.h` nicht mehr einbinden (`<cstdint>` genügt).

## 2. SI5351 / HF-Funktion

- [~] **P2 Fehlerzustand**: Bei einem Fehlschlag kommt ein Dialog, der Schalter zeigt aber weiter ON, obwohl der Ausgang aus ist. Nach einem fehlgeschlagenen IQ-Setup ist CLK2 jetzt abgeschaltet (Powerdown), CH3 zeigt aber weiter „ON“. Ein I2C-Ausfall wird inzwischen als „NO HW“ angezeigt. Offen: den Ist-Zustand je Ausgang im `AppController` führen und in der GUI als rotes „ERR“ zeigen.
- [ ] **P2 Frequenzbereich CH1/CH2 auf 8 kHz – 160 MHz erweitern** (Entscheidung 2026-10-27). Der Treiber kann den Bereich schon (`OUT_MIN_HZ` / `OUT_MAX_HZ` in [si5351_manager.cpp](src/drivers/si5351_manager.cpp), unter 1 MHz mit R-Teiler); die Simulation ergibt ≤ 0,5 Hz Fehler bis ≈ 110 MHz und ≤ 2 Hz darüber. Teilaufgaben:
  - `FREQ_MIN_HZ` / `FREQ_MAX_HZ` in [constants.h](include/constants.h) anpassen; Numpad-Fehlertext („1 - 100 MHz“) und Eingabeprüfung folgen den Konstanten.
  - **CH3 bleibt bei 4,73 – 100 MHz**: `setupIQPair()` lehnt Frequenzen über `IQ_MAX_HZ` ab (Fehlerdialog). `IQ_MAX_HZ` zusätzlich in `iqConfigured_()` prüfen, neuer `Ch3Status::FreqTooHigh` mit Anzeige „WAIT … > 100 MHz“ auf MAIN und CH3.
  - Numpad: maximale Länge 9 → 10 Zeichen (z. B. „150.000001“ MHz).
  - Anzeige unter 1 MHz in kHz (z. B. „455.000 kHz“) statt „0.455 000 MHz“ ([frequency_utils.cpp:14-27](src/utils/frequency_utils.cpp#L14-L27)).
  - Auf der Kanalseite einen Hinweis zeigen: über 100 MHz Pegel geringer, unter 1 MHz starke Oberwellen (Tiefpass nötig).
  - Über ≈ 110 MHz arbeitet der Treiber mit festem Integer-Teiler und fraktionaler PLL; dort ändert jeder Schritt die PLL und erzeugt einen kurzen Aussetzer (PLL-Reset). Prüfen, ob die PLL bei kleinen Schritten ohne Reset nachgeführt werden kann (Messplan Nr. 21).
  - Handbücher DE/EN, Spezifikation (Abschnitt „Approved Deviations“) und README nachziehen. Messplan Nr. 20–22.

## 3. Bedienung (UX)

- [ ] **P3 Info-Zeile in CONFIG**: Firmwareversion, SI5351-Status (gefunden / fehlt, I2C-Pins) und Display-Variante. Das erleichtert die Fehlersuche ohne serielle Konsole.
- [ ] **P3 Werksreset** (NVS löschen) in CONFIG, mit Bestätigung.

## 4. Code-Qualität & Wartbarkeit

- [ ] **P3 IQ-Grenze hart kodiert**: „4.730 MHz“ bzw. „4.73 MHz“ stehen als Text in [page_ch3.cpp:26](src/gui/pages/page_ch3.cpp#L26), [page_ch3.cpp:56](src/gui/pages/page_ch3.cpp#L56) und [page_main.cpp:74](src/gui/pages/page_main.cpp#L74). Aus `IQ_MIN_HZ` formatieren.
- [ ] **P3 Redundanter Drive-Zustand**: `drive_strength` steht in `Channel`, `Channel3` und `SystemSettings`, ist aber immer gleich (globale Einstellung). Nur `sys.default_drive_strength` behalten.
- [ ] **P3 Ungenutzter Code**: `BacklightDriver::percent()`, `Storage::end()`, `AppController::ch1()/ch2()/state()`, `Logger::debug()`, `PIN_FREE_INPUT_ONLY` und `LV_HOR_RES_MAX` (ein LVGL-7-Rest in [lv_conf.h](include/lv_conf.h)).
- [ ] **P3 Feste Koordinaten in den Pages**: Header (158/214), CH3 (114/134/186), CONFIG (92/140/180/236) usw. in [gui_layout.h](src/gui/gui_layout.h) bzw. als lokale Konstanten zusammenfassen, wie in [page_main.cpp](src/gui/pages/page_main.cpp) schon geschehen.
- [ ] **P3 Logging im Normalbetrieb**: `valueU64` bei jeder Frequenzänderung, I2C-Scan bei jedem Start und `CORE_DEBUG_LEVEL=3` hinter `LOG_DEBUG` bzw. Level 1 legen.
- [ ] **P3 Mehrere Anweisungen pro Zeile** in [touch.cpp:22-23](src/drivers/touch.cpp#L22-L23).
- [ ] **P3 Unit-Tests für die Teilerberechnung**: `approxRatio()` und die Teilerwahl aus [si5351_manager.cpp](src/drivers/si5351_manager.cpp) in eine Hardware-unabhängige Datei auslagern und mit `pio test -e native` prüfen (Fehler ≤ 0,5 Hz, Registergrenzen). Bisher nur per Python-Nachbildung simuliert.
- [ ] **P3 Display-Flush mit DMA und Doppelpuffer.**

## 5. Dokumentation

- [ ] **P3 Handbücher/Spezifikation** auf die Punkte aus 1–3 nachziehen, sobald sie umgesetzt sind.

## 6. Entschieden, nicht umsetzen

- RGB-LED als Ausgangsanzeige: Die LED zeigt weiter die aktive Seite (Spezifikation 8.1).
- Doppelte Statusanzeige im Header: Sie bleibt, Spezifikation 9.3 verlangt die Textanzeige ON/OFF.

---

## 7. Rückfragen

Beantwortet am 2026-10-27: Alle Ausgänge starten aus (Abschnitt 3). Die USB-C-Firmware läuft am Gerät. Vorerst bleibt es bei zwei Build-Varianten, die Ein-Firmware-Lösung steht in Abschnitt 1.

1. **Neue Funktionen**: Sind gewünscht:
   a) Speicherplätze (z. B. 5 Frequenzen je Kanal),
   b) Wobbeln (Start / Stopp / Schrittweite / Verweilzeit),
   c) Drive-Strength je Kanal statt global?

---

## 8. Messplan für die Hardware-Verifikation

Geräte: Frequenzzähler, Oszilloskop (2 Kanäle), Spektrumanalysator. Die Tests 1–15 stammen aus dem Review vom 2026-10-26 und sind am Gerät noch offen.

| # | Test | Erwartung |
|---|---|---|
| 1 | Booten ohne SI5351 (oder mit getrenntem SDA) | genau **ein** Dialog, „OK“ schließt ihn, Karten zeigen „NO HW“ |
| 1a | Danach SI5351 anschließen (SDA verbinden) | nach ≤ 2 s verschwindet „NO HW“, Kanäle schaltbar |
| 1b | Im Betrieb CH1 an, SDA kurz trennen und wieder verbinden | Dialog „Connection lost“, „NO HW“, danach läuft CH1 wieder mit gleicher Frequenz |
| 1c | SI5351-Versorgung kurz unterbrechen (CH1 an) | wie 1b: Neuinitialisierung, CH1 läuft wieder |
| 2 | Helligkeit am Slider ganz nach links | Display bleibt bei 5 % sichtbar |
| 3 | CH1 = 10 MHz, Zähler an CLK0 | Abweichung zum Zähler ≈ angezeigte „Deviation“ + Quarzfehler |
| 4 | Kalibrierung: Abweichung bei 10 MHz in Hz ÷ 10 = ppm eintragen | danach < 1 Hz Abweichung bei 10 MHz |
| 5 | CH1 = 80 MHz, Schrittweite 1 Hz, 10× `+` | Zähler steigt um ≈ 10 Hz und bleibt **nicht hängen** |
| 6 | Oszilloskop auf CH1 (Trigger auf CH1, Nachleuchten), CH2 mehrfach verstellen | **kein** Aussetzer an CH1 |
| 7 | CH3 an, Master CH1 = 10 MHz, Phase 0/90/180/270, Scope CLK0 gegen CLK2 | 0°, 90°, 180°, 270° (± Kabellaufzeit) |
| 8 | wie 7 bei 5 MHz, 30 MHz und 99 MHz | alle vier Phasen korrekt |
| 9 | Master auf 4,72 MHz, dann 4,73 MHz | CH3 erst „WAIT … < 4.73 MHz“, danach automatisch aktiv |
| 10 | Spektrum CLK2 bei 4,73 MHz und 10 MHz | keine auffälligen Nebenlinien gegenüber CLK0 |
| 11 | Korrektur um +1 ppm ändern, Zähler an CH1 = 10 MHz | Frequenz sinkt um ≈ 10 Hz, ohne Aussetzer |
| 12 | `+` gedrückt halten | erster Schritt nach 0,4 s, dann alle 0,1 s, ohne Flackern |
| 13 | Numpad: „500“ + MHz, „0.5“ + MHz, „10.7“ + MHz | Fehler, Fehler, 10,700 000 MHz |
| 13a | Numpad: Eingabe mit „Hz“ bzw. „kHz“ bestätigen, Finger kurz liegen lassen | Kanalseite erscheint, Numpad öffnet **nicht** erneut, Schrittweite bleibt unverändert |
| 13b | Korrektur −1 ppm, CH3 an, Master = 100 MHz | Master und CH3 laufen (keine Meldung „CH3 IQ setup failed“) |
| 14 | Einstellung ändern, > 5 s warten („Saved“), aus- und einschalten | Einstellung bleibt erhalten, alle Ausgänge sind aus |
| 14a | Einschalten und Display beobachten | kein weißes oder verrauschtes Bild, Hauptseite erscheint direkt |
| 15 | Sichtprüfung aller Seiten | nichts abgeschnitten, keine Scrollbalken, kein Überlappen |
| 17 | Original-CYD mit `pio run -t upload` (ohne `-e`) flashen | nur `esp32-2432S028R` wird geflasht, Display läuft |
| 18 | CH1 = 10 MHz und 99 MHz, Schrittweite 1 Hz bzw. 1 MHz, `+` halten, Scope mit Nachleuchten | keine Lücken im Signal |
| 18a | Kanalseite: auf die Ziffer „1 kHz“ tippen, dann auf „MHz“ | Schritt 1 kHz und Ziffer farbig markiert; NUMPAD öffnet sich |
| 19 | Korrektur +50 ppm, Zähler an CH1 bei 1,9 MHz und 10,9 MHz | Abweichung ≈ angezeigte „Deviation“ + Quarzfehler, bei beiden Frequenzen gleich gut |
| 20 | Nach Bereichserweiterung: CH1 = 8 kHz, 455 kHz, 999,999 kHz, 1 MHz, Zähler | Frequenz stimmt, Anzeige in kHz bzw. MHz |
| 21 | CH1 = 120 MHz und 160 MHz, Zähler und Spektrum | Abweichung ≤ 2 Hz, Ausgang vorhanden (Pegel geringer) |
| 22 | CH3 an, Master von 100 MHz auf 100,000001 MHz stellen | CH3 wechselt auf „WAIT … > 100 MHz“, CLK2 aus, Master läuft weiter |
