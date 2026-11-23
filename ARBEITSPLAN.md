# Arbeitsplan – CYD HF Generator

Stand: Release **2.1.0** (nach 2.0.0: einheitliche Bedienung U27, HF-Ruhe und WLAN-Ablauf AF11–AF19/AF55, Audit-Folgearbeiten Priorität 3, 4 und 6, Framework Arduino-ESP32 3.3.13 / ESP-IDF 5.5.6 über pioarduino N11, gekoppelter CH2 N14, Bildschirmdrehung N16, Handbuch §17/§18, API-Seite) · Arbeitszweig `dev`, `main` nur per Pull Request (Merge-Commit) · auf `dev` nach 2.1.0: Doku als Pages-Seiten (G9), Werkzeuge, Doku konsolidiert (DK1) · offen: N9, Unity 2.7.0 (N11), Rückkehr ins PlatformIO-Register (E-51); die Messungen (S41c, S49, S57, S59–S63) kommen zuletzt, danach S64

Einzige Planungsdatei: offene Befunde, WEB-HMI-Konzept, Entscheidungen und Aufgabenliste. Sie ersetzt `todo.md`, `AUDIT_2026-11.md` und `KONZEPT_WEB_HMI.md`. Die Befund-IDs aus dem Audit (C-n Code, S-n Security, U-n Bedienung, P-n Plattform, T-n Tests) und dem Konzept (W-E, W-S, V-n) bleiben als Namen gültig. Messungen am Gerät: [MESSPLAN.md](MESSPLAN.md). Verhalten: [USER_MANUAL_DE.md](USER_MANUAL_DE.md); Anforderungen und Entscheidungen: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md).

---

## 0. Arbeitsregeln

1. **Ein Schritt = ein Commit.** Jeder Schritt der Aufgabenliste (Abschnitt 4) endet mit genau einem Commit. Größere Schritte werden vorher geteilt (S25a/S25b); kein Sammel-Commit.
2. **Erst das Gate, dann der Commit:**
   - Host-Tests grün in `native-win` und `native-win-fault` (CI: `native`, `native-fault` und `native-cpp20` mit `-Werror`);
   - 0 Warnungen in `src/` (`-Wall -Wextra`, Host-Tests ebenso); die CI scheitert an Warnungen in `src/`/`include/` der Firmware-Builds und an jeder Warnung in `native-cpp20` (AF24);
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
| 6b LXI | 2.0.0 | LX1: Portmapper (UDP/TCP 111), VXI-11-Kernkanal (TCP 9009), mDNS `_lxi._tcp`/`_vxi-11._tcp`, `/lxi/identification` (E-37, D33, D42, D43); Gerätetest mit lxi-tools 2.8, PyVISA und python-vxi11 |
| 7 GitHub (Teil) | 2.0.0 | Release-Workflow mit Herkunftsnachweis und Webflasher-Seite (G1), Release nur von `main` mit Tests und ohne Überschreiben (AF7), Tag-Ruleset und Immutable Releases (D29), `SECURITY.md` und CodeQL (G4), Dependabot (G6), Größenbericht (G7), Issue- und PR-Vorlagen (G8), Badges und `docs/hardware.md` (G9a); Lizenz-Hinweise, Lizenzpaket, LGPL-Quellen, ESP Web Tools selbst gehostet (G11); gerenderte Bilder (G13); Ruleset `main` und Repo-Einstellungen (G5, D41); Webflasher an beiden Boards erprobt (G2, ohne Handbuchabschnitt); OpenSCAD-Gehäuse, Probedruck vom Nutzer bestätigt (G12); KI-Hinweis |
| 10 Audit (Teil) | 2.0.0 | Projekt-Audit, Rückfragen entschieden (D22–D43); AF1–AF4, AF6–AF10 (Sicherheit, Release-Integrität, Doku, E-40, E-41); AF29–AF31 (Notices, `LICENSE-EXCEPTION.md` E-38, Links auf den Tag E-39, SPDX D40); CodeQL-Funde behoben bzw. als Fehlalarm geschlossen |
| 2.0.0 | 2.0.0 | Version 2.0.0, Historie neu ab 2.0.0 (D44, D45); `v2.0.0-rc1` getestet, `v2.0.0` veröffentlicht |
| Nach 2.0.0 | `main` | U27 einheitliche Bedienung nach Nutzer-Feedback: CONFIG in fünf Registern (E-42), eine Kopfzeile mit `◀` auf allen Seiten außer MAIN (E-43), keine Anzeige `Saved`/`Saving...` (E-44); Handbuch: Wirkung der CONFIG-Optionen ausführlich; API-Referenz `docs/REMOTE_API.md` |
| Nach 2.0.0 (2) | `dev` | AF55 WLAN ohne Energiesparen (E-45); AF11 WLAN-Status zwischengespeichert; AF12 Neuzeichnen nur bei Änderung; AF13/AF14 Netzwahl in `WifiPolicy`, Suche mit Backoff (E-46); AF15/AF16 ein Besitzer der Netzeinstellungen, Hinweis `Web change pending` (E-47); AF17 `SAVE` höchstens alle 10 s, nur geänderte Werte; AF18 Login auch für Lesen mit Web-Passwort, WebSocket gegen PING-Flut und tote Clients (E-48); AF19 Push gebündelt, Speichermangel abgefangen; AF44 Texte an Konstanten; AF45 ein IPv4-Helfer; AF46 NUMPAD rundet exakt; AF48 Grenzen in `/api/state`; AF51a Planer aufgeteilt; AF52 Web-UI und Werkzeuge aufgeräumt; AF53 Log-Puffer und SI5351-Suche mit Backoff (1 h am Gerät geprüft); Handbuch §17 und §18 (Pegel typisch, Messwerte mit S63); Bedientest AF12/AF16/AF18 bestanden
| 2.1.0 | 2.1.0 | Priorität 3 (CI-Trigger, Warnungs-Gate, exakte Pins, reproduzierbare Stempel, Release-Probelauf als Pflicht-Check, Advisory-Prüfung, Werkzeug-Hygiene, Repo-Einstellungen; D49), Priorität 4 (Vertragsvektoren, ASan/UBSan, neue Fuzz-Ziele, NVS-Grenzen, Messhelfer; ein Log-Fehler behoben), Priorität 6 (Texteingabe als Logik, `Gui` und Webserver aufgeteilt, LED aus der GUI, asynchrone REST-Antworten E-50), N11 (3.3.13 / 5.5.6 über pioarduino, GHSA-ch63 behoben, E-51), N14 (CH2 = CH1 + Offset, E-52), N16 (`Rotate`, E-53); am Gerät geprüft |
| Nach 2.1.0 | `dev` | G9 Handbuch DE/EN, Hardware-Seite und Remote-API als Pages-Seiten mit Link- und CSP-Prüfung im Release-Probelauf (E-54); Werkzeuge kennen CH2-Kopplung und Drehung; DK1 Doku konsolidiert (README-Änderungen nach Themen, Links und Parität geprüft); 10-Mio.-Fuzz-Lauf |

