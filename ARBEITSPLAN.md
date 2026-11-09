# Arbeitsplan – CYD HF Generator

Stand: 2026-11-06 · Firmware **1.3.3** (`main` = Tag `v1.3.3` = `5ffc228`); `dev` = 1.3.3 + Phase 5 + Phase 4 bis S41b (Core 3.3.12, LVGL 8.4.0; die 2.x-Env bleibt bis S41c); `feature/web-hmi` = `dev` + Phase 6 bis S54a (gepusht), am Gerät geprüft · nächste Ziele: **1.4.0** (WEB-HMI, Phase 6), Abnahme der Migration mit Messgeräten (S41c)

Einzige Planungsdatei: offene Befunde, WEB-HMI-Konzept, Entscheidungen und Aufgabenliste. Sie ersetzt `todo.md`, `AUDIT_2026-11.md` und `KONZEPT_WEB_HMI.md` (vollständig in der Git-Historie bis `ee2b91a`). Die Befund-IDs aus dem Audit (C-n Code, S-n Security, U-n Bedienung, P-n Plattform, T-n Tests) und dem Konzept (W-E, W-S, V-n) bleiben als Namen gültig. Messungen am Gerät: [MESSPLAN.md](MESSPLAN.md). Verhalten: [USER_MANUAL_DE.md](USER_MANUAL_DE.md); Anforderungen und Entscheidungen: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md).

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
7. **Branches:** Phase 6 auf `feature/web-hmi` (von `dev` nach dem Core-3-Merge); S41c auf `feature/core3`, sobald Messgeräte da sind.
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
| 4 Migration bis S41b | `dev` | Env `-core3` mit espressif32 8.0.0 (Arduino 3.3.12 / IDF 5.5.5, `-Os`) neben 2.x, LEDC-API, I2C-Zugriffe in `I2CBus` (C-14, V-7), GCC-14-Warnungen behoben, Host-Env `native-cpp20`, LVGL 8.4.0 (E-28), Smoke-Test mit Core 3, Routing-Entscheidung D14 |
| 5 WEB-HMI-Vorarbeiten | `dev` | V-1 bis V-6, V-11, C-16, C-17 (S42–S48): Ergebniscodes, Änderungszähler, Ereignisse mit Quelle, Speichern je Quelle und Schlüssel, Logger mit Ringpuffer, Befehlsqueue mit Schnappschuss, Netz-Parser |
| 6 WEB-HMI bis S54a | `feature/web-hmi` | Befehlskern mit Quellen, Ratenbegrenzung und `Remote outputs`; SCPI über USB (E-29) mit Identify; Netzeinstellungen `hfnet` (E-30); WLAN-Manager (Client, Rückfall-AP, mDNS, Captive-DNS, D14); CONFIG `WiFi`/`Remote out.`, Statusleiste mit WLAN und `REMOTE`; Seiten WIFI (Scan, Tastatur), Access Point (QR) und Device (Name, statische IP); LVGL-Pool aus dem Heap (E-31); Host-Werkzeuge in `tools/` (`device_check.py`) |

Host-Tests: 350 Fälle in `native-win` und 4 in `native-win-fault` (Stand S54).

### 1.2 Referenzwerte

- **Firmware 1.3.0:** `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` unter `messungen/firmware/1.3.0/` (nicht im Repo).
- **Heap (Debug-Build 1.3.1, Original-CYD ohne SI5351):** frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei; Stack-Reserve `loop()` 4 796 B. Basis für den Vergleich nach der Migration (S37).
- **Flash/RAM 1.3.2:** 630 069 B (20,0 %) / 90 312 B (27,6 %); **1.3.3:** 638 033 B (20,3 %) / 90 568 B (27,6 %).
- **Touch-Abgleich (2026-11-04):** CYD2USB x 161…3778, y 245…3830; Original-CYD x 166…3716, y 280…3885 (vorher fest 300…3800); Original-CYD nach dem Werksreset (2026-11-05) x 158…3708, y 245…3856, Prüfkreuz −2/+1 px.
- **Core 3 (Spike S37, 2026-11-05):** Release-Build Flash 668 247 B (2.x 645 445 B), RAM 96 440 B (2.x 94 496 B); Debug-Build, Original-CYD ohne SI5351: Heap frei 250 684 B (2.x 272 604 B), Minimum 221 872 B (2.x 267 008 B), Stack-Reserve `loop()` 4 880 B, erstes Bild 172 ms (LVGL 8.4.0: 169 ms; 2.x 159 ms), `Setup complete` nach 0,9 s.
- **Flash/RAM `feature/web-hmi` S54a (Core 3):** 1 381 971 B (43,9 %) / 76 476 B (23,3 %); 2.x-Env 1 201 629 B / 74 384 B.
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

