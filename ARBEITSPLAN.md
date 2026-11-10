# Arbeitsplan – CYD HF Generator

Stand: 2026-11-09 · letztes Release **1.3.3** (Tag `v1.3.3` = `5da646c`); `main` = `dev` = `feature/web-hmi` = Entwicklungsstand nach 1.3.3 (Phase 6 bis S56, Phase 7 G1/G4/G6–G9a, nur Core 3; `APP_VERSION` noch 1.3.3), auf Wunsch des Nutzers nach `dev` und `main` zusammengeführt, CI/Fuzz/CodeQL grün · nächste Ziele: S57-Rest, S58 und **1.4.0**; Abnahme der Migration mit Messgeräten (S41c)

Einzige Planungsdatei: offene Befunde, WEB-HMI-Konzept, Entscheidungen und Aufgabenliste. Sie ersetzt `todo.md`, `AUDIT_2026-11.md` und `KONZEPT_WEB_HMI.md` (vollständig in der Git-Historie bis `3e452f8`). Die Befund-IDs aus dem Audit (C-n Code, S-n Security, U-n Bedienung, P-n Plattform, T-n Tests) und dem Konzept (W-E, W-S, V-n) bleiben als Namen gültig. Messungen am Gerät: [MESSPLAN.md](MESSPLAN.md). Verhalten: [USER_MANUAL_DE.md](USER_MANUAL_DE.md); Anforderungen und Entscheidungen: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md).

---

## 0. Arbeitsregeln

1. **Ein Schritt = ein Commit.** Jeder Schritt der Aufgabenliste (Abschnitt 4) endet mit genau einem Commit. Größere Schritte werden vorher geteilt (S25a/S25b); kein Sammel-Commit.
2. **Erst das Gate, dann der Commit:**
   - Host-Tests grün in `native-win` und `native-win-fault` (CI: `native`, `native-fault`);
   - 0 Warnungen in `src/` (`-Wall -Wextra`);
   - neue Logik nur mit Host-Test; der Test scheitert vor dem Fix (Gegenprobe);
   - Handbuch DE **und** EN, Spezifikation (A-n / E-n), README und diese Datei im **selben** Commit;
   - bei Netzcode Review gegen die Sicherheitsanforderungen W-S (Abschnitt 3.7).
3. **Der Haken gehört in den Commit.** Erledigte Schritte werden in Abschnitt 4 abgehakt und beim nächsten Aufräumen entfernt; erledigte Befunde verschwinden aus Abschnitt 2.
4. **Bedientest am Gerät vor dem Commit**, wenn sich die Oberfläche ändert: flashen, die Prüfpunkte dem Nutzer nennen, Rückmeldung abwarten.
5. **Commit-Text:** Betreff Englisch im Imperativ mit Schrittnummer (`S31: Add screen lock …`), im Text die Befund-IDs und eine Zeile zur Prüfung; am Ende die Attributionszeile.
6. **Push nach jedem Schritt.** Der Arbeitszweig wird nach jedem Commit gepusht, damit kein Zwischenstand verloren geht (Nutzer, 2026-11-06); Arbeitszweige der Agenten ebenso, sobald sie committet sind. Merge nach `main` und Tags veröffentlichen nur auf ausdrückliche Anweisung.
7. **Branches:** gearbeitet wird auf `feature/web-hmi`; `dev` und `main` werden auf Anweisung nachgezogen (zuletzt 2026-11-09). Agenten arbeiten auf eigenen Branches, die nach der Übernahme gelöscht werden; `agent/g11` (G11, ersetzt `agent/license-audit`) wartet auf die Übernahme.
8. **Smoke-Test nach jeder Phase** auf beiden Boards (MESSPLAN 4.3); Ergebnis im Release-Commit bzw. direkt danach.
9. **Nicht mischen:** Ein Commit enthält Verhalten oder Aufräumen, nicht beides.
10. **Messprotokolle** (`messungen/*.md`) werden committet, Medien und Binärdateien nicht (`messungen/*/` ist ignoriert).

---

## 1. Stand

### 1.1 Erledigt (Details in der Git-Historie)

| Phase | Version | Inhalt |
|---|---|---|
| 0 Hygiene | – | PlatformIO nur aus `penv` (6.2.0), CI mit per SHA gepinnten Actions, `-Wall -Wextra` ohne Warnungen, XPT2046 auf Commit gepinnt, Heap im Debug-Log, Referenz `v1.3.0` gesichert |
| 1 Robustheit | 1.3.1 | C-1 bis C-13, M-1; Assert- und Loop-Watchdog-Neustart (am Gerät geprüft), gehärteter Parser, Speichern nur bei Änderung und mit Wiederholung, Wiederverbinden im selben Zyklus |
| 2 Bedienung | 1.3.2 | U-1 bis U-10, U-12, U-14, U-15, U-17 bis U-19, U-21 bis U-26; GUI-Logik host-getestet (T-10 bis T-15); Statusleiste auf MAIN, CONFIG neu geordnet, Kontraste, Boot-Geste entfernt, Touch-Testbuilds, Touch-Abgleich, Handbuch mit Kurzanleitung und Glossar |
| 3 Laborbetrieb | 1.3.3 | U-11, U-13, U-16, U-20, E-16, N12; Bildschirmsperre, Grundzustand und Werksreset, Korrektur per Numpad, CH3-Seite mit Master-Steuerung, Halten langsamer am Anfang, Bereich 8 kHz–160 MHz, Fix um 112,5 MHz, kompakte Moduszeile |
| 4 Migration | `dev`, S41d `feature/web-hmi` | espressif32 8.0.0 (Arduino 3.3.12 / IDF 5.5.5, `-Os`), LEDC-API, I2C-Zugriffe in `I2CBus` (C-14, V-7), GCC-14-Warnungen behoben, Host-Env `native-cpp20`, LVGL 8.4.0 (E-28), Smoke-Test mit Core 3, Routing-Entscheidung D14; seit S41d nur noch Core 3 (D16, E-32), Abnahmemessung S41c offen |
| 5 WEB-HMI-Vorarbeiten | `dev` | V-1 bis V-6, V-11, C-16, C-17 (S42–S48): Ergebniscodes, Änderungszähler, Ereignisse mit Quelle, Speichern je Quelle und Schlüssel, Logger mit Ringpuffer, Befehlsqueue mit Schnappschuss, Netz-Parser |
| 6 WEB-HMI bis S56 | `main`, `dev`, `feature/web-hmi` | Befehlskern mit Quellen, Ratenbegrenzung und `Remote outputs`; SCPI über USB (E-29) mit Identify; Netzeinstellungen `hfnet` (E-30); WLAN-Manager (Client, Rückfall-AP, mDNS, Captive-DNS, D14); CONFIG `WiFi`/`Remote out.`, Statusleiste mit WLAN und `REMOTE`; Seiten WIFI (Scan, Tastatur), Access Point (QR) und Device (Name, statische IP); LVGL-Pool aus dem Heap (E-31); Host-Werkzeuge in `tools/` (`device_check.py`); SCPI über TCP 5025 (S54); Request-Guard und REST-JSON (S55a); Weboberfläche `data/www/` mit Mock-Server (S56a); Fuzzing mit 9 Zielen, CI-Job und Zufallstest, Fix langer Zahlen (S57a–c); Webserver mit REST, WebSocket, Login und eingebetteter Oberfläche (S55, E-33–E-35), Web-Passwort auf der Seite Device und Remote-Clients mit `Reconnect all` auf der Info-Karte (S55b), feste Kopfzeile und Abmelden (S56); Fuzz-Funde behoben (lange Exponenten, 32-bit-Zeit im Guard, doppelte Schlüssel, einzelnes Surrogat in Schlüsseln) |
| 7 GitHub (Teil) | `main`, `dev`, `feature/web-hmi` | Release-Workflow mit Herkunftsnachweis und Webflasher-Seite (G1), `SECURITY.md` und CodeQL (G4), Dependabot (G6), Größenbericht in der CI (G7), Issue- und PR-Vorlagen (G8), Badges und `docs/hardware.md` (G9a); LF für alle Textdateien (`.gitattributes`, `.editorconfig`) |

Host-Tests: 376 Fälle in `native-win` und 4 in `native-win-fault` (Stand S56).

