# Arbeitsplan – CYD HF Generator

Stand: Release **2.1.0** · Arbeitszweig `dev`, `main` nur per Pull Request (Merge-Commit) · auf `dev` nach 2.1.0: Doku als Pages-Seiten (G9), Werkzeuge kennen CH2-Kopplung und Drehung, Doku konsolidiert (DK1), Planungsdateien neu nummeriert · als Nächstes: MES-01 (Gerätetest 3.3.13), FW-07, FW-08, REL-02/REL-03 sobald möglich; die Messungen (MES-02 bis MES-08) kommen zuletzt, danach REL-01

Einzige Planungsdatei: Arbeitsregeln, Stand, offene Befunde und offene Aufgaben. Anforderungen (A-nnn) und alle Entscheidungen (E-nnn, auch die früheren Entscheidungen Dn dieser Datei und das WEB-HMI-Konzept mit W-E und W-S) stehen in [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md), Messungen (M-nnn) in [MESSPLAN.md](MESSPLAN.md), das Verhalten in [USER_MANUAL_DE.md](USER_MANUAL_DE.md).

**Nummern.** Offene Aufgaben tragen eine Bereichskennung und eine zweistellige Nummer je Bereich: `FW-nn` Firmware, `DOK-nn` Dokumentation, `REL-nn` Release, GitHub und CI, `MES-nn` Messungen und Gerätetests, `USR-nn` Aufgaben für den Nutzer (Einstellungen im Repo, Fotos, Meldungen). Eine Nummer bleibt stabil und wird nach dem Erledigen nie neu vergeben. Erledigte Arbeit behält ihre historischen Schrittnamen aus der Git-Historie (z. B. S41d, S55, AF54, U27, LX1, G9, N14, DK1), ebenso die Befund-IDs der Audits (C-n, S-n, U-n, P-n, T-n, V-n, SEC-n, SEC-V-n, LIC-n, CQ-n, DOC-n, PERF-n, OPS-n, TEST-n, A11Y-n). Das Präfix `DOK-` unterscheidet die Doku-Aufgaben von den Audit-Befunden `DOC-n`.

---

## 0. Arbeitsregeln

1. **Eine Aufgabe = ein Commit.** Jede Aufgabe aus Abschnitt 3 endet mit genau einem Commit. Größere Aufgaben werden vorher geteilt (FW-02a/FW-02b); kein Sammel-Commit.
2. **Erst das Gate, dann der Commit:**
   - Host-Tests grün in `native-win` und `native-win-fault` (CI: `native`, `native-fault` und `native-cpp20` mit `-Werror`);
   - 0 Warnungen in `src/` (`-Wall -Wextra`, Host-Tests ebenso); die CI scheitert an Warnungen in `src/`/`include/` der Firmware-Builds und an jeder Warnung in `native-cpp20` (E-807);
   - neue Logik nur mit Host-Test; der Test scheitert vor dem Fix (Gegenprobe);
   - Handbuch DE **und** EN, Spezifikation (A-nnn / E-nnn), README und diese Datei im **selben** Commit;
   - bei Netzcode Review gegen die Sicherheitsanforderungen (Spezifikation Kapitel 7, A-701 bis A-725).
