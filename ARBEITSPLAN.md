# Arbeitsplan – CYD HF Generator

Stand: 2026-11-04 · Ausgangsstand: Firmware 1.3.0 (Branch `dev`, Commit 8ab70bc) · Ziel: Firmware 1.4.0 (WEB-HMI), danach Messkampagne

Diese Datei fasst [AUDIT_2026-11.md](AUDIT_2026-11.md), [KONZEPT_WEB_HMI.md](KONZEPT_WEB_HMI.md), [todo.md](todo.md) und [MESSPLAN.md](MESSPLAN.md) zusammen und enthält die Aufgabenliste in der Reihenfolge der Abarbeitung. Details, Begründungen und Rechnungen stehen in den Quelldateien; hier stehen nur die Kernaussagen und die Schritte.

---

## 0. Arbeitsregel: nach jedem Schritt wird committet

1. **Ein Schritt = ein Commit.** Jeder Schritt der Liste in Abschnitt 2 endet mit genau einem Commit auf dem Arbeitsbranch. Ist ein Schritt größer als einen Tag, wird er vorher in Teilschritte zerlegt, und jeder Teilschritt bekommt seinen Commit. Kein Sammel-Commit über mehrere Schritte, kein „später committen“.
2. **Erst das Gate, dann der Commit.** Der Commit entsteht erst, wenn das Gate des Schritts erfüllt ist:
   - Host-Tests grün in `native-win` (und `native`, sobald die CI steht);
   - 0 Warnungen in `src/` (mit `-Wall -Wextra`, ab Schritt S05);
   - neue Logik nur mit Host-Test, mit Gegenprobe (absichtlich eingebauter Fehler lässt den Test scheitern);
   - Handbuch DE **und** EN, Spezifikation (A-n / E-n), README und `todo.md` im **selben** Commit wie die Änderung;
   - bei Netzcode zusätzlich Review gegen die Sicherheitsanforderungen W-S.
   Ist das Gate nicht erfüllt, wird nicht committet, sondern korrigiert.
3. **Der Haken gehört in den Commit.** Im selben Commit wird der Schritt hier abgehakt (`[x]`); erledigte Punkte werden aus `todo.md` entfernt.
4. **Commit-Text:** Betreff auf Englisch im Imperativ mit der Schritt-Nummer (`S07: Keep CLK2 off while CH3 is in error`), im Text die Befund-IDs (C-1, U-3, W-S4 …) und eine Zeile zur Prüfung. Am Ende die Attributionszeile des Werkzeugs.
5. **Nur lokal.** Commits bleiben lokal; Push, Merge nach `main` und das Veröffentlichen von Tags passieren **nur auf ausdrückliche Anweisung**. Lokale Tags (`v1.3.0`, `v1.3.1` …) gehören zu den Schritten.
6. **Branches (Vorschlag):** Phasen 0–3 direkt auf `dev`; Phase 4 auf `feature/core3`; Phasen 5–6 auf `feature/web-hmi`; Merge nach `dev` je nach Phasenabnahme, nach `main` nur bei der Freigabe (S73).
7. **Schritte ohne Dateiänderung** (z. B. Umgebung einrichten) bekommen keinen eigenen Commit; sie werden im Plan abgehakt, und der Haken geht in den nächsten Commit.
8. **Hardware-Smoke-Test nach jeder Phase** auf beiden Boards (MESSPLAN 4.3, ≈ 45 min); das Ergebnis steht im Commit der Phasen-Freigabe.
9. **Nicht mischen:** Ein Commit enthält entweder Verhalten oder reines Aufräumen, nicht beides.
10. **Messprotokolle** (`.md`) werden committet, Screenshots, Fotos, Binärdateien und Logs nicht (`.gitignore`, Schritt S03).

---

## 1. Zusammenfassung

### 1.1 Gesamtbewertung (Audit)

| Bereich | Bewertung | Kernaussage |
|---|---|---|
| Code | gut | 161 Host-Tests (160 bestanden, 1 übersprungen), 0 Warnungen auch mit `-Wall -Wextra`. Kleine Fehler (C-1, C-2, C-5, C-13). 45 % der `src`-Zeilen (GUI, `main`, `display_panel`, `touch`) ohne Host-Test. |
| Security heute | niedrig | keine Netzwerkschnittstelle; Eingaben und NVS-Werte validiert; physischer Zugriff = Vollzugriff. ESP-IDF 4.4 ist seit 07/2024 ohne Support. |
| Security WEB-HMI wie zuerst geplant | hoch, bei Provisionierung kritisch | offener AP + Klartext-Provisionierung (Nachbar liest das Heim-WLAN-Passwort mit, S-1), CSRF, DNS-Rebinding, SCPI-Port aus dem Browser, XSS über SSIDs. Mit den Maßnahmen des Konzepts: niedrig bis mittel. |
| Bedienung | solide Basis, Fehlbedienungsrisiken | Header zeigt die Anforderung statt des Ist-Zustands (U-1), Sondermodi unsichtbar (U-2), Schalter 0,5 mm neben der Kartenfläche (U-3), kleine Rand- und Dialogtasten (U-4, U-5), Kontraste (U-7). |
| Plattform | veraltet | Offizielles `espressif32@8.0.0` (Arduino 3.3.12 / IDF 5.5.5) macht die Migration einfacher; LVGL 8.3 ohne Support, TFT_eSPI 2.5.43 ist das letzte Release. |
| Hardware | offen | fast der gesamte Messplan nie am Gerät gelaufen. |

### 1.2 Audit: Befunde

