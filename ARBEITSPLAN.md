# Arbeitsplan – CYD HF Generator

Stand: 2026-11-06 · Firmware **1.3.3** (`main` = Tag `v1.3.3` = `5ffc228`); `dev` = 1.3.3 + Phase 5 (S42–S48, am Gerät geprüft) · nächste Ziele: Migration Core 3.x (Phase 4), **1.4.0** (WEB-HMI)

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
6. **Nur lokal.** Push, Merge nach `main` und Tags veröffentlichen nur auf ausdrückliche Anweisung.
7. **Branches:** Phase 4 auf `feature/core3`; Phase 6 auf `feature/web-hmi`, neu von `dev` nach der Migration. Phase 5 lief auf `feature/web-hmi` und ist seit 2026-11-05 in `dev` (Merge `262119f`), weil sie das Verhalten nicht ändert und die Migration sonst neben ihr auseinanderliefe.
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

Host-Tests: 251 Fälle in `native-win` und 4 in `native-win-fault` (Stand S39).

### 1.2 Referenzwerte

- **Firmware 1.3.0:** `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` unter `messungen/firmware/1.3.0/` (nicht im Repo).
- **Heap (Debug-Build 1.3.1, Original-CYD ohne SI5351):** frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei; Stack-Reserve `loop()` 4 796 B. Basis für den Vergleich nach der Migration (S37).
- **Flash/RAM 1.3.2:** 630 069 B (20,0 %) / 90 312 B (27,6 %); **1.3.3:** 638 033 B (20,3 %) / 90 568 B (27,6 %).
- **Touch-Abgleich (2026-11-04):** CYD2USB x 161…3778, y 245…3830; Original-CYD x 166…3716, y 280…3885 (vorher fest 300…3800); Original-CYD nach dem Werksreset (2026-11-05) x 158…3708, y 245…3856, Prüfkreuz −2/+1 px.
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

**Nicht umsetzen:** RGB-LED als Ausgangsanzeige (bleibt Seitenanzeige, E-3); doppelte Statusanzeige im Header bleibt (E-13); LVGL 9, LovyanGFX (außer als Fallback), espressif32 6.13.0, Secure Boot / Flash Encryption (S-22).

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

- Kanalwahl im Header (Rest von U-16) **nicht umgesetzt** (entschieden 2026-11-05): kein Platz im Header neben Zustand und `MAIN`; über MAIN und die Karten sind es 2 Tipps.

### 2.3 Code und Tests

- [ ] **Heap auf der Info-Karte** (V-8): freier/minimaler Heap und LVGL-Heap; kommt mit der Info-Karten-Erweiterung in S53.
- [ ] **DMA und Doppelpuffer** (nach der Migration): zwei Puffer 320 × 40 px (`DMA_ATTR`, je 25,6 KB), `LV_COLOR_16_SWAP` = 1, `setSwapBytes(false)`; in `DisplayPanel::begin()` erst Erkennung und Init mit rohem Buszugriff, dann `initDMA()` und einmal `startWrite()` (CS bleibt LOW, `endWrite()` nie); `flush()` = `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`, `lv_disp_flush_ready()` sofort. Erwartung bis ≈ 30 Bilder/s, +36 KB RAM; Messung mit `LV_USE_PERF_MONITOR`. → N13
- [ ] **Fakes:** Preferences mit mehreren Namespaces und Strings (für `hfnet`). Bei NVS-Schemaänderungen Migrationstest mit alten Schlüsseln. → S52

### 2.4 Plattform und Lieferkette