3. **Der Abschluss gehört in den Commit.** Eine erledigte Aufgabe wird im selben Commit aus Abschnitt 3 entfernt und in Abschnitt 1.1 zusammengefasst; erledigte Befunde verschwinden aus Abschnitt 2.
4. **Bedientest am Gerät vor dem Commit**, wenn sich die Oberfläche ändert: flashen, die Prüfpunkte dem Nutzer nennen, Rückmeldung abwarten.
5. **Commit-Text:** Betreff Englisch im Imperativ mit Aufgaben-ID (`FW-07: Detect SI5351 power loss by the sticky bit`), im Text die Befund-IDs und eine Zeile zur Prüfung; am Ende die Attributionszeile.
6. **Push nach jeder Aufgabe.** Der Arbeitszweig wird nach jedem Commit gepusht, damit kein Zwischenstand verloren geht (Nutzer); Arbeitszweige der Agenten ebenso, sobald sie committet sind. Merge nach `main` und Tags veröffentlichen nur auf ausdrückliche Anweisung.
7. **Branches:** gearbeitet wird auf `dev`. `main` ist per Ruleset geschützt: Änderungen nur per Pull Request `dev` → `main` mit grünen Pflicht-Prüfungen; GitHub legt den Merge-Commit mit der Kontoadresse an (vom Nutzer akzeptiert). Agenten arbeiten auf eigenen Branches, die nach der Übernahme gelöscht werden.
8. **Smoke-Test nach jedem größeren Block** auf beiden Boards (MESSPLAN 4.3); Ergebnis im Release-Commit bzw. direkt danach.
9. **Nicht mischen:** Ein Commit enthält Verhalten oder Aufräumen, nicht beides.
10. **Messprotokolle** (`messungen/*.md`) werden committet, Medien und Binärdateien nicht (`messungen/*/` ist ignoriert). Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md`; Messbefunde (MB-n, MESSPLAN Anhang E) kommen sofort in Abschnitt 2 und als Aufgabe in Abschnitt 3.

---

## 1. Stand

### 1.1 Erledigt (Zusammenfassung je Release, Details in der Git-Historie)

| Release | Inhalt |
|---|---|
| 1.3.1 | Hygiene (PlatformIO nur aus `penv`, CI mit per SHA gepinnten Actions, `-Wall -Wextra` ohne Warnungen, XPT2046 auf Commit gepinnt, Referenz `v1.3.0` gesichert); Robustheit C-1 bis C-13, M-1: Assert- und Loop-Watchdog-Neustart, gehärteter Parser, Speichern nur bei Änderung und mit Wiederholung, Wiederverbinden im selben Zyklus |
| 1.3.2 | Bedienung U-1 bis U-10, U-12, U-14, U-15, U-17 bis U-19, U-21 bis U-26; GUI-Logik host-getestet (T-10 bis T-15); Statusleiste auf MAIN, Kontraste, Boot-Geste entfernt (E-302), Touch-Testbuilds, Touch-Abgleich (E-303), Handbuch mit Kurzanleitung und Glossar |
| 1.3.3 | Laborbetrieb U-11, U-13, U-16, U-20, N12: Bildschirmsperre, Grundzustand und Werksreset, Korrektur per Numpad, CH3-Seite mit Master-Steuerung, Bereich 8 kHz–160 MHz (E-203), Fix um 112,5 MHz, kompakte Moduszeile |
| 1.4.0-rc1 | Migration auf Core 3 (S37–S41d: Arduino 3.3.12 / IDF 5.5.5, `-Os`, LVGL 8.4.0 E-304, nur noch Core 3 E-803); WEB-HMI-Vorarbeiten V-1 bis V-6, V-11, C-16, C-17 (S42–S48); WEB-HMI bis S58: Befehlskern mit Quellen, SCPI über USB (E-602) und TCP 5025, Netzeinstellungen `hfnet` (E-510), WLAN-Manager mit Rückfall-AP, mDNS und Captive-DNS (E-511), WIFI-, Access-Point- und Device-Seiten, LVGL-Pool aus dem Heap (E-305), Request-Guard, REST, WebSocket, Login und eingebettete Weboberfläche (E-621, E-515, E-702), Fuzzing mit 9 Zielen (S57a–c) |
| 2.0.0 | LXI und VXI-11 (LX1, E-614 bis E-616); GitHub: Release-Workflow mit Herkunftsnachweis und Webflasher (G1, G2, G11, E-905), Tag-Ruleset (E-809), `SECURITY.md`, CodeQL, Dependabot, Größenbericht, Vorlagen, gerenderte Bilder, Gehäuse (G4–G9, G11–G13); Projekt-Audit mit Folgearbeiten AF1–AF10, AF29–AF31 (E-611, E-704, E-906 bis E-908); Version 2.0.0 mit neuer Historie (E-904) |
| nach 2.0.0 | U27 einheitliche Bedienung: CONFIG in fünf Registern (E-311), eine Kopfzeile mit `◀` (E-312), kein `Saved`/`Saving...` am Gerät (E-313); API-Referenz `docs/REMOTE_API.md`; AF11–AF19, AF44–AF46, AF48, AF51a–AF53, AF55: WLAN ohne Energiesparen (E-512), Suche mit Backoff (E-513), ein Besitzer der Netzeinstellungen (E-514), `SAVE` höchstens alle 10 s (E-402), Login auch für Lesen (E-703), Push gebündelt; Handbuch §17 und §18 |
| 2.1.0 | Audit-Priorität 3 (CI-Trigger, Warnungs-Gate, exakte Pins, reproduzierbare Stempel E-806, Release-Probelauf, Advisory-Prüfung; E-807), Priorität 4 (Vertragsvektoren, ASan/UBSan, neue Fuzz-Ziele, NVS-Grenzen, Messhelfer), Priorität 6 (Texteingabe als Logik, `Gui` und Webserver aufgeteilt, asynchrone REST-Antworten E-622); Priorität 5 entfällt (E-315); N11 Arduino 3.3.13 / IDF 5.5.6 über pioarduino (E-804); N14 CH2 = CH1 + Offset (E-213); N16 `Rotate` (E-314); am Gerät geprüft |
| nach 2.1.0 (`dev`) | G9 Handbuch, Hardware-Seite und Remote-API als Pages-Seiten (E-903); Werkzeuge kennen CH2-Kopplung und Drehung; DK1 Doku konsolidiert; 10-Mio.-Fuzz-Lauf ohne Fund; Spezifikation, Messplan und Arbeitsplan neu aufgebaut und einheitlich nummeriert |

Host-Tests: 524 Fälle in `native-win` und 4 in `native-win-fault`.

### 1.2 Referenzwerte

- **Firmware 1.3.0:** `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` unter `messungen/firmware/1.3.0/` (nicht im Repo).
- **Heap (Debug-Build 1.3.1, Original-CYD ohne SI5351):** frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei; Stack-Reserve `loop()` 4 796 B. Basis für den Vergleich nach der Migration (S37).
- **Flash/RAM 1.3.2:** 630 069 B (20,0 %) / 90 312 B (27,6 %); **1.3.3:** 638 033 B (20,3 %) / 90 568 B (27,6 %).
- **Touch-Abgleich:** CYD2USB x 161…3778, y 245…3830; Original-CYD x 166…3716, y 280…3885 (vorher fest 300…3800); Original-CYD nach dem Werksreset x 158…3708, y 245…3856, Prüfkreuz −2/+1 px.
- **Core 3 (Spike S37):** Release-Build Flash 668 247 B (2.x 645 445 B), RAM 96 440 B (2.x 94 496 B); Debug-Build, Original-CYD ohne SI5351: Heap frei 250 684 B (2.x 272 604 B), Minimum 221 872 B (2.x 267 008 B), Stack-Reserve `loop()` 4 880 B, erstes Bild 172 ms (LVGL 8.4.0: 169 ms; 2.x 159 ms), `Setup complete` nach 0,9 s.
- **Flash/RAM `feature/web-hmi` S54a (Core 3):** 1 381 971 B (43,9 %) / 76 476 B (23,3 %); 2.x-Env 1 201 629 B / 74 384 B.
- **Flash/RAM nach S41d (nur Core 3):** 1 405 583 B (44,7 %) / 77 748 B (23,7 %); **nach S56:** 1 535 690 B (48,8 %) / 78 536 B (24,0 %); **vor N11 (3.3.12):** 1 527 058 B / 78 608 B; **nach N11 (3.3.13, pioarduino):** 1 553 065 B (49,4 %) / 78 992 B (24,1 %).
- **SCPI über TCP (CYD2USB, Büro-WLAN, S54):** `*IDN?` Median 14,7 ms, 99 % 97 ms, max. 1,4 s (WLAN-Latenzspitzen); Heap nach 8 s Flut 133 KB frei, Minimum 81 KB.
- **SCPI über USB (CYD2USB):** `*IDN?` Median 6,0 ms, 99 % < 11,6 ms (1000 Abfragen, S51); `device_check.py` 94 PASS (S54a).
- **Boards:** COM13 = CYD2USB (ST7789) mit SI5351, COM7 = Original-CYD (ILI9341) ohne SI5351.
- **Smoke-Tests 1.3.1, 1.3.2 und 1.3.3:** bestanden bis auf die Schritte 5 und 6 (10 MHz und IQ 90° messen) – kein Messgerät verfügbar; nachholen in MES-05 (MESSPLAN Stufe 1).

---

## 2. Offene Befunde

`[M]` = wartet auf eine Messung (MESSPLAN-ID in Klammern). Jeder Befund nennt die Aufgabe, die ihn erledigt.

### 2.1 SI5351, HF und IQ

- [M] **MultiSynth abgeschalteter Kanäle abschalten** (M-118, M-311): CH1/CH2 im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter; CLK2 bekommt `CLK2_PDN` nur bei Off state `Low` (E-208). → FW-05
- [M] **Störquellen der Plattform** (M-310, M-418): Backlight-PWM 5 kHz (Seitenlinien ±5 kHz möglich), I2C-Überwachung alle 2 s; das Neuzeichnen nur bei Änderung ist mit AF12 erledigt. → FW-06
- [M] **IQ-Abstimmen mit Aussetzer** (M-304, M-305, M-303, M-306): jede Frequenz- oder Korrekturänderung im IQ-Betrieb setzt die PLL zurück (1000 Schritte → 1000 Resets). → FW-02
- [M] **180°/270° bei ungeraden Teilern** (M-308): die Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°; der IQ-Teiler `min(127, ⌊900 MHz / f⌋)` ist in 54 % der Fälle ungerade (14,2 MHz → 63, 99 MHz → 9). → FW-03
- [M] **Spannungseinbruch des Moduls ohne aktiven Ausgang** (Gerätetest wie M-106): `checkAlive()` erkannte einen SI5351-Neustart nur, solange ein Ausgang konfiguriert war. Umgesetzt und host-getestet: die Überwachung liest `SYS_INIT`/`SYS_INIT_STKY` (Register 0/1) mit, die Info-Karte zeigt `PLL locked` (A-226, E-214); offen ist nur der Gerätetest mit SI5351. → FW-07
- [ ] **Hinweise fehlen im Handbuch:** SI5351 läuft beim Flashen und nach ESP32-Reset mit der letzten Einstellung weiter (M-119); Hardware-Hinweise (eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO nach M-313); VCO bei `Fractional` und +50 ppm 900,045 MHz (unkritisch, A-214). → DOK-01, DOK-02, DOK-03

### 2.2 Bedienung

- [ ] **Netzwahl nach falschem Passwort am Gerät nicht geprüft** (S53b, Prüfpunkt h): zwei Netze gespeichert, das stärkere mit falschem Passwort → nach dem Fehlschlag verbindet sich das Gerät mit dem anderen (A-504). Host-getestet (`WifiPolicy::chooseSlot`). → MES-07

### 2.3 Plattform und Lieferkette

- [ ] **Plattform außerhalb des PlatformIO-Registers** (P-2, P-4, S-19): pioarduino 55.03.313 / Arduino 3.3.13 / IDF 5.5.6 per Release-URL mit SHA-256-Prüfung (E-804). → REL-03; Gerätetest des Updates → MES-01
- [ ] **Unity 2.6.1** (P-8): 2.7.0 noch nicht im Register. → REL-02
- [ ] **Keine Warnung bei knappem Heap** (S-13): die Betriebsgrenze A-726 (40 KB / 16 KB) braucht eine Logzeile `Heap low`, die es noch nicht gibt. → FW-08

### 2.4 Dokumentation

- [M] **„50 % Tastverhältnis“** (Handbuch §1, A-201) ist nicht belegt (M-309). → DOK-01

---

## 3. Offene Aufgaben

Je Aufgabe: **Warum** (Anlass, Befund), **Was** (Umfang), **Prüfung** und Commit-Betreff. Abhängigkeiten stehen bei der Aufgabe.

### 3.1 Firmware (FW)

- [ ] **FW-01 · Standard-Tuning festlegen** (nach MES-06)
  - *Warum:* `Fractional` ist Standard, weil es ohne Aussetzer abstimmt (E-206); `Low noise` bringt womöglich weniger Nebenlinien. Offene Rückfrage an den Nutzer: bleibt `Fractional`, oder wird `Low noise` Standard, wenn die Stufe-2-Messungen einen klaren Vorteil zeigen?
  - *Was:* Kriterium aus MESSPLAN 6.1 mit M-301, M-302, M-303 auswerten; nur bei klarem Vorteil den Standard von `tune` ändern, Entscheidung als neue E-2nn.
  - *Prüfung:* Host-Test des Standardwerts und alter NVS-Inhalte (fehlender Schlüssel); Handbuch DE/EN.
  - *Commit:* `FW-01: Make Low noise the default tuning mode`
- [ ] **FW-02 · IQ-Abstimmen ohne Aussetzer** (nach MES-06)
  - *Warum:* Befund 2.1 „IQ-Abstimmen mit Aussetzer“; die Phase bleibt nur erhalten, wenn Teiler und Phasen-Offset unverändert bleiben.
  - *Was:* den IQ-Teiler mit Hysterese festhalten und nur die PLL ohne Reset schreiben (wie `Low noise` im Einzelbetrieb), ebenso bei Korrekturänderungen; vorher Testbuild „PLL ohne Reset“ für M-305.
  - *Prüfung:* Host-Test „PLL-Änderung ohne Reset, Offset und Teiler unverändert“; M-304, M-305, M-306 am Gerät.
  - *Commit:* `FW-02: Retune IQ pairs without PLL reset`
- [ ] **FW-03 · Ungerade IQ-Teiler bei 180°/270°** (nur wenn M-308 einen um > 1° größeren Fehler zeigt)
  - *Warum:* Befund 2.1 „180°/270° bei ungeraden Teilern“.
  - *Was:* zwischen 4,77 und 90 MHz einen geraden Teiler mit VCO 600–900 MHz wählen (es gibt immer einen) oder 180° über den Phasen-Offset (2 × Teiler ≤ 127) erzeugen.
  - *Prüfung:* Host-Test (Teiler gerade, Phase im Registermodell); M-115, M-116, M-308 wiederholen.
  - *Commit:* `FW-03: Use even IQ dividers for 180 and 270 degrees`
- [ ] **FW-04 · Phasen-Feinabgleich, Skew-Kompensation** (optional; nur wenn M-307 oder M-306 einen systematischen Fehler > 1° zeigen)
  - *Warum:* MESSPLAN 6.1 „Phasen-Feinabgleich“; optionale Aufgabe (Nutzer); ein freier, vom Nutzer einstellbarer Phasen-Feinabgleich bleibt ausgeschlossen (E-101), hier geht es nur um den Ausgleich eines gemessenen festen Versatzes.
  - *Was:* konstanten Ausgangs-Skew aus M-307 über den Phasen-Offset ausgleichen.
  - *Prüfung:* M-115, M-116, M-307 wiederholen; Host-Test des Offsets.
  - *Commit:* `FW-04: Compensate the output skew of the IQ pair`
- [ ] **FW-05 · MultiSynth abgeschalteter Kanäle abschalten, PDN-Strategie** (nach M-118, M-311)
  - *Warum:* Befund 2.1 „MultiSynth abgeschalteter Kanäle“ (E-208).
  - *Was:* ist ein Ausgang mit `CLKx_PDN` hochohmig (M-118), bei `High-Z` alle abgeschalteten Ausgänge mit PDN abschalten; zeigt M-311 einen abgeschalteten Kanal als Linie > −70 dBc, dessen MultiSynth abschalten. Handbuch-Hinweis „bis zur Hardwareprüfung M-118“ anpassen.
  - *Prüfung:* Host-Test der Register (PDN, OE) je Off state; M-117, M-118, M-311 wiederholen.
  - *Commit:* `FW-05: Power down the MultiSynth of switched-off outputs`
- [ ] **FW-06 · Störquellen-Abhilfen** (nach M-310, M-418)
  - *Warum:* Befund 2.1 „Störquellen der Plattform“; Kriterium MESSPLAN 6.1 (Störer > −60 dBc im Anwendungsbereich).
  - *Was:* Backlight-PWM ≥ 20 kHz oder 100 % ohne PWM, Ruhemodus der I2C-Überwachung; „Neuzeichnen nur bei Änderung“ (AF12) und der Backoff der SI5351-Suche (AF53, PERF-11) sind erledigt.
  - *Prüfung:* M-310 wiederholen; Host-Test der Überwachungstakte.
  - *Commit:* `FW-06: Reduce platform spurs`
- [ ] **FW-07 · Gerätetest Sticky-Bit und „PLL locked“** (Umsetzung erledigt; offen nur der Gerätetest mit SI5351, Hardware derzeit nicht verfügbar)
  - *Warum:* Befund 2.1 „Spannungseinbruch ohne aktiven Ausgang“.
  - *Erledigt:* Commit `FW-07: Detect SI5351 power loss by the sticky bit` – `begin()` löscht Register 1, die Überwachung liest Register 0/1 alle 2 s in einer Übertragung mit; `SYS_INIT`/`SYS_INIT_STKY` gesetzt → Wiederverbinden und neu einrichten, auch ohne Ausgang; Zeile `PLL locked: A ok  B ok` auf der Info-Karte (A-226, E-214); Host-Tests im Registermodell.
  - *Offen (Gerätetest):* alle Ausgänge aus, die Versorgung des Moduls kurz unterbrechen (wie M-106) → Meldung `Connection lost`, Log `[ERROR][SI5351] Device restarted (SYS_INIT)`, danach verbunden; Off state `High` bleibt nach dem Einbruch wirksam (Register 24 neu geschrieben). Info-Karte: `PLL locked: A ok  B ok`, ohne Modul `PLL locked: -`; Layout der 9 Zeilen ohne Abschneiden auf beiden Boards.
  - *Abschluss:* Ergebnis im Messprotokoll, Haken hier und in Befund 2.1.
- [ ] **FW-08 · Warnung `Heap low`** (vor MES-04)
  - *Warum:* A-726 verlangt eine Logzeile unter 40 KB freiem Heap; M-205 prüft sie, die Firmware hat sie noch nicht.
  - *Was:* in `loop()` den freien Heap prüfen; unter 40 KB einmal `Heap low` mit freiem Heap und größtem Block loggen, erneut erst nach Erholung über 48 KB (Hysterese); Grenzen in `constants.h`.
  - *Prüfung:* Host-Test der Hysterese; Handbuch DE/EN (Logzeilen); im Debug-Build mit 3 WebSockets keine Warnung.
  - *Commit:* `FW-08: Log a warning when the free heap drops below 40 KB`

### 3.2 Dokumentation (DOK)

- [ ] **DOK-01 · Messwerte in die Handbücher** (nach MES-05 und MES-06)
  - *Warum:* Befunde 2.1 „Hinweise fehlen“ und 2.4 „50 % Tastverhältnis“; Pegel und Mischerwerte in §17/§18 sind als „typisch, nicht gemessen“ markiert.
  - *Was:* Pegeltabelle (M-312), Mischer-Kapitel §18 (M-312, M-419, M-420, M-413, M-308), Tastverhältnis (M-309, A-201 präzisieren bei > 2 % Abweichung), Aufwärmzeit (M-313), WLAN-Hinweis (M-317), „SI5351 läuft beim Flashen weiter“ (M-119); DE und EN.
  - *Prüfung:* Werte gegen die Messprotokolle; Parität DE/EN; Docs-Site-Prüfung.
  - *Commit:* `DOK-01: Add measured values to the manuals`
- [ ] **DOK-02 · TCXO-Option** (nach M-313, M-411)
  - *Warum:* `docs/hardware.md` nennt einen 25-MHz-TCXO als optional, abhängig von der Aufwärmdrift.
  - *Was:* TCXO empfehlen, wenn die Drift nach 15 min Aufwärmzeit > 0,5 ppm/h beträgt (MESSPLAN 6.1); sonst den Hinweis streichen.
  - *Prüfung:* Werte gegen M-313; Handbuch DE/EN und `docs/hardware.md`.
  - *Commit:* `DOK-02: Recommend a TCXO for low drift`
- [ ] **DOK-03 · Hardware-Hinweise in den Handbüchern**
  - *Warum:* Befund 2.1 „Hinweise fehlen“.
  - *Was:* eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO (DOK-02); VCO-Hinweis bei `Fractional` und +50 ppm.
  - *Prüfung:* Parität DE/EN; Docs-Site-Prüfung.
  - *Commit:* `DOK-03: Add hardware hints to the manuals`

### 3.3 Release, GitHub und CI (REL)

- [ ] **REL-01 · Freigabe** (nach MES-02 bis MES-07)
  - *Warum:* das nächste Release erscheint mit dem gemessenen Stand.
  - *Was:* Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag; Merge nach `main` und Tag nur auf Anweisung.
  - *Prüfung:* Release-Probelauf grün, Advisory-Prüfung ohne Fund.
  - *Commit:* `REL-01: Release`
- [ ] **REL-02 · Unity 2.7.0** (sobald es im PlatformIO-Register steht)
  - *Warum:* Befund 2.3 „Unity 2.6.1“.
  - *Was:* Unity exakt auf 2.7.0 pinnen, `THIRD_PARTY_NOTICES.md` und `tools/dep_check.py` nachziehen.
  - *Prüfung:* alle Host-Envs grün, `dep_check.py` ohne Abweichung.
  - *Commit:* `REL-02: Update Unity to 2.7.0`
- [ ] **REL-03 · Zurück auf die Plattform des PlatformIO-Registers** (sobald sie Arduino 3.3.13 oder neuer anbietet)
  - *Warum:* Befund 2.3 „Plattform außerhalb des Registers“; E-804 nennt die Rückkehr als Ziel.
  - *Was:* Plattform und Framework aus dem Register, URL-Pins und SHA-Liste in `dep_check.py` entfernen, Notices nachziehen.
  - *Prüfung:* 0 Warnungen, Größenbericht, Advisory-Prüfung; Gerätetest wie MES-01.
  - *Commit:* `REL-03: Build on the PlatformIO registry platform again`

### 3.4 Messungen und Gerätetests (MES)

- [ ] **MES-01 · Gerätetest Arduino 3.3.13 / IDF 5.5.6** (ohne Messgeräte)
  - *Warum:* E-804 ist nur gebaut und host-getestet; der Gerätetest des Updates ist offen.
  - *Was:* Boot, Display-Erkennung beider Boards, Touch, SI5351, WLAN Client und AP, Weboberfläche, SCPI USB/TCP, VXI-11/LXI (`lxi discover -m` und das Gerät selbst, E-614), Heap nach 10 min.
  - *Prüfung:* Protokoll mit PASS/FAIL je Punkt, `device_check.py` und `web_check.py` gegen das Gerät.
  - *Commit:* `MES-01: Record the device test of Arduino 3.3.13`
- [ ] **MES-02 · Abnahme Core 3** (≈ 4 h, braucht ZÄHLER und OSZI)
  - *Warum:* Core 2 wurde vor der Abnahme verworfen (E-803); TEST-2.
  - *Was:* Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10), dabei CLK0 bei M-128/M-129 und M-314 (Code 5 bei Leitung auf GND).
  - *Prüfung:* Befunde nach MESSPLAN 3.6; bei einem Befund ist der Rückweg der letzte Release-Tag auf Core 2.
  - *Commit:* `MES-02: Record the core 3 acceptance measurements`
- [ ] **MES-03 · Vorab-Messung WLAN** (0,5 d, braucht SPEKTRUM)
  - *Warum:* WLAN läuft ohne Energiesparen und mit voller Sendeleistung (E-512); PERF-15.
  - *Was:* Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), `PS_NONE` gegen `MIN_MODEM` und eine reduzierte Sendeleistung (z. B. 11 dBm), Heap-Bilanz.
  - *Prüfung:* Ergebnis im Messprotokoll und in 1.2; eine Änderung danach als eigene E-5nn.
  - *Commit:* `MES-03: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **MES-04 · 24-h-Dauertest** (1 d Laufzeit)
  - *Warum:* der erste Lauf brach nach 35 min fehlerfrei ab (USB, 4086 Schreiben/Rücklesen, 0 Abweichungen, 0 Resets, min. Heap 81,5 KB mit WLAN), weil der USB-Port auf der PC-Seite verschwand; Fuzzing ist erledigt (S57a–c, manueller Lauf mit 10 Mio. je Ziel ohne Fund).
  - *Was:* 24-h-Lauf an stabilem Port (oder über TCP) mit Heap- und Stack-Bilanz (Weboberfläche offen, Debug-Log alle 60 s mit Stack-Reserve von `async_tcp` und `web_login`; ggf. Stack von `async_tcp` senken); Worst Case „alle Kanäle belegt“ (3 WebSockets, 1-Hz-Polling, SCPI-TCP-Flut, USB-SCPI gleichzeitig, PERF-8); High-Water-Marks `async_tcp`/`web_login` und LVGL `max_used` nach einer Seitenrunde (PERF-14); Betriebsgrenze A-726 (40 KB / 16 KB) nach M-205, M-319.
  - *Prüfung:* kein Neustart, minimaler Heap stabil und ≥ 40 KB, größter Block ≥ 16 KB (A-726), Stack-Reserve ≥ 1 KB (A-723).
  - *Commit:* `MES-04: Record the 24-hour run`