| Gruppe | Anzahl | Wichtigste Punkte | Schritte |
|---|---|---|---|
| Code C | 17 | **C-1** CLK2 bei ERR eingeschaltet · **C-3** Einfrieren bei vollem LVGL-Heap, kein Loop-Watchdog · **C-4** Parser nimmt `nan`, `1e7`, Hex · **C-5** ON/OFF löst 11 Flash-Schreibvorgänge aus · **C-13** Speichern wird nicht wiederholt · **C-16** Logger kollidiert mit SCPI über USB · **C-17** `*RST` gegen Autosave | S07 bis S16, S45, S46 |
| Security S | 28 | **S-1** Provisionierung über offenen AP (kritisch) · S-2/S-3 offener AP und Rückfall · S-4 CSRF · S-5 DNS-Rebinding · S-6 SCPI-Port aus dem Browser · S-7 XSS über SSIDs · S-16 kein NAPT · S-18 Fernschalten der Ausgänge · S-20 Abhängigkeiten pinnen | Konzept W-S1 bis W-S25, S05, S49 bis S58 |
| Bedienung U | 23 | **U-1** Header Ist-Zustand · **U-2** Sondermodi sichtbar · **U-3** Kartenschalter · U-4 bis U-7 Tasten und Kontraste · U-8 Lesbarkeit · U-10 Boot-Geste · U-11 Kalibrieren · U-13 Sperre/Grundzustand · U-23 Handbuch | S21 bis S33 |
| Plattform P | 11 | P-1 PlatformIO 6.1.5 im PATH · P-2 `espressif32@8.0.0` · P-4 Core 3.x · P-6 LVGL 8.4 später · P-8 Unity 2.7 später | S02, S37 bis S41, N11 |
| Testlücken T | 16 | T-1 `native-fault` · T-2 bis T-9 Controller, Speichern, Treiber · T-10 bis T-15 GUI-Logik testbar · T-16 Build-Info in der CI | S04, S07 bis S14, S18 bis S20 |

### 1.3 WEB-HMI (Konzept, Ziel 1.4.0)

- **Ziel:** WLAN (Client oder Access Point) über CONFIG, Weboberfläche mit allen Funktionen und Tastaturbedienung, Fernsteuerung per **SCPI** über TCP 5025 und USB-Seriell sowie REST; Bedienung am Gerät hat immer Vorrang.
- **Wichtigste Entscheidungen (W-E1 bis W-E28):**
  - Standard WLAN aus; Rückfall-AP nach 60 s;
  - AP mit **zufälligem WPA2-Passwort** je Gerät und **QR-Code**; Passwörter nie über einen offenen AP;
  - Captive-DNS nur im AP-Modus;
  - zwei gespeicherte WLANs; Web-Passwort optional (Sitzungs-Cookie, gehasht);
  - Netzänderungen zweistufig mit Rückfall nach 120 s;
  - ein SCPI-Client, Leerlauf 10 min; höchstens 3 Web-Clients;
  - Speichern nach 5 s (Touch, Web) bzw. 30 s (SCPI), nur geänderte Schlüssel, ON/OFF ohne Speichern;
  - `*RST` speichert nicht bis `SYSTem:SAVE`;
  - Option `Remote outputs: allowed / local only` (Standard allowed, nur am Gerät änderbar);
  - Statusleiste auf MAIN statt der Tasten CH1–CH3; Identify; Logger schweigt während SCPI über USB;
  - Plattform: offizielles `espressif32@8.0.0`.
- **Architektur:** CommandQueue und StateSnapshot (kein Netz-Task ruft Controller, LVGL, I2C oder NVS direkt); reine, host-testbare Module `request_guard`, `sanitize`, `line_framer`, SCPI-Kern; Bibliotheken ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, ArduinoJson 7.4.3, scpi-parser v2.3, LittleFS, mDNS; alle exakt gepinnt.
- **Sicherheit:** 25 prüfbare Anforderungen W-S1 bis W-S25 (Host-Allowlist, CSRF-Schutz, CSP, Limits, PMK-Speicherung, kein Routing, SCPI-Zeilenrahmen mit HTTP-Erkennung, Voll-Typ-Prüfung, Fuzzing, Watchdog, gepinnte Abhängigkeiten).
- **Aufwand:** Migration ≈ 2,5–3 d, Vorarbeiten V-1 bis V-6, V-11 ≈ 4 d, WEB-HMI ≈ 13,75 d (Konzept-Schritte 0–9), zusammen ≈ 20–21 d.

### 1.4 Offene Punkte aus todo.md und ihre Schritte

| todo-Abschnitt | Offen | Schritt |
|---|---|---|
| 1 Build | PATH 6.1.5, CI, Warnungen, Pinning, Migration, spätere Updates | S02, S04, S05, S37 bis S41, N11 |
| 2.1 Phasenrauschen | MultiSynth abgeschalteter Kanäle, Störquellen, Hardware-Hinweise (alle wartend auf Messung) | N5, N6, N10 |
| 2.2 IQ | IQ ohne Aussetzer, ungerade Teiler, Phasen-Feinabgleich (wartend auf Messung) | N2 bis N4 |
| 2.3 Robustheit | CLK2 bei ERR, Einfrieren, Wiederverbinden, Speichern, Flash-Schreibvorgänge; Sticky-Bit, Flash-Hinweis, VCO-Grenze | S07, S09, S11 bis S14, N9, S72 |
| 2.4 Bereich | 8 kHz bis 160 MHz mit Fix um 112,5 MHz | S34, S35 |
| 3 Bedienung | U-1 bis U-23 | S21 bis S27, S29, S31 bis S33 |
| 4 Code-Qualität | Parser, Dialoge, „~“, toter Code, Formatierung, feste Koordinaten, DMA | S08, S10, S15, S19, N12, N13 |
| 5 Doku | Doku-Abgleich, Handbuch ausbauen, Anwendungskapitel Mischer, 50 %-Tastverhältnis | S16, S29, S72, N8 |
| 6 Erweiterungen | WEB-HMI; Info-Seite, freie Phase, gekoppelter Betrieb, Speicherplätze, Wobbeln, Modulation, externe Referenz | S49 bis S58, N4, N14 |
| 7 Tests | `native-fault`, GUI-Logik, Fakes, CI-Prüfung des Commits | S04, S07 bis S14, S18 bis S20, S57 |
| 9 Rückfragen | neue Funktionen (a–f), Standard-Tuning | Abschnitt 1.6 |
| 10 Messplan | 150 Messungen | Phase 7 |

