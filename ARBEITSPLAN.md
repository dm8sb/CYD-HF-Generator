# Arbeitsplan – CYD HF Generator

Stand: LX1 (VXI-11/LXI), die Lizenzarbeiten AF29–AF31 und Priorität 1 der Audit-Folgearbeiten (AF1–AF10, Bedientest bestanden) sind auf `dev`; alle Rückfragen zum Audit sind entschieden (D22–D43) · Release-Kandidat **1.4.0-rc2** live (Webflasher https://dm8sb.github.io/CYD-HF-Generator/), letztes endgültiges Release **1.3.3**; Arbeitszweig `dev`, `main` nur per Pull Request · nächste Ziele: neue Historie und Release **2.0.0** (rc1, Test, final), dann AF11, AF12 und AF55; die Messungen (S41c, S49, S59–S61) kommen zuletzt (D38)

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
6. **Push nach jedem Schritt.** Der Arbeitszweig wird nach jedem Commit gepusht, damit kein Zwischenstand verloren geht (Nutzer); Arbeitszweige der Agenten ebenso, sobald sie committet sind. Merge nach `main` und Tags veröffentlichen nur auf ausdrückliche Anweisung.
7. **Branches:** gearbeitet wird auf `dev` (`feature/web-hmi` ist vollständig übernommen und gelöscht). `main` ist per Ruleset geschützt: Änderungen nur per Pull Request `dev` → `main` mit grünen Pflicht-Prüfungen; GitHub legt den Merge-Commit mit der Kontoadresse an (vom Nutzer akzeptiert). Agenten arbeiten auf eigenen Branches, die nach der Übernahme gelöscht werden.
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
| 6 WEB-HMI bis S58 | 1.4.0-rc1 | Befehlskern mit Quellen, Ratenbegrenzung und `Remote outputs`; SCPI über USB (E-29) mit Identify; Netzeinstellungen `hfnet` (E-30); WLAN-Manager (Client, Rückfall-AP, mDNS, Captive-DNS, D14); CONFIG `WiFi`/`Remote out.`, Statusleiste mit WLAN und `REMOTE`; Seiten WIFI (Scan, Tastatur), Access Point (QR) und Device (Name, statische IP); LVGL-Pool aus dem Heap (E-31); Host-Werkzeuge in `tools/` (`device_check.py`); SCPI über TCP 5025 (S54); Request-Guard und REST-JSON (S55a); Weboberfläche `data/www/` mit Mock-Server (S56a); Fuzzing mit 9 Zielen, CI-Job und Zufallstest, Fix langer Zahlen (S57a–c); Webserver mit REST, WebSocket, Login und eingebetteter Oberfläche (S55, E-33–E-35), Web-Passwort auf der Seite Device und Remote-Clients mit `Reconnect all` auf der Info-Karte (S55b), feste Kopfzeile und Abmelden (S56); Fuzz-Funde behoben (lange Exponenten, 32-bit-Zeit im Guard, doppelte Schlüssel, einzelnes Surrogat in Schlüsseln); Doku auf 1.4.0, Release-Kandidat (S58) |
| 6b LXI | `dev` | LX1: Portmapper (UDP/TCP 111), VXI-11-Kernkanal (TCP 9009), mDNS `_lxi._tcp`/`_vxi-11._tcp`, `/lxi/identification` (E-37, D33, D42, D43); Gerätetest mit lxi-tools 2.8, PyVISA und python-vxi11 |
| 7 GitHub (Teil) | 1.4.0-rc1 | Release-Workflow mit Herkunftsnachweis und Webflasher-Seite (G1), `SECURITY.md` und CodeQL (G4), Dependabot (G6), Größenbericht in der CI (G7), Issue- und PR-Vorlagen (G8), Badges und `docs/hardware.md` (G9a); LF für alle Textdateien (`.gitattributes`, `.editorconfig`); Lizenz-Hinweise, Lizenzpaket und LGPL-Quellen am Release, ESP Web Tools selbst gehostet (G11); gerenderte Bilder (G13); Ruleset für `main` (G5); OpenSCAD-Gehäuse; TFT_eSPI-Schriften entfernt, Build-Pfade gekürzt; KI-Hinweis in README und auf der Webflasher-Seite |
| 10 Audit (Teil) | `dev` | Projekt-Audit (Bericht in der Git-Historie bis `0eb2c1a`), Rückfragen entschieden (D22–D43); AF29–AF31: Notices gegen Linker-Map und Espressifs Liste geprüft, LGPL-Archiv vollständig, GPLv3-§7-Erlaubnis `LICENSE-EXCEPTION.md` (E-38), Quellverweise auf den Tag (E-39), SPDX-Köpfe und `REUSE.toml` (D40); Tag-Ruleset, Immutable Releases und Repo-Einstellungen (D29, D41); Priorität 1 (AF1–AF10): Off state `High` aus der Ferne bei `local only` abgelehnt (E-40), Web-`Off` nur mit Modus, kein Login über offenen AP, SSIDs im Log bereinigt, Netz-SCPI ohne Anmeldung dokumentiert (E-41), Bestätigungsdialoge in einer Warteschlange, Release nur von `main` mit Tests und ohne Überschreiben, REST-Vertrag mit Fehler- und Statustabellen, 64-Hex-PMK auch am Touch, Handbuch und README nachgezogen |

Host-Tests: 406 Fälle in `native-win` und 4 in `native-win-fault`.

### 1.2 Referenzwerte

- **Firmware 1.3.0:** `firmware.bin` SHA-256 `e0ca03b6c03ab4771f74c7018ca21bde2b8bef672d21b31d0deedf4783bdf577` unter `messungen/firmware/1.3.0/` (nicht im Repo).
- **Heap (Debug-Build 1.3.1, Original-CYD ohne SI5351):** frei 277 412 B, Minimum 271 820 B, größter Block 110 580 B; LVGL 38 484 von 49 152 B frei; Stack-Reserve `loop()` 4 796 B. Basis für den Vergleich nach der Migration (S37).
- **Flash/RAM 1.3.2:** 630 069 B (20,0 %) / 90 312 B (27,6 %); **1.3.3:** 638 033 B (20,3 %) / 90 568 B (27,6 %).
- **Touch-Abgleich:** CYD2USB x 161…3778, y 245…3830; Original-CYD x 166…3716, y 280…3885 (vorher fest 300…3800); Original-CYD nach dem Werksreset x 158…3708, y 245…3856, Prüfkreuz −2/+1 px.
- **Core 3 (Spike S37):** Release-Build Flash 668 247 B (2.x 645 445 B), RAM 96 440 B (2.x 94 496 B); Debug-Build, Original-CYD ohne SI5351: Heap frei 250 684 B (2.x 272 604 B), Minimum 221 872 B (2.x 267 008 B), Stack-Reserve `loop()` 4 880 B, erstes Bild 172 ms (LVGL 8.4.0: 169 ms; 2.x 159 ms), `Setup complete` nach 0,9 s.
- **Flash/RAM `feature/web-hmi` S54a (Core 3):** 1 381 971 B (43,9 %) / 76 476 B (23,3 %); 2.x-Env 1 201 629 B / 74 384 B.
- **Flash/RAM nach S41d (nur Core 3):** 1 405 583 B (44,7 %) / 77 748 B (23,7 %); **nach S56:** 1 535 690 B (48,8 %) / 78 536 B (24,0 %).
- **SCPI über TCP (CYD2USB, Büro-WLAN, S54):** `*IDN?` Median 14,7 ms, 99 % 97 ms, max. 1,4 s (WLAN-Latenzspitzen); Heap nach 8 s Flut 133 KB frei, Minimum 81 KB.
- **SCPI über USB (CYD2USB):** `*IDN?` Median 6,0 ms, 99 % < 11,6 ms (1000 Abfragen, S51); `device_check.py` 94 PASS (S54a).
- **Boards:** COM13 = CYD2USB (ST7789) mit SI5351, COM7 = Original-CYD (ILI9341) ohne SI5351.
- **Smoke-Tests 1.3.1, 1.3.2 und 1.3.3:** bestanden bis auf die Schritte 5 und 6 (10 MHz und IQ 90° messen) – kein Messgerät verfügbar; nachholen in MESSPLAN Stufe 1.

### 1.3 Entscheidungen

| # | Entscheidung (Datum) |
|---|---|
| D1 | WLAN-Passwörter nie über einen offenen AP |
| D2 | AP mit zufälligem WPA2-Passwort und QR; offen nur auf Wunsch |
| D3 | Offener Rückfall-AP nur nach Bestätigung am Touch |
| D4 | Option `Remote outputs: allowed / local only`, Standard `allowed` |
| D5 | Plattform offiziell `espressif32@8.0.0` (Spike S37 entscheidet endgültig) |
| D6 | Captive-DNS nur im AP-Modus |
| D7 | `*RST` speichert nicht bis `SYSTem:SAVE` |
| D8 | Statusleiste auf MAIN, Zeile `Step` in CONFIG gestrichen (umgesetzt) |
| D9 | Erst Qualität, Bedienung und WEB-HMI, dann die Messkampagne |
| D10 | Boot-Geste entfernt, nur automatische Display-Erkennung (E-26) |
| D11 | Touch-Abgleich beim ersten Start (E-27) |
| D12 | Ein Plan statt todo/Audit/Konzept; Messplan nach Priorität (Stufe 1 Pflicht, 1W Pflicht ab WEB-HMI, 2 Entscheidungen, 3 optional). Ändert D9: Stufe 1 (und möglichst 2) läuft, sobald Zähler und Oszilloskop verfügbar sind, auf 1.3.x vor der Migration und ist die Referenz R0; ein eigener R0-Lauf mit der alten Version entfällt |
| D13 | LVGL 8.4.0 kommt mit der Migration (S40a), LVGL 9 bleibt ausgeschlossen; Phase 5 ist ohne Verhaltensänderung schon in `dev` |
| D14 | Routing (S-16): offizielle Plattform mit vorkompiliertem IDF bleibt, obwohl `CONFIG_LWIP_IP_FORWARD=y` fest eingebaut ist. Stattdessen W-S3 strikt: AP und STA nie gleichzeitig mit Adresse, der AP geht im Event der STA-IP aus, Prüfung ≤ 1 s (L-S6). Kein pioarduino mit `custom_sdkconfig` (eigener IDF-Build, längere Builds, zweite Plattform-Quelle) |
| D15 | WLAN-Schlüssel als Passphrase statt PMK (W-S14 geändert, E-30): ESP-IDF 5.5 nimmt bei WPA2/WPA3-Übergangsnetzen immer SAE, das die Passphrase braucht; mit PMK scheiterte das Büro-WLAN (Grund 2/202), mit Passphrase verbunden in 3,4 s. Nie angezeigt, geloggt oder exportiert (Nutzer) |
| D16 | Nur noch Core 3 (`espressif32@8.0.0`), Core 2 vor der Abnahmemessung verworfen; Abnahmemessungen bleiben (S41c), Rückweg: der letzte Release-Tag auf Core 2 (Nutzer, E-32) |
| D17 | Lizenz GPL-3.0-or-later, Copyright-Zeile „Copyright (C) 2026 DM8SB“; privates Projekt, einzige Commit-Identität `DM8SB <dm8sb@gmx.de>`, Historie darauf umgeschrieben; ESP Web Tools auf GitHub Pages selbst gehostet (kein CDN); kein Impressum, sondern Name, Hobbyprojekt und Gewährleistungsausschluss auf der Flasher-Seite; Lizenzzeile nur in der Weboberfläche, nicht auf dem Display (Nutzer, A-77, E-36) |
| D18 | Neue Funktionen nach der Freigabe: nur der gekoppelte Betrieb CH2 = CH1 ± Offset (N14); Speicherplätze, Wobbeln, Drive je Kanal, Info-Seite „Synthesizer“, freie Phase/Feinabgleich, Modulation, externe Referenz und Kalibrierassistent vorerst nicht geplant (Nutzer) |
| D19 | Keine WLAN-Einrichtung beim Flashen (kein Improv Serial, G3 entfällt); WLAN nur über das Menü am Gerät, Standard `Off` (Nutzer) |
| D20 | Taste der Info-Karte heißt `Reconnect all` statt `Disconnect all`: sie trennt alle Remote-Clients, die sich danach selbst neu verbinden; keine Sperrzeit, Aussperren über Web-Passwort oder `Remote out.` (Nutzer) |
| D21 | Jeder `v*`-Tag deployt den Webflasher, auch Release-Kandidaten (`-rc`) (Nutzer) |
| D22 | Off state `High` aus der Ferne (SCPI, VXI-11, REST) wird bei `Remote out. local only` mit `-221` bzw. 403 abgelehnt; bei `allowed` bleibt er ohne Rückfrage erlaubt (SEC-V-1, AF1; Nutzer) |
| D23 | `POST /api/wifi` mit `"mode":"off"` und weiteren Feldern wird abgelehnt (400, `-108 Parameter not allowed`); `off` allein bleibt sofort gültig ohne Bestätigung (E-34). Weitere Änderungen vorher als eigene, bestätigte Änderung (SEC-2, AF2; Nutzer) |
| D24 | Netz-SCPI (TCP 5025, VXI-11 aus LX1) bleibt ohne Login und ohne eigene Einstellung, wie bei Laborgeräten üblich; es wird nur klar dokumentiert (Handbuch, `SECURITY.md`): Schutz über `Remote out. local only` und ein gesichertes Netz. AF20 entfällt (SEC-1, AF4; Nutzer) |
| D25 | Mit gesetztem Web-Passwort brauchen auch Lesen und Live-Werte eine Anmeldung: `/ws`, `GET /api/state` und `/api/token` nur mit Sitzung (`loginForReads`), die Seite fragt beim Öffnen nach dem Passwort. Ohne Web-Passwort bleibt alles offen wie bisher. Ändert den S55-Vertragspunkt 7 (SEC-5, AF18; Nutzer) |
| D26 | Sofortiges Speichern (`SYST:SAVE`, `POST /api/save`) höchstens 1 je 10 s je Quelle; weitere werden still zusammengefasst (Speichern spätestens 10 s nach dem letzten), kein Fehler; `SAVE` schreibt nur geänderte Werte (SEC-V-3, AF17; Nutzer) |
| D27 | Zusätzliche Erlaubnis nach GPLv3 §7 für das Linken mit den Binärbibliotheken von Espressif (WLAN/PHY/Bluetooth-Controller) aufnehmen; Wortlaut (FSF-Vorlage „GPLIncompatibleLibs“, Branch `agent/licenses`, `LICENSE-EXCEPTION.md`) vom Nutzer bestätigt (keine Rechtsberatung). Prüfen, ob sich `libbtdm_app` ohne eigenen IDF-Build aus dem Image halten lässt (LIC-1, AF30; Nutzer) |
| D28 | Kein Antrag beim GitHub-Support: die alten Commits von vor dem Umschreiben bleiben über ihre IDs erreichbar, sind aber in keinem Branch und nicht über die Suche zu finden (SEC-4, AF5 entfällt; Nutzer) |
| D29 | Tag-Ruleset `release-tags` (`refs/tags/v*`: kein Löschen, Verschieben oder Force-Push, ohne Ausnahmen) und Immutable Releases sind eingeschaltet. Der Tag des ersten Release-Kandidaten (ohne Release) bleibt bestehen und ist nur im Plan vermerkt (G2) (AF7, AF28; Nutzer) |
| D30 | Kein manueller Deploy des Webflashers mehr: öffentlich wird nur, was ein `v*`-Tag auslöst. Der manuelle Lauf baut und packt die Seite als Artefakt zum Testen, das Eingabefeld `deploy` entfällt (LIC-6, AF7; Nutzer) |
| D31 | Der WLAN-Schlüssel darf überall auch ein 64-stelliger Hex-PMK sein: Touch-Tastatur (Seite WIFI), REST und Weboberfläche nehmen 8–63 Zeichen Passphrase oder genau 64 Hex-Zeichen an; E-30, Vertrag und Handbuch werden angeglichen (DOC-13, AF8; Nutzer) |
| D32 | WLAN-Suche ohne Verbindung: ohne gespeichertes Netz kein Scan; sonst Backoff 15 s → 5 min, solange kein gespeichertes Netz gesehen wird; mit Stationen am Rückfall-AP höchstens alle 2–5 min. `WiFi.begin()` für ein ungesehenes gespeichertes Netz (versteckte SSID) nur noch im selben Takt. Wiederverbinden nach Router-Ausfall dauert damit bis zu 5 min (PERF-5, AF14; Nutzer) |
| D33 | `GET /lxi/identification` liefert der Web-Server auf Port 80 (Host-/Origin-Prüfung wie die übrige Seite) und bleibt auch mit Web-Passwort ohne Anmeldung lesbar, wie bei Laborgeräten üblich: es enthält nur, was `*IDN?` über Netz-SCPI ohne Login ohnehin liefert (D24). Ausnahme zu D25 (W-E12, LX1, AF18; Nutzer) |
| D34 | Einzeltasten-Kürzel der Weboberfläche sind standardmäßig **aus**; Schalter „Single-key shortcuts“ im Hilfedialog (im Browser gespeichert); eingeschaltet wirken sie nur im Kanalbereich, nie in Eingabefeldern (A11Y-3, AF41; Nutzer) |
| D35 | CH1-Schrift in Kanalfarbe auch am Gerät `#F4615D` (5,6 : 1 auf den Karten statt 4,1 : 1), Flächen und Rahmen bleiben `#E53935`; Web und Gerät gleich (A11Y-21, AF43; Nutzer) |
| D36 | Graue Info- und Modus-Zeilen am Gerät auf Montserrat 14, wo es ohne Abschneiden passt; eng bemessene Stellen (Statusleiste, festes Zeilenraster) behalten 12; Prüfung am Host-Rendering und am Gerät (A11Y-21, AF43; Nutzer) |
| D37 | Umfang vor der Freigabe (S64): AF1–AF10, AF28 (Einstellungen) und zusätzlich AF11/AF12 (HF-Ruhe), damit S59/S60 den endgültigen Stand messen (Nutzer) |
| D38 | WLAN vorerst ohne Energiesparen (`WIFI_PS_NONE`) und mit voller Sendeleistung: Reaktionszeit zuerst. Messungen und daraus abgeleitete Änderungen kommen zuletzt, die Hardware ist fest. Anlass: im LX1-Gerätetest 220–390 ms Ping mit Modem-Sleep, pyvisa-py scheiterte am Standard-Verbindungs-Timeout (PERF-15, S49; Nutzer) |
| D39 | Strukturarbeiten (CQ-3, CQ-6; AF49, AF50) erst nach der Freigabe: sie erscheint mit dem geprüften Stand (Nutzer) |
| D40 | SPDX-Köpfe (`SPDX-FileCopyrightText: 2026 DM8SB`, `SPDX-License-Identifier: GPL-3.0-or-later`) in allen eigenen Dateien, Rest über `REUSE.toml`; neue Dateien bekommen sie ebenfalls. `LICENSES/` wird vorerst nicht REUSE-konform umgebaut, kein `reuse lint` in der CI (LIC-8, AF31; Nutzer) |
| D41 | Repo-Einstellungen per API gesetzt: Wiki aus; Actions nur von GitHub selbst (`allowed_actions: selected`, `github_owned_allowed`, keine verifizierten Ersteller, keine Muster) mit SHA-Zwang (`sha_pinning_required`); PRs nur als Merge-Commit (Repo-Einstellung und Ruleset `main` `allowed_merge_methods: [merge]`) (OPS-7, OPS-12, OPS-16, AF28; Nutzer) |
| D42 | mDNS kündigt `_scpi-raw._tcp`, `_vxi-11._tcp` und `_lxi._tcp` auch während der Sperre bei offenem AP an; keine Rücknahme (am offenen AP hängt meist nur ein Gerät zur Einrichtung) (LX1; Nutzer) |
| D43 | `/lxi/identification` bleibt ohne `MACAddress` und `LXIVersion` (W-S16, keine Konformitätsbehauptung) und ist damit nicht streng schemagültig; lxi-tools, PyVISA und python-vxi11 brauchen es nicht (LX1; Nutzer) |
| D44 | Nächstes Release heißt **2.0.0** (Core 3, Weboberfläche, Netzsteuerung, LXI). Die Git-Historie wird vor `v2.0.0-rc1` durch einen neuen Wurzel-Commit ersetzt; alte Tags und Releases (`v1.3.3`, Release-Kandidaten 1.4.0) werden gelöscht, Rulesets kurz ab- und wieder eingeschaltet. Ablauf: `v2.0.0-rc1` als Pre-Release, Test durch den Nutzer, dann `v2.0.0`; Messungen danach (Nutzer) |

**Nicht umsetzen:** neue Funktionen außer N14 vorerst (D18); RGB-LED als Ausgangsanzeige (bleibt Seitenanzeige, E-3); doppelte Statusanzeige im Header bleibt (E-13); Kanalwahl im Header (Rest von U-16: kein Platz neben Zustand und `MAIN`, über MAIN und die Karten 2 Tipps); LVGL 9, LovyanGFX (außer als Fallback), espressif32 6.13.0, Secure Boot / Flash Encryption (S-22).

**Offene Rückfragen** (die Rückfragen zum Audit sind als D22–D43 entschieden):

1. **Standard-Tuning:** bleibt `Fractional`, oder wird `Low noise` Standard, wenn die Messungen (MESSPLAN Stufe 2) einen klaren Vorteil zeigen? (N1)

---

## 2. Offene Befunde

`[M]` = wartet auf eine Messung (MESSPLAN-ID in Klammern).

### 2.1 SI5351, HF und IQ

- [M] **MultiSynth abgeschalteter Kanäle abschalten** (K2, F7): CH1/CH2 im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter; CLK2 bekommt `CLK2_PDN` nur bei Off state `Low`. Ist ein Ausgang mit `CLKx_PDN` hochohmig (K2), bei `High-Z` alle abgeschalteten Ausgänge mit PDN abschalten. F7 zeigt, ob es sich lohnt. → N5
- [M] **Störquellen der Plattform** (F6, F9): Backlight-PWM 5 kHz (Seitenlinien ±5 kHz möglich; Abhilfe ≥ 20 kHz oder 100 % ohne PWM), I2C-Überwachung alle 2 s (Abhilfe: Ruhemodus), CONFIG schreibt den Speicherzustand alle 300 ms neu (C-15 = PERF-3; nur bei Änderung schreiben). → N6, AF12
- [M] **IQ-Abstimmen ohne Aussetzer** (D3, D5, K6, E9): Jede Frequenzänderung im IQ-Betrieb setzt die PLL zurück (1000 Schritte → 1000 Resets). Teiler mit Hysterese festhalten und nur die PLL ohne Reset schreiben (wie `Low noise` im Einzelbetrieb), dann bleibt die Phase erhalten; ebenso bei Korrekturänderungen. Host-Test „PLL-Änderung ohne Reset, Offset und Teiler unverändert“. → N2
- [M] **180°/270° bei ungeraden Teilern** (E4): Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°; der IQ-Teiler `min(127, ⌊900 MHz / f⌋)` ist in 54 % der Fälle ungerade (14,2 MHz → 63, 99 MHz → 9). Abhilfe: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, oder 180° über den Phasen-Offset (2 × Teiler ≤ 127). → N3
- [ ] **Spannungseinbruch über Sticky-Bit erkennen:** `checkAlive()` erkennt einen SI5351-Neustart nur, solange ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen; dasselbe Register meldet `LOL_A`/`LOL_B` → Zeile „PLL locked“ auf der Info-Karte. → N9
- [ ] **Hinweise:** SI5351 läuft beim Flashen und nach ESP32-Reset mit der letzten Einstellung weiter (A10, Handbuch); Hardware-Hinweise (eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO nach C7). VCO bei `Fractional` und +50 ppm 900,045 MHz (unkritisch, nur notieren). → S72, N7, N10

### 2.2 Bedienung

- [ ] **Netzwahl nach falschem Passwort am Gerät** (S53b, Prüfpunkt h): zwei Netze gespeichert, das stärkere mit falschem Passwort → nach dem Fehlschlag verbindet sich das Gerät mit dem anderen. Host-getestet (`WifiPolicy::chooseSlot`). → S61
- [ ] **Gestapelte Bestätigungsdialoge** (CQ-1, CQ-15): ein gemeinsames `pendingConfirm_`; Cancel im oberen und Delete im unteren Dialog geben den offenen Rückfall-AP frei. → AF6
- [ ] **Zwei Schreiber der Netzeinstellungen** (CQ-2, CQ-5): Web-Rückfall verwirft Touch-Änderungen (auch das Web-Passwort), Touch-Änderungen speichern eine unbestätigte Web-Änderung; eine Kopie ohne Löschen der Schlüssel. → AF15, AF16
- [ ] **Endlose Scans im Client-Modus** (PERF-5, TEST-9): Scan alle 15 s auch ohne gespeichertes Netz und neben dem Rückfall-AP; Failover-Ablauf ungetestet. → AF13, AF14

### 2.3 Code und Tests

- [ ] **DMA und Doppelpuffer** (nach der Migration): zwei Puffer 320 × 40 px (`DMA_ATTR`, je 25,6 KB), `LV_COLOR_16_SWAP` = 1, `setSwapBytes(false)`; in `DisplayPanel::begin()` erst Erkennung und Init mit rohem Buszugriff, dann `initDMA()` und einmal `startWrite()` (CS bleibt LOW, `endWrite()` nie); `flush()` = `dmaWait()`, `setAddrWindow()`, `pushPixelsDMA()`, `lv_disp_flush_ready()` sofort. Erwartung bis ≈ 30 Bilder/s, +36 KB RAM; Messung mit `LV_USE_PERF_MONITOR`. → N13
- [ ] **Offene CodeQL-Funde** (OPS-9): auf `dev` 9 offen. #12 `py/bad-tag-filter` in `tools/flasher_site.py:509` (neu geöffnet, nicht behoben), #3 `tools/web_check.py:154` (`<SCRIPT>`), `tools/screens/main.cpp:55`, `data/www/app.js:143/145` (Fehlalarm laut G4), `tools/cdp_shot.mjs` (3). Auf `main` stehen #4, #9, #10, #11 fälschlich auf „fixed“ (OPS-1). → AF21, AF27
- [ ] **Fakes:** Bei NVS-Schemaänderungen Migrationstest mit alten Schlüsseln (mehrere Namespaces und Strings im Preferences-Fake seit S52a). Dazu NVS-Grenzen im Fake (Schlüssel und Namespaces > 15 Zeichen, Strings > 4000 B) und `test_upgrade_from_1_3_3` (TEST-11). → AF38
- [ ] **WLAN-Status je Iteration und Neuzeichnen** (PERF-1, CQ-12, PERF-12; PERF-2, PERF-3 = C-15, PERF-4, PERF-16, PERF-17, CQ-14): 3 synchrone Treiber-/lwIP-Aufrufe je `loop()`, Seiten-Refresh bei RSSI-Schwankung und 1-KB-Heapänderung. → AF11, AF12 (übernimmt „Neuzeichnen nur bei Änderung“ aus N6)
- [ ] **WebSocket-Push und Speichermangel** (PERF-6, PERF-8): bis ≈ 80 Zustände/s ungebündelt; `bad_alloc` in `push()` führt zum Neustart. → AF19
- [ ] **Geräte-Glue ohne Test** (TEST-1, CQ-6): `web_server.cpp`, `wifi_manager.cpp`, `scpi_tcp_server.cpp` nicht host-kompiliert; `web_check` prüft nur den Mock. → AF32, AF33, AF34, S61
- [ ] **Testlücken** (TEST-4 bis TEST-8, TEST-10, TEST-12, TEST-14): überlebende Mutanten, Identify ungetestet, keine Sanitizer für die Unit-Tests, keine Fuzz-Ziele für Vorfilter und `RestWifi`, Python-Werkzeuge nicht in der CI. → AF34–AF38, AF47
- [ ] **Struktur** (CQ-3, CQ-4, CQ-6, CQ-7, CQ-8 und Niedrig-Punkte CQ-9 bis CQ-23): `Gui`- und `WebHmiServer::Impl`-Größe, Texteingabe an LVGL gebunden, Texte zwischen Firmware/JS/Mocks dupliziert, Farbe außerhalb der Palette. → AF43–AF52

### 2.4 Plattform und Lieferkette

| ID | Komponente | heute | Ziel |
|---|---|---|---|
| P-2, P-4, S-19 | espressif32 / Arduino / IDF | 8.0.0 / 3.3.12 / 5.5.5, einzige Plattform (D16, E-32; DHCP-Fix CVE-2026-45160 enthalten) | Updates → N11 |
| P-5 | TFT_eSPI | 2.5.43 (letztes Release) | bleibt; mit Core 3.x im Spike unverändert lauffähig (Flush 35 ms je Vollbild wie unter 2.x); Fork oder LovyanGFX nur als Ausweg (eigene Display-Erkennung bleibt, LovyanGFX erwartet ST7789-ID 0x85, der CYD2USB meldet `8181B3`) |
| P-6, P-8 | LVGL, Unity | 8.4.0 (E-28, LVGL 9 ausgeschlossen, D13), 2.6.1 | Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6 → N11 |
| P-9 | Web-Bibliotheken | scpi-parser `8866159` (v2.3 + `strings.h`- und `-Wdouble-promotion`-Fix, seit S51, E-29) | ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, ArduinoJson 7.4.3, alle exakt gepinnt |
| S-20 | Prüfung | XPT2046 auf Commit gepinnt | Plattform-Prüfsumme in der CI, Advisory-Check je Release (W-S24); Framework-Pakete exakt gepinnt (AF22), Advisory-Workflow (AF26); Actions-Policy mit SHA-Zwang gesetzt (D41) |

- [x] **Release-Integrität** (OPS-4 = OPS-5 + TEST-3, OPS-14, OPS-15; erledigt mit AF7 und D29): Tags/Releases veränderbar (`--clobber`, kein Tag-Ruleset, keine Immutable Releases), Pages-Deploy ohne Release (Release-Kandidat), keine Herkunfts- und Testprüfung des Tag-Commits, Attestation nur für `.bin`. → AF7, AF28
- [ ] **Framework nur per Tilde gepinnt** (OPS-3 = LIC-4, OPS-10, OPS-17): Arduino-Framework und `-esp32-libs` als Bereich; Release-Image hängt am CI-Cache; Notices behaupten exakte Pakete. → AF22
- [ ] **CI-Workflows** (OPS-1, OPS-2, OPS-6, OPS-11, OPS-12, OPS-13, OPS-18 = CQ-20 = TEST-13, OPS-19): CodeQL-Alerts auf `main` fälschlich „fixed“, CI auf `main` abgebrochen, Release-Pfad vor dem Tag ungeprüft, Warnungs-Gate nicht erzwungen, Stempel nicht reproduzierbar. → AF21, AF23, AF24, AF25
- [ ] **Advisory-Prüfung und Repo-Einstellungen** (OPS-8, OPS-16): keine automatische Schwachstellenprüfung der Firmware-Abhängigkeiten (ESPAsyncWebServer, AsyncTCP, ArduinoJson, scpi-parser, Arduino-ESP32/IDF, LVGL, ESP Web Tools; Dependabot sieht kein Manifest); aus OPS-16 offen: Dependabot-Security-Updates, Non-Provider-Patterns, Fork-Freigabe, Environment `copilot`, Topics (OPS-7 gesetzt, D41). → AF26, AF28

### 2.5 Dokumentation

- [ ] **Anwendungskapitel „Mischer-Experimente“** (beide Handbücher): Pegel je Drive-Stärke an 50 Ω (F2), Oberwellen des Rechtecks (3. ≈ −9,5 dBc, 5. ≈ −14 dBc) und Tiefpässe, LO-Pegel für Diodenringmischer (+7 dBm) bzw. Dämpfung für NE612/SA612, IQ an FST3253/74CBT3253 (Tayloe), gleich lange Kabel (1° bei 100 MHz ≈ 28 ps ≈ 5,6 mm Koax), Phasenfehler ↔ Spiegelunterdrückung (1° ≈ 41 dB, 2° ≈ 35 dB, 5° ≈ 27 dB), nie ungefiltert an eine Antenne. → N8
- [M] **„50 % Tastverhältnis“** (Handbuch §1) ist nicht belegt (E5). → S72
- [ ] **REST-Vertrag** (DOC-2, DOC-4, DOC-5, DOC-13): Fehlerliste Zeile 105 falsch (−104/−221/−224), Statuscodes unvollständig, `slot` 0/1 nicht festgelegt, PMK-Regel widersprüchlich. → AF8
- [ ] **Handbuch** (DOC-6, DOC-7, DOC-10, DOC-11, DOC-12, DOC-20): SCPI-Fehler/Einheiten unvollständig, „später Web/SCPI“, Logtabelle, Speicherliste, `Labor`/`Lab`. → AF9
- [ ] **README, Werkzeuge, Spezifikation, Plan** (DOC-8, DOC-9, DOC-15 bis DOC-18 + OPS-20, DOC-14): Changelog-Stand, `-core3`, `--password secret`, A-24/A-46/A-48/A-60, CLAUDE.md-Liste (G2-Text und 3.6 sind mit der Übernahme des Audits korrigiert). → AF10 (DOC-14 mit AF4)
  - DOC-9: README-Changelog nennt den falschen Release-Kandidaten, „Request guard … not yet wired to a server“ widerspricht dem Webserver-Eintrag, „both firmware envs“ (es gibt nur noch eine).
  - DOC-15: A-60 („later TCP“), A-24 (Seitenliste ohne WIFI-Seiten), A-46 (Pins ohne die Web-Bibliotheken), A-48 (Host-Tests ohne Netzmodule) veraltet.
  - DOC-16: doppelter Eintrag `+<net/request_guard.cpp>` in `[native_test]` `build_src_filter` (`platformio.ini`).
  - DOC-18 + OPS-20: Tags ohne Release (ältere Tags und der erste Release-Kandidat; nur vermerken, Löschen nur auf Anweisung, D29).
  - DOC-8: `tools/README.md` nennt die Env `esp32-2432S028R-core3` (gibt es nicht mehr, E-32) und `--password secret` (unter 8 Zeichen, abgelehnt); `web_mock.py`, `flasher_site.py`, `prefix_map.py` fehlen in der Tabelle.
  - DOC-16/DOC-17: Host-Quellenliste in CLAUDE.md ohne `net/scan_list`, `web_limits`, `web_state`; Test- und Architekturübersicht im README ohne die Web-, REST- und WLAN-Module.

### 2.6 Sicherheit und Netz (Audit)

- [ ] **Off state `High` aus der Ferne ohne Rückfrage** (SEC-V-1 = DOC-3): `OUTP:OFFS HIGH` und REST `offState:"high"` wirken sofort, auch bei `Remote out. local only`; A-52 verlangt eine Bestätigung. → AF1
- [ ] **Web-`Off` speichert Zusatzfelder** (SEC-2): eine finale Änderung übernimmt auch Netze, Schlüssel, `ap.open`, Name und statische IP ohne Touch-Bestätigung (W-S12). → AF2
- [ ] **Login über offenen AP** (SEC-3): `/api/login` prüft `checkSecrets()` nicht, gegen W-S1. **Rohe SSIDs im Log**, `SYST:LOG?` über TCP leert den USB-Puffer (SEC-6). → AF3
- [ ] **Netz-SCPI ohne Anmeldung** (SEC-1 = DOC-1): TCP 5025 (und VXI-11 aus LX1) bleibt auch mit Web-Passwort voll schreibbar; Handbuch 762/1248 legt das Gegenteil nahe. → AF4 (nur Doku, D24)
- [ ] **Flash-Verschleiß durch `SYST:SAVE`/`POST /api/save` aus der Ferne** (SEC-V-3): ≈ 10 NVS-Schreibvorgänge/s je Quelle ohne Anmeldung. → AF17
- [ ] **WebSocket-Erschöpfung** (SEC-5, SEC-V-2, PERF-18): drei WebSockets mit dem öffentlichen Token sperren die Seite (503); PINGs füllen eine unbegrenzte Control-Queue; versteckte Tabs halten Slots. → AF18

### 2.7 Barrierefreiheit (Web-UI, WCAG 2.2 AA)

- [ ] **Reflow bei Zoom** (A11Y-1 + A11Y-20): Kopfzeile und Ausgangsleiste belegen bei 400 % Zoom 222 von 256 px; Navigation scrollt bei 320 px. **Kontrast CH1-Rot** 4,13:1 (A11Y-2, Gerät A11Y-21). → AF40, AF43
- [ ] **Tastenkürzel** (A11Y-3, A11Y-4, A11Y-10, A11Y-15, A11Y-16): Einzeltasten ohne Abschaltung, `PgUp`/Pfeile global umbelegt, Zielkanal nur visuell. → AF41
- [ ] **Statusmeldungen und Beschriftungen** (A11Y-5 bis A11Y-9, A11Y-11, A11Y-12, A11Y-14, A11Y-17; A11Y-13, A11Y-18, A11Y-19): Toast, Speicher-/HW-Status, Login-Fehler und WLAN-Frist ohne Ansage; Gruppen und Dialogtext ohne Zuordnung. → AF40, AF42

---

## 3. WEB-HMI (umgesetzt)

Das WEB-HMI ist umgesetzt. Verhalten: Handbuch §8 (WIFI-Seiten, Info-Karte), §15 (SCPI, LXI/VXI-11) und §16 (Weboberfläche, REST); REST-Vertrag: [data/www/README.md](data/www/README.md); Module und Code-Regeln: [CLAUDE.md](CLAUDE.md). Hier bleiben die Entscheidungen W-E, die Sicherheitsanforderungen W-S (gelten weiter, Regel 0.2) und die Konzepte für offene Schritte (3.9). Die Abschnitte 3.1–3.8 behalten ihre Nummern, weil Code-Kommentare, Spezifikation und PR-Vorlage sie zitieren; der vollständige Konzepttext steht in der Git-Historie bis `0eb2c1a`.

### 3.1 Ziel

WLAN-Client oder eigener Access Point (Standard aus), Weboberfläche mit allen Funktionen des Touch-Bildschirms, SCPI über USB und TCP 5025, REST. Kein Mitlesen von WLAN-Passwörtern, kein Fernsteuern über fremde Webseiten, kein ungewolltes Senden; die Bedienung am Gerät hat Vorrang.

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
| W-E12 | Protokoll | SCPI (IEEE 488.2 / SCPI-99, eigener Baum) über TCP 5025 (mDNS `_scpi-raw._tcp`) und USB; mit LX1 dazu VXI-11 (Portmapper UDP/TCP 111, Kernkanal, mDNS `_vxi-11._tcp`) und LXI-Erkennung (`_lxi._tcp`, `/lxi/identification`), ersetzt „kein VXI-11“ (Identifikation im Web-Server ohne Anmeldung, D33); kein HiSLIP, MQTT. |
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
| W-E26 | Statusleiste | umgesetzt; WLAN-, AP-, `REMOTE`- und später `LOCK`-Symbole kommen in S53 dazu (Form statt nur Farbe, U-19). |
| W-E27 | SCPI über USB | während einer SCPI-USB-Sitzung kein Log auf `Serial` (Ringpuffer, `SYSTem:LOG?`) (C-16). |
| W-E28 | Identify | Web-Taste und `SYSTem:IDENtify`: Backlight und LED blinken 5 s. |

### 3.3 Architektur

Umgesetzt wie in [CLAUDE.md](CLAUDE.md) („Remote access“, „WiFi“, „Web“, „LXI / VXI-11“). Regeln, auf die Code-Kommentare verweisen:
- Kein Netz-Task ruft `AppController`, LVGL, I2C oder NVS direkt; Schreiben über die Befehlsqueue, Lesen über den Schnappschuss (Sequenzzähler, weil uint64/double nicht atomar sind).
- Fremde Zeichenketten erreichen LVGL und JSON nur über `sanitize`; Passwörter werden nie ausgegeben (nur „gesetzt / nicht gesetzt“).
- JSON-Verschachtelung ≤ 10 (ArduinoJson 7.4.3); SCPI wartet auf Quittungen höchstens 1 s.
- Sicherheitslogik (`request_guard`, `sanitize`, `line_framer`, SCPI-Kern, JSON-Abbildung) frei von Arduino-/Webserver-Abhängigkeiten, host-getestet und gefuzzt.
- WLAN aus: kein Netz-Task, `WIFI_OFF`; SCPI über USB bleibt.

### 3.4 WLAN am Gerät

Umgesetzt; Seiten, Rückmeldungen, Info-Karte und Verhalten von Client und Rückfall-AP im Handbuch §8 und §16. Scan-Liste ≤ 15 (W-S9); Heap-Schutz nach W-S10 (unter 24 KB freiem Heap neue Verbindungen ablehnen, 503; `SYSTem:HEAP?`).

### 3.5 SCPI-Befehlsbaum

Maßgeblich ist die Befehlstabelle im Handbuch §15 (Kurzform groß, `[ ]` optional, `<n>` Kanal-Suffix, Zahlen und Fehlercodes). Netzwerkeinstellungen und `Remote outputs` sind über SCPI nicht änderbar.

### 3.6 Weboberfläche und REST

Bedienung im Handbuch §16; REST-Vertrag, Fehlercodes und Tastatur (`Bild↑`/`Bild↓` × 10 / ÷ 10) in [data/www/README.md](data/www/README.md). Die für S55 offenen Punkte, auf die Code-Kommentare mit „3.6 point n“ verweisen, sind dort unter [Settled in S55](data/www/README.md#settled-in-s55-firmware--reference-the-mock-follows-it) festgelegt:

1. `wifi.confirmWithin` (Sekunden, 0 = nichts offen) im Zustand.
2. Betriebsart `Off` über das Web ist final (keine Bestätigung über eine neue Verbindung möglich).
3. `device.passwordSet`; das Web-Passwort wird nur am Gerät gesetzt.
4. Fester Text bei veraltetem Token: `-200,Invalid token: reload the page`.
5. Fehlercodes (ungültiges JSON −102, falscher Typ −104, unbekannter Schlüssel −108, außerhalb der Wertelisten −224) und Bedeutung von `modes` und `actual`.
6. Scan: `GET` startet, das erste `GET` nach dem Ende liefert `done`, ein neuer Start innerhalb von 10 s → 429.
7. `/api/state` und `/ws` ohne Login lesbar; mit Web-Passwort künftig nur mit Sitzung (D25, AF18).
8. WebSocket-Grenze ≤ 512 B (W-S6) nur für eingehende Frames.
9. `frequencyText` außerhalb des Bereichs → −222.
10. Ohne Verbindung `wifi.ssid`/`ip` = `""`, `rssi` = 0; mit AP `ip` = AP-Adresse.
11. Kurze Fehlertexte, Status 403/429/503/500/413.

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
| W-S23 | `loop()` vom Task-Watchdog überwacht; kein Netz-Task blockiert > 100 ms; Stack-Reserve ≥ 1 KB nach 24 h. (S-23) |
| W-S24 | Abhängigkeiten exakt gepinnt, Advisory-Check je Release; CI mit SHA-gepinnten Actions und `permissions: contents: read`. (S-20, S-27) |
| W-S25 | Fuzzing von `line_framer`, SCPI-Kern, `request_guard`, `sanitize`, JSON-Abbildung, VXI-11 mit Portmapper und RPC-Satzmarken (`vxi11`, LX1): libFuzzer mit ASan/UBSan ≥ 10 Mio. Ausführungen vor jedem Release; Zufallstest mit festem Seed (100 000 Eingaben) in `native-win`. (S-11, S-13) |

Bewusst nicht: TLS/HTTPS (kein Zertifikat verwaltbar), Secure Boot und Flash Encryption (S-22). Das Handbuch nennt die Folgen (Klartext, Gast-/IoT-WLAN empfohlen, physischer Zugriff = Vollzugriff).

### 3.8 Tests und Risiken

Umgesetzt: Host-Suites in `test/`, Fuzzing nach W-S25 ([test/fuzz/README.md](test/fuzz/README.md)), Prüfung am Gerät in MESSPLAN L und L-S (S61). Spätere Optionen: OTA (neue Partitionstabelle), HTTPS.

### 3.9 Konzepte aus dem Audit

Entwürfe zu größeren Audit-Befunden für offene Schritte in Phase 10. Entfernt sind Konzept 1 (Zugriffskontrolle für Netz-SCPI: nur Doku, D24; die Einstellung `Net SCPI` bleibt eine Option für später), Konzept 8 (GPLv3-§7-Erlaubnis und Notices: erledigt mit AF29–AF31, E-38, E-39) und Konzept 9 (Antrag zur alten Historie: entfällt, D28); ihr Text steht in der Git-Historie bis `0eb2c1a`. Die Nummern 2–7 bleiben, weil Schritte darauf verweisen.

#### Konzept 2 · Netzänderungen zwischen Touch und Web koordinieren

- **Problem:** Zwei Schreiber ohne gemeinsamen Besitzer (CQ-2). `NetChange` merkt sich `prev_`, und die GUI speichert `wifi_->settings()`, das während der Wartezeit dem unbestätigten Web-Stand entspricht. Dazu kommt, dass Web-`Off` Zusatzfelder übernimmt (SEC-2).
- **Option a – Sperren:** Während `NetChange::busy()` lehnen die WIFI-Seiten Änderungen ab („Web change pending“, mit `Keep`/`Revert` direkt dort).
- **Option b – 3-Wege-Merge:** Der Rückfall setzt nur die Felder zurück, die das Web geändert hat.
- **Option c – ein Besitzer:** `NetSettingsService` im `loop()`-Kontext mit `change(mutator, origin)`, `busy()` und einer kleinen Zustandsmaschine: `Idle → WebPending(needsTouch) → Confirmed | Reverted`. Gespeichert wird **nur** ein bestätigter Stand. Dann gelten nach einem Stromausfall immer die vorherigen Einstellungen (W-S12).
- **Empfehlung:** c mit der Semantik von a. Ein 3-Wege-Merge (b) ist bei Abhängigkeiten zwischen den Feldern fehleranfällig, etwa bei Modus, AP-Passwort und Web-Passwort.
  - Mit Web-Passwort zeigt das Gerät einen Hinweis mit `Revert`, ohne Pflicht zur Bestätigung.
  - `commitNet_` (AF15) ist der einzige Touch-Pfad.
- **Folgen:** Handbuch §16 (Verhalten während einer offenen Web-Änderung), W-S12 und A-76 präzisieren, neue E-n. Host-Tests in `test_web_state`: Touch-Änderung im Fenster, Timeout, Bestätigung, Stromausfall = NVS-Inhalt. Bedientest mit und ohne Passwort.

#### Konzept 3 · Dialog-Warteschlange mit Aktion je Dialog

- **Problem:** Ein gemeinsames `pendingConfirm_` für mehrere modale Boxen. Die Entscheidung fällt nach dem Button-Label (CQ-1, CQ-15). Automatische Rückfragen (Rückfall-AP, Web-Netzänderung) können einen offenen manuellen Dialog überdecken.
- **Optionen:**
  - a) `user_data` je Box mit `Confirm`-Art und Generationszähler, Button-Index statt Label (minimal).
  - b) Zusätzlich eine LVGL-freie `ConfirmQueue` (in `utils/message_queue.*` neben der vorhandenen Nachrichten-Queue): Einträge `{kind, payload, origin, priority}`, es ist immer nur ein Dialog offen, automatische Rückfragen warten. Eine dringende Rückfrage (z. B. der Countdown `WebNetwork`) darf einen manuellen Dialog ausdrücklich mit „Cancel“ schließen und danach erscheinen, aber nie dessen Antwort erben.
  - c) Tabellengetriebene `ConfirmSpec` (Titel, Text, Buttons, sicherer Default) statt der if/else-Kette (CQ-3).