- [ ] **MES-05 · Stufe 1** (Pflicht, ≈ 7 h, sobald Zähler und Oszilloskop verfügbar sind; dieser Lauf ist die Referenz R0, MESSPLAN 9.2)
  - *Warum:* belegt am Gerät, was nur die Hardware zeigen kann (MESSPLAN 1.1).
  - *Was:* MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10).
  - *Prüfung:* Zusammenfassung MESSPLAN 9.1; Messbefunde in Abschnitt 2.
  - *Commit:* `MES-05: Record tier-1 measurements`
- [ ] **MES-06 · Stufe 2** (Entscheidungen, ≈ 6 h)
  - *Warum:* die Folgeaufgaben FW-01 bis FW-06, DOK-01 und DOK-02 hängen an den Messwerten.
  - *Was:* MESSPLAN Abschnitt 6: Standard-Tuning (M-301, M-302, M-303), IQ ohne Reset (M-304, M-305, M-306), Phasenfehler und Tastverhältnis (M-307, M-308, M-309), Störquellen und PDN (M-310, M-311), Pegel (M-312), Drift (M-313).
  - *Prüfung:* Entscheidungsblatt MESSPLAN 9.3; Entscheidungen als E-nnn in die Spezifikation, Folgeaufgaben bestätigen oder streichen.
  - *Commit:* `MES-06: Record tier-2 measurements and decisions`