Host-Tests: 432 Fälle in `native-win` und 4 in `native-win-fault` (nach AF19).

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
| D34 | Einzeltasten-Kürzel der Weboberfläche sind standardmäßig **aus**; Schalter „Single-key shortcuts“ im Hilfedialog (im Browser gespeichert); eingeschaltet wirken sie nur im Kanalbereich, nie in Eingabefeldern (A11Y-3, AF41; Nutzer) — entfällt mit D48 |
| D35 | CH1-Schrift in Kanalfarbe auch am Gerät `#F4615D` (5,6 : 1 auf den Karten statt 4,1 : 1), Flächen und Rahmen bleiben `#E53935`; Web und Gerät gleich (A11Y-21, AF43; Nutzer) — entfällt mit D48 |
| D36 | Graue Info- und Modus-Zeilen am Gerät auf Montserrat 14, wo es ohne Abschneiden passt; eng bemessene Stellen (Statusleiste, festes Zeilenraster) behalten 12; Prüfung am Host-Rendering und am Gerät (A11Y-21, AF43; Nutzer) — entfällt mit D48 |
| D37 | Umfang vor der Freigabe (S64): AF1–AF10, AF28 (Einstellungen) und zusätzlich AF11/AF12 (HF-Ruhe), damit S59/S60 den endgültigen Stand messen (Nutzer) |
| D38 | WLAN vorerst ohne Energiesparen (`WIFI_PS_NONE`) und mit voller Sendeleistung: Reaktionszeit zuerst. Messungen und daraus abgeleitete Änderungen kommen zuletzt, die Hardware ist fest. Anlass: im LX1-Gerätetest 220–390 ms Ping mit Modem-Sleep, pyvisa-py scheiterte am Standard-Verbindungs-Timeout (PERF-15, S49; Nutzer) |
| D39 | Strukturarbeiten (CQ-3, CQ-6; AF49, AF50) erst nach der Freigabe: sie erscheint mit dem geprüften Stand (Nutzer) |
| D40 | SPDX-Köpfe (`SPDX-FileCopyrightText: 2026 DM8SB`, `SPDX-License-Identifier: GPL-3.0-or-later`) in allen eigenen Dateien, Rest über `REUSE.toml`; neue Dateien bekommen sie ebenfalls. `LICENSES/` wird vorerst nicht REUSE-konform umgebaut, kein `reuse lint` in der CI (LIC-8, AF31; Nutzer) |
| D41 | Repo-Einstellungen per API gesetzt: Wiki aus; Actions nur von GitHub selbst (`allowed_actions: selected`, `github_owned_allowed`, keine verifizierten Ersteller, keine Muster) mit SHA-Zwang (`sha_pinning_required`); PRs nur als Merge-Commit (Repo-Einstellung und Ruleset `main` `allowed_merge_methods: [merge]`) (OPS-7, OPS-12, OPS-16, AF28; Nutzer) |
| D42 | mDNS kündigt `_scpi-raw._tcp`, `_vxi-11._tcp` und `_lxi._tcp` auch während der Sperre bei offenem AP an; keine Rücknahme (am offenen AP hängt meist nur ein Gerät zur Einrichtung) (LX1; Nutzer) |
| D43 | `/lxi/identification` bleibt ohne `MACAddress` und `LXIVersion` (W-S16, keine Konformitätsbehauptung) und ist damit nicht streng schemagültig; lxi-tools, PyVISA und python-vxi11 brauchen es nicht (LX1; Nutzer) |
| D44 | Nächstes Release heißt **2.0.0** (Core 3, Weboberfläche, Netzsteuerung, LXI). Die Git-Historie wird vor `v2.0.0-rc1` durch einen neuen Wurzel-Commit ersetzt; alte Tags und Releases (`v1.3.3`, Release-Kandidaten 1.4.0) werden gelöscht, Rulesets kurz ab- und wieder eingeschaltet. Ablauf: `v2.0.0-rc1` als Pre-Release, Test durch den Nutzer, dann `v2.0.0`; Messungen danach (Nutzer) |
| D45 | `v2.0.0-rc1` vom Nutzer getestet und für gut befunden; **2.0.0** erscheint ohne AF11, AF12 und AF55 und vor den Messungen. Diese folgen in 2.0.x; S64 bleibt die Freigabe nach den Messungen (Nutzer) |
| D46 | CONFIG in Registern statt Scrollliste (Nutzertests am resistiven Touch: Scrollen schlecht und uneinheitlich, Blättern besser bewertet): Registerleiste unten mit `Signal`, `Freq`, `Net`, `Device`, `Info`, je höchstens drei Zeilen; `Reconnect all` nach `Net`, Touch-Abgleich neben `Lock` in `Device` (E-42). Dazu eine Kopfzeile für alle Seiten außer MAIN: `◀` oben links eine Ebene zurück statt `MAIN` rechts, Seitentasten rechts, Netzliste blättert mit `▲`/`▼` (E-43). Am Gerät keine Anzeige `Saved`/`Saving...` mehr, nur Speicherfehler in Rot; das Handbuch nennt das Speichern nach einigen Sekunden (E-44, Nutzer) |
| D47 | Nächster Block selbständig: AF55, AF11, AF12, AF13–AF19, G2 und N8. N8 jetzt mit Rechen- und Datenblattwerten, Pegel als „typisch, nicht gemessen“ markiert, nach S63 durch Messwerte ersetzt. UI-Schritte (AF12, AF16, AF18) gesammelt am Ende am Gerät testen. Release erst danach, zusammen mit U27 und der API-Seite (Nutzer) |
| D48 | Priorität 5 (Barrierefreiheit, AF40–AF43) wird nicht umgesetzt: Gerät und Weboberfläche lassen sich für den Anwendungsfall gut genug bedienen. D34–D36 entfallen damit; die A11Y-Befunde bleiben nur als Namen (Nutzer) |
| D49 | Priorität 3: CI, Fuzz und CodeQL bei Push nur auf `main`, `dev` und `agent/**` (PRs nach `main` immer), Tags nur im Release-Workflow, auf `main` kein Abbruch (AF21); Framework exakt pinnen, Release ohne Cache, Probelauf des Release-Pfads in PRs nach `main` (AF22, AF23); Warnungen in `src/` lassen die CI scheitern, kein zusätzlicher Lint (AF24); Stempel aus Tag und Commit-Zeit (UTC), rc-Suffix auf der Info-Karte (AF25); wöchentliche Advisory-Prüfung, Fund = roter Lauf und Issue, auch vor Tags (AF26); Werkzeug-Hygiene mit CI-Schutz gegen eine versionierte `include/test_wifi_local.h` (AF27); Repo-Einstellungen per API (AF28) (Nutzer) |
| D50 | N15 (Flash sparen) wird vorerst nicht umgesetzt; Flash ist mit ≈ 48 % ausreichend frei (Nutzer) |
| D51 | N11: Arduino-ESP32 3.3.13 / ESP-IDF 5.5.6 (behebt GHSA-ch63-jxm5-vmvf) über die pioarduino-Plattform 55.03.313, weil das PlatformIO-Register 3.3.13 noch nicht anbietet; GHSA-ch63 wird nicht als Ausnahme akzeptiert, das Release wartet auf das Update. Unity 2.7.0 folgt, sobald es im Register steht. „Release dry run“ ist Pflicht-Check für `main` (Nutzer) |
| D52 | Parallel zu N11: Priorität 4 komplett (AF32–AF39). N9 später (Gerätetest mit SI5351 nötig), N13 entfällt (Anzeige reicht, RAM bleibt frei). N14 wird umgesetzt: Kopplung auf der CH2-Seite (Chips `Free` / `CH1 + offset`, die Frequenzkarte zeigt den Offset, +/− und NUMPAD ändern ihn, MAIN-Moduszeile `CH1 +455 kHz`), Offset −160 … +160 MHz in 1 Hz, liegt CH1 + Offset außerhalb 8 kHz–160 MHz, wartet CH2 (`WAIT`) und startet von selbst wieder. G9: Handbuch DE/EN, Hardware-Beschreibung und Remote-API als Webseiten auf GitHub Pages neben dem Webflasher, mit dem Release deployt, nach N11 (Nutzer) |
| D53 | N16: Taste `Rotate` als dritte Taste in der Zeile `Screen` des Registers `Device` (neben `Lock` und `Touch cal.`), schaltet zwischen 0° und 180° um; Umsetzung nach N14 (beide berühren GUI, Einstellungen und Fernsteuerung) (Nutzer) |