| ID | Komponente | heute | Ziel |
|---|---|---|---|
| P-2, P-4, S-19 | espressif32 / Arduino / IDF | 6.7.0 / 2.0.16 / 4.4.7 (EOL, mbedTLS 2.28 EOL) | offiziell `espressif32@8.0.0` = Arduino 3.3.12 / IDF 5.5.5 (u. a. DHCP-Fix CVE-2026-45160); pioarduino 55.03.312-1 nur bei Bedarf an `custom_sdkconfig` → S37–S41. **Spike S37 (2026-11-05):** baut mit 8.0.0 nach zwei Änderungen: LEDC-API (`ledcAttach`/`ledcWrite(pin)`) und `-Os` – das PlatformIO-Skript von Arduino 3.3.12 setzt keine Optimierung (die Arduino-IDE ergänzt sie über `platform.txt`), mit `-O0` brauchte das erste Bild 636 statt 159 ms und der Flash 982 KB. Mit `-Os`, Release-Build: Flash 668 247 B (21,2 %; 2.x 645 445 B), RAM 96 440 B (2.x 94 496 B). Debug-Build, Original-CYD ohne SI5351: Heap frei 250 684 B (2.x 272 604 B), Minimum 221 872 B (2.x 267 008 B, Spitze beim Booten), größter Block und LVGL unverändert, Stack-Reserve `loop()` 4 880 B (2.x 4 780 B), erstes Bild 172 ms, `Setup complete` nach 0,9 s wie 2.x. Beide Boards booten, Display-Erkennung (MADCTL- und RDDID-Rücklesung) unverändert, SI5351 an 0x60. pioarduino nicht nötig. Offen für S40: `-Wvolatile` (`command.h`) und `-Wdeprecated-enum-enum-conversion` (LVGL-Makros mit `|`) unter gnu++2b; `CORE_DEBUG_LEVEL=0` des Frameworks per `build_unflags` ersetzt. |
| P-5 | TFT_eSPI | 2.5.43 (letztes Release) | bleibt; mit Core 3.x im Spike unverändert lauffähig (Flush 35 ms je Vollbild wie unter 2.x); Fork oder LovyanGFX nur als Ausweg (eigene Display-Erkennung bleibt, LovyanGFX erwartet ST7789-ID 0x85, der CYD2USB meldet `8181B3`) |
| P-6, P-8 | LVGL, Unity | 8.3.11, 2.6.1 | LVGL 8.4.0 (letztes 8.x, Fixes, API wie 8.3) in Phase 4 → S40a, damit die Abnahme S41 beides prüft; LVGL 9.x nicht: neue Display-, Eingabe-, Stil- und Widget-API, also Umbau aller Seiten und neuer Bedientest, ohne Funktion, die das Gerät braucht (D13). Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6 → N11 |
| P-9 | Web-Bibliotheken | – | ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, ArduinoJson 7.4.3, scpi-parser v2.3 (Commit), alle exakt gepinnt |
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
| `net/wifi_manager.*` | Betriebsarten, `WiFiMulti`, Rückfall-AP, statische IP, Gerätename, mDNS, Captive-DNS (nur AP), AP aus beim STA-Connect |
| `net/net_settings.*` | NVS `hfnet`: Modus, 2 × SSID/PSK, AP-SSID/-Passwort, Gerätename, statische IP, Web-Passwort-Hash, `remote_outputs`; bei WPA2 64-Hex-PMK statt Passphrase, `WiFi.persistent(false)` (S-8) |
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
| `OUTPut<n>:STATus?` | `ON`/`OFF`/`ERR`/`WAIT`/`NOHW` | tatsächlicher Zustand |
| `[SOURce<n>:]FREQuency[:CW] <f>` | `FREQ?` | Sollfrequenz CH1/CH2; CH3 nur Abfrage |
| `[SOURce<n>:]FREQuency:ACTual?` | Hz mit 0,1 Hz | wie die Zeile `Out` (bei Spread Mittelwert) |
| `SOURce3:IQ:MASTer 1\|2`, `SOURce3:PHASe 0\|90\|180\|270`, `SOURce3:IQ:RANGe?` | `?` | CH3 |
| `OUTPut:DRIVe 2\|4\|6\|8`, `OUTPut:OFFState LOW\|HIGH\|HIZ` | `?` | Ausgänge |
| `OUTPut:REMote?` | `ALLOWED`/`LOCAL` | W-E25 (nur lesbar) |
| `[SOURce1:]SSC OFF\|DOWN05\|DOWN15\|CENT05`, `SYSTem:TUNing FRAC\|LNO\|INT` | `?` | Signal |
| `SYSTem:CORRection <ppm>`, `SYSTem:STEP <Hz>`, `SYSTem:BACKlight <%>` | `?` | Einstellungen |
| `SYSTem:SI5351?`, `SYSTem:HEAP?`, `SYSTem:IDENtify`, `SYSTem:LOG?`, `SYSTem:SAVE` | | Diagnose, Identify, Log, Speichern |
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
| W-S14 | WPA2-PSK als PMK, `WiFi.persistent(false)`; Werksreset löscht `hfnet`, `nvs.net80211`, Coredump; Passwörter nie in Ausgaben. (S-8) |
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