- **Empfehlung:** a + b in AF6, c in AF49a. Eine Antwort gilt nur für die Generation, die sie geöffnet hat. `confirmOpenFallback()` wirkt nur, solange `needsOpenConfirmation` gilt.
- **Folgen:** Host-Tests der Queue (Reihenfolge, Vorrang, veraltete Antwort verworfen, Überlauf). Im Handbuch einen Satz „Kommt während einer Rückfrage eine zweite, erscheint sie danach“. Bedientest.

#### Konzept 4 · WLAN-Status zwischenspeichern und GUI-Neuzeichnen reduzieren

- **Problem:** Je Iteration 3 synchrone Treiber- und lwIP-Aufrufe und ein Heap-`String` (PERF-1). Die GUI-Signatur enthält RSSI und Heap, `refresh_()` invalidiert alle Labels (PERF-2/3/16/17), Heap-Walks laufen unter Lock (PERF-4). Jede Neuzeichnung ist ein SPI-Burst neben dem SI5351.
- **Entwurf WLAN:**
  - `WifiManager` hält `Status` ereignisgetrieben: SSID, IP, Maske, Gateway und DNS bei `GOT_IP` bzw. `DISCONNECTED` (Event setzt nur ein Flag, gelesen in `loop()`). RSSI und Stationen höchstens 1×/s.
  - Ein Zähler `statusRevision()` ersetzt das Vergleichen.
  - Die Verbindungsprüfung für D14 bleibt je Iteration (Feldzugriff).