### 1.2 Referenzwerte

- **Firmware 1.3.0:** `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` unter `messungen/firmware/1.3.0/` (nicht im Repo).
- **Heap (Debug-Build 1.3.1, Original-CYD ohne SI5351):** frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei; Stack-Reserve `loop()` 4 796 B. Basis für den Vergleich nach der Migration (S37).
- **Flash/RAM 1.3.2:** 630 069 B (20,0 %) / 90 312 B (27,6 %); **1.3.3:** 638 033 B (20,3 %) / 90 568 B (27,6 %).
- **Touch-Abgleich (2026-11-04):** CYD2USB x 161…3778, y 245…3830; Original-CYD x 166…3716, y 280…3885 (vorher fest 300…3800); Original-CYD nach dem Werksreset (2026-11-05) x 158…3708, y 245…3856, Prüfkreuz −2/+1 px.
- **Core 3 (Spike S37, 2026-11-05):** Release-Build Flash 668 247 B (2.x 645 445 B), RAM 96 440 B (2.x 94 496 B); Debug-Build, Original-CYD ohne SI5351: Heap frei 250 684 B (2.x 272 604 B), Minimum 221 872 B (2.x 267 008 B), Stack-Reserve `loop()` 4 880 B, erstes Bild 172 ms (LVGL 8.4.0: 169 ms; 2.x 159 ms), `Setup complete` nach 0,9 s.
- **Flash/RAM `feature/web-hmi` S54a (Core 3):** 1 381 971 B (43,9 %) / 76 476 B (23,3 %); 2.x-Env 1 201 629 B / 74 384 B.
- **Flash/RAM nach S41d (nur Core 3):** 1 405 583 B (44,7 %) / 77 748 B (23,7 %); **nach S56:** 1 535 690 B (48,8 %) / 78 536 B (24,0 %).
- **SCPI über TCP (CYD2USB, Büro-WLAN, S54):** `*IDN?` Median 14,7 ms, 99 % 97 ms, max. 1,4 s (WLAN-Latenzspitzen); Heap nach 8 s Flut 133 KB frei, Minimum 81 KB.
- **SCPI über USB (CYD2USB):** `*IDN?` Median 6,0 ms, 99 % < 11,6 ms (1000 Abfragen, S51); `device_check.py` 94 PASS (S54a).
- **Boards:** COM13 = CYD2USB (ST7789) mit SI5351, COM7 = Original-CYD (ILI9341) ohne SI5351.
- **Smoke-Tests 1.3.1, 1.3.2 und 1.3.3:** bestanden bis auf die Schritte 5 und 6 (10 MHz und IQ 90° messen) – kein Messgerät verfügbar; nachholen in MESSPLAN Stufe 1.

### 1.3 Entscheidungen

| # | Entscheidung (Datum) |
|---|---|
| D1 | WLAN-Passwörter nie über einen offenen AP (2026-11-03) |
| D2 | AP mit zufälligem WPA2-Passwort und QR; offen nur auf Wunsch |
| D3 | Offener Rückfall-AP nur nach Bestätigung am Touch |
| D4 | Option `Remote outputs: allowed / local only`, Standard `allowed` |
| D5 | Plattform offiziell `espressif32@8.0.0` (Spike S37 entscheidet endgültig) |
| D6 | Captive-DNS nur im AP-Modus |
| D7 | `*RST` speichert nicht bis `SYSTem:SAVE` |
| D8 | Statusleiste auf MAIN, Zeile `Step` in CONFIG gestrichen (umgesetzt in 1.3.2) |
| D9 | Erst Qualität, Bedienung und WEB-HMI, dann die Messkampagne (2026-11-04) |
| D10 | Boot-Geste entfernt, nur automatische Display-Erkennung (2026-11-04, E-26) |
| D11 | Touch-Abgleich beim ersten Start (2026-11-04, E-27) |
| D12 | Ein Plan statt todo/Audit/Konzept; Messplan nach Priorität (Stufe 1 Pflicht, 1W Pflicht ab WEB-HMI, 2 Entscheidungen, 3 optional). Ändert D9: Stufe 1 (und möglichst 2) läuft, sobald Zähler und Oszilloskop verfügbar sind, auf 1.3.x vor der Migration und ist die Referenz R0; ein eigener R0-Lauf mit 1.3.0 entfällt (2026-11-04) |
| D13 | LVGL 8.4.0 kommt mit der Migration (S40a), LVGL 9 bleibt ausgeschlossen; Phase 5 ist ohne Verhaltensänderung schon in `dev` (2026-11-05) |
| D14 | Routing (S-16, Rückfrage 3): offizielle Plattform mit vorkompiliertem IDF bleibt, obwohl `CONFIG_LWIP_IP_FORWARD=y` fest eingebaut ist. Stattdessen W-S3 strikt: AP und STA nie gleichzeitig mit Adresse, der AP geht im Event der STA-IP aus, Prüfung ≤ 1 s (L-S6). Kein pioarduino mit `custom_sdkconfig` (eigener IDF-Build, längere Builds, zweite Plattform-Quelle) (2026-11-06) |
| D15 | WLAN-Schlüssel als Passphrase statt PMK (W-S14 geändert, E-30): ESP-IDF 5.5 nimmt bei WPA2/WPA3-Übergangsnetzen immer SAE, das die Passphrase braucht; mit PMK scheiterte das Büro-WLAN (Grund 2/202), mit Passphrase verbunden in 3,4 s. Nie angezeigt, geloggt oder exportiert (2026-11-06, Nutzer) |
| D16 | Nur noch Core 3 (`espressif32@8.0.0`), Core 2 vor der Abnahmemessung verworfen; Abnahmemessungen bleiben (S41c), Rückweg Tag `v1.3.3` (2026-11-09, Nutzer, E-32) |
| D17 | Lizenz GPL-3.0-or-later, Copyright-Zeile „Copyright (C) 2026 DM8SB“; privates Projekt, einzige Commit-Identität `DM8SB <dm8sb@gmx.de>`, Historie darauf umgeschrieben (Branch-Spitzen alt → neu: `main` fa72ec5 → 5dd72ac, `dev` 23e2775 → 193056c, `feature/web-hmi` ecf15bf → 4b487c1); ESP Web Tools auf GitHub Pages selbst gehostet (kein CDN); kein Impressum, sondern Name, Hobbyprojekt und Gewährleistungsausschluss auf der Flasher-Seite; Lizenzzeile nur in der Weboberfläche, nicht auf dem Display (2026-11-10, Nutzer, A-77, E-36) |
| D18 | Neue Funktionen nach 1.4.0: nur der gekoppelte Betrieb CH2 = CH1 ± Offset (N14); Speicherplätze, Wobbeln, Drive je Kanal, Info-Seite „Synthesizer“, freie Phase/Feinabgleich, Modulation, externe Referenz und Kalibrierassistent vorerst nicht geplant (2026-11-10, Nutzer) |
| D19 | Keine WLAN-Einrichtung beim Flashen (kein Improv Serial, G3 entfällt); WLAN nur über das Menü am Gerät, Standard `Off` (2026-11-10, Nutzer) |
| D20 | Taste der Info-Karte heißt `Reconnect all` statt `Disconnect all`: sie trennt alle Remote-Clients, die sich danach selbst neu verbinden; keine Sperrzeit, Aussperren über Web-Passwort oder `Remote out.` (2026-11-10, Nutzer) |

**Nicht umsetzen:** neue Funktionen außer N14 vorerst (D18); RGB-LED als Ausgangsanzeige (bleibt Seitenanzeige, E-3); doppelte Statusanzeige im Header bleibt (E-13); Kanalwahl im Header (Rest von U-16, 2026-11-05: kein Platz neben Zustand und `MAIN`, über MAIN und die Karten 2 Tipps); LVGL 9, LovyanGFX (außer als Fallback), espressif32 6.13.0, Secure Boot / Flash Encryption (S-22).

**Offene Rückfragen:**

1. **Standard-Tuning:** bleibt `Fractional`, oder wird `Low noise` Standard, wenn die Messungen (MESSPLAN Stufe 2) einen klaren Vorteil zeigen? (N1)

---

## 2. Offene Befunde

`[M]` = wartet auf eine Messung (MESSPLAN-ID in Klammern).