Je Schritt: Inhalt, Prüfung, Commit-Betreff. `[ ]` offen, `[x]` erledigt (wird beim nächsten Aufräumen entfernt).

### Phase 3 – Laborbetrieb 1.3.3 (≈ 2–3 d, `dev`)

- [x] **S31 · Sperre, Grundzustand, Werksreset (U-13).** CONFIG-Zeilen `Screen` (`Lock`) und `Reset` (`Defaults`, `Factory`) mit Bestätigung; `InputLogic::ScreenLock` (2 s Halten entsperrt, die entsperrende Berührung wird verschluckt), `AppController::resetToDefaults()` (Korrektur bleibt, Touch-Abgleich liegt außerhalb), `SettingsService::eraseAll()`/`defaults()`; `LOCKED` in der MAIN-Statusleiste, Hinweis beim Berühren. Host-Tests für Sperre, Grundzustand und Löschen. Bedientest 2026-11-05 (CYD2USB): Sperre und `Defaults` OK; `Factory` auf dem Original-CYD OK (Neustart, Touch-Abgleich, Korrektur `+0.00 ppm`).
- [x] **S32 · Korrektur per Numpad (U-11).** `Corr.`-Wert (umrahmt) antippen öffnet das Numpad im ppm-Modus (`+/-`, `ppm`, Rundung auf 0,01 ppm, ±50,00 ppm); `InputLogic::numpadPpmKey()` mit Host-Test; Kalibrieren 20 → 7 Tipps. Assistent „gemessene Frequenz eingeben“ bleibt in N14. Bedientest 2026-11-05 OK.
- [x] **S33 · CH3-Seite und Halten (U-16, U-20).** CH3-Seite: Tipp auf die Frequenz öffnet das Numpad des Masters und kehrt zu CH3 zurück; bei wartendem Master `Master CHx is off: tap to switch on` (Tipp schaltet ihn ein). Halten nach dem Bedientest langsamer: ab 0,6 s ca. 3 Schritte/s, ab 3 s 10/s, ab 6 s 20/s (`InputLogic::repeatSteps`, `repeatDue`); Überbrückung 70 ms statt 35 ms, schwacher Druck zählt als Aussetzer. Host-Tests. Kanalwahl im Header bewusst nicht (Abschnitt 2.2). Bedientest 2026-11-05 OK.
- [x] **S34 · Fix um 112,5 MHz.** `planStandalone()` (Fractional) versucht über 100 MHz den größten geraden Integer-Teiler (8/6/4) mit fraktionaler PLL, wenn die festen PLLs schlechter als 0,25 Hz sind oder der MultiSynth unter 8 läge; der Zweig „aktuelle PLL behalten“ merkt sich seinen Fehler. Bis 100 MHz unverändert. Host-Tests 112 500 004 / 112 499 996 Hz (vorher 4 Hz Fehler), `test_standalone_above_100_mhz` läuft wieder (≤ 1,5 / 2 / 3 Hz bis 112,5 / 150 / 160 MHz). *Commit:* `S34: Fix tuning accuracy around 112.5 MHz`
- [x] **S35 · Bereich 8 kHz bis 160 MHz (E-16).** `FREQ_MIN_HZ`/`FREQ_MAX_HZ` 8 kHz / 160 MHz; Anzeige unter 1 MHz in kHz; Numpad 10 Zeichen, Fehlertext `8 kHz - 160 MHz` (passt neben `Now 160.000 000 MHz`); Hinweis auf der Kanalseite (`>100MHz: low level`, `<1MHz: harmonics`); CH3 wartet über 100 MHz (Test); auf MAIN entfällt das `~` ab 100 MHz (passt nicht in 183 px). Host-Tests für kHz-Format, Grenzen, R-Teiler in allen Modi, CH3-Grenze; Tests der Kerngenauigkeit (1–100 MHz) mit eigener Konstante. Bedientest 2026-11-05 OK (455 kHz, 150 MHz; Moduszeile danach gekürzt, 6bf0318). *Commit:* `S35: Extend CH1/CH2 range to 8 kHz-160 MHz (E-16)`
- [x] **S36 · Release 1.3.3.** Version, README, Smoke-Test beider Boards 2026-11-05 (Schritte 1–4 OK, 5 und 6 ohne Messgerät offen), lokaler Tag `v1.3.3`. *Commit:* `S36: Release 1.3.3`