- **Entwurf GUI:**
  - Je Seite ein POD-Struct `StatusView` mit genau den angezeigten Feldern und `operator==`, statt eines globalen Hashes (CQ-14).
  - `setTextIfChanged`/`setColorIfChanged` als Hilfen.
  - RSSI-Anzeige mit Hysterese (z. B. 3 dB). Heap-Zeile alle 5 s, nur auf CONFIG und nur wenn sichtbar.
  - QR nur bei neuem Text, `lastRevision_` nach sofortigem Refresh nachziehen.
- **Empfehlung:** AF11, dann AF12, vor den Störmessungen F6/F9, damit die Referenz R0 den verbesserten Stand misst. Den Teil „Neuzeichnen nur bei Änderung“ in N6 damit schließen.
- **Folgen:** Handbuch nur, falls sich die RSSI-Aktualisierung sichtbar ändert („etwa 1×/s“). Host-Tests für den `StatusView`-Vergleich und den Takt. Am Gerät: Refresh-Zähler, Oszilloskop an TFT-SCLK, Loop-Rate, SCPI-TCP-Latenz vorher und nachher, als MESSPLAN-Eintrag.

#### Konzept 5 · Release-Integrität

- **Erledigt bzw. in AF7:** Tag-Ruleset `refs/tags/v*` und Immutable Releases (D29); kein öffentlicher Deploy aus einem manuellen Lauf (D30); AF7 prüft Herkunft (`merge-base --is-ancestor … origin/main`) und Host-Tests des Tag-Commits, bricht bei vorhandenem Release ab, deployt erst nach dem Release (Concurrency `pages`) und attestiert `SHA256SUMS`.
- **Offen:**
  - Build hermetisch ohne Cache mit exakt gepinnten Paketen, `pio pkg list` in den Notes (OPS-3, OPS-10; AF22).
  - Probelauf des Release-Pfads in PRs ohne Veröffentlichung (OPS-6; AF23).
  - Reproduzierbare Stempel, damit sich ein unabhängiger Nachbau gegen die Attestation prüfen lässt (OPS-13; AF25).
