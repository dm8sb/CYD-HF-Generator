# Konzept: WEB-HMI (WLAN, Weboberfläche, SCPI-Fernsteuerung)

Stand: 2026-11-03, überarbeitet nach [AUDIT_2026-11.md](AUDIT_2026-11.md) · Basis: Firmware 1.3.0 (Commit `8ab70bc`) · Zielversion **1.4.0** · Status: **Konzept, Anforderungen und Sicherheitsanforderungen geklärt**

Voraussetzungen:
- Phase M: Migration auf Arduino-Core 3.x (Abschnitt 4).
- Phase V: Vorarbeiten im bestehenden Code (Abschnitt 5).
- Phasen 0–3 des Audits (Robustheit, Bedienung, Messungen) sollen davor abgeschlossen sein.

## 1. Ziel

1. **WLAN:** Das Gerät bucht sich über die CONFIG-Seite in ein bestehendes WLAN ein (Client) oder spannt ein eigenes WLAN auf (Access Point). Standard: WLAN aus.
2. **Weboberfläche:** Alle Funktionen des Touch-Bildschirms im Browser (PC, Tablet, Handy), zusätzlich mit Tastaturbedienung.
3. **Fernsteuerung:** Alle Funktionen über **SCPI** (TCP-Port 5025 und USB-Seriell) sowie über eine dokumentierte REST-API.
4. **Sicher im Laborbetrieb:** Kein Mitlesen von WLAN-Passwörtern, kein Fernsteuern über fremde Webseiten, kein ungewolltes Senden. Die Bedienung am Gerät hat immer Vorrang.

## 2. Entscheidungen

„(neu)“ bzw. „(geändert)“ = nach dem Audit am 2026-11-03 mit dir entschieden. Verweise: `S-n`, `U-n`, `C-n` → Befunde in [AUDIT_2026-11.md](AUDIT_2026-11.md).

| # | Thema | Entscheidung |
|---|---|---|
| W-E1 | Betriebsarten | `Off` (Standard) / `Client` / `Access point`. Ein Client ohne Verbindung startet nach 60 s zusätzlich den Access Point und sucht weiter. Ist der AP auf Wunsch offen geschaltet, startet der Rückfall-AP nur nach Bestätigung am Touch (geändert, S-3). |
| W-E2 | Zugangsdaten eingeben | am Gerät (Scan-Liste und Bildschirmtastatur) oder per Handy über den Access Point. Passwörter nimmt das Gerät über einen **offenen** AP nie an (geändert, S-1). |
| W-E3 | AP-Passwort | **Zufälliges WPA2-Passwort je Gerät** (12 Zeichen ohne verwechselbare Zeichen), erzeugt beim ersten Einschalten des AP und im NVS gespeichert. Es wird am Display als Text und als **QR-Code** zum Beitreten angezeigt und lässt sich neu erzeugen oder frei wählen. „Offen“ nur auf ausdrücklichen Wunsch, mit Warnhinweis (geändert, S-2). |
| W-E4 | IP-Adresse | DHCP; statische IP optional, mit Plausibilitätsprüfung (S-15) |
| W-E5 | Gerätename | frei wählbar, Standard `cyd-hf-<4 Stellen MAC>`; erlaubt sind nur Zeichen nach RFC 1123 (a–z, 0–9, `-`, max. 32). Er gilt für mDNS (`<name>.local`), den DHCP-Hostnamen und die AP-SSID `CYD-HF-<4 Stellen MAC>` (S-26). |
| W-E6 | WPA2-Enterprise | nein. **Captive-Portal-DNS nur im AP-Modus: ja.** Jeder Name zeigt auf das Gerät, das Handy öffnet die Einrichtungsseite selbst (geändert, Audit Abschnitt 3.6). |
| W-E7 | Gespeicherte WLANs | höchstens zwei; das stärkste gewinnt, bei Misserfolg das andere. Mindestsicherheit WPA2 bei Netzen mit Passwort, PMF „capable“; Warnung beim Speichern eines offenen Netzes (S-28). |
| W-E8 | Zugriffsschutz Web/REST | Passwort optional, Standard ohne. Ist eines gesetzt: Login mit Sitzungs-Cookie (kein Basic-Auth), Passwort nur gesalzen gehasht gespeichert, Sperre nach Fehlversuchen (geändert, S-9). |
| W-E9 | Umfang Weboberfläche | Alle Funktionen inkl. CONFIG und WLAN, außer Boot-Geste und Option W-E25. Netzänderungen sind zweistufig: Bestätigung über die neue Verbindung binnen 120 s, sonst automatischer Rückfall. Ohne Web-Passwort zusätzlich Bestätigung am Touch (geändert, S-15). Keine Funktionen über den Touch hinaus, aber Tastaturbedienung. |
| W-E10 | Web-Clients | höchstens 3 gleichzeitig, live synchron; ein vierter bekommt eine Fehlerseite |
| W-E11 | Sprache, Layout, Ressourcen | Englisch, responsiv, nur lokale Ressourcen |
| W-E12 | Fernsteuerprotokoll | SCPI (IEEE 488.2 / SCPI-99, eigener Befehlsbaum, Abschnitt 8) über TCP 5025 und USB-Seriell; Discovery über mDNS `_scpi-raw._tcp`. Kein VXI-11, HiSLIP, MQTT und keine Bode-Emulation. |
| W-E13 | SCPI-Verbindungen | Eine Verbindung; eine zweite wird abgelehnt. Leerlauf-Timeout 10 min. „Disconnect“ am Gerät (neu, S-17). |
| W-E14 | Bedienung während Fernsteuerung | uneingeschränkt, „der Letzte gewinnt“. Statusleiste zeigt `REMOTE <Quelle>` (geändert, S-18). |
| W-E15 | Ereignisse | Polling und `*OPC?`; Live-Push nur per WebSocket |
| W-E16 | Verbindungsabbruch | Ausgänge laufen wie zuletzt eingestellt weiter |
| W-E17 | Speichern | Touch und Web nach 5 s Ruhe, SCPI nach 30 s Ruhe; `SYSTem:SAVE` speichert sofort. Nur geänderte Schlüssel werden geschrieben; Ein/Aus löst kein Speichern aus (geändert, C-5, C-17). |
| W-E18 | Grenzen | identisch zum Touch. Jeder Wert wird im vollen Typ geprüft und bei Bedarf abgelehnt, nie still abgeschnitten (geändert, S-12). |
| W-E19 | `*RST` | Ausgänge aus und Standardwerte im RAM. Die Werte gelten bis `SYSTem:SAVE` als **nicht** zu speichern; eine spätere Änderung speichert nur ihren eigenen Schlüssel (präzisiert, C-17). |
| W-E20 | HF-Reinheit | WLAN in CONFIG abschaltbar, Standard aus |
| W-E21 | Ohne SI5351 | Web und SCPI funktionieren, Kanäle `NO HW` |
| W-E22 | OTA, NTP | nein (OTA als spätere Option) |
| W-E23 | Doku | SCPI-Referenz, REST-API, Python-Beispiel; Verweise auf PyVISA, LabVIEW, NI-MAX, MATLAB; Sicherheitshinweise |
| W-E24 | Plattform | Vor WEB-HMI Migration auf Core 3.x. Ziel ist **offizielles `platformio/espressif32@8.0.0`** (Arduino 3.3.12, IDF 5.5.5), pioarduino nur als Ausweichlösung. Heap wird laufend überwacht (geändert, P-2). |
| W-E25 | Fernschalten der Ausgänge | CONFIG-Option `Remote outputs: allowed / local only`, Standard `allowed`; nur am Gerät änderbar. Bei `local only` dürfen Netz-Clients alles einstellen, aber keinen Ausgang einschalten (Ausschalten bleibt erlaubt) (neu, S-18). |
| W-E26 | Statusleiste auf MAIN | Die Tasten CH1–CH3 unten auf MAIN entfallen (die Karten öffnen die Kanäle). An ihrer Stelle steht eine Statusleiste mit Symbolen für WLAN/AP/REMOTE/Speichern und der Taste CONFIG (neu, U-8, Audit 3.2). |
| W-E27 | SCPI über USB | Solange eine SCPI-Sitzung über USB aktiv ist, gibt der Logger nichts auf `Serial` aus (Ringpuffer statt Ausgabe; abrufbar über `SYSTem:LOG?`) (neu, C-16). |
| W-E28 | Identify | Web-Taste und `SYSTem:IDENtify` lassen Backlight und LED 5 s blinken. Das hilft im Raum mit mehreren Geräten (neu, U-WEB). |