### 2.1 SI5351, HF und IQ

- [M] **MultiSynth abgeschalteter Kanäle abschalten** (K2, F7): CH1/CH2 im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter; CLK2 bekommt `CLK2_PDN` nur bei Off state `Low`. Ist ein Ausgang mit `CLKx_PDN` hochohmig (K2), bei `High-Z` alle abgeschalteten Ausgänge mit PDN abschalten. F7 zeigt, ob es sich lohnt. → N5
- [M] **Störquellen der Plattform** (F6, F9): Backlight-PWM 5 kHz (Seitenlinien ±5 kHz möglich; Abhilfe ≥ 20 kHz oder 100 % ohne PWM), I2C-Überwachung alle 2 s (Abhilfe: Ruhemodus), CONFIG schreibt den Speicherzustand alle 300 ms neu (C-15; nur bei Änderung schreiben). → N6
- [M] **IQ-Abstimmen ohne Aussetzer** (D3, D5, K6, E9): Jede Frequenzänderung im IQ-Betrieb setzt die PLL zurück (1000 Schritte → 1000 Resets). Teiler mit Hysterese festhalten und nur die PLL ohne Reset schreiben (wie `Low noise` im Einzelbetrieb), dann bleibt die Phase erhalten; ebenso bei Korrekturänderungen. Host-Test „PLL-Änderung ohne Reset, Offset und Teiler unverändert“. → N2
- [M] **180°/270° bei ungeraden Teilern** (E4): Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°; der IQ-Teiler `min(127, ⌊900 MHz / f⌋)` ist in 54 % der Fälle ungerade (14,2 MHz → 63, 99 MHz → 9). Abhilfe: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, oder 180° über den Phasen-Offset (2 × Teiler ≤ 127). → N3
- [ ] **Spannungseinbruch über Sticky-Bit erkennen:** `checkAlive()` erkennt einen SI5351-Neustart nur, solange ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen; dasselbe Register meldet `LOL_A`/`LOL_B` → Zeile „PLL locked“ auf der Info-Karte. → N9
- [ ] **Hinweise:** SI5351 läuft beim Flashen und nach ESP32-Reset mit der letzten Einstellung weiter (A10, Handbuch); Hardware-Hinweise (eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO nach C7). VCO bei `Fractional` und +50 ppm 900,045 MHz (unkritisch, nur notieren). → S72, N7, N10

### 2.2 Bedienung

- [ ] **Netzwahl nach falschem Passwort am Gerät** (S53b, Prüfpunkt h): zwei Netze gespeichert, das stärkere mit falschem Passwort → nach dem Fehlschlag verbindet sich das Gerät mit dem anderen. Host-getestet (`WifiPolicy::chooseSlot`). → S61


### 2.3 Code und Tests

- [ ] **DMA und Doppelpuffer** (nach der Migration): zwei Puffer 320 × 40 px (`DMA_ATTR`, je 25,6 KB), `LV_COLOR_16_SWAP` = 1, `setSwapBytes(false)`; in `DisplayPanel::begin()` erst Erkennung und Init mit rohem Buszugriff, dann `initDMA()` und einmal `startWrite()` (CS bleibt LOW, `endWrite()` nie); `flush()` = `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`, `lv_disp_flush_ready()` sofort. Erwartung bis ≈ 30 Bilder/s, +36 KB RAM; Messung mit `LV_USE_PERF_MONITOR`. → N13
- [ ] **Fakes:** Bei NVS-Schemaänderungen Migrationstest mit alten Schlüsseln (mehrere Namespaces und Strings im Preferences-Fake seit S52a).

### 2.4 Plattform und Lieferkette

| ID | Komponente | heute | Ziel |
|---|---|---|---|
| P-2, P-4, S-19 | espressif32 / Arduino / IDF | 8.0.0 / 3.3.12 / 5.5.5, seit 2026-11-09 einzige Plattform (D16, E-32; DHCP-Fix CVE-2026-45160 enthalten) | Updates → N11 |
| P-5 | TFT_eSPI | 2.5.43 (letztes Release) | bleibt; mit Core 3.x im Spike unverändert lauffähig (Flush 35 ms je Vollbild wie unter 2.x); Fork oder LovyanGFX nur als Ausweg (eigene Display-Erkennung bleibt, LovyanGFX erwartet ST7789-ID 0x85, der CYD2USB meldet `8181B3`) |
| P-6, P-8 | LVGL, Unity | 8.4.0 (E-28, LVGL 9 ausgeschlossen, D13), 2.6.1 | Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6 → N11 |
| P-9 | Web-Bibliotheken | scpi-parser `8866159` (v2.3 + `strings.h`- und `-Wdouble-promotion`-Fix, seit S51, E-29) | ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, ArduinoJson 7.4.3, alle exakt gepinnt |
| S-20 | Prüfung | XPT2046 auf Commit gepinnt | Plattform-Prüfsumme in der CI, Advisory-Check je Release (W-S24) |

### 2.5 Dokumentation

- [ ] **Anwendungskapitel „Mischer-Experimente“** (beide Handbücher): Pegel je Drive-Stärke an 50 Ω (F2), Oberwellen des Rechtecks (3. ≈ −9,5 dBc, 5. ≈ −14 dBc) und Tiefpässe, LO-Pegel für Diodenringmischer (+7 dBm) bzw. Dämpfung für NE612/SA612, IQ an FST3253/74CBT3253 (Tayloe), gleich lange Kabel (1° bei 100 MHz ≈ 28 ps ≈ 5,6 mm Koax), Phasenfehler ↔ Spiegelunterdrückung (1° ≈ 41 dB, 2° ≈ 35 dB, 5° ≈ 27 dB), nie ungefiltert an eine Antenne. → N8
- [M] **„50 % Tastverhältnis“** (Handbuch §1) ist nicht belegt (E5). → S72

---

## 3. WEB-HMI (Konzept, Ziel 1.4.0)

Voraussetzungen: Migration (Phase 4) und Vorarbeiten V-1 bis V-6, V-11 (Phase 5). Aufwand ≈ 20–21 d einschließlich Migration.

### 3.1 Ziel

1. **WLAN:** Client in einem bestehenden WLAN oder eigener Access Point, über CONFIG; Standard aus.
2. **Weboberfläche:** alle Funktionen des Touch-Bildschirms im Browser (PC, Tablet, Handy), zusätzlich Tastaturbedienung.
3. **Fernsteuerung:** SCPI über TCP 5025 und USB-Seriell sowie eine dokumentierte REST-API.
4. **Sicher im Labor:** kein Mitlesen von WLAN-Passwörtern, kein Fernsteuern über fremde Webseiten, kein ungewolltes Senden; die Bedienung am Gerät hat Vorrang.

### 3.2 Entscheidungen W-E