### 1.5 Messplan

[MESSPLAN.md](MESSPLAN.md): vollständiger Plan mit Messgeräten, Aufbauten, 150 Messungen mit Einzelschritten (A Inbetriebnahme, B Bedienung, C bis H HF, I und J Erkennung, K CONFIG, L WEB-HMI, L-S Sicherheit, Q Qualität), Bewertungsregel, Auswertung, Freigabekriterien, Rückverfolgbarkeit und Anhängen. Benötigt: Frequenzzähler mit 10-MHz-Referenz, Oszilloskop ≥ 500 MHz, Spektrumanalysator mit RBW ≤ 10 Hz, Logikanalysator, Multimeter, Zubehör (Abschlüsse, Dämpfungsglieder, gleich lange Kabel, Mischer), Smartphones, Router, 5 + 3 Testpersonen. Aufwand ≈ 9 Arbeitstage plus Auswertung.

### 1.6 Entscheidungen

**Getroffen** (Audit D1 bis D9, 2026-11-03 und 2026-11-04):

| # | Entscheidung |
|---|---|
| D1 | Passwörter nie über einen offenen AP |
| D2 | AP mit Zufallspasswort und QR; „offen“ nur auf Wunsch |
| D3 | Rückfall-AP offen nur nach Bestätigung am Touch |
| D4 | Option `Remote outputs`, Standard erlaubt |
| D5 | Plattform offiziell `espressif32@8.0.0` (Spike entscheidet endgültig) |
| D6 | Captive-DNS nur im AP-Modus |
| D7 | `*RST` speichert nicht bis `SYSTem:SAVE` |
| D8 | Statusleiste auf MAIN; Zeile „Step“ in CONFIG streichen |
| D9 | **Reihenfolge: erst Qualität, Bedienung und WEB-HMI, dann die Messkampagne** (2026-11-04) |

**Offen:**

1. **Neue Funktionen** (todo 9.1): Speicherplätze, Wobbeln, Drive je Kanal, Info-Seite „Synthesizer“, freie Phase, gekoppelter Betrieb; Entscheidung vor N14.
2. **Standard-Tuning** `Fractional` oder `Low noise`: nach den Messungen K5, K6, K7, F3 (Schritt S71).

**Folge der Reihenfolge D9:** Die Referenz 1.3.0 wird nur als Tag und Binärdatei gesichert (S03) und später nachgemessen (S61). Ändert die Migration (Core 3.x, `Wire`) HF-Eigenschaften, zeigen das erst die Messungen am Ende; deshalb der Smoke-Test nach jeder Phase und der Vergleich R0/R1 (MESSPLAN 7.2).

### 1.7 Aufwand

| Phase | Inhalt | Aufwand |
|---|---|---|
| 0 | Hygiene und Basis | 0,5 d |
| 1 | Robustheit 1.3.1 | ≈ 2 d |
| 2 | Bedienung 1.3.2 | ≈ 3–4 d |
| 3 | Laborbetrieb 1.3.3, Bereichserweiterung | ≈ 2–3 d |
| 4 | Migration Core 3.x | ≈ 2,5–3 d |
| 5 | WEB-HMI-Vorarbeiten | ≈ 4 d |
| 6 | WEB-HMI 1.4.0 | ≈ 13,75 d (Konzept) bis 15–16 d (Audit, mit Reserve) |
| 7 | Messkampagne und Auswertung | ≈ 10–11 d |
| 8 | Folgearbeiten (je nach Messergebnis) | offen |

Entwicklung bis zur Messkampagne ≈ 29–33 Arbeitstage.

---

## 2. Aufgabenliste

**Erinnerung: Nach jedem Schritt wird committet (Abschnitt 0).** Je Schritt: Inhalt, Prüfung (Gate), Aufwand, Commit-Betreff. `[ ]` offen, `[x]` erledigt und committet.

### Phase 0 – Hygiene und Basis (0,5 d)