## 3. Ausgangslage (Stand 1.3.0)

| Thema | Befund | Folge |
|---|---|---|
| Funk | ESP32: WLAN 2,4 GHz, `WIFI_AP_STA` möglich | Rückfall-AP ohne Unterbrechung der Suche |
| Flash | 623 KB von 3 MB; Core 3.x ≈ +60–250 KB, WLAN/Web ≈ +600–800 KB | passt; Partition `spiffs` (896 KB) für LittleFS |
| RAM | statisch 27,5 %; WLAN ≈ 50–70 KB Heap, je Verbindung ≈ 5–10 KB | passt mit den Grenzen in 10.3; Heap zur Laufzeit wird heute nirgends gemessen (Vorarbeit V-8) |
| Architektur | ein Zustandshalter (`AppController`), Status als Enums, Planung als reine Funktionen | gute Basis für einen gemeinsamen Befehlskern |
| Was fehlt (Code-Audit) | Setter klemmen still; Fehler als Freitext über einen Funktionszeiger; kein Änderungszähler; ein Speicher-Zeitgeber, der immer alles schreibt; Logger fest auf `Serial` | Vorarbeiten V-1–V-11 (Abschnitt 5) |
| Nebenläufigkeit | `AppController`, LVGL, I2C nur im `loop()`-Task | Warteschlange + Schnappschuss (6.2) |
| Bildschirmtastatur | `LV_USE_KEYBOARD 0`, LVGL-Heap 48 KB, Malloc-Assert endet heute in `while(1);` | Tastatur auf eigener Seite, Assert-Handler mit Neustart (C-3, S-14) |
| Plattform | Core 2.0.16 / IDF 4.4.7 / mbedTLS 2.28.7, alle ohne Support | Migration M (Abschnitt 4) |

## 4. Phase M: Migration auf Arduino-Core 3.x

**Begründung:**
- ESP-IDF 4.4 und mbedTLS 2.28 werden nicht mehr gepflegt. Für das WLAN-Feature fehlen damit Fixes, zum Beispiel für den DHCP-Server (CVE-2026-45160, behoben in IDF 5.5.5).
- Die Web-Bibliotheken liefen auch auf Core 2.x; sie sind nicht der Grund für die Migration.

| Bereich | Änderung | Prüfung |
|---|---|---|
| Plattform | **Spike (0,5 d)**: `platform = espressif32@8.0.0` gegen `pioarduino 55.03.312-1`, beide exakt gepinnt. pioarduino nur, falls `custom_sdkconfig` nötig ist. | Build, Flash/RAM/Heap gegen die Basis aus Audit-Phase 0 |
| Übergang | beide Cores als eigene Umgebungen (A/B), bis die Abnahme steht; danach die 2.x-Umgebung entfernen | – |
| LEDC (Backlight) | `ledcAttach(pin, freq, bits)`, `ledcWrite(pin, duty)` | Fake in `test/fakes/Arduino.h` mitziehen |
| Wire | IDF 5.4+: neuer `i2c_master`-Treiber. Alle Transfers in `I2CBus` bündeln (`writeRegs`/`readRegs`, C-14). Die Annahmen aus 2.0.x stehen als benannte Fake-Tests fest und werden gegen den 3.x-Quelltext geprüft: Rückgabecode 5, `end()`/`begin()`, Lock. | Messplan J1–J4, J6, J9, J14 wiederholen |
| TFT_eSPI | 2.5.43 bleibt (für den klassischen ESP32 laut Diff unkritisch, Build ungetestet); bei Problemen gepinnter Fork, LovyanGFX nur als letzter Ausweg (eigene Display-Erkennung bleibt) | Messplan I1–I3, I7 |
| Compiler | GCC 14: `-Wall -Wextra` für `src/` muss sauber bleiben; zusätzliche Host-Test-Umgebung mit `-std=gnu++17` | CI |
| lwIP | `CONFIG_LWIP_IP_FORWARD`/`IPV4_NAPT` stehen in den Core-Libs auf `y`; NAPT wird nie aktiviert (Regel in 6.2, Test L-S6) | S-16 |
| LVGL, XPT2046 | unverändert (8.3.11, Commit `d57f64c…`) | Messplan B |