- **Folgen:** Release-Abschnitt im README (Prüfung mit `gh attestation verify` für alle Assets), `SECURITY.md` (Lieferkette). Prüfung per `workflow_dispatch` ohne Deploy; ein Probe-Tag nur auf Anweisung.

#### Konzept 6 · Geräte-Glue testen

- **Problem:** 42 % der Firmware-Zeilen sind nicht host-kompiliert (GUI, `web_server`, `wifi_manager`, `scpi_tcp_server`, `main`). `web_check` prüft nur den Mock. Unit-Tests laufen ohne Sanitizer (TEST-1, TEST-6, TEST-9, TEST-12).
- **Option a – Host-Shims für ESPAsyncWebServer/AsyncTCP und `WiFi`:** `web_server.cpp` unverändert kompilieren. Das ist viel Fake-API (Request, Response, WebSocket, `onPacket`), und die Fakes können Bibliotheksdetails falsch nachbilden. Für `WiFi` passt das Muster des vorhandenen `Wire`-Fakes eher.
- **Option b – Reine Module herausziehen:** Routen und Body (AF32), `JobSlot` (AF33), Ablauf der Netzwahl (AF13), Texte und Seitenaufteilung (AF47). Kostet wenig und folgt dem bewährten Muster von `request_guard` und `input_logic`.
- **Option c – Vertragstest gegen die Firmware:** `web_check.py` fest in S61 gegen das Gerät, dazu gemeinsame JSON-Vektoren, die C++-Tests und Mock lesen (AF34). Drift zwischen Mock und Firmware fällt dann auf beiden Seiten auf.
- **Option d – ASan/UBSan-Env für alle Unit-Tests (AF36):** Fuzz-Ziele für Vorfilter und `RestWifi` (AF37).
- **Empfehlung:** b + c + d. Option a nur für einen schmalen `WiFi`-Fake, falls nach AF13 noch Logik in `wifi_manager.cpp` übrig ist. Optional `screens-win` als Compile-Check der GUI in der CI.
- **Folgen:** `build_src_filter` und die Liste in CLAUDE.md, README-Tests, `test/fuzz/README.md`, MESSPLAN (S61 mit `web_check`-Protokoll).