- [x] **S01 · Dokumente committen.** `MESSPLAN.md`, `ARBEITSPLAN.md`, die Änderungen an `todo.md`, `AUDIT_2026-11.md`, `KONZEPT_WEB_HMI.md` und `CLAUDE.md` (Dokumentationsrollen). *Prüfung:* Verweise stimmen, `git status` sauber (außer `.codegraph/`). *Commit:* `S01: Add measurement plan and work plan, record order decision`
- [x] **S02 · PlatformIO 6.1.5 aus dem PATH entfernen** (P-1): nur `~/.platformio/penv/Scripts/pio.exe` (6.2.0) verwenden oder `pip install -U platformio`; `pio --version` prüfen. Keine Dateiänderung, kein eigener Commit. *Aufwand:* 10 min — *Erledigt 2026-11-04:* 6.1.5 aus der Python-3.10-Benutzerumgebung deinstalliert; `pio` steht nicht mehr im PATH, Aufruf über `~/.platformio/penv/Scripts/pio.exe` (6.2.0).
- [x] **S03 · Referenz 1.3.0 sichern.** Lokaler Tag `v1.3.0` auf 8ab70bc; Referenzbuild und SHA-256 unter `messungen/firmware/1.3.0/`; `.gitignore`: `messungen/*/` (Medien und Binärdateien, Protokolle `.md` direkt in `messungen/` bleiben im Repo). *Prüfung:* `git tag -l`, Build reproduzierbar. *Commit:* `S03: Ignore measurement artifacts, keep reference tag v1.3.0` (Tag lokal) — *Erledigt 2026-11-04:* `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` (Build im Worktree, Info-Karte zeigt Branch `HEAD`, Commit `8ab70bc` ohne `+`).
- [x] **S04 · CI.** GitHub Action mit `pio test -e native` und `pio run`; Actions per SHA gepinnt, `permissions: contents: read`, kein `pull_request_target` mit PR-Checkout (S-27); CI-Prüfung, dass das ELF den aktuellen Commit enthält (T-16). *Prüfung:* CI grün; `env:native` (Linux) läuft zum ersten Mal. *Aufwand:* 0,25 d. *Commit:* `S04: Add CI for host tests and firmware build` — *Stand 2026-11-04:* `.github/workflows/ci.yml` angelegt (checkout v7.0.1, setup-python v7.1.0, cache v6.1.0 per SHA; PlatformIO 6.2.0). Erster Lauf nach dem Push am 2026-11-04 (Run 41829737952, Commit 9a6f2ff) grün: Firmware-Build und `env:native` unter Linux.
- [x] **S05 · Warnungen und Pinning.** `-Wall -Wextra` für `src/`; XPT2046 von `#v1.4` auf den Commit `d57f64c8b5f2bc5b8d10d121550806eeff7b06d9` (S-20). *Prüfung:* 0 Warnungen, Tests grün, CI grün. *Commit:* `S05: Enable -Wall -Wextra and pin XPT2046 by commit` — *Erledigt 2026-11-04:* 21 Dateien aus `src/` mit `-Wall -Wextra`, 0 Warnungen; Bibliotheken ohne; Flash 623 045 B (19,8 %), RAM 90 236 B (27,5 %), unverändert. Hinweis: `pio run -v` scheitert bei `elf2image` (Ausgabefehler von PlatformIO im ausführlichen Modus), der normale Build läuft.
- [x] **S06 · Heap im Debug-Log.** Freier und minimaler Heap, größter Block, `lv_mem_monitor()` nach dem Boot und alle 30 s (nur `LOG_DEBUG`; Teil von V-8). *Prüfung:* Debug-Build auf dem Gerät zeigt Werte; Release unverändert. *Commit:* `S06: Log heap and LVGL memory in debug builds` — *Erledigt 2026-11-04* (Board an COM7, Original-CYD ohne SI5351, Debug-Build): Heap frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei, größter Block 38 396 B, maximal belegt 4 789 B; Stack-Reserve `loop()` 4 796 B. Release-Build +44 B Flash.

### Phase 1 – Robustheit 1.3.1 (≈ 2 d)

Jeder Fix kommt zusammen mit seinem Host-Test; der Test scheitert vor dem Fix (Gegenprobe).

- [x] **S07 · CLK2 bei ERR (C-1, T-1).** Bedingung `iqConfigured_() && !fault_[CLK_CH3]`; neue Umgebung `native-fault` mit `TEST_FAULT_IQ`. *Prüfung:* Test mit Off state High/High-Z; Hardware A8, A11 später. *Commit:* `S07: Keep CLK2 off while CH3 is in error (C-1)`
- [x] **S08 · „~“ nach dem Boot (C-2, T-2).** MAIN nach `applyStoredChannelSettings()` neu zeichnen; Test der Bootreihenfolge. *Commit:* `S08: Redraw MAIN after boot settings are applied (C-2)`
- [x] **S09 · Assert-Handler und Loop-Watchdog (C-3, S-23).** `LV_ASSERT_HANDLER` (Log + `esp_restart()`), `enableLoopWDT()`, Test-Flags `TEST_LVGL_ASSERT` und `TEST_LOOP_HANG` für Q4/Q5; Heap-Werte auf die Info-Karte (V-8). *Prüfung:* Host-Build, Hardware-Probe der Test-Flags. *Commit:* `S09: Restart on LVGL assert, enable loop watchdog (C-3)` — *Erledigt 2026-11-04:* am Board (COM7) geprüft: Assert → Neustart nach < 1 s (`SW_CPU_RESET`), blockiertes `loop()` → Task-Watchdog nach 5 s. Die Heap-Werte auf der Info-Karte sind nach S53 verschoben (Info-Karte wird dort ohnehin erweitert).
- [x] **S10 · Parser härten (C-4, T-4).** `isfinite()`, nur Ziffern und ein Punkt; Tests `nan`, `inf`, `1e7`, `0x…`, `1..2`, `.`, leer. *Commit:* `S10: Harden frequency parser (C-4)`
- [x] **S11 · Keine unnötigen Schreibvorgänge (C-5, T-7).** `markChanged_()` nur bei geänderten gespeicherten Feldern; ON/OFF und unveränderte Schrittweite speichern nichts. *Commit:* `S11: Save only when a stored value changed (C-5)`
- [x] **S12 · Speichern wiederholen (C-13, T-5).** `dirty_` behalten, Retry mit Backoff 30 s, Dialog einmal; Test mit NVS-Fehler beim Teilspeichern. *Commit:* `S12: Retry failed saves with backoff (C-13)`
- [x] **S13 · Wiederverbinden im selben Zyklus (C-12, T-3, T-6).** `connect_()` im selben 2-s-Zyklus; Tests für `millis()`-Überlauf und Fehlerinjektion `Wire.end()`/`begin()`. *Commit:* `S13: Reconnect in the same supervision cycle (C-12)`
- [x] **S14 · Integer-Grenzen und Spread bei Neuverbindung (C-7, T-8, T-9).** Grenzen aus `xtal4`; Test mit ±50 ppm; Neuverbindung mit Spread. *Commit:* `S14: Compute integer-mode VCO limits from the corrected crystal (C-7)`
- [x] **S15 · Unerreichbare Dialoge und toter Code (C-6, C-10).** Zweige entfernen bzw. auf Ergebniscodes vorbereiten; Handbuch DE/EN §11. *Commit:* `S15: Remove unreachable dialogs and dead code (C-6, C-10)`
- [x] **S16 · Doku-Abgleich (C-8, C-9).** Low noise „≈ 1,4 Hz“ gegen Test 1,5 Hz (A-12, Handbuch), WAIT-Gründe mit Spread-Grenzen, Kommentare in `pins.h` und `app_controller.cpp`, `todo.md` 2.2/2.4. *Commit:* `S16: Align documentation with code (C-8, C-9)`
- [x] **S17 · Release 1.3.1.** `APP_VERSION`, README „Changes“; Smoke-Test auf beiden Boards (MESSPLAN 4.3); lokaler Tag `v1.3.1`. *Commit:* `S17: Release 1.3.1` — *Erledigt 2026-11-04*, lokaler Tag `v1.3.1`. Smoke-Test nur teilweise: Original-CYD (COM7, ohne SI5351) bootet mit 1.3.1, ILI9341 erkannt, `SI5351 not found` wie erwartet; das CYD2USB mit SI5351 war nicht angeschlossen (COM13 fehlt). **Offen:** Smoke-Test MESSPLAN 4.3 mit Touch auf beiden Boards und mit SI5351.