**Nicht umsetzen:** neue Funktionen außer N14 vorerst (D18); RGB-LED als Ausgangsanzeige (bleibt Seitenanzeige, E-3); doppelte Statusanzeige im Header bleibt (E-13); Kanalwahl im Header (Rest von U-16: kein Platz neben Zustand und `MAIN`, über MAIN und die Karten 2 Tipps); LVGL 9, LovyanGFX (außer als Fallback), espressif32 6.13.0, Secure Boot / Flash Encryption (S-22).

**Offene Rückfragen** (die Rückfragen zum Audit sind als D22–D43 entschieden):

1. **Standard-Tuning:** bleibt `Fractional`, oder wird `Low noise` Standard, wenn die Messungen (MESSPLAN Stufe 2) einen klaren Vorteil zeigen? (N1)

---

## 2. Offene Befunde

`[M]` = wartet auf eine Messung (MESSPLAN-ID in Klammern).

### 2.1 SI5351, HF und IQ

- [M] **MultiSynth abgeschalteter Kanäle abschalten** (K2, F7): CH1/CH2 im Zustand OFF nur über Output-Enable abgeschaltet, ihr MultiSynth läuft weiter; CLK2 bekommt `CLK2_PDN` nur bei Off state `Low`. Ist ein Ausgang mit `CLKx_PDN` hochohmig (K2), bei `High-Z` alle abgeschalteten Ausgänge mit PDN abschalten. F7 zeigt, ob es sich lohnt. → N5
- [M] **Störquellen der Plattform** (F6, F9): Backlight-PWM 5 kHz (Seitenlinien ±5 kHz möglich; Abhilfe ≥ 20 kHz oder 100 % ohne PWM), I2C-Überwachung alle 2 s (Abhilfe: Ruhemodus); das Neuzeichnen nur bei Änderung ist mit AF12 erledigt. → N6
- [M] **IQ-Abstimmen ohne Aussetzer** (D3, D5, K6, E9): Jede Frequenzänderung im IQ-Betrieb setzt die PLL zurück (1000 Schritte → 1000 Resets). Teiler mit Hysterese festhalten und nur die PLL ohne Reset schreiben (wie `Low noise` im Einzelbetrieb), dann bleibt die Phase erhalten; ebenso bei Korrekturänderungen. Host-Test „PLL-Änderung ohne Reset, Offset und Teiler unverändert“. → N2
- [M] **180°/270° bei ungeraden Teilern** (E4): Invertierung ergibt nur bei exakt 50 % Tastverhältnis 180°; der IQ-Teiler `min(127, ⌊900 MHz / f⌋)` ist in 54 % der Fälle ungerade (14,2 MHz → 63, 99 MHz → 9). Abhilfe: zwischen 4,77 und 90 MHz gibt es immer einen geraden Teiler mit VCO 600–900 MHz, oder 180° über den Phasen-Offset (2 × Teiler ≤ 127). → N3
- [ ] **Spannungseinbruch über Sticky-Bit erkennen:** `checkAlive()` erkennt einen SI5351-Neustart nur, solange ein Ausgang konfiguriert ist. Robuster: Register 1 (`SYS_INIT_STKY`) nach `begin()` löschen und zyklisch lesen; dasselbe Register meldet `LOL_A`/`LOL_B` → Zeile „PLL locked“ auf der Info-Karte. → N9
- [ ] **Hinweise:** SI5351 läuft beim Flashen und nach ESP32-Reset mit der letzten Einstellung weiter (A10, Handbuch); Hardware-Hinweise (eigener LDO mit Ferrit, kurze Masse, Abschirmung, optional TCXO nach C7). VCO bei `Fractional` und +50 ppm 900,045 MHz (unkritisch, nur notieren). → S72, N7, N10