### Phase 4 – Migration Core 3.x (≈ 2,5–3 d, `feature/core3`)

- [x] **S37 · Spike.** Env `esp32-2432S028R-core3` mit `espressif32@8.0.0` (Arduino 3.3.12 / IDF 5.5.5) neben 2.x, `-Os` und `CORE_DEBUG_LEVEL` dort gesetzt, LEDC per `ESP_ARDUINO_VERSION_MAJOR`; TFT_eSPI/XPT2046/LVGL unverändert; Debug-Zeile `[GUI] First frame ms`; CI baut beide Cores. Ergebnis in Abschnitt 2.4; startet auf beiden Boards, pioarduino nicht nötig. *Commit:* `S37: Spike espressif32@8.0.0 with Arduino 3.3.12`
- [x] **S38 · LEDC.** `ledcAttach()`/`ledcWrite(pin, …)`; die Fakes bilden die 3.x-API nach (`ESP_ARDUINO_VERSION_MAJOR` 3, 2 wählt die alte), `ledcWrite` auf einen nicht angebundenen Pin bzw. Kanal zählt als Fehler; Host-Test `test_backlight_pwm_on_its_pin` (Gegenprobe: Schreiben auf Kanal 0 scheitert). *Commit:* `S38: Port backlight PWM to the Arduino 3 LEDC API`
- [x] **S39 · I2CBus bündeln (C-14, V-7).** `I2CBus::writeRegs`/`readRegs`, `Wire` nur noch in `hal/i2c_bus.cpp`. Quelltext von Arduino 3.3.12 geprüft (I2C-ng-Treiber): `begin()` auf laufendem Bus behält die Pins, Übertragung auf geschlossenem Bus nimmt den Lock dauerhaft (nächstes `begin()`/`end()` blockiert), `endTransmission(false)` sendet erst mit `requestFrom`, Codes 0/2/5 wie 2.0.x. Fake: Repeated Start wie die Cores (Lesen = eine Transaktion). Suite `test_i2c_bus` mit `test_core_*`-Fällen (Gegenprobe: ohne Prüfung auf offenen Bus scheitert `test_core_closed_bus_is_never_touched`). Code 5 bei Leitung auf GND unter 3.x am Gerät noch nicht geprüft (S41, MESSPLAN J6). *Commit:* `S39: Bundle I2C register transfers in I2CBus (C-14)`
- [x] **S40 · Compiler.** GCC-14-Warnungen in `src/` behoben (`++` auf `volatile` im Seqlock-Testhelfer, `|` zwischen LVGL-Teil- und Zustands-Enums → `partState()`, leere Host-`Lock`-Structs); beide Cores 0 Warnungen. Host-Env `native-cpp20` mit `-std=gnu++20 -Wextra` statt gnu++17, weil Arduino 3.x die Firmware als gnu++2a baut; läuft nur unter Linux/CI (MinGW von `windows_x86` ist GCC 5.1), lokal ersatzweise Syntax-Check mit Xtensa-GCC 14. CI baut beide Cores und testet in C++20. *Commit:* `S40: Fix GCC 14 warnings and add a C++20 host environment`
- [x] **S40a · LVGL 8.4.0.** `lvgl/lvgl@8.4.0` in beiden Envs (E-28). Template-Schlüssel und `lv_conf_internal.h` identisch; inhaltlich nur Bildschirm-Ladereihenfolge, Gruppen-Fokus und der Cast in `lv_obj_style.h` (beseitigt die C++20-Warnung aus dem Header); benutzte Widgets nur Leerzeichen. Erstes Bild 159 ms (2.x) / 169 ms (3.x), Heap und LVGL-Speicher gleich, Flash +72 B; beide Cores 0 Warnungen, auch in den Bibliotheken. *Commit:* `S40a: Update LVGL to 8.4.0`
- [x] **S41a · Vorprüfung.** S-16 geprüft: `IP_FORWARD` und `IPV4_NAPT` im vorkompilierten IDF an → Rückfrage 3, entschieden in S41b (D14). Q4 und Q5 unter Core 3 am CYD2USB bestanden (Log; CLK0 am OSZI offen, MESSPLAN). Beide Boards booten mit Core 3 und LVGL 8.4.0. *Commit:* `S41a: Pre-check the migration (S-16, Q4, Q5)`
- [x] **S41b · Smoke-Test und Routing.** Smoke-Test 4.3 Schritte 1 bis 4 auf beiden Boards mit Core 3 und LVGL 8.4.0 (`a32fb74`) am 2026-11-06 bestanden: Info-Karte, Erkennung `detected`, Touch, je ein Verbindungsdialog, Wiederverbinden, Speichern, alle Seiten, Halten wie 1.3.3; Schritte 5 und 6 offen (kein Messgerät). Neue Core-Meldungen beim Abziehen von SDA (`esp32-hal-i2c-ng.c:372`, `Wire.cpp:532`) im Handbuch. Rückfrage 3 entschieden: W-S3 strikt statt eigenem IDF-Build (D14, L-S6 mit ≤ 1 s). *Commit:* `S41b: Smoke test on core 3, decide S-16 routing`
- [ ] **S41c · Abnahme.** Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10, ≈ 4 h, braucht ZÄHLER und OSZI), dabei CLK0 bei Q4/Q5 und J6 (Code 5 bei Leitung auf GND); dann 2.x-Env entfernen (der 2.x-Zweig in `backlight.cpp`, `ESP_ARDUINO_VERSION_MAJOR` 2 im Fake und die Handbuchzeile `Wire.cpp:499` gehen mit), Spezifikation A-46, README, `CLAUDE.md`; Merge nach `dev` (lokal). *Commit:* `S41c: Switch to the official espressif32@8.0.0 platform`