**Nicht umsetzen:** RGB-LED als Ausgangsanzeige (bleibt Seitenanzeige, E-3); doppelte Statusanzeige im Header bleibt (E-13); Kanalwahl im Header (Rest von U-16, 2026-11-05: kein Platz neben Zustand und `MAIN`, über MAIN und die Karten 2 Tipps); LVGL 9, LovyanGFX (außer als Fallback), espressif32 6.13.0, Secure Boot / Flash Encryption (S-22).

**Offene Rückfragen:**

1. **Neue Funktionen:** Welche sind gewünscht? a) Speicherplätze (z. B. 5 je Kanal), b) Wobbeln, c) Drive-Strength je Kanal, d) Info-Seite „Synthesizer“, e) freie Phase / Phasen-Feinabgleich, f) gekoppelter Betrieb CH2 = CH1 ± Offset. Entscheidung vor N14.
2. **Standard-Tuning:** bleibt `Fractional`, oder wird `Low noise` Standard, wenn die Messungen (MESSPLAN Stufe 2) einen klaren Vorteil zeigen? (N1)

---

## 2. Offene Befunde

`[M]` = wartet auf eine Messung (MESSPLAN-ID in Klammern).

### 2.1 SI5351, HF und IQ

- [M] **MultiSynth abgeschalteter Kanäle abschalten** (K2, F7): CH1/CH2 im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter; CLK2 bekommt `CLK2_PDN` nur bei Off state `Low`. Ist ein Ausgang mit `CLKx_PDN` hochohmig (K2), bei `High-Z` alle abgeschalteten Ausgänge mit PDN abschalten. F7 zeigt, ob es sich lohnt. → N5
- [M] **Störquellen der Plattform** (F6, F9): Backlight-PWM 5 kHz (Seitenlinien ±5 kHz möglich; Abhilfe ≥ 20 kHz oder 100 % ohne PWM), I2C-Überwachung alle 2 s (Abhilfe: Ruhemodus), CONFIG schreibt den Speicherzustand alle 300 ms neu (C-15; nur bei Änderung schreiben). → N6
- [M] **IQ-Abstimmen ohne Aussetzer** (D3, D5, K6, E9): Jede Frequenzänderung im IQ-Betrieb setzt die PLL zurück (1000 Schritte → 1000 Resets). Teiler mit Hysterese festhalten und nur die PLL ohne Reset schreiben (wie `Low noise` im Einzelbetrieb), dann bleibt die Phase erhalten; ebenso bei Korrekturänderungen. Host-Test „PLL-Änderung ohne Reset, Offset und Teiler unverändert“. → N2
- [M] **180°/270° bei ungeraden Teilern** (E4): Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°; der IQ-Teiler `min(127, ⌊900 MHz / f⌋)` ist in 54 % der Fälle ungerade (14,2 MHz → 63, 99 MHz → 9). Abhilfe: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, oder 180° über den Phasen-Offset (2 × Teiler ≤ 127). → N3
- [ ] **Phasen-Feinabgleich und freie Phase:** Auflösung 90° / Teiler (10 MHz: 1°, 99 MHz: 10°); Skew- und Kabelausgleich (E3), Ausbildungsversuche zur Spiegelunterdrückung. Nach Rückfrage 1e. → N4
- [ ] **Spannungseinbruch über Sticky-Bit erkennen:** `checkAlive()` erkennt einen SI5351-Neustart nur, solange ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen; dasselbe Register meldet `LOL_A`/`LOL_B` → Zeile „PLL locked“ auf der Info-Karte. → N9
- [ ] **Hinweise:** SI5351 läuft beim Flashen und nach ESP32-Reset mit der letzten Einstellung weiter (A10, Handbuch); Hardware-Hinweise (eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO nach C7). VCO bei `Fractional` und +50 ppm 900,045 MHz (unkritisch, nur notieren). → S72, N7, N10