- [ ] **MES-07 · Stufe 1W** (Pflicht ab WEB-HMI, ≈ 8 h + 24 h)
  - *Warum:* Netz- und Sicherheitsanforderungen (Kapitel 5–7 der Spezifikation) am Gerät; Befund 2.2; offen aus dem Audit-Konzept 6 der Vertragstest gegen das Gerät; TEST-1, TEST-2.
  - *Was:* MESSPLAN 5.11 auf beiden Boards; dazu M-317, M-318, M-319 aus Stufe 2; aus S54: Abbau einer verwaisten TCP-Verbindung nach ≈ 90 s (Keepalive), Sperre bei offenem AP (auch Portmapper und VXI-11), mDNS `_scpi-raw._tcp`; M-211: der AP geht in `WifiManager::update()` aus `loop()` aus, nicht im IP-Event (E-511, A-703), die Grenze ≤ 1 s hängt an der `loop()`-Latenz; `tools/web_check.py <Gerät>` ohne und mit Passwort und `tools/contract_check.py <Gerät>` (AF34); M-210 mit `slowloris.py` (AF39); Scan und Netzänderung aus dem Web über den Job-Postfach-Weg (AF33, AF50); Netzwahl nach falschem Passwort (Befund 2.2).
  - *Prüfung:* PASS/FAIL-Liste ins Protokoll; ein ✘ bei M-206 bis M-216 stoppt die Freigabe (MESSPLAN 9.4).
  - *Commit:* `MES-07: Record network and security measurements`