### 2.2 Bedienung

- [ ] **Netzwahl nach falschem Passwort am Gerät** (S53b, Prüfpunkt h): zwei Netze gespeichert, das stärkere mit falschem Passwort → nach dem Fehlschlag verbindet sich das Gerät mit dem anderen. Host-getestet (`WifiPolicy::chooseSlot`). → S61

### 2.3 Code und Tests


### 2.4 Plattform und Lieferkette

| ID | Komponente | heute | Ziel |
|---|---|---|---|
| P-2, P-4, S-19 | Plattform / Arduino / IDF | pioarduino 55.03.313 / 3.3.13 / 5.5.6 (`release/v5.5` @ `a45de8ccca1`), einzige Plattform (D16, D51, E-32, E-51; GHSA-ch63 und DHCP-Fix CVE-2026-45160 enthalten), Plattform und Framework per Release-URL mit SHA-256-Prüfung in der CI | zurück auf das PlatformIO-Register, sobald es 3.3.13 oder neuer anbietet; Gerätetest N11 offen |
| P-5 | TFT_eSPI | 2.5.43 (letztes Release) | bleibt; mit Core 3.x im Spike unverändert lauffähig (Flush 35 ms je Vollbild wie unter 2.x); Fork oder LovyanGFX nur als Ausweg (eigene Display-Erkennung bleibt, LovyanGFX erwartet ST7789-ID 0x85, der CYD2USB meldet `8181B3`) |
| P-6, P-8 | LVGL, Unity | 8.4.0 (E-28, LVGL 9 ausgeschlossen, D13), 2.6.1 | Unity 2.7.0 (noch nicht im Register) → N11 |
| P-9 | Web-Bibliotheken | scpi-parser `8866159` (v2.3 + `strings.h`- und `-Wdouble-promotion`-Fix, seit S51, E-29) | ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, ArduinoJson 7.4.3, alle exakt gepinnt |
| S-20 | Prüfung | XPT2046 auf Commit gepinnt | Plattform-Prüfsumme in der CI, Advisory-Check je Release (W-S24); Framework-Pakete exakt gepinnt (AF22), Advisory-Workflow wöchentlich und vor dem Release (AF26); Actions-Policy mit SHA-Zwang gesetzt (D41) |