### 2.2 Bedienung

- [ ] **Netzwahl nach falschem Passwort am Gerät** (S53b, Prüfpunkt h): zwei Netze gespeichert, das stärkere mit falschem Passwort → nach dem Fehlschlag verbindet sich das Gerät mit dem anderen. Host-getestet (`WifiPolicy::chooseSlot`). → S61

### 2.3 Code und Tests

- [ ] **DMA und Doppelpuffer** (nach der Migration): zwei Puffer 320 × 40 px (`DMA_ATTR`, je 25,6 KB), `LV_COLOR_16_SWAP` = 1, `setSwapBytes(false)`; in `DisplayPanel::begin()` erst Erkennung und Init mit rohem Buszugriff, dann `initDMA()` und einmal `startWrite()` (CS bleibt LOW, `endWrite()` nie); `flush()` = `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`, `lv_disp_flush_ready()` sofort. Erwartung bis ≈ 30 Bilder/s, +36 KB RAM; Messung mit `LV_USE_PERF_MONITOR`. → N13
- [ ] **CodeQL-Funde in `data/www/app.js`** (G4): `js/request-forgery` (Zeile 138, `fetch(path)`) und `js/user-controlled-bypass` (Zeile 139) über WebSocket-Daten → `S.ch3.master` → `master(n)` → Pfad `/api/channel/<n>`. Praktisch Fehlalarm (eigener, per Token geschützter WebSocket derselben Herkunft, relativer Pfad; Zugriffsschutz prüft das Gerät), aber billig zu härten: `master()` liefert nur 1 oder 2. → S56
- [ ] **Fakes:** Bei NVS-Schemaänderungen Migrationstest mit alten Schlüsseln (mehrere Namespaces und Strings im Preferences-Fake seit S52a).

### 2.4 Plattform und Lieferkette

| ID | Komponente | heute | Ziel |
|---|---|---|---|
| P-2, P-4, S-19 | espressif32 / Arduino / IDF | Standard-Env 6.7.0 / 2.0.16 / 4.4.7 (EOL, mbedTLS 2.28 EOL); Env `-core3` 8.0.0 / 3.3.12 / 5.5.5 seit S37–S41b am Gerät geprüft (Werte in 1.2) | nur noch `espressif32@8.0.0` nach der Abnahme S41c (u. a. DHCP-Fix CVE-2026-45160); pioarduino nicht nötig |
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
- **Info-Karte:** WLAN-Modus, SSID, RSSI, IP, `<name>.local`, SCPI-Status, Web-Clients, Heap, LVGL-Heap; Taste `Disconnect all` (S-10, S-17).
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

### Phase 4 – Migration Core 3.x (Rest, `feature/core3`)

- [ ] **S41c · Abnahme.** Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10, ≈ 4 h, braucht ZÄHLER und OSZI), dabei CLK0 bei Q4/Q5 und J6 (Code 5 bei Leitung auf GND); dann 2.x-Env entfernen (der 2.x-Zweig in `backlight.cpp`, `ESP_ARDUINO_VERSION_MAJOR` 2 im Fake und die Handbuchzeile `Wire.cpp:499` gehen mit), Spezifikation A-46, README, `CLAUDE.md`; Merge nach `dev` (lokal). *Commit:* `S41c: Switch to the official espressif32@8.0.0 platform`

### Phase 6 – WEB-HMI 1.4.0 (Rest ≈ 8 d, `feature/web-hmi`)