- [ ] **MES-08 · Stufe 3** (optional)
  - *Warum:* Charakterisierung, Ausbildungsversuche und Bedientests mit Personen bei Anlass.
  - *Was:* MESSPLAN Abschnitt 7 nach Bedarf.
  - *Prüfung:* kein Freigabekriterium; Befunde in Abschnitt 2.
  - *Commit:* `MES-08: Record optional characterization measurements`

### 3.5 Aufgaben für den Nutzer (USR)

Einstellungen im Repo, Fotos und Meldungen, die nur der Nutzer erledigen kann. Bereits per API gesetzt (AF28, E-811): Topics, Dependabot-Sicherheitsupdates, Environment `copilot` entfernt; Pages (Quelle GitHub Actions) mit Umgebung `github-pages` (Regeln `main`, `v*`), Secret Scanning und Push-Schutz sind eingerichtet.

- [ ] **USR-01 · Vorschaubild hochladen** (kein Commit)
  - *Warum:* das Vorschaubild lässt sich nur im Browser setzen.
  - *Was:* `docs/img/social-preview.png` (1280 × 640, < 1 MB) unter Settings → General → Social preview hochladen.
  - *Prüfung:* Vorschau eines Links auf das Repo.
- [ ] **USR-02 · Projektseite vervollständigen** (kein Commit, außer `docs/hardware.md` muss korrigiert werden)
  - *Warum:* die Projektseite auf GitHub ist noch unvollständig.
  - *Was:* Beschreibung, Website `https://dm8sb.github.io/CYD-HF-Generator/`, Topics; CN1-Steckerraster (1,25 mm) und Pinfolge am Board bestätigen.
  - *Prüfung:* Repo-Startseite; bei Abweichung `docs/hardware.md` korrigieren.