### 2.5 Dokumentation

- [M] **„50 % Tastverhältnis“** (Handbuch §1) ist nicht belegt (E5). → S72

---

## 3. WEB-HMI (umgesetzt)

Das WEB-HMI ist umgesetzt. Verhalten: Handbuch §8 (WIFI-Seiten, Info-Karte), §15 (SCPI, LXI/VXI-11) und §16 (Weboberfläche, REST); REST-Vertrag: [data/www/README.md](data/www/README.md); Module und Code-Regeln: [CLAUDE.md](CLAUDE.md). Hier bleiben die Entscheidungen W-E, die Sicherheitsanforderungen W-S (gelten weiter, Regel 0.2) und die Konzepte für offene Schritte (3.9). Die Abschnitte 3.1–3.8 behalten ihre Nummern, weil Code-Kommentare, Spezifikation und PR-Vorlage sie zitieren (der frühere vollständige Konzepttext ist mit dem Neustart der Historie entfallen).

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
| W-E17 | Speichern | Touch/Web nach 5 s, SCPI nach 30 s Ruhe; `SYSTem:SAVE` sofort, höchstens 1 je 10 s je Quelle (weitere zusammengefasst, D26); nur geänderte Schlüssel; Ein/Aus ohne Speichern. |
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
7. `/api/state` und `/ws` ohne Web-Passwort ohne Login lesbar; mit Web-Passwort nur mit Sitzung, ebenso `/api/token` (D25, AF18, E-48); `/lxi/identification` bleibt offen (D33).
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
| W-S6 | WebSocket-Upgrade mit Host-, Origin- und Token-Prüfung (mit Web-Passwort auch Sitzung, D25); nur Push; ≤ 512 B; Ping alle 15 s mit Zufallsnutzlast, Trennen nach 30 s ohne passendes Pong; > 3 Pings des Clients in 10 s trennen (AF18). (S-10) |
| W-S7 | Header: CSP `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`, `nosniff`, `Referrer-Policy: no-referrer`; `/api/*` mit `no-store`. (S-7) |
| W-S8 | Fremde Zeichenketten im Browser nur über `textContent`/DOM, nie `innerHTML`; JSON nur über ArduinoJson. (S-7) |
| W-S9 | Vor LVGL und JSON läuft jede fremde Zeichenkette durch `sanitize`; Scan-Liste ≤ 15, kein Recolor; LVGL-Assert → Neustart. (S-14, C-3) |
| W-S10 | Body ≤ 1 KB (413), Multipart/Chunked 415, URL ≤ 256 B, Header ≤ 2 KB, 10 s Timeout; ≤ 3 WebSocket- und ≤ 4 HTTP-Verbindungen; < 24 KB Heap oder größter Block < 8 KB → 503; Push höchstens alle 100 ms (AF19). (S-13) |
| W-S11 | ≤ 20 Schreibbefehle/s je Quelle (429), Scan ≤ alle 10 s, nach 5 Login-Fehlern 30 s Sperre (verdoppelnd). (S-9, S-13) |
| W-S12 | Netzänderungen zweistufig mit Rückfall nach 120 s, ohne Web-Passwort zusätzlich am Touch; gespeichert wird nur ein bestätigter Stand, während der Wartezeit keine Netzänderung am Touch (`Web change pending`, `Revert`/`Keep`, E-47); statische IP ≠ Gateway, im Subnetz, keine Netz-/Broadcast-Adresse. (S-15) |
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