- [ ] **S49 · Vorab-Messung WLAN (0,5 d, braucht SPEKTRUM).** Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis in Abschnitt 3. *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [x] **S54 · SCPI über TCP.** `remote/scpi_tcp.*` (rein, host-getestet: eine Verbindung, HTTP-Zeile schließt vor der Ausführung, Leerlauf 10 min, Antworten je Zeile gesammelt und ohne Blockieren gesendet, Rest bei vollem Puffer gehalten, sonst `not reading`; eigene Sitzung mit `clearStatus()` je Verbindung) und `remote/scpi_tcp_server.*` (Port 5025, nicht blockierende lwIP-Aufrufe statt `WiFiClient::write` wegen bis zu 10 s Blockieren, Keepalive 60/10/3 s, Nagle aus; an bei STA verbunden oder AP mit Passwort); mDNS `_scpi-raw._tcp`; Backend `loopBackend` für USB und TCP. Suite `test_scpi_tcp` (13). Core 3: Flash +23,7 KB, RAM +1,26 KB (lwIP-DNS-Client jetzt gelinkt), Heap ≈ 2,3 KB. Am CYD2USB über das Büro-WLAN 2026-11-06: `device_check.py --tcp` 94 PASS, 0 FAIL; 1000 × `*IDN?` Median 14,7 ms, 99 % 97 ms, max. 1,4 s (WLAN-Latenzspitzen, Ping bis 2,6 s; Modem-Sleep → S49); zweiter Client `refused (busy)`, neuer Client 0,03 s nach dem Schließen, HTTP-Zeile `closed (HTTP request)` ohne Ausführung, 8 s Flut ohne Lesen → `closed (send failed)` ohne Neustart, Heap danach 133 KB frei, Minimum 81 KB. Offen: Client-IP auf der Info-Karte und `Disconnect` (GUI, mit S55), Keepalive-Abbau und offener AP am Gerät (S61). *Commit:* `S54: Add SCPI over TCP`
- [x] **S55a · Request-Guard und REST-JSON.** `net/request_guard.*` (Host-, Origin-, `Sec-Fetch-Site`-, Token-, Content-Type- und Größenprüfung, IPv6 abgelehnt, Captive-302 nur für Seiten über den AP; Sitzungen: Cookie `cyd_sid` 128 bit, 12 h ab Login, ≤ 4, Vergleich in konstanter Zeit; Login-Sperre global nach 5 Fehlversuchen 30 s, verdoppelt bis 960 s; Web-Passwort `pbkdf2-sha1$10000$salt$hash`; Passwortfelder über offenen AP → 403, W-S1) und `remote/rest_json.*` (strenge RFC-8259-Prüfung vor ArduinoJson 7.4.3: ungültig/doppelter Schlüssel −102, falscher Typ −104, unbekannter Schlüssel −108, leer −109, zu groß −222; bis 8 Commands in fester Reihenfolge, Abbruch beim ersten Fehler; Zustand-JSON mit `save`, `remote`, `modes` aus `StatusText::modeSummary`). `Snapshot` um `saveState`, `lastRemote`, `lastRemoteMs` erweitert. Suites `test_http_guard` (16) und `test_rest_json` (13), je mit Zufallstest (100 000). Noch nicht im Server verdrahtet (S55). *Commit:* `S55a: Add request guard and REST JSON mapping`
- [ ] **S55 · Webserver (2 d).** Vorschlag (für G2): Weboberfläche gzip in die Firmware einbetten statt LittleFS. Zusätzlich aus S55a: Schnappschuss auch bei Änderung von `saveState()`/`lastRemoteMs()` veröffentlichen und das Ablaufen von `remote` per WebSocket pushen; `verify()` des Web-Passworts (≈ 0,2–0,3 s) nicht im `async_tcp`-Task (W-S23); Verbindungsgrenzen, 10-s-Timeout, Header und CSP im Server. `request_guard`, REST, WebSocket, Login und Web-Passwort (Seite Device), Header, CSP; Netzänderungen aus dem Web zweistufig mit Rückfall (W-S12); `test_http_guard`, `test_rest_json`. *Commit:* `S55: Add guarded web server and REST API`
- [x] **S56a · Weboberfläche statisch und Mock.** `data/www/` (`index.html`, `app.js`, `style.css`: 49 275 B, gzip 16 758 B; ohne `innerHTML`, Inline-Handler, Inline-Stil und externe URLs, CSP-tauglich) und `tools/web_mock.py` (REST, WebSocket, Request-Guard, Login, zweistufige Netzänderung auf simuliertem Gerät). Geprüft gegen den Mock: Endpunkt-Skript 137/137, Headless-Edge 107/107 (Tastatur, Mausrad, Fokus, CH3 WAIT, Login, `local only`, offener AP, Polling-Rückfall, Handy 390 px ohne Querscrollen, Bedienelemente ≥ 44 px, keine CSP-Verletzung); `tools/rest_example.py` läuft gegen den Mock. Noch nicht auf echtem Handy, Safari, Firefox. Offene Vertragsfragen in 3.6. *Commit:* `S56a: Add web user interface and mock server`
- [ ] **S56 · Weboberfläche (2,5 d).** Alle Bereiche, Tastatur, responsiv, Live-Hervorhebung, zweistufige Netzänderung, Identify. *Commit:* `S56: Add web user interface`
- [x] **S57a · Fuzz-Harnesses und CI.** `test/fuzz/`: 7 Ziele (`line_framer`, `sanitize`, `scpi` über `LineFramer` → `ScpiSession::execute`, `scpi_number`, `remote_frequency`, `net_settings`, `scan_list`) mit Seed-Korpus (69 Dateien), Wörterbuch, `build.sh` (libFuzzer/ASan/UBSan mit clang; `--standalone` mit gcc/MinGW); Suite `test_fuzz_random` (W-S25: Korpus und 100 000 Eingaben mit festem Seed je Ziel, +7 Fälle); `.github/workflows/fuzz.yml` (je Ziel 60 s bei Push/PR, manuell mit `runs` ≥ 10 Mio., Funde und Korpus als Artefakt, gcc-ASan-Job; Actions per SHA, `contents: read`); `.gitattributes` (`*.sh` LF, Korpus binär). Lokal (MinGW, Standalone): je Ziel 1 Mio. Eingaben ohne Fund außer `ScpiNumber::decimal` bei > 256 B (Abschnitt 2.3). libFuzzer-Lauf erst in der CI. *Commit:* `S57a: Add fuzzing harnesses and harden CI`
- [x] **S57b · Zahlen mit sehr langem Exponenten.** `ScpiNumber::decimal` begrenzte den Exponenten auf 4 Stellen, zählte aber jede führende Null des Bruchteils (`0.` + 1216 Nullen + `1E12345` → `Ok` mit 10^17); jetzt sättigt der Exponent oberhalb der Tokenlänge (int64), Ergebnis dann `Range` bzw. 0. `test_number_parser` +4 Fälle (Gegenprobe: scheitert ohne Fix), Harness `scpi_number` prüft jede Länge gegen die Referenz. Über SCPI vorher nicht erreichbar (Zeile ≤ 256 B). *Commit:* `S57b: Fix SCPI numbers with very long exponents`
- [ ] **S57 · Fuzzing und Dauertest (1 d + Laufzeit).** Rest: Harnesses für `request_guard` und REST-JSON (mit S55), manueller CI-Lauf mit ≥ 10 Mio. je Ziel, 24-h-Lauf mit Heap- und Stack-Bilanz. libFuzzer-CI-Job (≥ 10 Mio.), Zufallstest mit festem Seed; 24-h-Lauf mit Heap- und Stack-Bilanz. *Commit:* `S57: Add fuzzing job and long-run test`
- [ ] **S58 · Doku und Release-Kandidat (1,5 d).** Handbuch DE/EN (WLAN, Web, SCPI-Referenz, REST, Python-Beispiel, Sicherheitshinweise, Fernbetrieb und Antenne), Spezifikation, README, `CLAUDE.md`, diese Datei; Version 1.4.0, lokaler Tag `v1.4.0-rc1`; Smoke-Test einschließlich L1 und `*IDN?`. *Commit:* `S58: Document WEB-HMI, release candidate 1.4.0-rc1`