- [ ] **USR-03 · Fotos** (später)
  - *Warum:* bis dahin stehen gerenderte Bilder in README, `docs/hardware.md`, `case/README.md` und Handbuch Anhang A (G13), darunter eine gerenderte `social-preview.png`.
  - *Was:* nach der Liste in `docs/hardware.md`: `board-cyd-front.jpg` und `board-cyd2usb-front.jpg` (MAIN-Seite, CH1 an), `si5351-wiring.jpg` (Modul mit den 4 Leitungen zu CN1 und CLK0–2), `cn1-connector.jpg` (Rückseite, Beschriftung lesbar), `setup-bench.jpg` (Aufbau mit Koax zum Messgerät), je ≤ 500 KB, kein WLAN-Name auf dem Display; ablegen unter `docs/img/`. Danach die Platzhalter in `docs/hardware.md` aktivieren; das Vorschaubild kann durch eine Version mit Foto ersetzt werden.
  - *Prüfung:* Docs-Site-Prüfung (Bilder und Links).
  - *Commit:* `USR-03: Add hardware photos`
- [ ] **USR-04 · PING-Flut an ESPAsyncWebServer melden** (optional, kein Commit)
  - *Warum:* ESPAsyncWebServer beantwortet jedes PING in einer unbegrenzten Kontrollwarteschlange (E-703); das Gerät begrenzt es selbst, die Bibliothek sollte es auch.
  - *Was:* Issue im Projekt `ESP32Async/ESPAsyncWebServer` mit Beschreibung und Abhilfe.
  - *Prüfung:* Link zum Issue in dieser Datei vermerken.

---

## 4. Pflege dieser Datei

- Eine erledigte Aufgabe verschwindet im selben Commit aus Abschnitt 3; Abschnitt 1.1 fasst sie je Release kurz zusammen, Details stehen in der Git-Historie. Erledigte Befunde verschwinden aus Abschnitt 2.
- Neue Aufgaben bekommen die nächste freie Nummer ihres Bereichs (FW, DOC, REL, MES, USR); Nummern werden nie neu vergeben, auch nicht nach dem Erledigen. Eine Aufgabe, die sich aufteilt, bekommt Buchstaben (FW-02a, FW-02b).
- Neue Befunde (Messbefunde MB-n aus Messungen, Befunde aus Bedientests, Reviews oder Audits) kommen mit Bezug in Abschnitt 2 und als Aufgabe in Abschnitt 3.
- Entscheidungen stehen nur in der Spezifikation (E-nnn, mit Begründung, ohne Datum und ohne Release-Nummern des Projekts); hier steht keine eigene Entscheidungsliste. Eine offene Rückfrage an den Nutzer steht bei der Aufgabe, die von ihr abhängt.