**Abnahme:**
- Host-Tests grün in `native-win`, `native` und der gnu++17-Umgebung.
- 0 Warnungen in `src/`.
- Auf beiden Boards: Messplan A1–A7, A10, B, I1–I3, I7 und J, verglichen mit der Basis.

**Aufwand:** ≈ 2,5–3 d.

## 5. Phase V: Vorarbeiten im bestehenden Code

Diese Vorarbeiten verbessern auch ohne WEB-HMI Fehlerbehandlung und Testbarkeit. Alle Punkte werden auf dem Host getestet.

| # | Vorarbeit | Inhalt | Aufwand |
|---|---|---|---|
| V-1 | Ergebniscodes | Setter liefern `Result {Ok, OutOfRange, Invalid, Conflict, NoChange, HwError}`, statt still zu klemmen. Klemmen gibt es nur noch in `adjust*`. Der Kanalindex wird streng geprüft (`channel(3)` liefert heute CH2). | 1 d |
| V-2 | Änderungszähler | `AppController::revision()` zählt Änderungen an Zustand, Status, `fault_`, Verbindung und `actualHz`. Darüber laufen GUI-Neuzeichnen (behebt auch C-2), Schnappschuss und WebSocket-Push. | 0,5 d |
| V-3 | Ereignisse | `Event {code, category, source}` mit mehreren Empfängern (GUI-Dialog, SCPI-Fehlerwarteschlange, WebSocket) statt `ErrorCallback(title, text)`; `isLinkText()` entfällt. Fehler aus Fernbefehlen öffnen am Gerät **keinen** Dialog, sie landen in der SCPI-Fehlerwarteschlange bzw. der HTTP-Antwort. | 0,5 d |
| V-4 | Speichern | `markDirty(key, Source)` mit Frist 5 s / 30 s; Dirty je Schlüssel mit Schattenkopie; `saveNow()`; Retry mit Backoff (C-13); `*RST`-Semantik W-E19. `Storage` mit String-/Blob-API, Fake mit mehreren Namespaces. | 1 d |
| V-5 | Logger | Ausgabe umschaltbar (Serial / Ringpuffer / aus), mit Mutex; `lv_log_register_print_cb` und `esp_log_level_set` auf den Logger (W-E27). | 0,5 d |
| V-6 | Ausführer | Touch-Aktionen laufen synchron über denselben Ausführer, Netz-Tasks über die FreeRTOS-Queue (begrenzt, 16 Einträge). Der Schnappschuss wird mit Sequenzzähler kopiert, weil uint64/double nicht atomar sind. | 0,5 d |
| V-7 | I2C bündeln | siehe Phase M | (M) |
| V-8 | Heap-Basis | freier, minimaler und größter freier Block, `lv_mem_monitor()`, Stack-Reserve; Debug-Log und Info-Karte. `LV_ASSERT_HANDLER` → Log + `esp_restart()`, `enableLoopWDT()` (C-3, S-23). | 0,5 d (Audit-Phase 0/1) |
| V-11 | Parser | gehärteter Frequenzparser für REST/SCPI: Ganzzahl in Hz oder Dezimalzahl mit Einheit, `isfinite`, keine Exponenten/Hex (C-4). Komma und Leerzeichen werden akzeptiert (`10,7 MHz`, `7 100 000`). | 0,5 d |

**Summe ≈ 4 d** (ohne V-7/V-8, die zu M bzw. Audit-Phase 0/1 gehören).

## 6. Architektur

### 6.1 Komponenten

```
 Touch/LVGL ───────────── Ausführer (synchron) ──┐
 Web-UI ─┐                                       │
 REST ───┼─► RequestGuard ─► CommandQueue ───────┼─► AppController ─► SI5351 / NVS
 SCPI TCP 5025 ─► LineFramer ─► scpi ─┘          │
 SCPI USB-Seriell ─► LineFramer ─► scpi ─┘       │
 Abfragen (Web, REST, SCPI) ◄── StateSnapshot ◄──┘  (Sequenzzähler, revision())
```