| # | Thema | Entscheidung |
|---|---|---|
| W-E1 | Betriebsarten | `Off` (Standard) / `Client` / `Access point`. Ein Client ohne Verbindung startet nach 60 s zusätzlich den AP und sucht weiter; ein offener Rückfall-AP nur nach Bestätigung am Touch (S-3). |
| W-E2 | Zugangsdaten | am Gerät (Scan-Liste, Bildschirmtastatur) oder per Handy über den AP; Passwörter nie über einen **offenen** AP (S-1). |
| W-E3 | AP-Passwort | zufälliges WPA2-Passwort je Gerät (12 Zeichen ohne verwechselbare), beim ersten Einschalten des AP erzeugt, im NVS; am Display als Text und **QR-Code**; neu erzeugbar oder frei wählbar; offen nur auf Wunsch mit Warnung (S-2). |
| W-E4 | IP | DHCP; statische IP optional mit Plausibilitätsprüfung (S-15). |
| W-E5 | Gerätename | Standard `cyd-hf-<4 Stellen MAC>`, nur RFC 1123 (a–z, 0–9, `-`, ≤ 32); gilt für mDNS, DHCP-Hostnamen, AP-SSID `CYD-HF-<4 Stellen MAC>` (S-26). |
| W-E6 | Captive-DNS | nur im AP-Modus; kein WPA2-Enterprise. |
| W-E7 | Gespeicherte WLANs | höchstens zwei, das stärkste gewinnt; Mindestsicherheit WPA2, PMF „capable“; Warnung bei offenem Netz (S-28). |
| W-E8 | Zugriffsschutz | Web-Passwort optional; dann Login mit Sitzungs-Cookie (kein Basic-Auth), gesalzen gehasht, Sperre nach Fehlversuchen (S-9). |
| W-E9 | Umfang Web | alle Funktionen inkl. CONFIG und WLAN außer W-E25; Netzänderungen zweistufig (Bestätigung über die neue Verbindung binnen 120 s, sonst Rückfall; ohne Web-Passwort zusätzlich am Touch, S-15). |
| W-E10 | Web-Clients | höchstens 3, live synchron; ein vierter bekommt eine Fehlerseite. |
| W-E11 | Sprache | Englisch, responsiv, nur lokale Ressourcen. |
| W-E12 | Protokoll | SCPI (IEEE 488.2 / SCPI-99, eigener Baum) über TCP 5025 und USB; mDNS `_scpi-raw._tcp`; kein VXI-11, HiSLIP, MQTT. |
| W-E13 | SCPI-Verbindungen | eine; eine zweite wird abgelehnt; Leerlauf-Timeout 10 min; `Disconnect` am Gerät (S-17). |
| W-E14 | Gleichzeitige Bedienung | uneingeschränkt, „der Letzte gewinnt“; Statusleiste `REMOTE <Quelle>` (S-18). |
| W-E15 | Ereignisse | Polling und `*OPC?`; Live-Push nur per WebSocket. |
| W-E16 | Verbindungsabbruch | Ausgänge laufen wie eingestellt weiter. |
| W-E17 | Speichern | Touch/Web nach 5 s, SCPI nach 30 s Ruhe; `SYSTem:SAVE` sofort; nur geänderte Schlüssel; Ein/Aus ohne Speichern. |
| W-E18 | Grenzen | wie am Touch; jeder Wert im vollen Typ geprüft, nie still abgeschnitten (S-12). |
| W-E19 | `*RST` | Ausgänge aus, Standardwerte im RAM, nicht gespeichert bis `SYSTem:SAVE`; eine spätere Änderung speichert nur ihren Schlüssel (C-17, D7). |
| W-E20 | HF-Reinheit | WLAN abschaltbar, Standard aus. |
| W-E21 | Ohne SI5351 | Web und SCPI laufen, Kanäle `NO HW`. |
| W-E22 | OTA, NTP | nein (OTA später als Option). |
| W-E23 | Doku | SCPI-Referenz, REST-API, Python-Beispiel, Verweise auf PyVISA, LabVIEW, NI-MAX, MATLAB, Sicherheitshinweise. |
| W-E24 | Plattform | vorher Migration auf `espressif32@8.0.0`; Heap laufend überwacht. |
| W-E25 | Fernschalten | CONFIG `Remote outputs: allowed / local only` (Standard `allowed`, nur am Gerät änderbar); `local only`: Netz darf alles einstellen, aber nicht einschalten (S-18). |
| W-E26 | Statusleiste | in 1.3.2 umgesetzt; WLAN-, AP-, `REMOTE`- und später `LOCK`-Symbole kommen in S53 dazu (Form statt nur Farbe, U-19). |
| W-E27 | SCPI über USB | während einer SCPI-USB-Sitzung kein Log auf `Serial` (Ringpuffer, `SYSTem:LOG?`) (C-16). |
| W-E28 | Identify | Web-Taste und `SYSTem:IDENtify`: Backlight und LED blinken 5 s. |

### 3.3 Architektur

```
 Touch/LVGL ───────────── Ausführer (synchron) ──┐
 Web-UI ─┐                                       │
 REST ───┼─► RequestGuard ─► CommandQueue ───────┼─► AppController ─► SI5351 / NVS
 SCPI TCP 5025 ─► LineFramer ─► scpi ─┘          │
 SCPI USB-Seriell ─► LineFramer ─► scpi ─┘       │
 Abfragen (Web, REST, SCPI) ◄── StateSnapshot ◄──┘  (Sequenzzähler, revision())
```