- [x] **S17a · Verbindungszeile beim Boot (Befund M-1, 2026-11-04).** Beim Smoke-Test am CYD2USB fehlte `[APP] SI5351 at 0x60, SDA=… SCL=…` nach einem erfolgreichen ersten Verbinden, weil der Link-Zustand als `Connected` startete. Fix mit Host-Test (Serial-Fake merkt sich jetzt das ganze Log). *Commit:* `S17a: Log the first SI5351 connection at boot (M-1)`

### Phase 2 – Bedienung 1.3.2 (≈ 3–4 d)

- [ ] **S18 · Numpad und Ziffer-Antippen testbar (T-10, T-11).** `NumpadInput` (ein Dezimalpunkt, Länge, ⌫/C, Einheiten, Grenzen) und `charIndexAt`/`stepMarkup` als reine Funktionen. *Commit:* `S18: Extract numpad and digit-tap logic for host tests`
- [ ] **S19 · Message-Queue, Formatierung, Konstanten (T-12, T-13, C-11).** `FrequencyUtils::formatIqLimit()`, Zeiten nach `constants.h`, Textbausteine und Dropdown-Index ↔ Enum mit `static_assert`. *Commit:* `S19: Extract formatting and message queue logic (C-11)`
- [ ] **S20 · Touch-Filter und Display-Erkennung testbar (T-14, T-15).** Touch-Filter, Schwelle der Boot-Geste, Erkennung mit Wiederholungen. *Commit:* `S20: Make touch filter and display detection host-testable`
- [ ] **S21 · Header Ist-Zustand und Kartenschalter (U-1, U-3).** Header zeigt ON/OFF/WAIT/ERR/NO HW wie MAIN; Schalter in eigene Spalte, ≥ 12 px Abstand. *Commit:* `S21: Show actual channel state in header, separate card switch (U-1, U-3)`
- [ ] **S22 · Sondermodi sichtbar (U-2).** Detailzeile auf MAIN und Kanalseite („SSC -0.5 % · INT · 8 mA“, „OFF = HIGH“); Off state `High` einmal bestätigen. *Commit:* `S22: Show special modes on MAIN and channel pages (U-2)`
- [ ] **S23 · Tasten (U-4, U-5, U-6).** Dialog-OK ≥ 120 × 40 px mit ≥ 3:1 Kontrast, Randtasten bis an den Bildrand (`ext_click_area`), MAIN 36 px, Phasen-Chips 6 px Abstand. *Commit:* `S23: Enlarge dialog and edge buttons, space phase chips (U-4..U-6)`
- [ ] **S24 · Kontraste und Farbenblindheit (U-7, U-19).** Textfarbe nach Kontrast, Schalter-Spur und Chip-Rand ≥ 3:1, Zustände zusätzlich über Form („!“ bei ERR, gefülltes ON-Kennzeichen). *Commit:* `S24: Improve contrast and add non-color state cues (U-7, U-19)`
- [ ] **S25 · Statusleiste und CONFIG-Reihenfolge (W-E26, U-8, U-14, U-18).** Navigationstasten CH1–CH3 auf MAIN durch Statusleiste mit Speicherzustand und CONFIG ersetzen (WLAN-Symbole folgen in S53); ON/OFF-Kennzeichen 16 px; CONFIG-Reihenfolge, Zeile „Step“ streichen, Hinweis „(Integer: off)“. *Commit:* `S25: Replace MAIN channel buttons with a status bar, reorder CONFIG (U-8, U-14)`
- [ ] **S26 · Kleinere Punkte (U-9, U-12, U-15, U-17, U-21).** Numpad behält die Eingabe und zeigt einen Fehlertext; „°“ statt „deg“; Ziffer-Antippen als Komfortfunktion; Kanalseite „Off | …“; Meldungen mit Kanal und Abhilfe. *Commit:* `S26: Polish numpad errors, labels and messages (U-9, U-12, U-15, U-17, U-21)`
- [ ] **S27 · Boot-Geste absichern (U-10).** Dialog „Display OK?“, ohne Tipp nach 10 s zurück, nichts speichern; Handbuch §11. *Commit:* `S27: Confirm boot gesture before storing the controller (U-10)`
- [ ] **S28 · Test-Builds für Bedientests.** Touch-Log (x, y, Druck, Dauer, Ziel je Berührung) und Trefferkarte (20 Zielkreuze) als Build-Flags, nicht im Release (MESSPLAN B8, B9, B12). *Commit:* `S28: Add touch log and hit map test builds`
- [ ] **S29 · Handbuch ausbauen (U-23).** Inhaltsverzeichnis, einseitige Kurzanleitung, Seitenbilder, Glossar „Anzeige → Bedeutung“, Symptomtabelle, Hinweise zu Schrittweite und Boot-Geste; EN im selben Commit. *Commit:* `S29: Extend user manual with quick start, glossary and symptoms (U-23)`
- [ ] **S30 · Release 1.3.2.** Version, README, Smoke-Test auf beiden Boards, Tag `v1.3.2`. *Commit:* `S30: Release 1.3.2`