#### Konzept 7 · Barrierefreiheit der Weboberfläche (und Kontrast am Gerät)

- **Problem:**
  - Reflow bei Zoom (A11Y-1), CH1-Rot unter 4,5:1 (A11Y-2/21).
  - Globale Einzeltasten und umbelegte Scroll-Tasten, die HF-Ausgänge schalten bzw. verstellen (A11Y-3/4).
  - Statusmeldungen ohne Ansage (A11Y-5/6/7/9/12), fehlende Zuordnungen (A11Y-8/10/11/14).
- **Entwurf:**
  - **CSS:** Kopfzeile und Leiste nur bei ausreichender Höhe fixieren (`max-height`-Query), `nav` umbrechen.
  - **Farbe:** Textfarbe CH1 `#f4615d` im Web und `Palette::CH1_TEXT` am Gerät. Rahmen und Schalter bleiben im Kanalrot (Non-Text 3:1 erfüllt).
  - **Tastatur:** Schalter „Single-key shortcuts“ im Hilfedialog. Kürzel wirken nur bei Fokus auf `body` bzw. im Kanalbereich. Frequenzanzeige als `spinbutton`, Pfeile und `PgUp` nur dort.
  - **Ansagen:** Zwei dauerhaft vorhandene Live-Regionen (`status`, `alert`) als einzige Ansagewege. Fehler in `alert`, Werte nach eigenen Änderungen verzögert in `status`.
  - **Struktur:** `<main>`, `h1`, Gruppen mit `aria-labelledby`, Dialoge mit `aria-describedby`.
  - **Bewegung:** `prefers-reduced-motion`.
- **Empfehlung:** AF40, AF41, AF42, AF43 in dieser Reihenfolge. Kürzel standardmäßig aus, einschaltbar, nur im Kanalbereich wirksam (D34).
- **Folgen:** `data/www/README.md` (Tastatur, Rollen, keine `hidden`-Live-Regionen), Handbuch §16 (Tabelle der Tasten, Schalter), `test_contrast` (neue Paare), `render_images.py`. Optional ein axe-Lauf über `tools/cdp_shot.mjs` als nicht verpflichtender CI-Job.

---

## 4. Aufgabenliste

Je Schritt: Inhalt, Prüfung, Commit-Betreff. `[ ]` offen, `[x]` erledigt (wird beim nächsten Aufräumen entfernt; Zusammenfassung in 1.1, Details in der Git-Historie).

### Phase 4 – Migration Core 3.x (Rest: Abnahmemessungen)

- [ ] **S41c · Abnahme Core 3 (Messungen).** Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10, ≈ 4 h, braucht ZÄHLER und OSZI), dabei CLK0 bei Q4/Q5 und J6 (Code 5 bei Leitung auf GND). Bei einem Befund ist der Rückweg der letzte Release-Tag auf Core 2. *Commit:* `S41c: Record the core 3 acceptance measurements`

### Phase 6 – WEB-HMI (Rest: Messung S49, Dauertest S57)

- [ ] **S49 · Vorab-Messung WLAN (0,5 d, braucht SPEKTRUM).** Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis im Messprotokoll und in 1.2. WLAN-Energiesparen und Sendeleistung sind vorerst entschieden (D38: `WIFI_PS_NONE`, volle Sendeleistung, umgesetzt mit AF55); S49 misst zuletzt `PS_NONE` gegen `MIN_MODEM` und eine reduzierte Sendeleistung (z. B. 11 dBm), eine Änderung danach als E-n (PERF-15). *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **S57 · Fuzzing und Dauertest (1 d + Laufzeit).** Erledigt: libFuzzer-CI-Job und Zufallstest mit festem Seed (S57a–c); manueller Fuzz-Lauf mit 10 Mio. je Ziel auf `main`, alle Ziele grün, ohne Fund (Lauf 42824946289). 24-h-Lauf: gestartet, lief 35 min fehlerfrei (USB, 4086 Schreiben/Rücklesen, 0 Abweichungen, 0 Resets, min. Heap 81,5 KB mit WLAN) und brach ab, weil der USB-Port auf der PC-Seite verschwand; **zu wiederholen**. Rest: 24-h-Lauf an stabilem Port (oder über TCP) mit Heap- und Stack-Bilanz (Weboberfläche offen, Debug-Log alle 60 s mit Stack-Reserve von `async_tcp` und `web_login`; ggf. Stack von `async_tcp` senken, aus S55b); zusätzlich Worst Case „alle Kanäle belegt“ (3 WebSockets, 1-Hz-Polling, SCPI-TCP-Flut, USB-SCPI gleichzeitig, PERF-8) und High-Water-Marks `async_tcp`/`web_login` sowie LVGL `max_used` nach einer Seitenrunde (PERF-14). *Commit:* `S57: Add fuzzing job and long-run test`

### Phase 7 – GitHub und Verteilung (Rest ≈ 1 d, je ein Commit; `[Nutzer]` = Einstellung im Repo, nur durch den Nutzer)

Stand: G1, G4–G9a, G11, G13 erledigt (Abschnitt 1.1), Workflows und Ruleset auf `main`; Pages (Quelle GitHub Actions) mit Umgebung `github-pages` (Regeln `main`, `v*`) sowie Secret Scanning und Push-Schutz sind eingerichtet.

- [ ] **G2 · Webflasher am Gerät (Workflows liegen auf `main`).** Erstes Release-Tag (ein manueller Lauf deployt seit AF7 nicht mehr, D30); beide Boards über https://dm8sb.github.io/CYD-HF-Generator/ flashen; Update ohne Erase behält Touch-Abgleich, Korrektur und WLAN; Erstinstallation mit Erase startet mit dem Touch-Abgleich. Dann Handbuch DE/EN neuer Abschnitt „17. Firmware aktualisieren“ (Browser Chrome/Edge am Desktop, CH340-Treiber, `Connect` und Port, Erase nur bei der Erstinstallation, Update schreibt nur 0x1000/0x8000/0xE000/0x10000, BOOT halten bei Verbindungsproblemen, `SHA256SUMS` und `gh attestation verify`, esptool-Befehl); das README verlinkt die Seite bereits. Jeder `v*`-Tag deployt die Seite, auch Release-Kandidaten (D21); ab AF7 erst nach erfolgreichem Release. Der Tag des ersten Release-Kandidaten ist ein Tag ohne Release (DOC-18, OPS-20). Offen: Firefox-Unterstützung von Web Serial prüfen. *Commit:* `G2: Add web flasher on GitHub Pages` Der Release-Job (Upload, `attest-build-provenance`) läuft erstmals mit dem ersten Tag. Stand: Seite mit einem Release-Kandidaten live, Release-Job korrigiert (Download des einzelnen Build-Artefakts); offen ist das Flashen beider Boards über die Seite.
- [ ] **G5 · Rest Repo-Einstellungen.** Erledigt: Ruleset „main“ (ID 26945350) per API, private Vulnerability Reports und Dependabot-Warnungen eingeschaltet, CodeQL-Default-Setup aus. Offen `[Nutzer]`: Topics speichern, Vorschaubild `docs/img/social-preview.png` hochladen; optional Renovate. Kein Commit nötig.
- [ ] **G9 · Projektseite (Rest).** `[Nutzer]` Beschreibung, Website `https://dm8sb.github.io/CYD-HF-Generator/`, Topics, Vorschaubild 1280 × 640; CN1-Steckerraster (1,25 mm) und Pinfolge am Board bestätigen; Handbuch als Pages-Seite (nach G2). *Commit:* `G9b: Add manual page`
- [ ] **G12 · Gehäuse am Gerät prüfen (`[Nutzer]`).** `case/CYD_Si5351_Gehaeuse.scad` (vom Nutzer; rendert fehlerfrei, außen 90,6 × 57 × 32,3 mm): Si5351-Modul nachmessen (`si_l`, `si_w`, `si_sma`, `sma_axis_off`), drucken, Passung von Display, USB-C, SMA, Rasthaken prüfen; danach Foto in G10 und ggf. STL-Export als Release-Asset. *Commit:* `G12: Adjust the case after the test print`
- [ ] **G10 · Fotos (später, `[Nutzer]`).** Nach der Liste in `docs/hardware.md`: `board-cyd-front.jpg` und `board-cyd2usb-front.jpg` (MAIN-Seite, CH1 an), `si5351-wiring.jpg` (Modul mit den 4 Leitungen zu CN1 und CLK0–2), `cn1-connector.jpg` (Rückseite, Beschriftung lesbar), `setup-bench.jpg` (Aufbau mit Koax zum Messgerät), je ≤ 500 KB, kein WLAN-Name auf dem Display; ablegen unter `docs/img/`. Danach Platzhalter in `docs/hardware.md` aktivieren. Vorschaubild `social-preview.png` 1280 × 640 (< 1 MB) lädt der Nutzer unter Settings → General → Social preview hoch. Zwischenstand (G13): gerenderte Bilder in `docs/img/` stehen bis dahin in README, `docs/hardware.md`, `case/README.md` und Handbuch Anhang A, darunter eine gerenderte `social-preview.png` (liegt jetzt doch im Repo; durch eine Version mit Foto ersetzbar). *Commit:* `G10: Add hardware photos`