| Modul | Aufgabe |
|---|---|
| `net/wifi_manager.*`, `net/wifi_policy.*` | Betriebsarten, eigene asynchrone Netzwahl statt `WiFiMulti` (dessen `run()` blockiert), Rückfall-AP, statische IP, Gerätename, mDNS, Captive-DNS (nur AP), AP aus beim STA-Connect |
| `net/net_settings.*` | NVS `hfnet`: Modus, 2 × SSID/PSK, AP-SSID/-Passwort, Gerätename, statische IP, Web-Passwort-Hash (`Remote outputs` liegt seit S50 als `rem_out` in `hfgen`, weil der Befehlskern ihn vor dem WLAN braucht); WLAN-Schlüssel als Passphrase (D15), `WiFi.persistent(false)` (S-8) |
| `net/request_guard.*` | rein, host-testbar: Host-Allowlist, Origin/Sec-Fetch-Site, Content-Type, Token, Sitzung, Rate-Limits, Größen |
| `net/sanitize.*` | fremde Zeichenketten → gültiges UTF-8 ohne Steuerzeichen, ≤ 32 B, für LVGL und JSON (S-7, S-14) |
| `remote/command.*`, `remote/state_snapshot.*` | Befehle mit Quelle (Touch/Web/SCPI/USB) und `Result`; Zustandskopie für Abfragen und Push |
| `remote/line_framer.*` | rein: Zeilen ≤ 256 B (Überlänge bis `\n` verwerfen, −363), ≤ 16 Befehle je Zeile, NUL/Nicht-ASCII verwirft, HTTP-Erkennung auf TCP (S-6, S-11) |
| `remote/scpi.*`, `remote/scpi_server.*` | Befehlsbaum auf j123b567/scpi-parser v2.3 (Commit gepinnt, Beispielcode nicht übernehmen, Issue #174); TCP 5025 (eine Verbindung, Timeout, Keepalive) und USB |
| `web/web_server.*`, `data/` | ESPAsyncWebServer 3.12.1 + AsyncTCP 3.5.0: LittleFS (gzip, ≈ 30–60 KB, eine Seite ohne Framework, JS nur als Datei), REST, WebSocket `/ws`, Sicherheits-Header; nie im `async_tcp`-Task blockieren |
| JSON | ArduinoJson 7.4.3, Verschachtelung ≤ 10 |
| GUI | Statusleisten-Symbole, CONFIG `WiFi` und `Remote outputs`, Seiten WIFI (Unterseiten, kein Scrollen), Tastatur, QR (`lv_qrcode`), Info-Karte |

**Regeln:**
- Kein Netz-Task ruft `AppController`, LVGL, I2C oder NVS direkt; Schreiben über die Queue (16 Einträge), Lesen über den Schnappschuss (Sequenzzähler, weil uint64/double nicht atomar sind).
- SCPI wartet auf Quittungen höchstens 1 s; HTTP-Handler antworten asynchron.
- Touch, Web und SCPI sind gleichberechtigt; jede Änderung erscheint sofort überall.
- Fremde Zeichenketten erreichen LVGL und JSON nur über `sanitize`; Passwörter werden nie ausgegeben (nur „gesetzt / nicht gesetzt“).
- WLAN aus: kein Netz-Task, `WIFI_OFF`, Verhalten wie heute; SCPI über USB bleibt.
- Kein Routing zwischen AP und STA: NAPT aus, AP aus im Event der STA-IP, DHCP ohne Gateway (S-16).
- Sicherheitslogik (`request_guard`, `sanitize`, `line_framer`, SCPI-Kern, JSON-Abbildung) frei von Arduino-/Webserver-Abhängigkeiten, im `native`-Env getestet und gefuzzt.

### 3.4 WLAN am Gerät

- **Statusleiste:** links WLAN-Signal bzw. `AP`, `REMOTE <Quelle>`, Speicherzustand, später `LOCK`; rechts `CONFIG`.
- **CONFIG:** `WiFi` (`Off` / `Client` / `Access point`, Taste `Setup…`), `Remote outputs` (`allowed` / `local only`).
- **Seite WIFI:** *Networks* (zwei Einträge; `Scan` mit Spinner, ≤ 15 Netze bereinigt und ohne Doppelte; „Other…“ für versteckte SSIDs); *Password* (Vollbildseite, Tastatur ≈ 190 px, „anzeigen“, Zeichenzähler; ä, ö, ü, ß, € nur per Handy); *Access point* (Name, Passwort mit `Regenerate`, `Set…`, `Open (not recommended)`, großer QR „Join“, darunter URL als Text und QR); *Device* (Gerätename, IP, Web-Passwort).
- **Rückmeldungen:** `Connecting…`, `Connected 192.168.1.50`, `Wrong password`, `Network not found`, `Fallback: AP active`.
- **Info-Karte:** WLAN-Modus, SSID, RSSI, IP, `<name>.local`, SCPI-Status, Web-Clients, Heap, LVGL-Heap; Taste `Reconnect all` (S-10, S-17).
- **Verhalten:** Client mit `WiFiMulti` (WPA2), nach 60 s ohne Verbindung AP (WPA2), Suche läuft weiter, AP aus sobald verbunden. Offener AP (nur auf Wunsch): `AP open` in Warnfarbe, aus nach 15 min ohne Station, ≤ 2 Stationen, Passwortfelder 403, SCPI über TCP aus, Rückfall nur nach Bestätigung.
- **Heap:** Warnung bei minimalem freiem Heap < 40 KB oder größtem Block < 16 KB; unter 24 KB neue Verbindungen ablehnen (503, SCPI schließen); `SYSTem:HEAP?`; 24-h-Dauertest.

### 3.5 SCPI-Befehlsbaum

Kurzform groß; `n` = Kanal 1–3 (Standard 1). Zahlen: Ganzzahl in Hz oder Dezimalzahl mit `HZ`/`KHZ`/`MHZ`, `MIN`/`MAX`/`DEF`; `NAN`, `INF`, Exponenten außerhalb des Bereichs, Hex → −222/−224. Antworten in Grundeinheiten.

| Befehl | Abfrage | Bedeutung |
|---|---|---|
| `*IDN?` | `CYD,HF-Generator,<MAC>,1.4.0` | Identifikation |
| `*RST` | – | W-E19 |
| `*CLS`, `*ESR?`, `*ESE`, `*STB?`, `*SRE`, `*OPC`, `*OPC?`, `*WAI`, `*TST?` | | IEEE 488.2 |
| `OUTPut<n>[:STATe] ON\|OFF\|1\|0` | `OUTPut<n>?` | Ausgang (CH3 = Anforderung); `local only` → −221 für ON |
| `OUTPut<n>:CONDition?` | `ON`/`OFF`/`ERR`/`WAIT`/`NOHW` | tatsächlicher Zustand (nicht `:STATus?`: Kurzform `STAT` wäre mehrdeutig mit `STATe`, E-29) |
| `[SOURce<n>:]FREQuency[:CW] <f>` | `FREQ?` | Sollfrequenz CH1/CH2; CH3 nur Abfrage |
| `[SOURce<n>:]FREQuency:ACTual?` | Hz mit 0,1 Hz | wie die Zeile `Out` (bei Spread Mittelwert) |
| `SOURce3:IQ:MASTer 1\|2`, `SOURce3:PHASe 0\|90\|180\|270`, `SOURce3:IQ:RANGe?` | `?` | CH3 |
| `OUTPut:DRIVe 2\|4\|6\|8`, `OUTPut:OFFState LOW\|HIGH\|HIZ` | `?` | Ausgänge |
| `OUTPut:REMote?` | `ALLOWED`/`LOCAL` | W-E25 (nur lesbar) |
| `[SOURce1:]SSC OFF\|DOWN05\|DOWN15\|CENT05`, `SYSTem:TUNing FRAC\|LNO\|INT` | `?` | Signal |
| `SYSTem:CORRection <ppm>`, `SYSTem:STEP <Hz>`, `SYSTem:BACKlight <%>` | `?` | Einstellungen |
| `SYSTem:SI5351?`, `SYSTem:HEAP?`, `SYSTem:IDENtify`, `SYSTem:LOG?`, `SYSTem:SAVE` | | Diagnose (`CONN`/`NFO`/`BLOC`/`UNKN`; frei, Minimum, größter Block), Identify, Log (eine Zeile je Abfrage, `""` wenn leer), Speichern |
| `SYSTem:ERRor[:NEXT]?`, `SYSTem:VERSion?` | | Fehlerwarteschlange, `1999.0` |

Netzwerkeinstellungen und `Remote outputs` sind über SCPI nicht änderbar.

```python
import pyvisa
rm = pyvisa.ResourceManager()
gen = rm.open_resource("TCPIP0::cyd-hf-1a2b.local::5025::SOCKET",
                       read_termination="\n", write_termination="\n")
print(gen.query("*IDN?"))
gen.write("FREQ 7.1 MHZ")
gen.write("OUTP1 ON")
print(gen.query("FREQ:ACT?"))
print(gen.query("SYST:ERR?"))
```

Über USB `ASRL5::INSTR` mit DTR und RTS aus, damit das Öffnen des Ports nicht neu startet (MESSPLAN L6).

### 3.6 Weboberfläche und REST

- **Bereiche:** MAIN, CH1, CH2, CH3, CONFIG, WiFi; auf dem Handy untereinander mit fixierter Ausgangsleiste; Tasten ≥ 44 CSS-px, `touch-action: manipulation`.
- **Frequenzeingabe:** `10.7M`, `455k`, `7 100 000`, `10,7 MHz`; bei Fehler bleibt die Eingabe mit Fehlertext stehen.
- **Live:** geänderte Felder kurz hervorheben, Feld mit Fokus nie überschreiben, bei Verbindungsabbruch grau; Sondermodi sichtbar wie am Gerät (Moduszeile).
- **Tastatur:** `↑`/`↓` (auch Ziffernblock `+`/`−`) Frequenz im Schritt; `←`/`→` Schrittweite (`←` = größer); `Bild↑`/`Bild↓` × 10 / ÷ 10; Mausrad über einer Ziffer; `1`/`2`/`3` Kanal (nur ohne Fokus); `F` oder `Enter` fokussiert, `Enter` übernimmt, `Esc` verwirft; `O` Ausgang ein/aus (nicht `Space`); `?` Belegung.
- **Netzänderungen:** Countdown „Confirm within 120 s, otherwise the previous settings return“; `Identify`-Taste; Login nur mit Web-Passwort; vierter Client „Too many clients (max. 3)“.

| Methode und Pfad | Inhalt |
|---|---|
| `GET /api/state` | Zustand als JSON; ohne Passwort nur die verbundene SSID; MAC und Commit erst nach Login, falls ein Passwort gesetzt ist (S-26) |
| `POST /api/channel/<n>` | `{"enabled":true,"frequency":10000000}` |
| `POST /api/ch3` | `{"master":1,"phase":90}` |
| `POST /api/config` | `{"drive":8,"tuning":"lownoise","spread":"off","offState":"low","correction":0,"step":1000,"backlight":80}` |
| `GET/POST /api/wifi`, `POST /api/wifi/confirm` | Netzwerk (Passwörter nur schreibend, über offenen AP gesperrt), zweistufige Bestätigung |
| `GET /api/wifi/scan` | höchstens alle 10 s |
| `POST /api/save`, `POST /api/identify`, `POST /api/login` | |
| WebSocket `/ws?t=<token>` | nur Push |

Fehler: HTTP 400 mit `{"error":"-222,Data out of range"}` (dieselben Codes wie SCPI); unbekannte Schlüssel und falsche Typen (`{"drive":"4"}`, `{"drive":4.5}`) → 400. Token je Boot im HTML-Kopf, REST-POST verlangt `X-CYD-Token`; für Skripte auf der Info-Karte und unter `GET /api/token` (nur same-origin).

**Offen für S55 (aus S56a, Weboberfläche gegen den Mock):** 1) `wifi.confirmWithin` (Sekunden, 0 = nichts offen) im Zustand, damit eine unter der neuen Adresse geöffnete Seite bestätigen kann; 2) Verhalten bei Betriebsart `Off` über das Web (keine Bestätigung über die neue Verbindung möglich); 3) Zustand zeigt, ob ein Web-Passwort gesetzt ist, und ob es auch über das Web gesetzt werden kann; 4) fester Fehlertext bei veraltetem Token nach Neustart (z. B. `-200,Invalid token: reload the page`); 5) festlegen: ungültiges JSON → −104, Master/Phase/Drive/Schritt außerhalb der Wertelisten → −224, Bedeutung von `modes` (nur CONFIG-Einträge) und `actual` (bei Spread der Mittelwert); 6) Scan: `GET` startet, das erste `GET` nach dem Ende liefert `done`, ein neuer Start innerhalb von 10 s → 429; 7) `/api/state` und `/ws` ohne Login lesbar, auch wenn ein Passwort gesetzt ist (nur MAC/Commit verborgen)? Aus S55a: 8) WebSocket-Grenze ≤ 512 B (W-S6) nur für eingehende Frames, das Zustand-JSON hat ≈ 750 B (Puffer 1 KB); 9) `frequencyText` außerhalb des Bereichs ergibt −224 statt −222 (dafür bräuchte `parseRemoteFrequency` ein Bereichs-Flag); 10) ohne Verbindung `wifi.ssid`/`ip` = `""`, `rssi` = 0, nur mit AP `ip` = AP-Adresse; 11) Fehlertexte kurz (`-221,Remote outputs: local only`, `-200,Too many commands`, `-240,SI5351 setup failed`), Status 403/429/503/500/413. Am Web nicht vorgesehen: Grundzustand/Werksreset, Touch-Abgleich, Bildschirmsperre, AP-Passwort neu erzeugen, Web-Passwort setzen (Hinweis „am Gerät“).