### Phase 3 – Laborbetrieb 1.3.3 (≈ 2–3 d)

- [ ] **S31 · Sperre, Grundzustand, Werksreset (U-13).** Sperre durch 2 s Halten mit „LOCK“; „Reset to defaults (keep calibration)“; Werksreset (NVS löschen) mit Bestätigung. *Commit:* `S31: Add screen lock, reset to defaults and factory reset (U-13)`
- [ ] **S32 · Kalibrieren (U-11).** Wert antippen öffnet das Numpad (ppm mit Vorzeichen); optional Kalibrierassistent (gemessene Frequenz eingeben). *Commit:* `S32: Enter correction via numpad (U-11)`
- [ ] **S33 · CH3-Seite und Touch-Feinheiten (U-16, U-20, U-22).** Masterfrequenz und Master-Schalter auf der CH3-Seite, Kanalwahl im Header; Wiederholung beschleunigen und Druckaussetzer überbrücken; Speicherzustand in der Statusleiste. *Commit:* `S33: Add master controls on CH3 page, tune touch repeat (U-16, U-20, U-22)`
- [ ] **S34 · Fix um 112,5 MHz.** Treiberfehler in `planStandalone()` (MultiSynth rundet auf 8/1; „aktuelle PLL behalten“ setzt `bestErr` nicht); Host-Tests 112 500 004 Hz. *Commit:* `S34: Fix tuning accuracy around 112.5 MHz`
- [ ] **S35 · Bereichserweiterung 8 kHz bis 160 MHz (E-16).** Planer, Controller, GUI (kHz-Anzeige, Numpad 10 Zeichen), `iqMaxHz()`, `FreqTooHigh`-Testfall mit Master > 100 MHz; Spezifikation A-1, E-16, Handbuch DE/EN, README. *Commit:* `S35: Extend CH1/CH2 range to 8 kHz-160 MHz (E-16)`
- [ ] **S36 · Release 1.3.3.** Version, README, Smoke-Test, Tag `v1.3.3`. *Commit:* `S36: Release 1.3.3`

### Phase 4 – Migration Core 3.x (≈ 2,5–3 d, Branch `feature/core3`)

- [ ] **S37 · Spike.** `espressif32@8.0.0` gegen pioarduino 55.03.312-1; TFT_eSPI, XPT2046 und LVGL unverändert; Flash, RAM und Heap gegen 1.3.x; Ergebnis als Notiz in `KONZEPT_WEB_HMI.md` Abschnitt 4. *Prüfung:* Firmware startet auf beiden Boards. *Commit:* `S37: Spike espressif32@8.0.0 with Arduino 3.3.12`
- [ ] **S38 · LEDC.** `ledcAttach()`/`ledcWrite(pin, …)`; Fake in `test/fakes/Arduino.h`. *Commit:* `S38: Port backlight PWM to the Arduino 3 LEDC API`
- [ ] **S39 · I2CBus bündeln (C-14, V-7).** `writeRegs`/`readRegs` in `I2CBus`; Annahmen aus 2.0.x als benannte Fake-Tests (Rückgabecode 5, `end()`/`begin()`, Lock). *Commit:* `S39: Bundle I2C register transfers in I2CBus (C-14)`
- [ ] **S40 · Compiler.** GCC-14-Warnungen beheben; Host-Test-Umgebung mit `-std=gnu++17`; CI erweitern. *Commit:* `S40: Fix GCC 14 warnings and add gnu++17 host environment`
- [ ] **S41 · Abnahme der Migration.** Beide Cores als Umgebungen bis zur Abnahme, danach 2.x-Umgebung entfernen; Spezifikation A-46 (gepinnte Versionen), README, `CLAUDE.md`; S-16 prüfen (`IP_FORWARD`/`NAPT`); Smoke-Test beider Boards; Merge nach `dev` (lokal). *Commit:* `S41: Switch to the official espressif32@8.0.0 platform`

### Phase 5 – WEB-HMI-Vorarbeiten (≈ 4 d, Branch `feature/web-hmi`)