### Phase 5 – WEB-HMI-Vorarbeiten (≈ 4 d, `feature/web-hmi`, in `dev` seit 2026-11-05)

- [x] **S42 · Ergebniscodes (V-1).** `Result {Ok, NoChange, OutOfRange, Invalid, Conflict, HwError}` für alle Setter; `set*` klemmen nicht mehr (Bereich → `OutOfRange`, Liste → `Invalid`, Spread in Integer → `Conflict`, Übertragungsfehler → `HwError`), nur `adjust*`/`stepBacklight` und das neue `adjustCorrection` (GUI) klemmen; `channel(3)` liefert einen leeren Kanal statt CH2 (S-12). Host-Tests für alle Codes. Branch `feature/web-hmi`.
- [x] **S43 · Änderungszähler (V-2).** `AppController::revision()` zählt Änderungen an gespeichertem Zustand, Ausgängen, CH3-Zustand, Fehlerflags, Verbindung und tatsächlicher Frequenz (nicht im Leerlauf, nicht beim Autosave); `Gui::update()` zeichnet darüber neu statt über die eigene Hardware-Signatur. Host-Test mit Verbindungsverlust und -rückkehr.
- [x] **S44 · Ereignisse (V-3).** `Event {code, category, source, channel, title, text}` an bis zu 3 Empfänger (`addListener`) statt `ErrorCallback`; Quelle `Touch`/`Remote`/`Internal` (`setSource`, Überwachung läuft als `Internal`); die GUI zeigt Dialoge nur für nicht entfernte Quellen und schließt Verbindungsdialoge über die Kategorie (`MessageQueue::dropTagged`) statt über Textvergleich; `isLinkText()` entfällt. Host-Tests.
- [x] **S45 · Speichern je Quelle und Schlüssel (V-4, C-17).** Dirty-Bit je NVS-Schlüssel (`SettingKey`), das Autosave schreibt nur geänderte Schlüssel; Ruhezeit je Quelle (Touch 5 s, Remote 30 s, `SETTINGS_REMOTE_AUTOSAVE_MS`); `saveNow()` (SYSTem:SAVE) schreibt alles; `resetForRemote()` (*RST): Standardwerte im RAM, Korrektur bleibt, nichts vorgemerkt, eine spätere Änderung speichert nur ihren Schlüssel. Host-Tests. Namespaces und String-API des Storage/Fakes nach S52 verschoben (erst `hfnet` braucht sie).
- [x] **S46 · Logger umschaltbar (V-5, C-16).** `Logger::setOutput(ToSerial | ToBuffer | Off)`; Ringpuffer mit 32 Zeilen à 96 Zeichen (älteste fallen heraus, Zähler `dropped()`), `readBuffered()` für `SYSTem:LOG?`; Mutex auf dem ESP32; ESP-IDF-Log (`esp_log_set_vprintf`) und LVGL-Log (`lv_log_register_print_cb`, `LV_LOG_PRINTF 0`) laufen über den Logger. Neue Suite `test_logger`. Arduino-`log_e` (CORE_DEBUG_LEVEL) schreibt weiter direkt auf die UART; vor SCPI über USB prüfen (S51).
- [x] **S47 · Ausführer und Schnappschuss (V-6).** `src/remote/command.*`: `Command` für alle Setter (Wert im vollen Typ geprüft, W-S19), `execute()` setzt die Quelle; `CommandQueue` (16, Mutex, Ergebnis je Ticket, voll → abgelehnt statt blockiert); `StateSnapshot` mit Seqlock (Zustand, Status, tatsächliche Frequenzen, Revision). `loop()` arbeitet die Queue ab und veröffentlicht bei neuer Revision. Neue Suite `test_command`; RAM +4 KB (94 488 B).
- [x] **S48 · Netz-Parser (V-11).** `FrequencyUtils::parseRemoteFrequency()`: Ganzzahl in Hz oder Dezimalzahl mit Einheit (`Hz`, `kHz`, `MHz`, `k`, `M`, jede Schreibweise), Komma als Dezimaltrenner, Leerzeichen nur zwischen Dreiergruppen und um die Einheit; kein Vorzeichen, Exponent oder Hex; Rechnung in Ganzzahlen mit Rundung auf 1 Hz; Bereich 8 kHz–160 MHz. Host-Tests mit 24 abgelehnten Eingaben.