### 3.7 Sicherheitsanforderungen W-S

| Nr | Anforderung (Audit-Befund) |
|---|---|
| W-S1 | Über einen offenen AP lehnt der Server WLAN- und Web-Passwortfelder ab (403); Standard ist der WPA2-AP mit Zufallspasswort. (S-1, S-2) |
| W-S2 | Offener Rückfall-AP nur nach Bestätigung am Touch; offener AP ohne Station nach 15 min aus, ≤ 2 Stationen, Statusleiste `AP open`. (S-2, S-3) |
| W-S3 | Kein Routing AP ↔ STA: NAPT aus, DHCP ohne Gateway, AP und STA nie gleichzeitig mit Adresse (AP aus im Event der STA-IP, ≤ 1 s), weil das vorkompilierte IDF `IP_FORWARD` fest eingeschaltet hat (D14). (S-16) |
| W-S4 | Host-Allowlist für HTTP und WebSocket (eigene IPv4, `<name>.local`, `<name>`, im AP fürs Captive-DNS), sonst 421; IPv6 aus. (S-5) |
| W-S5 | POST nur mit `application/json` (415), gültigem `X-CYD-Token` (403), `Origin` leer oder eigener Host, `Sec-Fetch-Site` nicht `cross-site`; keine CORS-Header, `OPTIONS` abgelehnt, GET ohne Wirkung. (S-4) |
| W-S6 | WebSocket-Upgrade mit Host-, Origin- und Token-Prüfung; nur Push; ≤ 512 B; Ping alle 15 s, Trennen nach 30 s ohne Pong. (S-10) |
| W-S7 | Header: CSP `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`, `nosniff`, `Referrer-Policy: no-referrer`; `/api/*` mit `no-store`. (S-7) |
| W-S8 | Fremde Zeichenketten im Browser nur über `textContent`/DOM, nie `innerHTML`; JSON nur über ArduinoJson. (S-7) |
| W-S9 | Vor LVGL und JSON läuft jede fremde Zeichenkette durch `sanitize`; Scan-Liste ≤ 15, kein Recolor; LVGL-Assert → Neustart. (S-14, C-3) |
| W-S10 | Body ≤ 1 KB (413), Multipart/Chunked 415, URL ≤ 256 B, Header ≤ 2 KB, 10 s Timeout; ≤ 3 WebSocket- und ≤ 4 HTTP-Verbindungen; < 24 KB Heap → 503. (S-13) |
| W-S11 | ≤ 20 Schreibbefehle/s je Quelle (429), Scan ≤ alle 10 s, nach 5 Login-Fehlern 30 s Sperre (verdoppelnd). (S-9, S-13) |
| W-S12 | Netzänderungen zweistufig mit Rückfall nach 120 s, ohne Web-Passwort zusätzlich am Touch; statische IP ≠ Gateway, im Subnetz, keine Netz-/Broadcast-Adresse. (S-15) |
| W-S13 | Web-Passwort ≥ 8 Zeichen, PBKDF2-Hash (mbedTLS); Cookie `HttpOnly; SameSite=Strict`, 12 h; kaputte `Authorization`/`Cookie` abgelehnt. (S-9) |
| W-S14 | WLAN-Schlüssel als Passphrase (D15: WPA3-SAE braucht sie; vorher PMK), `WiFi.persistent(false)`; Werksreset löscht `hfnet`, `nvs.net80211`, Coredump; Passwörter nie in Ausgaben. (S-8) |
| W-S15 | STA mit PMF „capable“ und mindestens WPA2; AP als WPA2/WPA3-SAE-Übergang, falls stabil (sonst WPA2). (S-28) |
| W-S16 | Gerätename RFC-1123, AP-SSID druckbares ASCII ≤ 32 B; mDNS-TXT ohne Version und Commit. (S-26) |
| W-S17 | SCPI-Zeilen ≤ 256 B, Überlänge bis `\n` verwerfen (−363), ≤ 16 Befehle je Zeile, NUL/Nicht-ASCII verwirft. (S-11) |
| W-S18 | SCPI über TCP schließt ohne Ausführung bei HTTP-artigen Zeilen (`^[A-Z]+ \S+ HTTP/1\.[01]`, `^[A-Za-z0-9-]+:\s`). (S-6) |
| W-S19 | Parameter im vollen Typ prüfen (endlich, ganzzahlig, im Bereich), erst dann verengen; Frequenzen ganzzahlig in Hz. (S-12, C-4) |
| W-S20 | SCPI: Leerlauf 10 min, TCP-Keepalive, IP auf der Info-Karte, `Disconnect` am Gerät; auf offenem AP aus. (S-17) |
| W-S21 | `Remote outputs: local only`: Einschalten aus Web/REST/SCPI abgelehnt (403 / −221), Ausschalten erlaubt, nur am Touch änderbar; Statusleiste `REMOTE <Quelle>`. (S-18) |
| W-S22 | Während SCPI-USB keine Logzeilen auf `Serial`. (C-16, S-25) |
| W-S23 | `loop()` vom Task-Watchdog überwacht (seit 1.3.1); kein Netz-Task blockiert > 100 ms; Stack-Reserve ≥ 1 KB nach 24 h. (S-23) |
| W-S24 | Abhängigkeiten exakt gepinnt, Advisory-Check je Release; CI mit SHA-gepinnten Actions und `permissions: contents: read`. (S-20, S-27) |
| W-S25 | Fuzzing von `line_framer`, SCPI-Kern, `request_guard`, `sanitize`, JSON-Abbildung: libFuzzer mit ASan/UBSan ≥ 10 Mio. Ausführungen vor jedem Release; Zufallstest mit festem Seed (100 000 Eingaben) in `native-win`. (S-11, S-13) |

Bewusst nicht: TLS/HTTPS (kein Zertifikat verwaltbar), Secure Boot und Flash Encryption (S-22). Das Handbuch nennt die Folgen (Klartext, Gast-/IoT-WLAN empfohlen, physischer Zugriff = Vollzugriff).

### 3.8 Tests und Risiken

**Neue Host-Suites:** `test_command` (Befehle, `Result`, Quelle und Speicherfristen, Dirty je Schlüssel, `*RST`, W-E25, Rate-Limit, volle Queue), `test_scpi` (Kurz-/Langform, Einheiten, MIN/MAX/DEF, NAN/INF/Hex, Grenzen, Fehlerwarteschlange, IEEE 488.2), `test_line_framer` (Überlänge, `;`-Fluten, NUL, HTTP-Erkennung), `test_http_guard`, `test_sanitize` (`"\xF0A"`, `"\xC3"`, `\n`, `\0`, 32 B), `test_rest_json` (Escaping, Ablehnung unbekannter Schlüssel und falscher Typen), `test_net_settings` (`hfnet`, PMK, IP-Plausibilität, Export ohne Passwörter); Fuzzing als CI-Job. Hardware: MESSPLAN L und L-S.