| Modul (neu) | Aufgabe |
|---|---|
| `net/wifi_manager.*` | Betriebsarten, `WiFiMulti`, Rückfall-AP, statische IP, Gerätename, mDNS, Captive-DNS (nur AP), AP aus beim STA-Connect |
| `net/net_settings.*` | NVS-Namespace `hfnet`: Modus, 2 × SSID/PSK, AP-SSID und -Passwort, Gerätename, statische IP, Web-Passwort-Hash, `remote_outputs`. Gespeichert wird bei WPA2 der 64-Hex-PMK statt der Passphrase (WPA3-SAE braucht die Passphrase), dazu `WiFi.persistent(false)` (S-8). |
| `net/request_guard.*` | **reine Funktionen, host-testbar**: Host-Allowlist, Origin/Sec-Fetch-Site, Content-Type, Token, Sitzung, Rate-Limits, Größen (Abschnitt 10) |
| `net/sanitize.*` | fremde Zeichenketten (SSIDs, Namen) → gültiges ASCII/UTF-8 ohne Steuerzeichen, ≤ 32 B, für LVGL und JSON (S-7, S-14) |
| `remote/command.*` | Befehlsstruktur, Quelle (Touch/Web/SCPI/USB), Ausführung über `AppController`, `Result` zurück |
| `remote/state_snapshot.*` | Zustandskopie für Abfragen; Push-Auslöser |
| `remote/line_framer.*` | **reine Funktion**: Zeilen ≤ 256 B (Überlänge bis `\n` verwerfen, Fehler −363), höchstens 16 Befehle je Zeile, NUL/Nicht-ASCII verwirft die Zeile, auf TCP Erkennung von HTTP-Zeilen (S-6, S-11) |
| `remote/scpi.*` | Befehlsbaum auf Basis von **j123b567/scpi-parser** v2.3 (BSD, per Commit gepinnt; Beispielcode der Bibliothek nicht übernehmen, Issue #174) |
| `remote/scpi_server.*` | TCP 5025 (eine Verbindung, Timeout, Keepalive) und USB-Seriell |
| `web/web_server.*` | **ESP32Async/ESPAsyncWebServer 3.12.1** + **AsyncTCP 3.5.0** (exakt gepinnt): LittleFS (gzip), REST, WebSocket `/ws`, Sicherheits-Header; nie im `async_tcp`-Task blockieren (pausierte Requests) |
| `data/` (LittleFS) | Weboberfläche: eine Seite ohne Framework, JavaScript nur als Datei (CSP), gzip ≈ 30–60 KB |
| JSON | **ArduinoJson 7.4.3** (enthält einen Sicherheitsfix), Verschachtelungslimit 10 |
| GUI | MAIN-Statusleiste (W-E26), CONFIG-Zeilen `WiFi` und `Remote outputs`, Seiten WIFI (Unterseiten, kein Scrollen), Tastatur, QR, Info-Karte |

### 6.2 Regeln

- **Nebenläufigkeit:** Kein Netzwerk-Task ruft `AppController`, LVGL, I2C oder NVS direkt auf. Schreibbefehle laufen über die Queue, Abfragen über den Schnappschuss.
- **Antworten:** SCPI wartet auf Quittungen höchstens 1 s. HTTP-Handler antworten asynchron und blockieren nie den Netzstack.
- **Gleichberechtigung:** Touch, Web und SCPI sind gleichberechtigt (W-E14). Jede Änderung erscheint sofort überall.
- **Fremde Webseiten:** Eingehende Anfragen prüft der `RequestGuard` vor jeder Ausführung.
- **Zeichenketten aus dem Funk:** SSIDs und ähnliche Texte erreichen LVGL und JSON nur über `sanitize`.
- **WLAN aus:** kein Netz-Task, Modem aus (`WIFI_OFF`), Verhalten wie 1.3.0; SCPI über USB bleibt verfügbar.
- **Passwörter:** werden nie ausgegeben (Web, REST, SCPI, Log, mDNS). Die Oberfläche zeigt nur „gesetzt / nicht gesetzt“.
- **Routing:** Kein Weiterleiten zwischen AP und STA. NAPT bleibt aus, der AP wird im selben Event abgeschaltet, in dem STA eine IP bekommt, und der DHCP-Server vergibt kein Gateway (S-16).
- **Sicherheitslogik:** `request_guard`, `sanitize`, `line_framer`, SCPI-Kern und JSON-Abbildung bleiben frei von Arduino- und Webserver-Abhängigkeiten. Sie laufen im `native`-Env und werden dort gefuzzt.

## 7. WLAN am Gerät

### 7.1 Bedienung

- **MAIN-Statusleiste (W-E26), unten:**
  - links Symbole für WLAN (Signalstärke) bzw. `AP`, `REMOTE <Quelle>`, Speicherpunkt und später `LOCK`;
  - rechts die Taste `CONFIG`.
  - Die Symbole unterscheiden sich in der Form, nicht nur in der Farbe (U-19).
- **CONFIG:**
  - `WiFi`: `Off` / `Client` / `Access point` und Taste `Setup…`;
  - `Remote outputs`: `allowed` / `local only` (W-E25).
- **Seite WIFI** (Unterseiten statt Scrollen):
  - **Networks:** zwei Einträge.
    - `Scan` zeigt einen Spinner und dann höchstens 15 gefundene Netze, bereinigt und ohne Doppelte.
    - Eine Zeile „Other…“ dient für versteckte SSIDs.
  - **Password:** eigene Vollbildseite mit Tastatur (≈ 190 px hoch), Taste „anzeigen“ und Zeichenzähler. Hinweis: ä, ö, ü, ß und € nur per Handy.
  - **Access point:** Name, Passwort (`Regenerate`, `Set…`, `Open (not recommended)`) und **QR-Code „Join“** groß. Darunter die URL als Text und als QR.
  - **Device:** Gerätename, IP (DHCP/statisch), Web-Passwort.
- **Rückmeldungen im Klartext:**
  - `Connecting…`
  - `Connected 192.168.1.50`
  - `Wrong password`
  - `Network not found`
  - `Fallback: AP active`
- **Info-Karte:** WLAN-Modus, SSID, RSSI, IP, `<name>.local`, SCPI-Status (`connected <IP>` / `rejected <IP>`), Web-Clients, Heap, LVGL-Heap. Dazu die Taste `Disconnect all` für SCPI und Web (S-10, S-17).

### 7.2 Verhalten

- **Client:** `WiFiMulti` mit Mindestsicherheit WPA2 verbindet mit dem stärkeren Netz. Ohne Verbindung startet nach 60 s der AP (WPA2), die Suche läuft weiter. Mit Verbindung schaltet sich der AP sofort ab.
- **Offener AP (nur auf Wunsch):**
  - Die Statusleiste zeigt `AP open` in Warnfarbe.
  - Ohne Station ist der AP nach 15 min aus; höchstens 2 Stationen.
  - WLAN- und Web-Passwortfelder lehnt der Server ab (403).
  - SCPI über TCP ist auf dem AP aus.
  - Der Rückfall-AP startet nur nach Bestätigung am Touch.
- **Captive-DNS:** nur im AP-Modus, beantwortet alle Namen mit der AP-Adresse.
- **WLAN aus:** Das Modem ist aus, alle Verbindungen sind getrennt.

### 7.3 Heap-Überwachung

- **Werte:** freier und minimaler Heap, größter Block und LVGL-Heap auf der Info-Karte und über `SYSTem:HEAP?`.
- **Warnung**, wenn der minimale freie Heap unter 40 KB fällt oder der größte Block unter 16 KB.
- **Schutz:** Unter 24 KB freiem Heap werden neue Verbindungen abgelehnt (HTTP 503, SCPI schließen). Die Bedienung am Gerät hat immer Vorrang.
- **Abnahme:** 24-h-Dauertest mit Last, siehe L-S5.

## 8. SCPI-Befehlsbaum

- **Schreibweise:** Kurzform in Großbuchstaben; `n` = Kanal 1–3, Standard 1.
- **Zahlen:** Ganzzahl in Hz oder Dezimalzahl mit `HZ`/`KHZ`/`MHZ`, dazu `MIN`/`MAX`/`DEF`. `NAN`, `INF`, Exponenten außerhalb des Bereichs und Hex werden mit −222/−224 abgelehnt.
- **Antworten** in Grundeinheiten.

| Befehl | Abfrage | Bedeutung |
|---|---|---|
| `*IDN?` | `CYD,HF-Generator,<MAC>,1.4.0` | Identifikation |
| `*RST` | – | W-E19 |
| `*CLS`, `*ESR?`, `*ESE`, `*STB?`, `*SRE`, `*OPC`, `*OPC?`, `*WAI`, `*TST?` | | IEEE 488.2 |
| `OUTPut<n>[:STATe] ON\|OFF\|1\|0` | `OUTPut<n>?` | Ausgang (CH3 = Anforderung); bei `Remote outputs: local only` → −221 „Settings conflict“ für ON |
| `OUTPut<n>:STATus?` | `ON`/`OFF`/`ERR`/`WAIT`/`NOHW` | tatsächlicher Zustand |
| `[SOURce<n>:]FREQuency[:CW] <f>` | `FREQ?` | Sollfrequenz CH1/CH2; CH3 nur Abfrage |
| `[SOURce<n>:]FREQuency:ACTual?` | Hz mit 0,1 Hz | tatsächliche Frequenz wie die Zeile `Out` (bei Spread der Mittelwert) |
| `SOURce3:IQ:MASTer 1\|2`, `SOURce3:PHASe 0\|90\|180\|270`, `SOURce3:IQ:RANGe?` | `?` | CH3 |
| `OUTPut:DRIVe 2\|4\|6\|8`, `OUTPut:OFFState LOW\|HIGH\|HIZ` | `?` | Ausgänge |
| `OUTPut:REMote?` | `ALLOWED`/`LOCAL` | Option W-E25 (nur lesbar) |
| `[SOURce1:]SSC OFF\|DOWN05\|DOWN15\|CENT05`, `SYSTem:TUNing FRAC\|LNO\|INT` | `?` | Signal |
| `SYSTem:CORRection <ppm>`, `SYSTem:STEP <Hz>`, `SYSTem:BACKlight <%>` | `?` | Einstellungen |
| `SYSTem:SI5351?`, `SYSTem:HEAP?` | | Diagnose |
| `SYSTem:IDENtify` | – | Identify (W-E28) |
| `SYSTem:LOG?` | Ringpuffer-Zeilen | Log während SCPI-USB (W-E27) |
| `SYSTem:SAVE` | – | sofort speichern |
| `SYSTem:ERRor[:NEXT]?`, `SYSTem:VERSion?` | | Fehlerwarteschlange, `1999.0` |

Nicht über SCPI änderbar sind die Netzwerkeinstellungen und `Remote outputs`. Sonst könnte sich eine Verbindung selbst abschneiden oder ihre eigene Sperre aufheben.

## 9. Weboberfläche und REST-API

### 9.1 Oberfläche

- **Bereiche:** MAIN (drei Karten), CH1, CH2, CH3, CONFIG, WiFi; auf dem Handy untereinander mit einer fixierten Leiste für den Zustand der Ausgänge. Tasten ≥ 44 CSS-px, `touch-action: manipulation`.
- **Frequenzeingabe:** direkte Eingabe (`10.7M`, `455k`, `7 100 000`, `10,7 MHz`). Bei einem Fehler bleibt die Eingabe stehen, und der Fehlertext erscheint am Feld.
- **Live-Updates:**
  - Geänderte Felder werden kurz hervorgehoben.
  - Ein Feld mit Eingabefokus wird nie überschrieben.
  - Bricht die Verbindung ab, wird die Seite grau.
- **Sondermodi sichtbar wie am Gerät:** Spread, Integer, Low noise, Drive, Off state ≠ Low (U-2).
- **Tastatur** (geändert, Audit 3.5):

  | Taste | Wirkung |
  |---|---|
  | `↑` / `↓` (auch Ziffernblock `+`/`−`) | Frequenz in der aktuellen Schrittweite |
  | `←` / `→` | markierte Ziffer, also Schrittweite (`←` = größer) |
  | `Bild↑` / `Bild↓` | Schrittweite × 10 / ÷ 10 |
  | Mausrad über einer Ziffer | diese Stelle verstellen |
  | `1` / `2` / `3` (nur ohne Eingabefokus) | Kanal wählen |
  | `F` oder `Enter` | Frequenzfeld fokussieren; `Enter` übernimmt, `Esc` verwirft |
  | `O` | gewählten Kanal ein/aus (nicht `Space`) |
  | `?` | Tastenbelegung anzeigen |

- **Netzänderungen:** zweistufig nach W-E9, mit Countdown „Confirm within 120 s, otherwise the previous settings return“.
- **Weitere Funktionen:** `Identify`-Taste; Login-Seite nur bei gesetztem Web-Passwort. Ein vierter Client bekommt die Seite „Too many clients (max. 3)“.

### 9.2 REST (dokumentiert im Handbuch)

| Methode und Pfad | Inhalt |
|---|---|
| `GET /api/state` | Zustand als JSON. Ohne Passwort zeigt er keine gespeicherten SSIDs anderer Netze, nur die verbundene; MAC und Commit erst nach Login, sofern ein Passwort gesetzt ist (S-26). |
| `POST /api/channel/<n>` | `{"enabled":true,"frequency":10000000}` |
| `POST /api/ch3` | `{"master":1,"phase":90}` |
| `POST /api/config` | `{"drive":8,"tuning":"lownoise","spread":"off","offState":"low","correction":0,"step":1000,"backlight":80}` |
| `GET/POST /api/wifi`, `POST /api/wifi/confirm` | Netzwerkeinstellungen (Passwörter nur schreibend, über offenen AP gesperrt), zweistufige Bestätigung |
| `GET /api/wifi/scan` | Scan, höchstens alle 10 s (Zwischenspeicher) |
| `POST /api/save`, `POST /api/identify`, `POST /api/login` | |
| WebSocket `/ws?t=<token>` | Push des Zustands (nur Push, keine Befehle) |

- **Fehler:** HTTP 400 mit `{"error":"-222,Data out of range"}`, also denselben Codes wie SCPI. Unbekannte Schlüssel ergeben 400, falsche Typen ebenfalls (`{"drive":"4"}`, `{"drive":4.5}`).
- **Token:** Die Seite erhält ein Token je Boot im HTML-Kopf; REST-POST verlangt es als Header `X-CYD-Token`. Für Skripte steht es auf der Info-Karte und unter `GET /api/token` (nur same-origin).

## 10. Sicherheitsanforderungen (W-S)

Prüfbar; Tests in Abschnitt 12. Bezug zu den Audit-Befunden in Klammern.

| Nr | Anforderung |
|---|---|
| W-S1 | Über einen **offenen** AP lehnt der Server WLAN- und Web-Passwortfelder ab (403). Standard ist der WPA2-AP mit Zufallspasswort (W-E3). (S-1, S-2) |
| W-S2 | Ein offener Rückfall-AP startet nur nach Bestätigung am Touch. Ein offener AP ist ohne Station nach 15 min aus, nimmt höchstens 2 Stationen an, und die Statusleiste zeigt `AP open`. (S-2, S-3) |
| W-S3 | Kein Routing zwischen AP und STA (NAPT aus, AP aus bei STA-IP, DHCP ohne Gateway-Option). (S-16) |
| W-S4 | Host-Allowlist für HTTP und WebSocket: eigene IPv4-Adressen, `<name>.local`, `<name>`, im AP-Modus zusätzlich für das Captive-DNS. Sonst 421. IPv6 bleibt aus. (S-5) |
| W-S5 | POST nur mit `application/json` (sonst 415), gültigem `X-CYD-Token` (sonst 403), `Origin` leer oder gleich dem eigenen Host, `Sec-Fetch-Site` nicht `cross-site`. Keine `Access-Control-Allow-*`-Header, `OPTIONS` abgelehnt, GET ohne Wirkung. (S-4) |
| W-S6 | WebSocket-Upgrade mit Host-, Origin- und Token-Prüfung; nur Push; Nachrichten ≤ 512 B; Ping alle 15 s, Trennen nach 30 s ohne Pong. (S-10) |
| W-S7 | Header auf jeder Antwort: CSP `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`, `X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer`; `/api/*` mit `Cache-Control: no-store`. (S-7) |
| W-S8 | Fremde Zeichenketten im Browser nur über `textContent`/DOM-APIs, nie `innerHTML`. JSON wird nur über ArduinoJson erzeugt. (S-7) |
| W-S9 | Vor LVGL und JSON durchläuft jede fremde Zeichenkette `sanitize`. Die Scan-Liste hat höchstens 15 Einträge, kein Recolor. Der LVGL-Assert führt zum Neustart. (S-14, C-3) |
| W-S10 | Grenzen: Body ≤ 1 KB (413), Multipart und Chunked → 415, URL ≤ 256 B, Header ≤ 2 KB, 10 s Timeout; ≤ 3 WebSocket- und ≤ 4 HTTP-Verbindungen; unter 24 KB Heap → 503. (S-13) |
| W-S11 | Rate-Limits: ≤ 20 Schreibbefehle/s je Quelle (429), Scan höchstens alle 10 s, nach 5 Login-Fehlern 30 s Sperre (verdoppelnd). (S-9, S-13) |
| W-S12 | Netzänderungen zweistufig mit Rückfall nach 120 s; ohne Web-Passwort zusätzlich Bestätigung am Touch. Statische IP ≠ Gateway, im Subnetz, keine Netz- oder Broadcast-Adresse. (S-15) |
| W-S13 | Web-Passwort: mindestens 8 Zeichen, gespeichert als gesalzener Hash (PBKDF2 aus mbedTLS). Sitzungs-Cookie `HttpOnly; SameSite=Strict`, Ablauf 12 h. Kaputte `Authorization`-/`Cookie`-Header werden abgelehnt. (S-9) |
| W-S14 | WPA2-PSK als PMK gespeichert, `WiFi.persistent(false)`. Der Werksreset löscht `hfnet`, `nvs.net80211` und den Coredump. Passwörter erscheinen nie in Ausgaben. (S-8) |
| W-S15 | STA mit PMF „capable“ und Mindestsicherheit WPA2. AP mit Passwort als WPA2/WPA3-SAE-Übergangsmodus, falls der Core das stabil unterstützt (sonst WPA2). (S-28) |
| W-S16 | Gerätename als RFC-1123-Label, AP-SSID druckbares ASCII ≤ 32 B; mDNS-TXT ohne Version und Commit. (S-26) |
| W-S17 | SCPI-Zeilen ≤ 256 B; bei Überlänge bis `\n` verwerfen und −363 melden. ≤ 16 Befehle je Zeile; NUL oder Nicht-ASCII verwirft die Zeile. (S-11) |
| W-S18 | SCPI über TCP schließt ohne Ausführung, sobald eine Zeile nach HTTP aussieht (`^[A-Z]+ \S+ HTTP/1\.[01]`, `^[A-Za-z0-9-]+:\s`). (S-6) |
| W-S19 | Alle Parameter werden im vollen Typ geprüft (endlich, ganzzahlig wo nötig, im Bereich) und erst dann verengt. Frequenzen sind Ganzzahlen in Hz. (S-12, C-4) |
| W-S20 | SCPI: Leerlauf-Timeout 10 min, TCP-Keepalive, IP auf der Info-Karte, `Disconnect` am Gerät; auf einem offenen AP aus. (S-17) |
| W-S21 | Option `Remote outputs: local only`: Einschaltbefehle aus Web, REST und SCPI werden abgelehnt (403 bzw. −221), Ausschalten bleibt erlaubt. Änderbar nur am Touch. Die Statusleiste zeigt `REMOTE <Quelle>`. (S-18) |
| W-S22 | Während einer SCPI-USB-Sitzung keine Logzeilen auf `Serial` (W-E27). (C-16, S-25) |
| W-S23 | `loop()` vom Task-Watchdog überwacht; kein Netz-Task blockiert länger als 100 ms; Stack-Reserve ≥ 1 KB nach 24 h. (S-23) |
| W-S24 | Alle Abhängigkeiten exakt gepinnt (Version oder Commit), Advisory-Check je Release. CI mit per SHA gepinnten Actions und `permissions: contents: read`. (S-20, S-27) |
| W-S25 | Fuzzing: `line_framer`, SCPI-Kern, `request_guard`, `sanitize` und JSON-Abbildung als reine Funktionen. libFuzzer mit ASan/UBSan auf Linux vor jedem Release ≥ 10 Mio. Ausführungen; Zufallstest mit festem Seed (100 000 Eingaben) in `native-win`. (S-11, S-13) |

Bewusst nicht vorgesehen: TLS/HTTPS (kein Zertifikat im LAN-Gerät verwaltbar), Secure Boot und Flash Encryption (S-22). Das Handbuch nennt die Folgen: Verkehr im Klartext, Gast- oder IoT-WLAN empfohlen, physischer Zugriff bedeutet Vollzugriff.

## 11. Implementierungsplan

Jeder Schritt endet mit den Gates aus Audit 5.8: Host-Tests in `native-win` und `native`, 0 Warnungen, Review (bei Netz-Code zusätzlich gegen W-S), Smoke-Test auf beiden Boards, Doku DE und EN im selben Commit.

| # | Schritt | Ergebnis / Prüfung | Aufwand |
|---|---|---|---|
| M | Migration Core 3.x (Abschnitt 4) | Abnahme Abschnitt 4 | 2,5–3 d |
| V | Vorarbeiten V-1–V-6, V-11 (Abschnitt 5) | Host-Tests für Ergebniscodes, Änderungszähler, Ereignisse, Speichern je Quelle/Schlüssel, Logger, Parser | 4 d |
| 0 | Vorab-Messung WLAN: Client mit Dauerverkehr und AP; Spektrum an CLK0 bei 10 MHz (Span 200 kHz / 20 kHz), Modem-Sleep an/aus, Heap-Bilanz | Nebenlinien dokumentiert, Modem-Sleep entschieden | 0,5 d |
| 1 | Befehlskern: `command`, `state_snapshot`, Quelle, W-E25 | `test_command` | 0,75 d |
| 2 | SCPI über USB: `line_framer`, scpi-parser, Befehlsbaum 8, Logger-Umschaltung | `test_scpi`, `test_line_framer`; PyVISA `ASRL<COM>::INSTR` mit DTR/RTS aus; 1000 × `*IDN?` ohne Fremdzeilen | 1,5 d |
| 3 | WLAN-Manager, `net_settings` (PMK, Zufallspasswort), mDNS, Captive-DNS, Routing-Regeln | `test_net_settings`; L1–L5, L-S6 | 1,5 d |
| 4 | WLAN am Gerät: Statusleiste MAIN, CONFIG-Zeilen, Seiten WIFI mit Tastatur, QR, Spinner, `sanitize` | `test_sanitize`; Einrichtung ohne PC, LVGL-Heap gemessen | 2 d |
| 5 | SCPI über TCP 5025 mit HTTP-Erkennung, Timeout, eine Verbindung | PyVISA `TCPIP0::<name>.local::5025::SOCKET`; L6, L7, L-S3 | 0,5 d |
| 6 | Webserver: `request_guard`, REST, WebSocket, Login, Limits, Header, Token | `test_http_guard`, `test_rest_json`; `curl`-Matrix L-S1/L-S2 | 2 d |
| 7 | Weboberfläche (alle Bereiche, Tastatur, responsiv, Live-Hervorhebung, zweistufige Netzänderung, Identify) | L8–L11, L16–L21 | 2,5 d |
| 8 | Fuzzing (W-S25), 24-h-Dauertest mit Last, Heap- und Stack-Bilanz | L14, L-S5 | 1 d (+ Laufzeit) |
| 9 | Doku: Handbuch DE/EN (WLAN, Web, SCPI-Referenz, REST, Python-Beispiel, Sicherheitshinweise, Fernbetrieb und Antenne), Spezifikation (A-n, E-n), README, CLAUDE.md, todo.md, Messplan L | Doku vollständig | 1,5 d |
| | **Summe** | | **≈ 20–21 d** |

### 11.1 Python-Beispiel (Entwurf für die Doku)

```python
import pyvisa

rm = pyvisa.ResourceManager()
gen = rm.open_resource("TCPIP0::cyd-hf-1a2b.local::5025::SOCKET",
                       read_termination="\n", write_termination="\n")
print(gen.query("*IDN?"))
gen.write("FREQ 7.1 MHZ")
gen.write("OUTP1 ON")
print(gen.query("FREQ:ACT?"))      # tatsächliche Frequenz in Hz
print(gen.query("SYST:ERR?"))      # 0,"No error"
```

Über USB: `rm.open_resource("ASRL5::INSTR")`. DTR und RTS müssen aus bleiben, damit das Öffnen des Ports das Board nicht neu startet (am CYD prüfen, Messplan L6).

## 12. Tests

**Host-Tests (neu):**

| Suite | Inhalt |
|---|---|
| `test_command` | jede Funktion als Befehl, `Result`-Codes, Quelle und Speicherfristen, Dirty je Schlüssel, `*RST`-Semantik, W-E25, Rate-Limit, volle Queue |
| `test_scpi` | Kurz-/Langform, Einheiten, MIN/MAX/DEF, NAN/INF/Hex/Exponenten, Grenzen, Fehlerwarteschlange, IEEE 488.2, `*RST` |
| `test_line_framer` | Überlänge, `;`-Fluten, NUL/Nicht-ASCII, HTTP-Erkennung |
| `test_http_guard` | Host, Origin, Sec-Fetch-Site, Content-Type, Token, Sitzung, Größen, Fehlversuche |
| `test_sanitize` | `"\xF0A"`, `"\xC3"`, `\n`, `\0`, 32-B-Grenze |
| `test_rest_json` | Zustand ↔ JSON (Escaping, Rücklesen), Ablehnung unbekannter Schlüssel und falscher Typen |
| `test_net_settings` | NVS-Schema `hfnet`, PMK statt Passphrase, IP-Plausibilität, Export ohne Passwörter |
| Fuzzing | W-S25 (Linux, CI-Job mit Korpus im Repo) |

**Messplan L (Hardware):**

- **Funktion:**
  - L1: Modi und Rückfall.
  - L2: zwei WLANs, das stärkste gewinnt.
  - L3: WPA2-AP mit Zufallspasswort und QR, offener AP.
  - L4: statische IP.
  - L5: mDNS und Captive-DNS mit Android und iOS.
  - L6: SCPI über USB und TCP mit PyVISA, DTR/RTS.
  - L7: zweite Verbindung wird abgelehnt.
  - L8: Web auf Handy und PC, 3 Clients synchron, ein vierter abgelehnt.
  - L9: Tastaturbedienung.
  - L10: zweistufige Netzänderung mit Rückfall.
  - L11: Speichern nach 5 s / 30 s, Ein/Aus ohne Speichern.
  - L12: ohne SI5351.
  - L13: WLAN-Störungen im Spektrum.
  - L14: 24-h-Dauertest.
  - L15: WLAN aus → Spektrum wie 1.3.0.
- **Bedienung (Audit U):**
  - L16: Einrichtung durch Novizen, mit und ohne QR.
  - L17: 16-stelliges Passwort mit der Bildschirmtastatur.
  - L18: fremde Änderungen bemerkt?
  - L19: Tastatur ohne Maus.
  - L20: `.local` unter Windows, Android, iOS, im Schulnetz; Identify mit drei Geräten.
  - L21: Großansicht am Beamer.
- **Sicherheit:**

  | Nr | Test |
  |---|---|
  | L-S1 | `curl`-Matrix: `Host: evil.example` → 421; `text/plain` → 415; ohne Token → 403; `Origin: http://evil.example` → 403; Body 100 KB → 413; Passwort über offenen AP → 403 |
  | L-S2 | Testseite unter `python -m http.server` mit `fetch no-cors`, `<form enctype=text/plain>` und WebSocket von fremdem Origin → Zustand unverändert |
  | L-S3 | `curl -X POST --data-binary $'OUTP1 ON\n' http://IP:5025/` → CH1 aus; 20 000 × `A` + `OUTP1 ON` → CH1 aus |
  | L-S4 | Test-AP mit SSIDs `<svg onload=alert(1)>`, `a"b\c`, ungültigem UTF-8; 200 Beacons → Gerät bleibt bedienbar, keine Skriptausführung |
  | L-S5 | Slowloris mit 30 Sockets, WebSocket-Flut, Multipart-PoCs aus den Advisories → Touch reagiert, Heap erholt sich |
  | L-S6 | Handy am AP bekommt kein Gateway; Laptop mit statischer Route über 192.168.4.1 erreicht keinen Host im Heimnetz |
  | L-S7 | `esptool read_flash` auf die NVS-Partition, dann `strings` → keine Passphrase |
  | L-S8 | `Remote outputs: local only` → `OUTP1 ON` per SCPI und REST abgelehnt, Touch schaltet |

## 13. Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| TFT_eSPI mit Core 3.x | Spike zu Beginn von M; gepinnter Fork; LovyanGFX nur als letzter Ausweg |
| Wire im neuen IDF-Treiber anders | `I2CBus` gebündelt, Annahmen als Fake-Tests, Messplan J |
| WLAN-Nebenlinien | Schritt 0; WLAN abschaltbar, Standard aus; Handbuch |
| Heap-Fragmentierung | Grenzen (W-S10), Heap-Schutz, Dauertest; Befunde ausdrücklich berichten |
| Nebenläufigkeit | Regeln 6.2, Host-Tests, Review auf direkte Aufrufe aus Netz-Tasks |
| Angriffe über das LAN und fremde Webseiten | W-S4–W-S8, W-S18; L-S1–L-S4 |
| Offener AP auf Wunsch | Warnung, Zeitlimit, Passwörter gesperrt (W-S1, W-S2) |
| Unbeabsichtigtes Senden | W-E25, Statusleiste `REMOTE`, Handbuch: nie ungefiltert an eine Antenne |
| Flash-Verschleiß | W-E17 (30 s, nur geänderte Schlüssel) |
| Neue Lücken in Bibliotheken | exakt gepinnt, Advisory-Check je Release (W-S24); Arduino 3.3.13 / IDF 5.5.6 übernehmen, sobald verfügbar |

Spätere Optionen: OTA (neue Partitionstabelle), VXI-11/LXI, HTTPS, LVGL 8.4/9, DMA-Ausgabe (todo 4).

## 14. Abgrenzung zu todo.md und Audit

- **Erledigt mit diesem Konzept:**
  - todo 6 „Fernsteuerung über USB-Seriell“;
  - todo 1 „Arduino-Core 3.x“ (Phase M);
  - Audit C-14, C-16, C-17, S-1 bis S-28 (soweit WEB-HMI) und U-8 (Statusleiste).
- **Bei Freigabe neu in todo.md:**
  - WEB-HMI mit Verweis auf dieses Konzept und die Phasen M und V;
  - Messplan L und L-S.
- **Voraussetzung:** Audit-Phasen 0–3 (Robustheit, Bedienung, Messungen) sind vorher erledigt.