Alle Konzepte sind umgesetzt oder entschieden (Phase 10 abgeschlossen, D24, D28, D48); ihr Text steht im Git-Verlauf. Offen aus Konzept 6 bleibt nur der Vertragstest gegen das Gerät in S61 (`web_check`, `contract_check`).

---

## 4. Aufgabenliste

Je Schritt: Inhalt, Prüfung, Commit-Betreff. `[ ]` offen, `[x]` erledigt (wird beim nächsten Aufräumen entfernt; Zusammenfassung in 1.1, Details in der Git-Historie).

### Phase 4 – Migration Core 3.x (Rest: Abnahmemessungen)

- [ ] **S41c · Abnahme Core 3 (Messungen).** Stufe-1-Auswahl nach der Migration (MESSPLAN 5.10, ≈ 4 h, braucht ZÄHLER und OSZI), dabei CLK0 bei Q4/Q5 und J6 (Code 5 bei Leitung auf GND). Bei einem Befund ist der Rückweg der letzte Release-Tag auf Core 2. *Commit:* `S41c: Record the core 3 acceptance measurements`

### Phase 6 – WEB-HMI (Rest: Messung S49, Dauertest S57)

- [ ] **S49 · Vorab-Messung WLAN (0,5 d, braucht SPEKTRUM).** Client mit Dauerverkehr und AP, Spektrum an CLK0 bei 10 MHz (Span 200 kHz und 20 kHz), Modem-Sleep an/aus, Heap-Bilanz; Ergebnis im Messprotokoll und in 1.2. WLAN-Energiesparen und Sendeleistung sind vorerst entschieden (D38: `WIFI_PS_NONE`, volle Sendeleistung, umgesetzt mit AF55); S49 misst zuletzt `PS_NONE` gegen `MIN_MODEM` und eine reduzierte Sendeleistung (z. B. 11 dBm), eine Änderung danach als E-n (PERF-15). *Commit:* `S49: Record WLAN spectrum pre-measurement and modem-sleep decision`
- [ ] **S57 · Fuzzing und Dauertest (1 d + Laufzeit).** Erledigt: libFuzzer-CI-Job und Zufallstest mit festem Seed (S57a–c); manueller Fuzz-Lauf mit 10 Mio. je Ziel auf `main`, alle Ziele grün, ohne Fund (Lauf 42824946289). 24-h-Lauf: gestartet, lief 35 min fehlerfrei (USB, 4086 Schreiben/Rücklesen, 0 Abweichungen, 0 Resets, min. Heap 81,5 KB mit WLAN) und brach ab, weil der USB-Port auf der PC-Seite verschwand; **zu wiederholen**. Rest: 24-h-Lauf an stabilem Port (oder über TCP) mit Heap- und Stack-Bilanz (Weboberfläche offen, Debug-Log alle 60 s mit Stack-Reserve von `async_tcp` und `web_login`; ggf. Stack von `async_tcp` senken, aus S55b); zusätzlich Worst Case „alle Kanäle belegt“ (3 WebSockets, 1-Hz-Polling, SCPI-TCP-Flut, USB-SCPI gleichzeitig, PERF-8) und High-Water-Marks `async_tcp`/`web_login` sowie LVGL `max_used` nach einer Seitenrunde (PERF-14). *Commit:* `S57: Add fuzzing job and long-run test`