- [ ] **S42 · Ergebniscodes (V-1).** Setter liefern `Result {Ok, OutOfRange, Invalid, Conflict, NoChange, HwError}`, Klemmen nur in `adjust*`, Kanalindex streng (S-12). *Commit:* `S42: Return result codes from controller setters (V-1)`
- [ ] **S43 · Änderungszähler (V-2).** `AppController::revision()`; GUI-Neuzeichnen läuft darüber. *Commit:* `S43: Add controller revision counter (V-2)`
- [ ] **S44 · Ereignisse (V-3).** `Event {code, category, source}` mit mehreren Empfängern statt `ErrorCallback`; Fernbefehle öffnen am Gerät keinen Dialog. *Commit:* `S44: Replace error callback with events (V-3)`
- [ ] **S45 · Speichern je Quelle und Schlüssel (V-4, C-17).** Fristen 5 s / 30 s, Dirty je Schlüssel, `saveNow()`, `*RST`-Semantik, `Storage` mit String-API und Fake mit mehreren Namespaces. *Commit:* `S45: Save per source and per key (V-4, C-17)`
- [ ] **S46 · Logger umschaltbar (V-5, C-16).** Serial / Ringpuffer / aus, mit Mutex; LVGL- und IDF-Log auf den Logger. *Commit:* `S46: Make logger output switchable (V-5, C-16)`
- [ ] **S47 · Ausführer und Schnappschuss (V-6).** Touch synchron, Netz über FreeRTOS-Queue (16 Einträge), Schnappschuss mit Sequenzzähler. *Commit:* `S47: Add command executor and state snapshot (V-6)`
- [ ] **S48 · Gehärteter Netz-Parser (V-11).** Ganzzahl in Hz oder Dezimalzahl mit Einheit, Komma und Leerzeichen, keine Exponenten/Hex. *Commit:* `S48: Add hardened frequency parser for remote input (V-11)`

### Phase 6 – WEB-HMI 1.4.0 (≈ 13,75 d, Konzept-Schritte 0–9)

Netzcode wird nach W-S geprüft; reine Module (`request_guard`, `sanitize`, `line_framer`, SCPI-Kern) frei von Arduino-Abhängigkeiten.

- [ ] **S49 · Vorab-Messung WLAN (Konzept-Schritt 0, 0,5 d).** Kurze Messung mit SPEKTRUM (Ausnahme zur Reihenfolge, weil das Ergebnis die Architektur betrifft): Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis in `KONZEPT_WEB_HMI.md`. *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **S50 · Befehlskern (Schritt 1, 0,75 d).** `command`, `state_snapshot`, Quelle, Option `Remote outputs` (W-E25); `test_command`. *Commit:* `S50: Add command core with source and remote-outputs option`
- [ ] **S51 · SCPI über USB (Schritt 2, 1,5 d).** `line_framer`, scpi-parser v2.3 (Commit gepinnt), Befehlsbaum, Logger-Umschaltung, `SYSTem:LOG?`; `test_scpi`, `test_line_framer`; PyVISA mit DTR/RTS aus, 1000 × `*IDN?`. *Commit:* `S51: Add SCPI over USB serial`
- [ ] **S52 · WLAN-Manager (Schritt 3, 1,5 d).** Betriebsarten, `WiFiMulti`, Rückfall-AP, `net_settings` (PMK, Zufallspasswort), mDNS, Captive-DNS nur im AP, kein Routing; `test_net_settings`. *Commit:* `S52: Add WiFi manager and network settings storage`
- [ ] **S53 · WLAN am Gerät (Schritt 4, 2 d).** WLAN-Symbole in der Statusleiste, CONFIG-Zeilen `WiFi` und `Remote outputs`, Seiten WIFI mit Tastatur, QR, Spinner, `sanitize`; Info-Karte mit WLAN, Heap und LVGL-Heap (V-8, aus S09); `test_sanitize`. *Commit:* `S53: Add WiFi setup pages with keyboard and QR`
- [ ] **S54 · SCPI über TCP (Schritt 5, 0,5 d).** Port 5025 mit HTTP-Erkennung, Leerlauf-Timeout, eine Verbindung, mDNS `_scpi-raw._tcp`. *Commit:* `S54: Add SCPI over TCP`
- [ ] **S55 · Webserver (Schritt 6, 2 d).** `request_guard` (Host, Origin, Sec-Fetch-Site, Content-Type, Token, Sitzung, Limits), REST, WebSocket, Login, Header, CSP; `test_http_guard`, `test_rest_json`. *Commit:* `S55: Add guarded web server and REST API`
- [ ] **S56 · Weboberfläche (Schritt 7, 2,5 d).** Alle Bereiche, Tastatur, responsiv, Live-Hervorhebung, zweistufige Netzänderung, Identify. *Commit:* `S56: Add web user interface`
- [ ] **S57 · Fuzzing und Dauertest (Schritt 8, 1 d + Laufzeit).** libFuzzer mit ASan/UBSan als CI-Job (≥ 10 Mio. Ausführungen), Zufallstest mit festem Seed in `native-win`; 24-h-Lauf mit Heap- und Stack-Bilanz (MESSPLAN L14 als Vorabprobe). *Commit:* `S57: Add fuzzing job and long-run test`
- [ ] **S58 · Doku und Release-Kandidat (Schritt 9, 1,5 d).** Handbuch DE/EN (WLAN, Web, SCPI-Referenz, REST, Python-Beispiel, Sicherheitshinweise), Spezifikation (A-n, E-n), README, `CLAUDE.md`, `todo.md`; Version 1.4.0, lokaler Tag `v1.4.0-rc1`; Smoke-Test beider Boards einschließlich L1 und `*IDN?`. *Commit:* `S58: Document WEB-HMI, release candidate 1.4.0-rc1`

### Phase 7 – Messkampagne ([MESSPLAN.md](MESSPLAN.md), ≈ 10–11 d)

Je Tag ein Commit mit dem Messprotokoll (Datei `messungen/<Datum>_<Board>_<Firmware>.md`, ohne Medien). Befunde (M-n) sofort in `todo.md` eintragen.