### Phase 6 – WEB-HMI 1.4.0 (≈ 13,75 d)

- [ ] **S49 · Vorab-Messung WLAN (0,5 d).** Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis in Abschnitt 3. *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **S50 · Befehlskern (0,75 d).** `command`, `state_snapshot`, Quelle, W-E25; `test_command`. *Commit:* `S50: Add command core with source and remote-outputs option`
- [ ] **S51 · SCPI über USB (1,5 d).** `line_framer`, scpi-parser v2.3, Befehlsbaum 3.5, Logger-Umschaltung, `SYSTem:LOG?`; `test_scpi`, `test_line_framer`; PyVISA mit DTR/RTS aus, 1000 × `*IDN?`. *Commit:* `S51: Add SCPI over USB serial`
- [ ] **S52 · WLAN-Manager (1,5 d).** `Storage` mit String-API und Preferences-Fake mit mehreren Namespaces (aus S45); Betriebsarten, `WiFiMulti`, Rückfall-AP, `net_settings` (PMK, Zufallspasswort), mDNS, Captive-DNS nur im AP, kein Routing; `test_net_settings`. *Commit:* `S52: Add WiFi manager and network settings storage`
- [ ] **S53 · WLAN am Gerät (2 d).** Statusleisten-Symbole, CONFIG `WiFi` und `Remote outputs`, Seiten WIFI mit Tastatur, QR, Spinner, `sanitize`; Info-Karte mit WLAN, Heap und LVGL-Heap (V-8); `test_sanitize`. *Commit:* `S53: Add WiFi setup pages with keyboard and QR`
- [ ] **S54 · SCPI über TCP (0,5 d).** Port 5025 mit HTTP-Erkennung, Leerlauf-Timeout, eine Verbindung, mDNS `_scpi-raw._tcp`. *Commit:* `S54: Add SCPI over TCP`
- [ ] **S55 · Webserver (2 d).** `request_guard`, REST, WebSocket, Login, Header, CSP; `test_http_guard`, `test_rest_json`. *Commit:* `S55: Add guarded web server and REST API`
- [ ] **S56 · Weboberfläche (2,5 d).** Alle Bereiche, Tastatur, responsiv, Live-Hervorhebung, zweistufige Netzänderung, Identify. *Commit:* `S56: Add web user interface`
- [ ] **S57 · Fuzzing und Dauertest (1 d + Laufzeit).** libFuzzer-CI-Job (≥ 10 Mio.), Zufallstest mit festem Seed; 24-h-Lauf mit Heap- und Stack-Bilanz. *Commit:* `S57: Add fuzzing job and long-run test`
- [ ] **S58 · Doku und Release-Kandidat (1,5 d).** Handbuch DE/EN (WLAN, Web, SCPI-Referenz, REST, Python-Beispiel, Sicherheitshinweise, Fernbetrieb und Antenne), Spezifikation, README, `CLAUDE.md`, diese Datei; Version 1.4.0, lokaler Tag `v1.4.0-rc1`; Smoke-Test einschließlich L1 und `*IDN?`. *Commit:* `S58: Document WEB-HMI, release candidate 1.4.0-rc1`