### Phase 7 – GitHub und Verteilung (Rest ≈ 1 d, je ein Commit; `[Nutzer]` = Einstellung im Repo, nur durch den Nutzer)

Stand: G1, G4–G9, G11, G13 erledigt (Abschnitt 1.1), Workflows und Ruleset auf `main`; Pages (Quelle GitHub Actions) mit Umgebung `github-pages` (Regeln `main`, `v*`) sowie Secret Scanning und Push-Schutz sind eingerichtet.

- [ ] **G5 · Vorschaubild (`[Nutzer]`, kein Commit).** `docs/img/social-preview.png` unter Settings → General → Social preview hochladen (geht nur im Browser); optional den PING-Flut-Befund (AF18) an ESPAsyncWebServer melden. Per API gesetzt (AF28, D49): Topics, Dependabot-Sicherheitsupdates, Environment `copilot` entfernt.
- [ ] **G9b · Projektseite (Rest, `[Nutzer]`, kein Commit).** Beschreibung, Website `https://dm8sb.github.io/CYD-HF-Generator/`, Topics, Vorschaubild 1280 × 640; CN1-Steckerraster (1,25 mm) und Pinfolge am Board bestätigen.
- [ ] **G10 · Fotos (später, `[Nutzer]`).** Nach der Liste in `docs/hardware.md`: `board-cyd-front.jpg` und `board-cyd2usb-front.jpg` (MAIN-Seite, CH1 an), `si5351-wiring.jpg` (Modul mit den 4 Leitungen zu CN1 und CLK0–2), `cn1-connector.jpg` (Rückseite, Beschriftung lesbar), `setup-bench.jpg` (Aufbau mit Koax zum Messgerät), je ≤ 500 KB, kein WLAN-Name auf dem Display; ablegen unter `docs/img/`. Danach Platzhalter in `docs/hardware.md` aktivieren. Vorschaubild `social-preview.png` 1280 × 640 (< 1 MB) lädt der Nutzer unter Settings → General → Social preview hoch. Zwischenstand (G13): gerenderte Bilder in `docs/img/` stehen bis dahin in README, `docs/hardware.md`, `case/README.md` und Handbuch Anhang A, darunter eine gerenderte `social-preview.png` (liegt jetzt doch im Repo; durch eine Version mit Foto ersetzbar). *Commit:* `G10: Add hardware photos`

### Phase 8 – Messungen ([MESSPLAN.md](MESSPLAN.md))