### Phase 7 – GitHub und Verteilung (≈ 2–2,5 d, je ein Commit; `[Nutzer]` = Einstellung im Repo, nur durch den Nutzer)

Stand 2026-11-06: im Repo nur `ci.yml`, `fuzz.yml`, `LICENSE` und Tags `v1.3.0`–`v1.3.3` ohne Release-Dateien.

- [ ] **G1 · Release-Workflow (0,5 d).** Bei Tag `v*`: beide Envs bauen, ans GitHub-Release `firmware.bin`, die Teile für den Webflasher (`bootloader.bin` 0x1000, `partitions.bin` 0x8000, `boot_app0.bin` 0xE000, `firmware.bin` 0x10000) und `SHA256SUMS` hängen; Herkunftsnachweis mit `actions/attest-build-provenance` (W-S24); Actions per SHA, `contents: write` nur in diesem Job. *Commit:* `G1: Add release workflow with build provenance`
- [ ] **G2 · Webflasher über WebSerial (0,5–1 d, nach G1; Voraussetzung `[Nutzer]` Pages-Quelle „GitHub Actions“).** ESP Web Tools (feste Version vom CDN) auf GitHub Pages: `index.html` mit `<esp-web-install-button manifest="manifest.json">`, `manifest.json` mit `chipFamily` `ESP32` und den **einzelnen Teilen** aus G1 – kein `merge_bin` ab 0, das füllt Lücken mit `0xFF` und löscht bei jedem Update NVS (Touch-Abgleich, Korrektur, WLAN); `new_install_prompt_erase` für die Erstinstallation. Die `.bin` liegen auf Pages selbst (Release-Downloads haben keine CORS-Header), HTTPS liefert Pages. Deployment per `actions/deploy-pages` aus dem Release-Workflow. Handbuch DE/EN: Chrome/Edge am Desktop (nicht Firefox/Safari), CH340-Treiber unter Windows, Update ohne Erase behält die Einstellungen. Prüfung: beide Boards über die Seite flashen, Update ohne Erase behält Abgleich und WLAN. Abhängig von S55: Weboberfläche in die Firmware einbetten (gzip ≈ 17 KB), damit es ein einziges Image bleibt; sonst LittleFS-Image als weiterer Teil. *Commit:* `G2: Add web flasher on GitHub Pages`
- [ ] **G3 · Improv Serial (optional, 0,5 d, nach G2).** WLAN direkt nach dem Flashen aus ESP Web Tools einrichten. Kollidiert mit SCPI über USB: der Zeilenleser lehnt Binärpakete ab; Improv-Pakete (Kopf `IMPROV`) vor dem `LineFramer` erkennen. Zugangsdaten nur RAM → `NetConfig`, nie im Log. *Commit:* `G3: Add Improv serial WiFi provisioning`
- [x] **G4 · Sicherheit im Repo.** `SECURITY.md` (unterstützt: letztes Release und `dev`; Meldung nur über private Vulnerability Reports; Umfang Netz-Schnittstellen, NVS, Parser, `tools/`, Workflows; außerhalb: physischer Zugriff/Flash-Auslesen S-22, kein TLS; Antwort ≈ 7/30 Tage). `.github/workflows/codeql.yml`: `c-cpp`, `python`, `javascript-typescript`, `actions`, alle `build-mode: none`, Suite `security-extended`, bei Push/PR und wöchentlich, `security-events: write` nur im Job. C/C++ nur teilweise analysiert (78 Dateien, 46 ohne Arduino-/LVGL-/IDF-Header; Netzdaten aus `WiFiClient`/AsyncWebServer nicht als Quelle erkannt): Fuzzing und W-S-Review bleiben die Hauptsicherung. Secret Scanning und Push-Schutz waren schon an (2026-11-09); `[Nutzer]` private Vulnerability Reports einschalten; CodeQL-Default-Setup muss aus bleiben (sonst werden die Uploads abgelehnt). Lauf auf `agent/g4-g6`: CodeQL, CI, Fuzz grün; 2 Funde in `app.js` (Abschnitt 2.3). *Commit:* `G4: Add security policy and CodeQL`
- [ ] **G5 · Regeln für `main`.** `[Nutzer]` Branch-Ruleset: CI und Fuzz grün, kein Force-Push, kein Löschen, Änderungen nur per Pull Request. Kein Commit nötig; Ergebnis in Abschnitt 1.3 notieren.
- [ ] **G6 · Abhängigkeiten aktuell halten.** Dependabot für GitHub Actions (hält die SHA-Pins aktuell, wöchentlich, gruppiert); optional Renovate für die PlatformIO-Bibliotheken (Dependabot kann `platformio.ini` nicht). Updates nur über Pull Request mit grüner CI. *Commit:* `G6: Add Dependabot for GitHub Actions`
- [ ] **G7 · Größenbericht in der CI.** Flash/RAM beider Envs je Build als Job-Zusammenfassung und Artefakt, Differenz zum Ziel-Branch im Pull Request. *Commit:* `G7: Report firmware size in CI`
- [x] **G8 · Vorlagen.** Issue-Formulare `bug_report.yml` (Board, Firmware-Version und Commit von der Info-Karte, Installationsweg, SI5351-Modul, betroffener Bereich, Log, Pflicht-Haken „keine Passwörter oder Tokens“), `feature_request.yml`, `config.yml` (leere Issues aus, Links auf Handbuch und Sicherheitsrichtlinie), `PULL_REQUEST_TEMPLATE.md` mit der Gate-Checkliste aus Abschnitt 0 und Flash/RAM-Tabelle; gegen die SchemaStore-Schemas geprüft. Discussions nicht nötig (kein Link darauf). *Commit:* `G8: Add issue and pull request templates`
- [x] **G9a · Badges und Hardware-Doku.** README mit Badges (CI, Fuzz, Release, Lizenz) und Links auf Webflasher, Handbücher und `docs/hardware.md` (Stückliste, Board-Varianten, Verdrahtung CN1 mit SDA/SCL in beliebiger Reihenfolge, Versorgung, Ausgänge, Hinweise aus 2.1, Sicherheit, Fotoliste mit Platzhaltern); Links und Anker per Skript geprüft. *Commit:* `G9: Add project page, badges and hardware docs`
- [ ] **G9 · Projektseite (Rest).** `[Nutzer]` Beschreibung, Website `https://dm8sb.github.io/CYD-HF-Generator/`, Topics, Vorschaubild 1280 × 640; CN1-Steckerraster (1,25 mm) und Pinfolge am Board bestätigen; Handbuch als Pages-Seite (nach G2). *Commit:* `G9b: Add manual page`
- [ ] **G10 · Fotos (später, `[Nutzer]`).** Nach der Liste in `docs/hardware.md`: `board-cyd-front.jpg` und `board-cyd2usb-front.jpg` (MAIN-Seite, CH1 an), `si5351-wiring.jpg` (Modul mit den 4 Leitungen zu CN1 und CLK0–2), `cn1-connector.jpg` (Rückseite, Beschriftung lesbar), `setup-bench.jpg` (Aufbau mit Koax zum Messgerät), je ≤ 500 KB, kein WLAN-Name auf dem Display; ablegen unter `docs/img/`. Danach Platzhalter in `docs/hardware.md` aktivieren. Vorschaubild `social-preview.png` 1280 × 640 (< 1 MB) lädt der Nutzer unter Settings → General → Social preview hoch (nicht im Repo). *Commit:* `G10: Add hardware photos`