| Risiko | Gegenmaßnahme |
|---|---|
| TFT_eSPI mit Core 3.x | Spike S37; gepinnter Fork; LovyanGFX nur als letzter Ausweg |
| Wire im neuen IDF-Treiber anders | `I2CBus` gebündelt (S39), Annahmen als Fake-Tests, MESSPLAN J |
| WLAN-Nebenlinien | Vorab-Messung S49; WLAN abschaltbar, Standard aus |
| Heap-Fragmentierung | W-S10, Heap-Schutz, 24-h-Dauertest |
| Nebenläufigkeit | Regeln 3.3, Host-Tests, Review auf direkte Aufrufe aus Netz-Tasks |
| Angriffe über LAN und fremde Webseiten | W-S4 bis W-S8, W-S18 |
| Unbeabsichtigtes Senden | W-E25, `REMOTE` in der Statusleiste, Handbuch: nie ungefiltert an eine Antenne |
| Flash-Verschleiß | W-E17 (30 s, nur geänderte Schlüssel) |

Spätere Optionen: OTA (neue Partitionstabelle), VXI-11/LXI, HTTPS.

---

## 4. Aufgabenliste

Je Schritt: Inhalt, Prüfung, Commit-Betreff. `[ ]` offen, `[x]` erledigt (wird beim nächsten Aufräumen entfernt; Zusammenfassung in 1.1, Details in der Git-Historie).

### Phase 4 – Migration Core 3.x (Rest: Abnahmemessungen)

- [ ] **S41c · Abnahme Core 3 (Messungen).** Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10, ≈ 4 h, braucht ZÄHLER und OSZI), dabei CLK0 bei Q4/Q5 und J6 (Code 5 bei Leitung auf GND). Bei einem Befund ist der Rückweg Tag `v1.3.3` (Core 2). *Commit:* `S41c: Record the core 3 acceptance measurements`

### Phase 6 – WEB-HMI 1.4.0 (Rest ≈ 2,5 d + Laufzeit, `feature/web-hmi`)

- [ ] **S49 · Vorab-Messung WLAN (0,5 d, braucht SPEKTRUM).** Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis in Abschnitt 3. *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **S57 · Fuzzing und Dauertest (1 d + Laufzeit).** Rest: manueller CI-Lauf mit ≥ 10 Mio. je Ziel (Workflow „Fuzz“ liegt jetzt auf `main` und ist dort manuell mit `runs` startbar), 24-h-Lauf mit Heap- und Stack-Bilanz (Weboberfläche offen, Debug-Log alle 60 s mit Stack-Reserve von `async_tcp` und `web_login`; ggf. Stack von `async_tcp` senken, aus S55b). libFuzzer-CI-Job (≥ 10 Mio.), Zufallstest mit festem Seed; 24-h-Lauf mit Heap- und Stack-Bilanz. *Commit:* `S57: Add fuzzing job and long-run test`
- [ ] **S58 · Doku und Release-Kandidat (1,5 d).** Handbuch DE/EN (WLAN, Web, SCPI-Referenz, REST, Python-Beispiel, Sicherheitshinweise, Fernbetrieb und Antenne), Spezifikation, README, `CLAUDE.md`, diese Datei; Version 1.4.0, lokaler Tag `v1.4.0-rc1`; Smoke-Test einschließlich L1 und `*IDN?`. *Commit:* `S58: Document WEB-HMI, release candidate 1.4.0-rc1` Aus S56: Weboberfläche in Safari und Firefox prüfen.

### Phase 7 – GitHub und Verteilung (Rest ≈ 1 d, je ein Commit; `[Nutzer]` = Einstellung im Repo, nur durch den Nutzer)

Stand 2026-11-09: G1, G4, G6–G9a erledigt (Abschnitt 1.1), Workflows auf `main`; Pages (Quelle GitHub Actions) mit Umgebung `github-pages` (Regeln `main`, `v*`) sowie Secret Scanning und Push-Schutz sind eingerichtet.

- [ ] **G2 · Webflasher am Gerät (Workflows liegen seit 2026-11-09 auf `main`).** Manuell „Release“ auf `main` mit `deploy` starten (Dev-Build wird öffentlich) oder erstes Release-Tag; beide Boards über https://dm8sb.github.io/CYD-HF-Generator/ flashen; Update ohne Erase behält Touch-Abgleich, Korrektur und WLAN; Erstinstallation mit Erase startet mit dem Touch-Abgleich. Dann Handbuch DE/EN neuer Abschnitt „16. Firmware aktualisieren“ (Browser Chrome/Edge am Desktop, CH340-Treiber, `Connect` und Port, Erase nur bei der Erstinstallation, Update schreibt nur 0x1000/0x8000/0xE000/0x10000, BOOT halten bei Verbindungsproblemen, `SHA256SUMS` und `gh attestation verify`, esptool-Befehl) und README-Link aktivieren. Offen: rc-Tags veröffentlichen die Seite nicht (Absicht, eine Bedingung in `meta`); Firefox-Unterstützung von Web Serial prüfen. *Commit:* `G2: Add web flasher on GitHub Pages` Der Release-Job (Upload, `attest-build-provenance`) läuft erstmals mit dem ersten Tag.
- [x] **G11 · Lizenz-Hinweise vor dem ersten Release (2026-11-10, D17, A-77, E-36).** `THIRD_PARTY_NOTICES.md` (alle Komponenten laut Linker-Map, dazu ESP Web Tools mit gebündelten Paketen und den Flasher-Stubs, Gehäuse `case/` unter der Repo-Lizenz) und `LICENSES/`; `tools/flasher_site.py notices` sammelt Lizenztexte und LGPL-Quellen, `release.yml` hängt `…-licenses.tar.gz` und `…-lgpl-sources.tar.gz` ans Release; `vendor` lädt ESP Web Tools 10.4.0 mit Lizenzdateien und Stub-Quellen gegen die Hashes in `flasher/esp-web-tools.json`, die Seite liefert alles selbst aus (CSP `script-src 'self'`, keine Anfrage an Dritte), nennt Sebastian Brandes (DM8SB), Hobbyprojekt ohne Gewähr, Datenschutzhinweis; Fußzeile mit Lizenzzeile in der Weboberfläche; README „Libraries“/„License“, Herkunftskommentar in `display_detect`, Handbuch „Lizenzen“ und Funkhinweis in Abschnitt 14; Commit-IDs nach dem Umschreiben der Historie angepasst. Offen: ungenutzte TFT_eSPI-Fonts aus den Build-Flags (optional). Deutscher Haftungssatz auf Wunsch des Nutzers: „Jegliche Haftung ist ausgeschlossen, soweit gesetzlich zulässig.“ Bedientest der Fußzeile am Gerät steht aus (Nutzer 2026-11-10: ohne Prüfung übernommen) → beim nächsten Bedientest nachholen. *Commit:* `G11: Add third-party notices and license texts to releases`
- [ ] **G5 · Einstellungen im Repo.** `[Nutzer]` Branch-Ruleset für `main`: CI, Fuzz und CodeQL grün, kein Force-Push, kein Löschen, Änderungen nur per Pull Request (erst wenn die Workflows auf `main` liegen); `[Nutzer]` private Vulnerability Reports einschalten (aus G4); CodeQL-Default-Setup aus lassen, sonst werden die Uploads von `codeql.yml` abgelehnt; optional Renovate (Mend-App) für `platformio.ini` (Vorschlag aus G6). Kein Commit nötig; Ergebnis in Abschnitt 1.3 notieren.
- [ ] **G9 · Projektseite (Rest).** `[Nutzer]` Beschreibung, Website `https://dm8sb.github.io/CYD-HF-Generator/`, Topics, Vorschaubild 1280 × 640; CN1-Steckerraster (1,25 mm) und Pinfolge am Board bestätigen; Handbuch als Pages-Seite (nach G2). *Commit:* `G9b: Add manual page`
- [ ] **G12 · Gehäuse am Gerät prüfen (`[Nutzer]`).** `case/CYD_Si5351_Gehaeuse.scad` (vom Nutzer, 2026-11-10; rendert fehlerfrei, außen 90,6 × 57 × 32,3 mm): Si5351-Modul nachmessen (`si_l`, `si_w`, `si_sma`, `sma_axis_off`), drucken, Passung von Display, USB-C, SMA, Rasthaken prüfen; danach Foto in G10 und ggf. STL-Export als Release-Asset. *Commit:* `G12: Adjust the case after the test print`
- [ ] **G10 · Fotos (später, `[Nutzer]`).** Nach der Liste in `docs/hardware.md`: `board-cyd-front.jpg` und `board-cyd2usb-front.jpg` (MAIN-Seite, CH1 an), `si5351-wiring.jpg` (Modul mit den 4 Leitungen zu CN1 und CLK0–2), `cn1-connector.jpg` (Rückseite, Beschriftung lesbar), `setup-bench.jpg` (Aufbau mit Koax zum Messgerät), je ≤ 500 KB, kein WLAN-Name auf dem Display; ablegen unter `docs/img/`. Danach Platzhalter in `docs/hardware.md` aktivieren. Vorschaubild `social-preview.png` 1280 × 640 (< 1 MB) lädt der Nutzer unter Settings → General → Social preview hoch. Zwischenstand (G13): gerenderte Bilder in `docs/img/` stehen bis dahin in README, `docs/hardware.md`, `case/README.md` und Handbuch Anhang A, darunter eine gerenderte `social-preview.png` (liegt jetzt doch im Repo; durch eine Version mit Foto ersetzbar). *Commit:* `G10: Add hardware photos`