Je Messtag ein Commit mit dem Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` (ohne Medien); Befunde (M-n) sofort in Abschnitt 2 eintragen. Stufe 1 und 2 laufen **sobald Zähler und Oszilloskop verfügbar sind**; dieser Lauf ist die Referenz R0 (MESSPLAN 9.2).

- [ ] **S59 · Stufe 1 (Pflicht, ≈ 7 h).** MESSPLAN Abschnitt 5 auf beiden Boards, mit den offenen Smoke-Test-Schritten 5 und 6; danach je Release nur die geänderten Blöcke (MESSPLAN 5.10). *Commit:* `S59: Record tier-1 measurements`
- [ ] **S60 · Stufe 2 (Entscheidungen, ≈ 6 h).** MESSPLAN Abschnitt 6: Standard-Tuning (K5, F3, K6), IQ ohne Reset (D3, D5, E9), ungerade Teiler und Phasenfehler (E3, E4, E5), Störquellen und PDN (F6, F7), Pegel (F2), Drift (C7); Entscheidungen als E-n in die Spezifikation, N1–N8 bestätigen oder streichen. *Commit:* `S60: Record tier-2 measurements and decisions`
- [ ] **S61 · Stufe 1W (Pflicht ab WEB-HMI, ≈ 8 h + 24 h).** MESSPLAN 5.11 nach S58 auf beiden Boards; dazu L13, L15, Q1 aus Stufe 2; aus S54: Abbau einer verwaisten TCP-Verbindung nach ≈ 90 s (Keepalive), Sperre bei offenem AP (auch Portmapper und VXI-11 aus LX1), mDNS `_scpi-raw._tcp`; L-S6: der AP geht in `WifiManager::update()` aus `loop()` aus, nicht im IP-Event (D14, W-S3), die Grenze ≤ 1 s hängt an der `loop()`-Latenz. Dazu `tools/web_check.py <Gerät>` ohne und mit Passwort und `tools/contract_check.py <Gerät>` (AF34), PASS/FAIL-Liste ins Protokoll (TEST-1); L-S5 mit `slowloris.py` (AF39); Scan und Netzänderung aus dem Web über den Job-Postfach-Weg (AF33, AF50). *Commit:* `S61: Record network and security measurements`
- [ ] **S62 · Stufe 3 (optional).** Charakterisierung und Bedientests mit Personen nach Bedarf (MESSPLAN Abschnitt 7). *Commit:* `S62: Record optional characterization measurements`
- [ ] **S63 · Handbuchwerte.** Pegeltabelle (F2), Mischer-Kapitel Abschnitt 18 (F2, G1, G2, E10, E4), Tastverhältnis (E5), Aufwärmzeit (C7), WLAN-Hinweis (L13), „SI5351 läuft beim Flashen weiter“ (A10), Hardware-Hinweise; DE und EN. *Commit:* `S63: Add measured values to the manuals`
- [ ] **S64 · Freigabe.** Kriterien MESSPLAN 9.4 erfüllt; Version, README, Tag; Merge nach `dev` und `main` nur auf Anweisung. *Commit:* `S64: Release`

### Phase 9 – Folgearbeiten (je ein Commit, nach Messergebnis oder Rückfrage)

- [ ] **N1 · Standard-Tuning** ändern, wenn die Stufe-2-Messungen es tragen (Rückfrage 1, E-19).
- [ ] **N2 · IQ-Abstimmen ohne Aussetzer** (Abschnitt 2.1).
- [ ] **N3 · Ungerade IQ-Teiler bei 180°/270°**, wenn E4 einen Fehler > 1° zeigt.
- [ ] **N5 · MultiSynth abgeschalteter Kanäle abschalten, PDN-Strategie** nach K2 und F7.
- [ ] **N6 · Störquellen-Abhilfen** (Backlight-PWM ≥ 20 kHz, Ruhemodus der Überwachung) nach F6, F9; „Neuzeichnen nur bei Änderung“ übernimmt AF12, den Backoff der SI5351-Suche ohne Chip AF53 (PERF-11).
- [ ] **N7 · TCXO-Option** nach C7, C8.
- [ ] **N9 · Sticky-Bit und „PLL locked“** (Abschnitt 2.1).
- [ ] **N10 · Hardware-Hinweise** in den Handbüchern.
- [ ] **N11 · Spätere Updates:** Unity 2.7.0, sobald es im Register steht (offen). Erledigt: Arduino 3.3.13 / IDF 5.5.6 über pioarduino 55.03.313 (D51, E-51; behebt GHSA-ch63, Advisory-Check ohne Fund; `lib_archive = yes`; Flash 1 553 065 B, RAM 78 992 B) – **Gerätetest offen** (Boot, Display-Erkennung beider Boards, Touch, SI5351, WLAN Client und AP, Weboberfläche, SCPI USB/TCP, VXI-11/LXI, Heap nach 10 min). Advisory-Check je Release läuft (AF26).

### Phase 10 – Audit-Folgearbeiten

Abgeschlossen: Priorität 1–4 und 6 umgesetzt, Priorität 5 entfällt (D48), N15 entfällt (D50); die Befund-IDs bleiben als Namen gültig, Details im Git-Verlauf. In bestehende Schritte eingeordnet: PERF-14 und der PERF-8-Worst-Case → S57, PERF-15 → S49 (D38), TEST-2 → S41c/S59/S61.

---

## 5. Pflege dieser Datei

- Der Haken eines Schritts steht im selben Commit wie die Änderung; erledigte Schritte, Befunde, beantwortete Rückfragen und umgesetzte Konzepte werden beim nächsten Aufräumen entfernt (Abschnitt 1.1 fasst sie zusammen, Details in der Git-Historie).
- Neue Befunde (M-n aus Messungen, B-n aus Bedientests) kommen mit Bezug in Abschnitt 2 und als Schritt in Abschnitt 4; bestehende Schrittnummern bleiben stabil.
- Neue Entscheidungen ohne Datum in Abschnitt 1.3; dauerhafte Entscheidungen zusätzlich als E-n in der Spezifikation.
- Die Befund-IDs des Projekt-Audits (SEC-n, SEC-V-n, LIC-n, CQ-n, DOC-n, PERF-n, OPS-n, TEST-n, A11Y-n, die offenen Befunde stehen mit ihren Fakten in Abschnitt 2 und Phase 10) bleiben wie die früheren Audit-IDs als Namen gültig, auch nachdem der Befund erledigt ist. Schritte aus dem Audit heißen AF-n (Phase 10) und der LXI-Schritt LX1, weil A-n und L-n schon Mess-IDs im MESSPLAN sind.