### Phase 8 – Messungen ([MESSPLAN.md](MESSPLAN.md))

Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` (ohne Medien); Befunde (M-n) sofort in Abschnitt 2 eintragen. Stufe 1 und 2 laufen **sobald Zähler und Oszilloskop verfügbar sind**; dieser Lauf ist die Referenz R0 (MESSPLAN 9.2).

- [ ] **S59 · Stufe 1 (Pflicht, ≈ 7 h).** MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10). *Commit:* `S59: Record tier-1 measurements`
- [ ] **S60 · Stufe 2 (Entscheidungen, ≈ 6 h).** MESSPLAN Abschnitt 6: Standard-Tuning (K5, F3, K6), IQ ohne Reset (D3, D5, E9), ungerade Teiler und Phasenfehler (E3, E4, E5), Störquellen und PDN (F6, F7), Pegel (F2), Drift (C7); Entscheidungen als E-n in die Spezifikation, N1–N8 bestätigen oder streichen. *Commit:* `S60: Record tier-2 measurements and decisions`
- [ ] **S61 · Stufe 1W (Pflicht ab WEB-HMI, ≈ 8 h + 24 h).** MESSPLAN 5.11 nach S58 auf beiden Boards; dazu L13, L15, Q1 aus Stufe 2; aus S54: Abbau einer verwaisten TCP-Verbindung nach ≈ 90 s (Keepalive), Sperre bei offenem AP (auch Portmapper und VXI-11 aus LX1), mDNS `_scpi-raw._tcp`; L-S6: der AP geht in `WifiManager::update()` aus `loop()` aus, nicht im IP-Event (D14, W-S3), die Grenze ≤ 1 s hängt an der `loop()`-Latenz. Dazu `tools/web_check.py <Gerät>` ohne und mit Passwort, PASS/FAIL-Liste ins Protokoll (TEST-1); L-S5 mit `slowloris.py` (AF39). *Commit:* `S61: Record network and security measurements`
- [ ] **S62 · Stufe 3 (optional).** Charakterisierung und Bedientests mit Personen nach Bedarf (MESSPLAN Abschnitt 7). *Commit:* `S62: Record optional characterization measurements`
- [ ] **S63 · Handbuchwerte.** Pegeltabelle (F2), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise; DE und EN. *Commit:* `S63: Add measured values to the manuals`
- [ ] **S64 · Freigabe.** Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag; Merge nach `dev` und `main` nur auf Anweisung. *Commit:* `S64: Release`

### Phase 9 – Folgearbeiten (je ein Commit, nach Messergebnis oder Rückfrage)

- [ ] **N1 · Standard-Tuning** ändern, wenn die Stufe-2-Messungen es tragen (Rückfrage 1, E-19).
- [ ] **N2 · IQ-Abstimmen ohne Aussetzer** (Abschnitt 2.1).
- [ ] **N3 · Ungerade IQ-Teiler bei 180°/270°**, wenn E4 einen Fehler > 1° zeigt.
- [ ] **N5 · MultiSynth abgeschalteter Kanäle abschalten, PDN-Strategie** nach K2 und F7.
- [ ] **N6 · Störquellen-Abhilfen** (Backlight-PWM ≥ 20 kHz, Ruhemodus der Überwachung) nach F6, F9; „Neuzeichnen nur bei Änderung“ übernimmt AF12, den Backoff der SI5351-Suche ohne Chip AF53 (PERF-11).
- [ ] **N7 · TCXO-Option** nach C7, C8.
- [ ] **N8 · Anwendungskapitel „Mischer-Experimente“** (Abschnitt 2.5) nach F2, G1, G2, E10.
- [ ] **N9 · Sticky-Bit und „PLL locked“** (Abschnitt 2.1).
- [ ] **N10 · Hardware-Hinweise** in den Handbüchern.
- [ ] **N11 · Spätere Updates:** Unity 2.7.0, Arduino 3.3.13 / IDF 5.5.6; Advisory-Check je Release (AF26).
- [ ] **N13 · DMA und Doppelpuffer** (Abschnitt 2.3, nach der Migration).
- [ ] **N14 · Gekoppelter Betrieb CH2 = CH1 ± Offset (D18).** CH2 folgt CH1 mit festem Offset (positiv oder negativ, z. B. Überlagerungsempfänger mit fester ZF): Einstellung und Anzeige auf der CH2-Seite, Grenzen 8 kHz–160 MHz für beide, CH3-IQ und Spread berücksichtigen, SCPI/REST und Weboberfläche; Planung vor der Umsetzung. *Commit:* `N14: Add CH2 coupled to CH1 with offset`
- [ ] **N15 · Flash sparen (bei Bedarf).** Teilschriften mit `lv_font_conv` aus denselben Quellen (Montserrat, Font Awesome, OFL): Montserrat 28 (37 KB, nur die Frequenz auf CH1/CH2/CH3: Ziffern, `.`, Leerzeichen, `~`, `kHz`/`MHz`) auf ≈ 5 KB → ≈ 30 KB; Montserrat 20 (22 KB) ohne ungenutzte Symbole (genutzt: BACKSPACE, LEFT, RIGHT, MINUS, PLUS, TRASH, WIFI) → einige KB. Vorher alle Texte dieser Schriften prüfen, danach Bedientest. Nur mit eigenem IDF-Build (D14 ausgeschlossen): Bluetooth-Reste ≈ 6 KB, Core-Dump ≈ 13 KB, IPv6 ≈ 11 KB, Fehlertexte ≈ 8 KB. Schon erledigt: TFT_eSPI-Schriften −27,6 KB, Build-Pfade −0,9 KB (Stand 1 507 650 B, 47,9 %). Beim Subsetting `--no-kern` (Kerning ≈ 16,5 KB) und `-fno-exceptions` für `src/` testen (≈ 12,5 KB `.eh_frame`), PERF-13. *Commit:* `N15: Subset the large LVGL fonts`

### Phase 10 – Audit-Folgearbeiten (je ein Commit)

Jeder Schritt nennt Befund-IDs, Ort, Problem und Richtung selbst; der vollständige Bericht (`AUDIT_REPORT.md`, Stand `dev` @ `1367d56`) liegt in der Git-Historie bis `0eb2c1a`. „Konzept n“ = Abschnitt 3.9. Reihenfolge nach Priorität. Entschieden (D37, D38): **AF1–AF12 und AF55 vor S64 (Freigabe)**, AF13–AF19 im Release danach. AF5 und AF20 entfallen (D28, D24), AF29–AF31 sind erledigt (1.1). Aufwand: S < 2 h, M 2 h–1 d, L > 1 d. `[Nutzer]` = Einstellung, nur durch den Nutzer. Für UI-Änderungen gilt Regel 0.4 (Bedientest am Gerät vor dem Commit).

#### Priorität 1 – Sicherheit, Sicherheitszusagen, Freigabe

Erledigt (AF1–AF4, AF6–AF10; Bedientest AF6/AF8 bestanden, Details im Git-Verlauf).

#### Priorität 2 – Korrektheit und HF-Ruhe (vor S59/S60)

- [ ] **AF11 · WLAN-Status zwischenspeichern (S–M).** PERF-1, CQ-12, PERF-12. Heute ruft `WifiManager::updateStatus_()` je `loop()` `WiFi.SSID()`, `RSSI()` und `dnsIP()` synchron auf (≈ 450–570 Cross-Task-Aufrufe/s, ein Heap-`String`), im AP-Betrieb zweimal `softAPgetStationNum`; Disconnect-Gründe als Zahlen (`201`, `2`, `15`, `202`, `204`); `WebHmiServer::loop()` formatiert und hasht je Iteration (PERF-12). SSID/Maske/Gateway/DNS einmal bei `GOT_IP`, RSSI und AP-Stationen ≤ 1×/s mit einem `esp_wifi_sta_get_ap_info()` ohne `String`; `WifiPolicy::classifyFailure()` mit `WIFI_REASON_*`; `updateShared()` nur bei Änderung, Wipe nur nach Aufbau. Prüfung: Host-Test `classifyFailure`; Debug-Build mit Loop-Rate und `updateStatus_`-Zeit vorher/nachher, SCPI-TCP-Latenz (`device_check.py`), AP aus ≤ 1 s unverändert (L-S6). *Commit:* `AF11: Cache the WiFi status instead of polling the driver every loop`
- [ ] **AF12 · Neuzeichnen nur bei Änderung (M).** PERF-2, PERF-3 (C-15), PERF-4 (GUI-Teil), PERF-16, PERF-17, CQ-14. Heute enthält die Signatur (`page_main.cpp` `statusSignature_`, XOR-Hash) auf allen Seiten `rssi/5` und auf CONFIG den Heap in KB, `refresh_()` setzt alle Labels neu (LVGL 8.4 invalidiert bedingungslos): bis 3,3 Neuzeichnungen ≈ 70 % des Bildschirms/s; das Speicherzustand-Label alle 300 ms (C-15), der QR der AP-Seite bei jedem Refresh, jede Touch-Aktion zeichnet zweimal; Heap-Walks unter Spinlock. RSSI nur auf CONFIG/WIFI mit Hysterese. `StatusView` je Seite mit `operator==` statt XOR-Hash; Hilfen `setTextIfChanged`/`setColorIfChanged`; Heap-Zeile CONFIG alle 5 s in 4-KB-Schritten; QR nur bei geändertem Text; `lastRevision_` nach sofortigem Refresh nachziehen; `SYST:HEAP?` aus einem Cache (≤ 100 ms). Übernimmt den Teil „Neuzeichnen nur bei Änderung“ aus N6. Prüfung: Host-Test für `StatusView`-Vergleich (rein, `utils/`), Refresh-Zähler im Debug-Build (MAIN idle ≈ 0/min), Oszilloskop an TFT-SCLK; Bedientest. *Commit:* `AF12: Redraw GUI labels only when their content changes`
- [ ] **AF55 · WLAN ohne Energiesparen (S, D38).** PERF-15. `WiFi.setSleep(false)` bzw. `esp_wifi_set_ps(WIFI_PS_NONE)` nach jedem Start von Client/AP, volle Sendeleistung; Handbuch §14/§16 (Reaktionszeit, Stromaufnahme), neue E-n. Prüfung: Ping-Zeit vorher/nachher am Gerät (vorher 220–390 ms), `lxi benchmark`, Heap. *Commit:* `AF55: Turn off WiFi power save`
- [ ] **AF13 · Netzwahl-Ablauf nach `WifiPolicy` (M, Aufräumen).** TEST-9. Die Zustandsmaschine (`attempting_`, `CONNECT_TIMEOUT_MS`, `lastFailedSlot_`) liegt nur in `wifi_manager.cpp` und ist ungetestet; daran hängt „Netzwahl nach falschem Passwort“ (2.2). Scan-Merge (stärkste RSSI je SSID), Verbindungs-Timeout, `lastFailedSlot_`, Scan-Takt und AP-Leerlauf als reine Funktionen; `wifi_manager.cpp` ruft sie nur noch auf. Prüfung: Host-Tests (stärkeres Netz scheitert zweimal, beide versteckt, doppelte SSID, Timeout an der Grenze, `millis()`-Überlauf); Verhalten unverändert. *Commit:* `AF13: Move the WiFi connect and scan timing into WifiPolicy`
- [ ] **AF14 · Netzsuche mit Backoff (S–M).** PERF-5. Heute aktiver Scan alle 15 s ohne IP, auch ohne gespeichertes Netz und neben dem Rückfall-AP (der AP verlässt dafür etwa ein Viertel der Zeit seinen Kanal), dazu `WiFi.begin()` für ungesehene Netze. Ohne gespeichertes Netz kein Hintergrund-Scan; Backoff 15 s → 5 min, solange nichts gesehen wird; `WiFi.begin()` für ungesehene gespeicherte Netze (versteckte SSID) nur im Backoff-Takt (D32); mit Stationen am Rückfall-AP höchstens alle 2–5 min. Handbuch §16 (WLAN-Verhalten), ggf. A-n. Prüfung: Host-Tests in `test_wifi_policy`; am Gerät `ping 192.168.4.1` 5 min mit Rückfall-AP. *Commit:* `AF14: Back off WiFi scans while no network is reachable`
- [ ] **AF15 · Netzeinstellungen über `commitNet_` (S).** CQ-5. Heute steht „kopieren → speichern → Fehlerdialog → anwenden → löschen“ 6-mal in `page_wifi.cpp` und `gui.cpp`; die Kopie in `gui.cpp` legt die vollständigen `NetSettings` (Netzschlüssel, AP- und Web-Passwort) auf den Stack und löscht sie nicht. Eine Funktion `commitNet_(NetSettings&, NetApply)` speichert, meldet, wendet an und löscht; `Action::WifiMode` und `Confirm::DeleteNetwork` nutzen sie (fehlendes Löschen behoben). Prüfung: Build, Bedientest WIFI-Seiten. *Commit:* `AF15: Route all touch network changes through one commit helper`
- [ ] **AF16 · Ein Besitzer für die Netzeinstellungen (M).** CQ-2, Konzept 2. Heute merkt sich `NetChange` bis zu 120 s `prev_`, die WIFI-Seiten bleiben bedienbar: ein `Revert` verwirft alle Touch-Änderungen dieser Zeit (auch ein neues Web-Passwort), eine Touch-Änderung speichert das unbestätigte Web-`next` (nach Stromausfall gilt es, gegen W-S12). Zustandsmaschine im reinen `net/web_state.*` (bzw. `NetSettingsService`): Touch-Änderung während einer offenen Web-Änderung → `Busy` mit Hinweis „Web change pending“ und `Keep`/`Revert`; gespeichert wird nur ein bestätigter Stand. Handbuch §16, W-S12/A-76 präzisieren, neue E-n. Prüfung: Host-Tests (Touch-Änderung im Fenster, Rückfall nach Timeout, „Stromausfall“ = NVS hat den alten Stand); Bedientest mit und ohne Web-Passwort. *Commit:* `AF16: Give network settings a single owner during web changes`
- [ ] **AF17 · Explizites Speichern begrenzen (S).** SEC-V-3. Heute erzwingt `FREQ …;:SYST:SAVE` im Wechsel ≈ 10 echte NVS-Schreibvorgänge/s je Quelle, über SCPI-TCP ohne Anmeldung (`command.cpp`, `saveNow()` = `SettingKey::All`); bei 100 k Zyklen ≈ 1 Monat Dauerangriff. `SaveNow` je Quelle höchstens 1 je 10 s, weitere still zusammengefasst und spätestens 10 s nach dem letzten gespeichert, kein Fehler (D26); `saveNow()` schreibt nur Dirty-Schlüssel; optional Log-Warnung bei > N NVS-Schreibvorgängen/h. Handbuch §15/§16, W-E17/A-60. Prüfung: `test_command`/`test_settings` mit Fake-Uhr und NVS-Schreibzähler. *Commit:* `AF17: Rate-limit explicit saves from remote sources`
- [ ] **AF18 · WebSocket-Slots und Control-Frames (M).** SEC-V-2, SEC-5, PERF-18. Heute: drei WebSockets mit dem öffentlichen Token sperren `/` (503), auch mit Passwort; jede PING legt ein PONG in die unbegrenzte `_controlQueue` von ESPAsyncWebServer (Heap voll → Neustart); unaufgeforderte PONGs halten tote Clients; ein versteckter Tab hält einen Slot. `wsEvent` schließt nach PING-Flut (die UI pingt nie) und ignoriert unaufgeforderte PONGs für den 30-s-Test; mit Web-Passwort brauchen `/ws`, `GET /api/state` und `/api/token` eine Sitzung (D25; `/lxi/identification` bleibt offen, D33; Web-UI fragt beim Öffnen nach dem Passwort, Vertrag in `data/www/README.md`, `web_mock.py` und `web_check.py` ziehen nach); Web-UI schließt den WebSocket nach 60 s `hidden`, kein Heartbeat im Hintergrund; PING-Flut upstream (ESPAsyncWebServer) melden. Handbuch §16, Vertrag. Prüfung: Host-Test `WsClients` (Verdrängen), Python-Probe gegen das Gerät (PING-Flut, Heap stabil), `web_check`. *Commit:* `AF18: Bound WebSocket control frames and keep a slot for sessions`
- [ ] **AF19 · Push bündeln, Speichermangel abfangen (M).** PERF-6, PERF-8. Heute je Revisionsänderung ein JSON-Aufbau und Versand (bis ≈ 80 Zustände/s × 3 Clients, je 15–25 Allokationen); `make_shared` in `push()` wirft bei Fragmentierung `bad_alloc` → `terminate`, Neustart, Ausgänge aus; `accept()` prüft nur den freien Heap. Push höchstens alle 100 ms (erste Änderung sofort, dann der letzte Stand); in `push()` und `accept()` größten freien Block prüfen, sonst überspringen bzw. 503. Prüfung: Host-Test des Bündel-Takts (rein); am Gerät SCPI-Flut + 3 Tabs, WS-Frames/s im Browser, `SYST:HEAP?`. *Commit:* `AF19: Coalesce WebSocket pushes and skip them on low memory`

#### Priorität 3 – Lieferkette und CI

- [ ] **AF21 · CI-Trigger und Concurrency (S).** OPS-1, OPS-2, OPS-11, OPS-12, OPS-19. `push`/`pull_request` auf `[main, dev]`, Tags ausgenommen; Concurrency mit `event_name` und PR-Nummer, auf `main`/Tags kein Abbruch; CodeQL-PR-Läufe ohne `refs/pull/` abbrechen; Kommentar „Pflicht-Check im Ruleset main“ an den betroffenen `name:`; `gh run list --event push`. Danach einmal CodeQL auf `main` starten. Prüfung: Läufe auf `dev` und PR sehen, Alerts auf `main` wieder offen. *Commit:* `AF21: Narrow CI triggers and stop cancelling runs on main`
- [ ] **AF22 · Framework exakt pinnen, Release ohne Cache (M).** OPS-3 = LIC-4, OPS-10, OPS-17. `platform_packages` mit `framework-arduinoespressif32@4.30312.261024`, `-esp32-libs@3.3.12`, `toolchain-xtensa-esp-elf@14.2.0+20260121`, `native@<x.y.z>`; `pio pkg list` in die Job-Summary und die Release-Notes; CI-Schritt vergleicht aufgelöste Versionen und IDF-Commit mit `THIRD_PARTY_NOTICES.md`; Release ohne `~/.platformio`-Cache, CI speichert Cache nur auf `main`/`dev`; `tools/requirements-ci.txt` mit Hashes + Dependabot `pip`. Notices-Zeile 21 anpassen. Prüfung: kalter Build ergibt dieselben Paketversionen; Größe unverändert. *Commit:* `AF22: Pin the Arduino framework packages exactly`
- [ ] **AF23 · Release-Pfad vor dem Tag prüfen (M).** OPS-6. Benennung der Release-Dateien in ein Skript; Release-Workflow (ohne `release`/`deploy`) bei PRs nach `main` und bei Änderungen an `release.yml`, `tools/flasher_site.py`, `flasher/**`. Prüfung: PR-Lauf grün, absichtlich falscher Pfad schlägt im PR fehl. *Commit:* `AF23: Dry-run the release path on pull requests`
- [ ] **AF24 · Warnungs-Gate und Lint in der CI (S).** OPS-18 = CQ-20 = TEST-13. Firmware-Log auf `src/.*warning:` prüfen (oder `-Werror` in einer CI-Env), `-Wextra` in `[native_test]`, `-Werror` in `native-cpp20`; optional `ruff`, `node --check`, `lizard -C 30 -L 120` mit Ausnahmeliste; nicht verpflichtender Job für `touchlog`/`screens`. Regel 0.2: `native-cpp20` als CI-Gate nennen. Prüfung: absichtliche Warnung lässt die CI scheitern. *Commit:* `AF24: Enforce the zero-warning gate in CI`
- [ ] **AF25 · Reproduzierbare Stempel (S–M).** OPS-13. Heute meldet das Image eines Release-Kandidaten `<Version> | HEAD` (kein rc-Suffix, Branch `HEAD`, Lokalzeit ohne Zone; `tools/build_info.py`), die Tarballs sind nicht deterministisch. Version aus dem Tag (`GITHUB_REF_NAME`/`git describe`), Branch bei detached HEAD = Tag, Zeit aus `SOURCE_DATE_EPOCH` (Commit-Zeit, UTC); Tarballs sortiert, feste mtime, `gzip -n`. Handbuch (Info-Karte zeigt rc-Suffix), README. Prüfung: zwei Builds desselben Commits ergeben identische Tarballs; `build_info`-Check in der CI. *Commit:* `AF25: Stamp builds with the tag and commit time`
- [ ] **AF26 · Advisory-Prüfung der Abhängigkeiten (M).** OPS-8, S-20, W-S24, N11. Workflow wöchentlich und vor Tags: gepinnte Versionen aus `platformio.ini`/`esp-web-tools.json` gegen GHSA/OSV der Upstream-Repos; Fund → Job rot und Summary. Prüfung: Testlauf mit einer bekannten alten Version. *Commit:* `AF26: Check pinned dependencies against advisories`
- [ ] **AF27 · Werkzeug-Hygiene (S).** OPS-9, SEC-7, SEC-8. `flasher_site.py:509` und `web_check.py:154` mit `re.I` bzw. HTML-Parser, `tools/screens/main.cpp` mit eingeschränkten Rechten; `git rm --cached tools/__pycache__/…pyc`, `.gitignore` um `__pycache__/`, `*.pyc`, `.codegraph/`; `getpass` bzw. `CYD_WEB_PASSWORD` in `rest_example.py`/`web_check.py`. `[Nutzer]` Fehlalarme in GitHub begründet schließen. Prüfung: CodeQL-Lauf, `git status` sauber. *Commit:* `AF27: Fix tool CodeQL findings and ignore caches`
- [ ] **AF28 · Repo-Einstellungen (`[Nutzer]`, kein Commit).** OPS-4 (Tag-Ruleset `refs/tags/v*`: Erstellen/Ändern/Löschen nur Admin, Immutable Releases), OPS-7 (`allowed_actions: selected`, `sha_pinning_required: true`), OPS-12 (`allowed_merge_methods: ["merge"]`), OPS-16 (Dependabot-Security-Updates, Non-Provider-Patterns, Fork-Freigabe „all external contributors“, Wiki aus oder beschränkt, Environment `copilot` entfernen, Topics). Prüfung: API-Abfrage der Einstellungen. Stand: Tag-Ruleset und Immutable Releases (D29), OPS-7, OPS-12 und Wiki aus (D41) gesetzt; offen aus OPS-16: Dependabot-Security-Updates, Non-Provider-Patterns, Fork-Freigabe, Environment `copilot`, Topics.

#### Priorität 4 – Tests

- [ ] **AF32 · Routing und Body-Zusammenbau als reines Modul (M–L).** TEST-1, CQ-6 (Routen). Heute sind die Routen in `handle()`, `RequestGuard` und `isApi`/`isWs` doppelt gepflegt und `web_server.cpp` ist nicht host-kompiliert. `net/web_routes.*`: Routentabelle (Methode, Pfad, Handler-Art, Sitzung) für `handle()` und `RequestGuard`, Body-Zusammenbau (`index/len/total`, NUL wird nicht mehr per `strlen` abgeschnitten → `-102`). Prüfung: Host-Tests (alle Pfade aus `data/www/README.md`, 404, GET auf POST-Pfad, `/index.html`, 4. WebSocket → 503, Body in 3 Teilen, `index+len > total`, eingebettetes NUL); `build_src_filter`, CLAUDE.md. *Commit:* `AF32: Extract web routing and body assembly into a tested module`
- [ ] **AF33 · Job-Postfach als reines Modul (M).** CQ-6 (JobSlot). Das Job-Postfach in `WebHmiServer::Impl` (`Free/Queued/Taken/Done/Abandoned`, `malloc`/`free` an 4 Stellen, `runJob` in `async_tcp`, `serveJob` in `loop()`) ist das einzige Cross-Thread-Protokoll ohne Host-Test. `JobSlot` (take/finish/abandon/collect, Besitz des Ausgabepuffers) in `net/web_state.*`. Prüfung: Host-Tests für Abandon, späte Fertigmeldung, doppeltes Abholen; Gerät: Scan und Netzänderung aus dem Web. *Commit:* `AF33: Extract the web job mailbox and test its races`
- [ ] **AF34 · Python-Werkzeuge und Vertragsvektoren in der CI (M).** TEST-8, TEST-1 (Teil 3), CQ-7/CQ-19 (CI-Teil), OPS-6 (`--self-test`). CI-Job: `size_report --self-test`, `web_mock` + `web_check` (ohne/mit Passwort, `--login-all`), `fake_scpi_device` + `device_check --tcp`; `web_mock.py` mit `allow_reuse_address = False`; `embed_www.py` bricht bei `innerHTML`/`outerHTML`/`eval(`/`new Function` ab; gemeinsame JSON-Vektoren (Anfrage → Status, Code) für `test_http_guard`/`test_rest_json` und einen Python-Test des Mocks. Prüfung: CI grün, absichtliche Abweichung im Mock fällt auf. *Commit:* `AF34: Run the tool checks and shared contract vectors in CI`
- [ ] **AF35 · Grenzfälle, Identify und Fehlerpfade (M).** TEST-4, TEST-5, TEST-10, TEST-14. Überlebende Mutanten (TEST-4): M2 `app_controller.cpp` CH3-Master genau bei 100 MHz, M5 `wifi_policy.cpp` RSSI-Gleichstand, M6 `command.cpp` Bruchteil beim Refill des Ratenlimits, M7 `request_guard.cpp` Authorization 1024/1025 B, M10 `sanitize.cpp` überlanges `E0 9F BF`, M12 umgekehrte Blinkphase von Identify (TEST-5: `updateIdentify_` nach dem ersten Aufruf nie ausgeführt), M13 `settings.cpp` Autosave-Grenze. Fälle: CH3 an IQ_MIN/IQ_MAX, Ratenlimit im 75-ms-Raster und über den `millis()`-Überlauf, Authorization 1024/1025, fünf UTF-8-Grenzfälle (auch in `corpus/sanitize`), Autosave `MS-1`/`MS`, RSSI-Gleichstand dokumentieren; `test_identify_blinks_and_restores_backlight` (Duty im Wechsel, danach eingestellte Helligkeit, zweites `identify()`, Helligkeitsänderung beim Blinken). Fehlerpfade (TEST-10): `SI5351Manager::begin()` (75 % Zweige) mit `failAfter`, `initReads`, NoReadback, `failWrites`; `i2c_bus.cpp`, Logger-Schnittzeile, `touch_cal.cpp`, NVS-Schreibfehler in `settings.cpp`, `Failure::Inconsistent`. TEST-14: gemeinsamer Zufallszustand in `test_si5351` und `test_rest_wifi`, Seed in `setUp()` zurücksetzen und in der Meldung ausgeben. Prüfung: die überlebenden Mutanten M2, M5–M7, M10–M13 werden getötet (Gegenprobe). *Commit:* `AF35: Pin boundary, identify and error-path behavior in host tests`
- [ ] **AF36 · Host-Tests unter ASan/UBSan (S–M).** TEST-6. Env `native-asan` (Linux, `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`, Link-Flags per `extra_scripts`) in `ci.yml`. Prüfung: CI grün; absichtlicher Überlauf in einem Test wird gemeldet. *Commit:* `AF36: Run the host tests under ASan and UBSan in CI`
- [ ] **AF37 · Fuzz-Ziele für Vorfilter und `RestWifi` (M).** TEST-7. `fuzz_prefilter` (Chunks, Zeitschritte, Monotonie, Grenzen 414/431/413, differentiell gegen `RequestGuard`), `fuzz_rest_wifi` gegen unabhängige Referenz (unbekannter Schlüssel → −108, PSK nie im Echo, Ablehnung → `next == cur`); `fuzz.yml`, `test/fuzz/README.md`. Prüfung: 60-s-Lauf je Ziel in der CI, danach 10-Mio.-Lauf vor dem nächsten Release. *Commit:* `AF37: Add fuzz targets for the pre-filter and the WiFi REST parser`
- [ ] **AF38 · NVS-Grenzen im Fake, Upgrade-Test (S–M).** TEST-11 (Abschnitt 2.3 „Fakes“). Fake lehnt Schlüssel/Namespaces > 15 Zeichen und Strings > 4000 B ab; `test_upgrade_from_old_keys` mit dem vollständigen Schlüsselsatz des letzten Release. Prüfung: Test scheitert mit einem absichtlich zu langen Schlüssel. *Commit:* `AF38: Model NVS limits in the fake and test the upgrade`
- [ ] **AF39 · Messhelfer für S61 (M).** TEST-2. `tools/slowloris.py` (L-S5: 30 offene Anfragen, Abschluss nach 10 s), fehlende Helfer aus dem MESSPLAN-Anhang anlegen oder dort streichen (`lsec_check.py`, `scpi_selftest.py`, `touchstat.py`, `counter_log.py`), Firmware-Env mit `TEST_FAULT_IQ=1` für die Mess-ID A8 (MESSPLAN), `device_check.py`-Profile für L6/L22. `tools/README.md`, MESSPLAN-Anhang. Prüfung: Helfer gegen Mock/Fake. *Commit:* `AF39: Add the measurement helpers for tier 1W`

#### Priorität 5 – Barrierefreiheit

- [ ] **AF40 · Web-UI: Reflow, Kontrast, Struktur, Bewegung (S–M).** A11Y-1 + A11Y-20, A11Y-2, A11Y-13, A11Y-18, A11Y-19. `@media (max-height:500px)` ohne sticky/fixed, `nav{flex-wrap:wrap}`, flachere Kopfzeile; `--c1` für Text `#f4615d`; `<main>`, `h1`, Sections mit `aria-labelledby`; `prefers-reduced-motion`; Hilfe-Button-Name, `thead`. `data/www/README.md`, Handbuch §16 (falls Bilder/Text betroffen), `render_images.py web`. Prüfung: axe-core-Lauf (320 × 256 CSS-px = 400 % Zoom, 1280 × 720 bei 200 %; heute belegen Kopfzeile und Leiste 222 von 256 bzw. 198 von 360 px, „WiFi“ liegt bei 320 px außerhalb, CH1-Text 4,13 : 1) ohne `color-contrast`-/Landmark-Verstöße, 400-%-Probe; Sichtprüfung im Browser. *Commit:* `AF40: Make the web UI reflow at high zoom and fix CH1 text contrast`
- [ ] **AF41 · Tastenkürzel abschaltbar und lokal (M).** A11Y-3, A11Y-4, A11Y-10, A11Y-15, A11Y-16. Schalter „Single-key shortcuts“ im Hilfedialog, Standard aus (D34; `localStorage` mit try/catch); Buchstaben-/Ziffernkürzel nur bei Fokus auf `body`/Kanalbereich; Frequenzanzeige als `role="spinbutton"` mit `aria-valuetext`, Pfeile/`PgUp` nur dort; `aria-current` für den gewählten Kanal; Leerzeichen in der Ziffernanzeige; Selects erst bei Enter/Blur bzw. entprellt. Tastaturtabelle in `data/www/README.md`, Handbuch §16. Heute wirken `1`/`2`/`3`, `f`, `o`, `?`, `+`, `-` überall außerhalb von Eingabefeldern (`app.js` `onKey`), dreimal `PgUp` nach dem Laden hob CH1 von 10 auf 100 MHz, der Zielkanal ist nur am Rahmen erkennbar, jede Pfeiltaste in einer Auswahlliste schreibt sofort. Prüfung: Tab-Durchlauf, `PgUp` auf `body` scrollt; Bedientest. *Commit:* `AF41: Scope keyboard shortcuts and let users turn them off`
- [ ] **AF42 · Statusmeldungen und Beschriftungen (M).** A11Y-5 bis A11Y-9, A11Y-11, A11Y-12, A11Y-14, A11Y-17. Dauerhafte `status`-/`alert`-Regionen; `#save`/`#hw`; `aria-describedby`/`aria-invalid`; Dialog `aria-describedby`; Fokus auf `Confirm` der WLAN-Frist mit Ansage; Gruppen für Master/Phase, Korrekturschritt; Wertansage nach −/+ und Enter, Fokus bleibt im Feld; eindeutige Labels WiFi/CONFIG; `#conn` nur bei Änderung. Prüfung: Accessibility-Tree-Dump, axe; optional Stichprobe mit NVDA. *Commit:* `AF42: Announce status changes and label grouped controls in the web UI`
- [ ] **AF43 · Gerät: CH1-Textfarbe und Aktionsfarbe in der Palette (S).** A11Y-21, CQ-8. `Palette::CH1_TEXT = 0xF4615D` für Text in Kanalfarbe, `Palette::ACTION_AMBER`/`Colors::action()` statt `0xF9A825`; Paare in `test_contrast` (≥ 4,5 für Text); Modus-/Infozeilen, wo Platz ist, auf Montserrat 14 (D36). Prüfung: `test_contrast` scheitert vor dem Fix; `render_images.py screens`; Bedientest. *Commit:* `AF43: Use a readable CH1 text color and keep all colors in the palette`

#### Priorität 6 – Struktur und Kleinkram

- [ ] **AF44 · Konstanten, Tabellen, Kommentare (S, Aufräumen).** CQ-9, CQ-16, CQ-18, CQ-21 + DOC-19, CQ-23. Feste Zahlen in Texten („120 s“, „15 min“, „max. 3“, `{{TOKEN}}` mit Länge 9) in `gui.cpp`, `page_wifi.cpp`, `web_server.cpp`; doppelter Reset-Code in `AppController`; Namentabellen in `rest_json.cpp`, `scpi.cpp`, `gui.cpp` ohne Längenprüfung; implizite „aktuelle Quelle“ in `AppController` (ein Setter ohne `execute()` liefe still als `Touch`). Texte aus Konstanten bzw. `static_assert`, `TOKEN_MARKER`; `applyDefaults_()`, `reconfigureReport_()`; `static_assert` auf Namentabellen, `DRIVE_OPTIONS`; Kommentar-Drift (`net_settings.h:79`, `gui.h`, `constants.h:86`, `wifi_manager.cpp:87`, `request_guard.h`); RAII `SourceScope`. Prüfung: Host-Tests unverändert grün. *Commit:* `AF44: Tie texts to constants and fix stale comments`
- [ ] **AF45 · Gemeinsame IPv4-Helfer (S, Aufräumen).** CQ-10. Heute 2 Parser, 4 Formatierer, 3 Konverter in `net_settings.cpp`, `request_guard.cpp`, `rest_json.cpp`, `scpi_tcp.cpp` u. a. `utils/ipv4.h` (parse/format, `fromIpAddress` gerätseitig); bestehende Tests als Regression. *Commit:* `AF45: Share one IPv4 parser and formatter`
- [ ] **AF46 · Exakte Rundung am Numpad (S–M).** CQ-11. Das Numpad (`frequency_utils.cpp`) rundet in `double`, REST und SCPI (`scpi.cpp`) exakt; bei „x,5 Hz“ ist 1 Hz Unterschied möglich (abgeleitet). Gemeinsamer Kern `scaleDecimal()` (half-up, ganzzahlig) für Numpad, REST und SCPI; ein `pow10u`. Prüfung: Host-Test „x,5 Hz“ gleich auf allen Wegen. *Commit:* `AF46: Round numpad frequencies exactly like remote input`
- [ ] **AF47 · Texteingabe und WLAN-Texte als reine Logik (M).** CQ-4, TEST-12. Heute liegen die sechs `EntryPurpose`-Zweige (Name, IP-Feld, Web-Passwort mit „leer = entfernen“, AP-Passwort, SSID → Passwort, verstecktes offenes Netz) im LVGL-Handler von `page_wifi.cpp`, „Zweck setzen und navigieren“ steht 7-mal dort; WLAN-Statustext und Seitenaufteilung der Scan-Liste ebenso. `openEntry_()`, `InputLogic::submitEntry()`, `StatusText::wifiLine()`, Seitenaufteilung der Scan-Liste in `InputLogic`. Prüfung: Host-Tests je Eingabezweck und Zustand, Textbreite; Bedientest WIFI-Seiten. *Commit:* `AF47: Move text entry validation and WiFi status texts into tested logic`
- [ ] **AF48 · Anzeigetexte und Grenzen teilen (M).** CQ-7. CH3-Statustexte, Bereichshinweise, 8 kHz–160 MHz, Schrittliste und SSC-Labels stehen in `page_ch3.cpp`/`page_channel.cpp`, `app.js`, `web_mock.py` und `fake_scpi_device.py` statt einmal (`constants.h`, `StatusText`). `StatusText::ch3Info()`, `limits: {fmin, fmax, steps}` in `/api/state` (Vertrag, Mock), `app.js` liest sie; Python-Konstanten aus `constants.h` erzeugen. Prüfung: Host-Tests, `web_check`. *Commit:* `AF48: Share CH3 texts and frequency limits between device and web UI`
- [ ] **AF49 · `Gui` aufteilen (L, drei Commits).** CQ-3. Heute ≈ 100 Member, `handleAction_` (45 Actions, CCN 53) reicht 16 WLAN-Actions an `handleWifiAction_` (218 Zeilen, CCN 65), `confirm_` ist eine if/else-Kette mit 9 Zweigen, ein `Widgets`-Struct für alle Seiten (`entrySsid_` für drei Zwecke). **AF49a** `confirm_` über `ConfirmSpec`-Tabelle; **AF49b** Klasse `WifiPages` mit eigenem Zustand und Widgets; **AF49c** `GuiServices` statt 9 Callbacks. Je Teil Bedientest. *Commits:* `AF49a: Drive confirmation dialogs from a table`, `AF49b: Split the WiFi pages out of Gui`, `AF49c: Pass GUI services as one struct`
- [ ] **AF50 · `WebHmiServer::Impl` aufteilen (M–L, Aufräumen).** CQ-6 (Rest). Heute ein Struct mit ≈ 910 Zeilen über drei Ausführungskontexte (`async_tcp`, `loop()`, Login-Task); die Zuordnung der Felder steht nur in Kommentaren. `ConnectionGate`, `LoginService`, `Pusher` mit dokumentiertem Thread je Struct, nach AF32/AF33. Prüfung: `web_check` gegen das Gerät unverändert. *Commit:* `AF50: Split the web server into connection, login and push parts`
- [ ] **AF51 · LED/Heap-Schichtung, Planer aufteilen (S, Aufräumen).** CQ-13, CQ-17. Heute steuert die GUI die Status-LED und liest den Heap direkt (`gui.cpp`, `page_config.cpp`, `page_main.cpp`; Identify hat zwei Besitzer, die LED bleibt dunkel, wenn die GUI nicht aktualisiert); `planStandalone` in `si5351_plan.cpp` behandelt alle drei Tuning-Modi (CCN 31, Literal `8` fünfmal). `StatusLed` aus `AppController`/`main`, Heap per Callback; `planLowNoise_`/`planInteger_`/`planFractional_`, `MS_MIN_FRACTIONAL_DIV`, `DividerRange`. Prüfung: `test_si5351_plan` unverändert grün; Identify am Gerät. *Commit:* `AF51: Move the status LED out of the GUI and split the PLL planner`
- [ ] **AF52 · Web-UI- und Python-Werkzeuge aufräumen (M, Aufräumen).** CQ-22, CQ-19. Heute ≈ 20 veränderliche Globale mit Ein-Buchstaben-Namen in `app.js`, `render` CCN 46, `IPRE` nimmt führende Nullen an, die die Firmware ablehnt; `flasher_site.py` (CCN 82), `web_check.py` (64), `fake_scpi_device.py` (46) mit linearen Prüflisten, `host:port` unterschiedlich geparst. `render` aufteilen, `conn`/`auth`-Objekte, `IPRE` ohne führende Nullen; `check_site`/`web_check.main`/`fake_scpi_device.execute` aufteilen, einheitliches `host:port`. Prüfung: AF34-Job grün. *Commit:* `AF52: Split the large web UI and tool functions`
- [ ] **AF53 · Logger und SI5351-Suche (S).** PERF-10, PERF-11 (Teil von N6). Heute `Serial.printf("%s")` ohne TX-Puffer (≈ 8,7 ms je 100 B unter dem Mutex, auch in `async_tcp`); ohne SI5351 baut `connect_()` den I2C-Treiber alle 2 s zweimal ab und auf. `Serial.write` statt `printf("%s")`, `setTxBufferSize(1024)`; Backoff der Suche ohne Chip (2 s erste Minute, dann 10 s). Handbuch §12 (Hinweis Suchtakt). Prüfung: `test_app_controller` (Takt), Heap über 1 h am Board ohne SI5351. *Commit:* `AF53: Make logging non-blocking and back off the SI5351 search`
- [ ] **AF54 · Asynchrone REST-Antworten, JSON ohne Allokation (M–L).** PERF-7, PERF-9. Heute blockieren REST-Schreibvorgänge und WLAN-Jobs `async_tcp` bis 80/180 ms mit 1-ms-Polling (laut W-S23 akzeptiert), der ArduinoJson-Zustandsaufbau allokiert je Aufruf ≈ 20–30-mal. `req->pause()` + Antwort aus `onPoll` für Schreibvorgänge und Jobs; Writer mit festem Puffer (bzw. Arena) für Zustand und Fehler. Prüfung: Host-Test des Writers gegen ArduinoJson-Ausgabe; Gerät: WS-Latenz während Halten-Wiederholen in einem zweiten Tab. *Commit:* `AF54: Answer REST writes asynchronously and build JSON without allocations`

In bestehende Schritte eingeordnet (kein eigener AF-Schritt): PERF-13 → N15, PERF-14 und der PERF-8-Worst-Case → S57, PERF-15 → AF55 (D38) und S49, TEST-2 → S41c/S59/S61 (+ AF39), DOC-14 → AF4.

---

## 5. Pflege dieser Datei

- Der Haken eines Schritts steht im selben Commit wie die Änderung; erledigte Schritte, Befunde, beantwortete Rückfragen und umgesetzte Konzepte werden beim nächsten Aufräumen entfernt (Abschnitt 1.1 fasst sie zusammen, Details in der Git-Historie).
- Neue Befunde (M-n aus Messungen, B-n aus Bedientests) kommen mit Bezug in Abschnitt 2 und als Schritt in Abschnitt 4; bestehende Schrittnummern bleiben stabil.
- Neue Entscheidungen ohne Datum in Abschnitt 1.3; dauerhafte Entscheidungen zusätzlich als E-n in der Spezifikation.
- Die Befund-IDs des Projekt-Audits (SEC-n, SEC-V-n, LIC-n, CQ-n, DOC-n, PERF-n, OPS-n, TEST-n, A11Y-n; Bericht `AUDIT_REPORT.md` in der Git-Historie bis `0eb2c1a`, die offenen Befunde stehen mit ihren Fakten in Abschnitt 2 und Phase 10) bleiben wie die früheren Audit-IDs als Namen gültig, auch nachdem der Befund erledigt ist. Schritte aus dem Audit heißen AF-n (Phase 10) und der LXI-Schritt LX1, weil A-n und L-n schon Mess-IDs im MESSPLAN sind.