- [x] **G13 · Gerenderte Bilder (2026-11-10, Zwischenstand bis G10).** Gerätebilder aus dem echten GUI-Code auf dem PC (Env `screens-win`/`screens`: `src/gui` mit LVGL 8.4 auf den Fakes, `tools/screens/` ersetzt Display, Touch, WLAN, LED, Build-Info; `lv_conf_host.h` = `include/lv_conf.h` mit Host-Tick), Weboberfläche auf `tools/web_mock.py` (Chrome/Edge über DevTools, `tools/cdp_shot.mjs`), Gehäuseansichten aus OpenSCAD mit der MAIN-Seite im Display, Vorschaubild 1280 × 640; alles mit `tools/render_images.py`, nur Beispielwerte (Netze `Lab`/`Workshop`, Beispielpasswörter, lokal verwaltete MAC). Eingebunden in README (Titelbild, Galerie, Abschnitt „Documentation images“), `docs/hardware.md`, `case/README.md`, Handbuch DE/EN Anhang A (EN-Titel jetzt „Page Images“). Nicht Teil der Firmware und des Test-Gates. *Commit:* `Add rendered images of the screens, the web UI and the case`

### Phase 8 – Messungen ([MESSPLAN.md](MESSPLAN.md))

Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` (ohne Medien); Befunde (M-n) sofort in Abschnitt 2 eintragen. Stufe 1 und 2 laufen **sobald Zähler und Oszilloskop verfügbar sind**; dieser Lauf ist die Referenz R0 (MESSPLAN 9.2).

- [ ] **S59 · Stufe 1 (Pflicht, ≈ 7 h).** MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10). *Commit:* `S59: Record tier-1 measurements`
- [ ] **S60 · Stufe 2 (Entscheidungen, ≈ 6 h).** MESSPLAN Abschnitt 6: Standard-Tuning (K5, F3, K6), IQ ohne Reset (D3, D5, E9), ungerade Teiler und Phasenfehler (E3, E4, E5), Störquellen und PDN (F6, F7), Pegel (F2), Drift (C7); Entscheidungen als E-n in die Spezifikation, N1–N8 bestätigen oder streichen. *Commit:* `S60: Record tier-2 measurements and decisions`
- [ ] **S61 · Stufe 1W (Pflicht ab WEB-HMI, ≈ 8 h + 24 h).** MESSPLAN 5.11 nach S58 auf beiden Boards; dazu L13, L15, Q1 aus Stufe 2; aus S54: Abbau einer verwaisten TCP-Verbindung nach ≈ 90 s (Keepalive), Sperre bei offenem AP, mDNS `_scpi-raw._tcp`. *Commit:* `S61: Record network and security measurements`
- [ ] **S62 · Stufe 3 (optional).** Charakterisierung und Bedientests mit Personen nach Bedarf (MESSPLAN Abschnitt 7). *Commit:* `S62: Record optional characterization measurements`
- [ ] **S63 · Handbuchwerte.** Pegeltabelle (F2), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise; DE und EN. *Commit:* `S63: Add measured values to the manuals`
- [ ] **S64 · Freigabe 1.4.0.** Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag `v1.4.0`; Merge nach `dev` und `main` nur auf Anweisung. *Commit:* `S64: Release 1.4.0`

### Phase 9 – Folgearbeiten (je ein Commit, nach Messergebnis oder Rückfrage)

- [ ] **N1 · Standard-Tuning** ändern, wenn die Stufe-2-Messungen es tragen (Rückfrage 2, E-19).
- [ ] **N2 · IQ-Abstimmen ohne Aussetzer** (Abschnitt 2.1).
- [ ] **N3 · Ungerade IQ-Teiler bei 180°/270°**, wenn E4 einen Fehler > 1° zeigt.
- [ ] **N5 · MultiSynth abgeschalteter Kanäle abschalten, PDN-Strategie** nach K2 und F7.
- [ ] **N6 · Störquellen-Abhilfen** (Backlight-PWM ≥ 20 kHz, Ruhemodus der Überwachung, Neuzeichnen nur bei Änderung) nach F6, F9.
- [ ] **N7 · TCXO-Option** nach C7, C8.
- [ ] **N8 · Anwendungskapitel „Mischer-Experimente“** (Abschnitt 2.5) nach F2, G1, G2, E10.
- [ ] **N9 · Sticky-Bit und „PLL locked“** (Abschnitt 2.1).
- [ ] **N10 · Hardware-Hinweise** in den Handbüchern.
- [ ] **N11 · Spätere Updates:** Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6; Advisory-Check je Release.
- [ ] **N13 · DMA und Doppelpuffer** (Abschnitt 2.3, nach der Migration).
- [ ] **N14 · Gekoppelter Betrieb CH2 = CH1 ± Offset (D18).** CH2 folgt CH1 mit festem Offset (positiv oder negativ, z. B. Überlagerungsempfänger mit fester ZF): Einstellung und Anzeige auf der CH2-Seite, Grenzen 8 kHz–160 MHz für beide, CH3-IQ und Spread berücksichtigen, SCPI/REST und Weboberfläche; Planung vor der Umsetzung. *Commit:* `N14: Add CH2 coupled to CH1 with offset`
- [ ] **N15 · Flash sparen (bei Bedarf, Prüfung 2026-11-10).** Teilschriften mit `lv_font_conv` aus denselben Quellen (Montserrat, Font Awesome, OFL): Montserrat 28 (37 KB, nur die Frequenz auf CH1/CH2/CH3: Ziffern, `.`, Leerzeichen, `~`, `kHz`/`MHz`) auf ≈ 5 KB → ≈ 30 KB; Montserrat 20 (22 KB) ohne ungenutzte Symbole (genutzt: BACKSPACE, LEFT, RIGHT, MINUS, PLUS, TRASH, WIFI) → einige KB. Vorher alle Texte dieser Schriften prüfen, danach Bedientest. Nur mit eigenem IDF-Build (D14 ausgeschlossen): Bluetooth-Reste ≈ 6 KB, Core-Dump ≈ 13 KB, IPv6 ≈ 11 KB, Fehlertexte ≈ 8 KB. Schon erledigt: TFT_eSPI-Schriften −27,6 KB, Build-Pfade −0,9 KB (Stand 1 507 650 B, 47,9 %). *Commit:* `N15: Subset the large LVGL fonts`

---

## 5. Pflege dieser Datei

- Der Haken eines Schritts steht im selben Commit wie die Änderung; erledigte Schritte und Befunde werden beim nächsten Aufräumen entfernt (Abschnitt 1.1 fasst sie zusammen).
- Neue Befunde (M-n aus Messungen, B-n aus Bedientests) kommen mit Bezug in Abschnitt 2 und als Schritt in Abschnitt 4; bestehende Schrittnummern bleiben stabil.
- Neue Entscheidungen mit Datum in Abschnitt 1.3; dauerhafte Entscheidungen zusätzlich als E-n in der Spezifikation.