### Phase 8 – Messungen ([MESSPLAN.md](MESSPLAN.md))

Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` (ohne Medien); Befunde (M-n) sofort in Abschnitt 2 eintragen. Stufe 1 und 2 laufen **sobald Zähler und Oszilloskop verfügbar sind**; dieser Lauf ist die Referenz R0 (MESSPLAN 9.2).

- [ ] **S59 · Stufe 1 (Pflicht, ≈ 7 h).** MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10). *Commit:* `S59: Record tier-1 measurements`
- [ ] **S60 · Stufe 2 (Entscheidungen, ≈ 6 h).** MESSPLAN Abschnitt 6: Standard-Tuning (K5, F3, K6), IQ ohne Reset (D3, D5, E9), ungerade Teiler und Phasenfehler (E3, E4, E5), Störquellen und PDN (F6, F7), Pegel (F2), Drift (C7); Entscheidungen als E-n in die Spezifikation, N1–N8 bestätigen oder streichen. *Commit:* `S60: Record tier-2 measurements and decisions`
- [ ] **S61 · Stufe 1W (Pflicht ab WEB-HMI, ≈ 8 h + 24 h).** MESSPLAN 5.11 nach S58 auf beiden Boards; dazu L13, L15, Q1 aus Stufe 2. *Commit:* `S61: Record network and security measurements`
- [ ] **S62 · Stufe 3 (optional).** Charakterisierung und Bedientests mit Personen nach Bedarf (MESSPLAN Abschnitt 7). *Commit:* `S62: Record optional characterization measurements`
- [ ] **S63 · Handbuchwerte.** Pegeltabelle (F2), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise; DE und EN. *Commit:* `S63: Add measured values to the manuals`
- [ ] **S64 · Freigabe 1.4.0.** Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag `v1.4.0`; Merge nach `dev` und `main` nur auf Anweisung. *Commit:* `S64: Release 1.4.0`

### Phase 9 – Folgearbeiten (je ein Commit, nach Messergebnis oder Rückfrage)

- [ ] **N1 · Standard-Tuning** ändern, wenn die Stufe-2-Messungen es tragen (Rückfrage 2, E-19).
- [ ] **N2 · IQ-Abstimmen ohne Aussetzer** (Abschnitt 2.1).
- [ ] **N3 · Ungerade IQ-Teiler bei 180°/270°**, wenn E4 einen Fehler > 1° zeigt.
- [ ] **N4 · Phasen-Feinabgleich, freie Phase, Info-Seite „Synthesizer“** nach E1–E3 und Rückfrage 1.
- [ ] **N5 · MultiSynth abgeschalteter Kanäle abschalten, PDN-Strategie** nach K2 und F7.
- [ ] **N6 · Störquellen-Abhilfen** (Backlight-PWM ≥ 20 kHz, Ruhemodus der Überwachung, Neuzeichnen nur bei Änderung) nach F6, F9.
- [ ] **N7 · TCXO-Option** nach C7, C8.
- [ ] **N8 · Anwendungskapitel „Mischer-Experimente“** (Abschnitt 2.5) nach F2, G1, G2, E10.
- [ ] **N9 · Sticky-Bit und „PLL locked“** (Abschnitt 2.1).
- [ ] **N10 · Hardware-Hinweise** in den Handbüchern.
- [ ] **N11 · Spätere Updates:** Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6; Advisory-Check je Release.
- [ ] **N13 · DMA und Doppelpuffer** (Abschnitt 2.3, nach der Migration).
- [ ] **N14 · Erweiterungen nach Rückfrage 1:** Speicherplätze, Wobbeln, Drive je Kanal, gekoppelter Betrieb CH2 = CH1 ± Offset (Überlagerungsempfänger mit fester ZF), Modulationsarten (AM/FM über den PLL-Bruchteil), externe Referenz (SI5351C mit `CLKIN`), Kalibrierassistent (falls nicht in S32).

---

## 5. Pflege dieser Datei

- Der Haken eines Schritts steht im selben Commit wie die Änderung; erledigte Schritte und Befunde werden beim nächsten Aufräumen entfernt (Abschnitt 1.1 fasst sie zusammen).
- Neue Befunde (M-n aus Messungen, B-n aus Bedientests) kommen mit Bezug in Abschnitt 2 und als Schritt in Abschnitt 4; bestehende Schrittnummern bleiben stabil.
- Neue Entscheidungen mit Datum in Abschnitt 1.3; dauerhafte Entscheidungen zusätzlich als E-n in der Spezifikation.