### Phase 7 – Messungen ([MESSPLAN.md](MESSPLAN.md))

Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` (ohne Medien); Befunde (M-n) sofort in Abschnitt 2 eintragen. Stufe 1 und 2 laufen **sobald Zähler und Oszilloskop verfügbar sind** (auch vor Phase 4); dieser Lauf ist die Referenz R0 für den Vergleich nach der Migration (MESSPLAN 9.2).

- [ ] **S59 · Stufe 1 (Pflicht, ≈ 7 h).** MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10). *Commit:* `S59: Record tier-1 measurements`
- [ ] **S60 · Stufe 2 (Entscheidungen, ≈ 6 h).** MESSPLAN Abschnitt 6: Standard-Tuning (K5, F3, K6), IQ ohne Reset (D3, D5, E9), ungerade Teiler und Phasenfehler (E3, E4, E5), Störquellen und PDN (F6, F7), Pegel (F2), Drift (C7); Entscheidungen als E-n in die Spezifikation, N1–N8 bestätigen oder streichen. *Commit:* `S60: Record tier-2 measurements and decisions`
- [ ] **S61 · Stufe 1W (Pflicht ab WEB-HMI, ≈ 8 h + 24 h).** MESSPLAN 5.11 nach S58 auf beiden Boards; dazu L13, L15, Q1 aus Stufe 2. *Commit:* `S61: Record network and security measurements`
- [ ] **S62 · Stufe 3 (optional).** Charakterisierung und Bedientests mit Personen nach Bedarf (MESSPLAN Abschnitt 7). *Commit:* `S62: Record optional characterization measurements`
- [ ] **S63 · Handbuchwerte.** Pegeltabelle (F2), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise; DE und EN. *Commit:* `S63: Add measured values to the manuals`
- [ ] **S64 · Freigabe 1.4.0.** Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag `v1.4.0`; Merge nach `dev` und `main` nur auf Anweisung. *Commit:* `S64: Release 1.4.0`

### Phase 8 – Folgearbeiten (je ein Commit, nach Messergebnis oder Rückfrage)

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
- [x] **N12 · Feste Koordinaten** durch benannte Konstanten ersetzt (Karte der Kanal- und CH3-Seite und Header-`MAIN` in `gui_layout.h`, Chips/Tasten/Numpad als lokale Konstanten). Geprüft: Maschinencode von `gui.cpp`, `page_channel.cpp`, `page_ch3.cpp`, `page_numpad.cpp` vorher und nachher identisch.
- [ ] **N13 · DMA und Doppelpuffer** (Abschnitt 2.3, nach der Migration).
- [ ] **N14 · Erweiterungen nach Rückfrage 1:** Speicherplätze, Wobbeln, Drive je Kanal, gekoppelter Betrieb CH2 = CH1 ± Offset (Überlagerungsempfänger mit fester ZF), Modulationsarten (AM/FM über den PLL-Bruchteil), externe Referenz (SI5351C mit `CLKIN`), Kalibrierassistent (falls nicht in S32).

---

## 5. Pflege dieser Datei

- Der Haken eines Schritts steht im selben Commit wie die Änderung; erledigte Schritte und Befunde werden beim nächsten Aufräumen entfernt (Abschnitt 1.1 fasst sie zusammen).
- Neue Befunde (M-n aus Messungen, B-n aus Bedientests) kommen mit Bezug in Abschnitt 2 und als Schritt in Abschnitt 4; bestehende Schrittnummern bleiben stabil.
- Neue Entscheidungen mit Datum in Abschnitt 1.3; dauerhafte Entscheidungen zusätzlich als E-n in der Spezifikation.