- [ ] **S59 · Eintrittskriterien und Testbuilds.** EK-1 bis EK-9 (MESSPLAN 4.2), Testbuilds `Debug`, `IQ-Fehler`, `Touch-Log`, `Trefferkarte`, `PLL ohne Reset`, `Assert/WDT`. *Commit:* `S59: Prepare test builds for the measurement campaign` (falls Dateien fehlen)
- [ ] **S60 · Vorbereitung V1 bis V5.** Boards und Ports zuordnen (COM7/COM13), Firmware sichern, Messgeräte prüfen, Eigenmessung des Aufbaus, Protokoll anlegen. *Commit:* `S60: Add measurement protocol header and setup measurements`
- [ ] **S61 · Referenz R0 (Firmware 1.3.0).** Auswahl aus MESSPLAN 5.3 auf dem Messboard. *Commit:* `S61: Record reference measurements for firmware 1.3.0`
- [ ] **S62 · Tag 1.** V-Schritte, A (außer A7), K1, K2, A9 bis A11. *Commit:* `S62: Measurement day 1 (robustness)`
- [ ] **S63 · Tag 2.** Modul umstecken: A6, A7, B6, B7, I1 bis I8, K10. *Commit:* `S63: Measurement day 2 (display detection)`
- [ ] **S64 · Tag 3.** J1 bis J15; C7 am Morgen starten. *Commit:* `S64: Measurement day 3 (I2C detection)`
- [ ] **S65 · Tag 4.** C1 bis C8, D1 bis D5. *Commit:* `S65: Measurement day 4 (accuracy and tuning)`
- [ ] **S66 · Tag 5.** E1 bis E10. *Commit:* `S66: Measurement day 5 (IQ)`
- [ ] **S67 · Tag 6.** F1 bis F9, G1, G2, K3 bis K7, H falls umgesetzt. *Commit:* `S67: Measurement day 6 (spectrum)`
- [ ] **S68 · Tag 7.** L1 bis L15, L22 bis L24; L14 starten. *Commit:* `S68: Measurement day 7 (network function)`
- [ ] **S69 · Tag 8.** L-S1 bis L-S13. *Commit:* `S69: Measurement day 8 (security)`
- [ ] **S70 · Tag 9.** Q1 bis Q5; Auswertung L14, C7; Bedientests B8 bis B20, L16 bis L21 mit Personen. *Commit:* `S70: Measurement day 9 (quality and usability)`
- [ ] **S71 · Auswertung und Entscheidungen.** MESSPLAN Abschnitt 7: Zusammenfassung, Vergleich R0/R1, Entscheidungsblatt (Standard-Tuning, IQ ohne Reset, ungerade Teiler, PDN, TCXO, Tastverhältnis, Störquellen, Stift, WLAN, Heap, Klone, Phasen-Feinabgleich); neue E-n in der Spezifikation; Folgeaufgaben in `todo.md`. *Commit:* `S71: Evaluate measurements and record decisions`
- [ ] **S72 · Handbuchwerte aus den Messungen.** Pegeltabelle (F2), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise, Anwendungskapitel Mischer (G1, G2) beginnen; DE und EN. *Commit:* `S72: Add measured values to the manuals`
- [ ] **S73 · Freigabe 1.4.0.** Kriterien MESSPLAN 7.4 erfüllt; Version, README, Tag `v1.4.0`; Merge nach `dev` und `main` **nur auf Anweisung**. *Commit:* `S73: Release 1.4.0`

### Phase 8 – Folgearbeiten (je nach Messergebnis, je ein Commit)

- [ ] **N1 · Standard-Tuning** ändern, wenn K5/K6/K7 es tragen (Rückfrage 9.2, E-19).
- [ ] **N2 · IQ-Abstimmen ohne Aussetzer** (Teiler mit Hysterese, PLL ohne Reset), wenn D3 bis D5 und E9 es zeigen.
- [ ] **N3 · Ungerade IQ-Teiler bei 180°/270°**, wenn E4 einen Fehler > 1° zeigt.
- [ ] **N4 · Phasen-Feinabgleich und freie Phase**, wenn E1 bis E3 systematische Fehler zeigen; Info-Seite „Synthesizer“ nach Rückfrage 9.1.
- [ ] **N5 · MultiSynth abgeschalteter Kanäle abschalten** und **PDN-Strategie** nach K2 und F7.
- [ ] **N6 · Störquellen-Abhilfen** (Backlight-PWM ≥ 20 kHz oder ohne PWM, Ruhemodus der Überwachung, Neuzeichnen nur bei Änderung) nach F6, F9.
- [ ] **N7 · TCXO-Option** nach C7, C8.
- [ ] **N8 · Anwendungskapitel „Mischer-Experimente“** vollständig (beide Handbücher) nach F2, G1, G2, E10.
- [ ] **N9 · Spannungseinbruch über Sticky-Bit erkennen**.
- [ ] **N10 · Hardware-Hinweise** (eigener LDO mit Ferrit, kurze Masse, Abschirmung) in den Handbüchern.
- [ ] **N11 · Spätere Updates:** LVGL 8.4.0, Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6; Advisory-Check je Release (nicht: LVGL 9, espressif32 6.13.0).
- [ ] **N12 · Feste Koordinaten in den Pages** durch das Layoutgitter ersetzen.
- [ ] **N13 · Display-Ausgabe mit DMA und Doppelpuffer** (nach der Migration, gleicher SPI-Treiber).
- [ ] **N14 · Erweiterungen nach Rückfrage 9.1:** Speicherplätze, Wobbeln, Drive je Kanal, gekoppelter Betrieb, Modulationsarten, externe Referenz (SI5351C).

---

## 3. Pflege dieser Datei

- Nach jedem Commit steht der Haken im selben Commit (Abschnitt 0, Punkt 3).
- Kommt ein Befund dazu (M-n, neue Audit-Punkte), wird er mit Priorität in [todo.md](todo.md) eingetragen und hier als Schritt eingeordnet; die Nummern bestehender Schritte bleiben stabil.
- Änderungen der Reihenfolge werden in Abschnitt 1.6 vermerkt (Entscheidung mit Datum).
