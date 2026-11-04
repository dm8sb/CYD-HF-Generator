# Messplan – CYD HF Generator

Stand: 2026-11-04 · Plan-Version 2.0 · Prüflinge: Firmware 1.3.0 (Referenz) und Release-Kandidat 1.4.0 (WEB-HMI)

Dieses Dokument beschreibt alle Messungen und Bedientests am Gerät mit Einzelschritten, Messgeräten, Sollwerten und Bewertung. Es ersetzt Abschnitt 10 von [todo.md](todo.md); die Nummern A1 … K12 aus dem bisherigen Plan bleiben gleich, „(alt n)“ nennt die Nummer im Plan vom 2026-10-26. Quellen der Sollwerte: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md) (A-n Anforderungen, E-n Entscheidungen), [USER_MANUAL_DE.md](USER_MANUAL_DE.md), [KONZEPT_WEB_HMI.md](KONZEPT_WEB_HMI.md) (W-E, W-S), [AUDIT_2026-11.md](AUDIT_2026-11.md) (C, S, U).

Bei einem Widerspruch zwischen diesem Plan und dem Handbuch der geprüften Firmware gilt das Handbuch; der Plan wird dann nachgezogen.

## Inhalt

1. Ziel, Umfang, Durchläufe
2. Messgeräte und Material
3. Gemeinsame Grundlagen: Sicherheit, Bedingungen, Methoden, Bewertung, Aufbauten
4. Vorbereitung und Eintrittskriterien
5. Ablauf und Zeitplan
6. Messungen
   - A Inbetriebnahme und Robustheit · B Bedienung · C Frequenzgenauigkeit · D Abstimmen und Umschalten · E IQ-Betrieb · F Spektrale Reinheit · G Mischer-Versuche · H Bereichserweiterung · I Display-Erkennung · J SDA/SCL-Erkennung · K CONFIG-Optionen · L WEB-HMI · L-S WEB-HMI Sicherheit · Q Qualität und Dauerbetrieb
7. Auswertung und Entscheidungen
- Anhang A Protokollkopf · B Rückverfolgbarkeit (Anforderung → Messung) · C Aufgaben und SUS-Fragebogen · D Skripte · E Befundvorlage

---

## 1. Ziel, Umfang, Durchläufe

### 1.1 Ziel

1. Die Anforderungen (A-n) und Entscheidungen (E-n) der Spezifikation am Gerät belegen. Fast nichts davon ist bisher gemessen.
2. Messwerte für offene Entscheidungen liefern (Abschnitt 1.4).
3. Die Sicherheitsanforderungen der WEB-HMI (W-S1 … W-S25) am Gerät prüfen.
4. Bedienung mit Testpersonen messen (Trefferquoten, Lesbarkeit, Aufgabenerfolg).
5. Eine Referenz festhalten, damit spätere Plattform- oder Treiberänderungen (Core 3.x, LVGL, SI5351-Treiber) auf Abweichungen geprüft werden können.

### 1.2 Reihenfolge (entschieden 2026-11-04)

Zuerst werden WEB-HMI und Qualität umgesetzt (Audit-Phasen 1, 2, 4, 5, 6), danach läuft die Messkampagne **einmal** am Release-Kandidaten. Daraus folgt:

- **Je Entwicklungsphase nur der Smoke-Test** (Abschnitt 4.3, ≈ 45 min), kein volles Programm.
- **Referenz R0:** Die Firmware 1.3.0 wird gesichert (Tag und Binärdatei, Schritt V2), damit die wichtigsten Werte nach der Migration auf demselben Gerät mit beiden Ständen gemessen werden können.
- **Folgen der Messungen** (Standard-Tuning, IQ ohne Reset, Pegel- und Tastverhältnis-Angaben, Störquellen) kommen danach als Version 1.4.x bzw. 1.5.0. Funktionen wie Sperre oder Kalibrierassistent (Audit-Phase 3) sollten vor der Kampagne fertig sein, sonst folgt ein kleiner Nachlauf für B16 und C2.

### 1.3 Prüflinge und Durchläufe

| Code | Prüfling | Zweck |
|---|---|---|
| R0 | Firmware **1.3.0** (Commit 8ab70bc, Tag `v1.3.0` anzulegen) | Referenz vor Migration und WEB-HMI; Auswahl in Abschnitt 5.3 |
| R1 | Release-Kandidat **1.4.0** (sauberer Commit, kein `+` hinter dem Commit auf der Info-Karte) | vollständige Kampagne |
| S | Zwischenstand je Entwicklungsphase | nur Smoke-Test (Abschnitt 4.3) |

**Boards** (nach Spezifikation 2.1; beide laufen mit derselben Firmware):

| Code | Board | Erkennung | Controller |
|---|---|---|---|
| Board A | Original-CYD | nur Micro-USB | ILI9341 |
| Board B | CYD2USB | zusätzlich USB-C | ST7789 |

Stand 2026-11-04: Die Boards hängen an **COM7 und COM13**, und **nur ein Board hat das SI5351-Modul angeschlossen**. Beim Schritt S06 des Arbeitsplans zeigte COM7: Original-CYD (ILI9341), **ohne** SI5351; COM13 ist demnach das CYD2USB mit Modul (noch nicht geprüft; am 2026-10-28 war es umgekehrt). Folgen:

- Schritt V1 ordnet Ports, Boards und Modul zu.
- Alle HF-Messungen (C bis H, K) laufen auf dem Board mit Modul („**Messboard**“). Das Modul bestimmt die HF-Werte, das Board nur Störungen (F6, F9).
- Tests mit beiden Displays (A6, A7, B6, B7, I, K10) verlangen das **Umstecken des Moduls** (CN1) auf das andere Board. Diese Tests stehen deshalb in einem Block (Abschnitt 5.2, Tag 2).
- Die Tests, die ein fehlendes Modul verlangen (A1, J3, J12, J13), laufen am Board ohne Modul.

### 1.4 Entscheidungen, die aus den Messungen folgen

Die Schwellen sind **Vorschläge**; sie werden vor der Kampagne bestätigt oder geändert.

| Entscheidung | Messungen | Vorgeschlagenes Kriterium |
|---|---|---|
| Standard-Tuning `Fractional` oder `Low noise` (todo 9.2, E-19) | F3, F4, K5, K6, K7 | `Low noise` wird Standard, wenn die Nebenlinien innerhalb ±10 kHz um den Träger im Mittel ≥ 10 dB niedriger sind, die Abweichung ≤ 1,4 Hz bleibt und die Aussetzer beim Abstimmen (K6) selten und kurz sind |
| IQ-Abstimmen ohne Aussetzer (todo 2.2) | D3, D4, D5, E9 | umsetzen, wenn die PLL-Änderung ohne Reset die Phase hält (E9) und der Aussetzer in D3 > 1 ms stört |
| Ungerade IQ-Teiler bei 180° (todo 2.2) | E2, E4 | umsetzen, wenn der Phasenfehler bei ungeradem Teiler um > 1° größer ist als bei geradem |
| `High-Z` über `CLKx_PDN` (E-21) | K1, K2 | `PDN` für alle abgeschalteten Ausgänge, wenn der Pin dabei hochohmig ist |
| TCXO-Option (todo 6) | C7, C8 | empfehlen, wenn die Drift nach 15 min Aufwärmzeit > 0,5 ppm/h beträgt |
| Aussage „50 % Tastverhältnis“ im Handbuch (A-1) | E5 | Handbuch präzisieren, wenn die Abweichung > 2 % beträgt |
| Pegeltabelle und LO-Hinweise im Handbuch | F1, F2, G1 | Tabelle übernehmen |
| Abhilfen für Störquellen (todo 2.1) | F6, F7, F9 | Backlight-PWM, I2C-Takt oder Display-SPI ändern, wenn ein Störer < 60 dBc und im Frequenzbereich der Anwendung liegt |
| Stift am Laborplatz, Mindestgrößen (Audit U-4, U-5) | B8, B9, B19 | Randtasten und Mindestmaße nach der Trefferkarte |
| WLAN während Messungen aus (Handbuch-Hinweis) | L13, L15 | Hinweis, wenn Nebenlinien durch WLAN sichtbar sind |
| Heap-Grenzen (W-S10, Konzept 7.3) | L14, Q1 | Grenzwerte anpassen, wenn der minimale Heap im Dauertest sinkt |
| Klon-Module (MS5351M) akzeptieren (E-17) | J7 | Prüfregeln lockern, wenn ein echtes Modul abgelehnt wird |
| Phasen-Feinabgleich (todo 2.3) | E1 bis E3, E8, E9 | umsetzen, wenn der Phasenfehler systematisch > 1° ist |

---

## 2. Messgeräte und Material

Spalte „Vorhanden“ vor Beginn ausfüllen (Modell, Seriennummer, Kalibrierdatum). Fehlt ein Gerät, steht in der letzten Spalte, welche Messungen dann entfallen oder nur eingeschränkt möglich sind.

### 2.1 Messgeräte

| Code | Gerät | Mindestanforderung | Wofür | Vorhanden | Ohne das Gerät |
|---|---|---|---|---|---|
| **ZÄHLER** | Frequenzzähler | Eingang ≥ 200 MHz, 50 Ω, Auflösung ≤ 0,1 Hz bei 100 MHz in 1 s Torzeit (≥ 10 Stellen/s), Eingang für externe 10-MHz-Referenz | C, D5, H, K7 | Ja | C, H, K7 entfallen; Ersatz nur mit SPEKTRUM und Referenzeingang, dann Auflösung ≈ 1 Hz |
| **REF** | 10-MHz-Referenz | GPS-gestützt oder Rubidium; für relative Prüfungen genügt ≤ 0,1 ppm (Abschnitt 3.3); Verteiler für ZÄHLER und SPEKTRUM | C, E8 | JA, GPS-TCXO 10MHz | Absolute Genauigkeit unbekannt; C3 bis C6 gelten nur relativ |
| **OSZI** | Oszilloskop | ≥ 2 Kanäle (besser 4), Bandbreite ≥ 500 MHz, ≥ 2 GS/s (besser 5), Persistenz („Nachleuchten“), Single-Trigger, **Timeout-/Dropout-Trigger**, Δt-Messung mit Mittelung, 50-Ω-Eingang oder 50-Ω-Durchgangsabschlüsse | A9, A10, D, E, K1, K6 | Ja R&S RTO 2GHz Bandbreite | E, D entfallen |
| **SPEKTRUM** | Spektrumanalysator | bis ≥ 1 GHz (Oberwellen bis zur 5. bei 160 MHz), RBW ≤ 10 Hz, Rauschmarker (dBc/Hz), Eingang 50 Ω, zulässiger Dauerpegel ≥ +20 dBm, DC-Eingang nicht belastbar, Anschluss für externe 10-MHz-Referenz | F, G, K3, K5, L13, L15 | Ja Siglent 3,2GHz | F, K3, K5 entfallen; Ersatz: SDR (Abschnitt 2.2), dann nur Nebenlinien, kein Phasenrauschen |
| **LOGIK** | Logikanalysator | ≥ 8 Kanäle, ≥ 24 MS/s, I2C-Dekoder, 3,3-V-Pegel | J6, J11, Q | Ja Salea | J6, J11 entfallen |
| **DMM** | Multimeter | DC-Spannung, Eingang 10 MΩ | A9, K1, K2, V3 | Ja Agilent 34401A | mit OSZI (1 MΩ) möglich |
| **STROM** | USB-Leistungsmesser | 5 V, Auflösung ≤ 1 mA, Mittelwert über ≥ 10 s | Q2 | nein | Q2 entfällt |
| **LUX** | Luxmeter | 10 … 1000 lx | B14 | nein | Beleuchtung nur beschreiben |
| **SONDE** | Nahfeldsonde (H-Feld, ≈ 1 cm) | 1 MHz … 1 GHz | F9 | Ja | F9 entfällt |
| **THERMO** | Thermometer | Raumtemperatur, Auflösung 0,1 K | C7, E8 | Ja | Temperatur nur schätzen |
| **VIDEO** | Smartphone mit Slow-Motion und Stoppuhr | ≥ 120 Bilder/s | A2, A6, B, I | eventuell | Zeiten nur mit Stoppuhr |

### 2.2 Zubehör

| Material | Menge | Hinweis |
|---|---|---|
| 50-Ω-Durchgangsabschlüsse (BNC oder SMA, je nach Anschlüssen) | 3 | Abschluss an jedem gemessenen Ausgang |
| Dämpfungsglieder 10 / 20 / 30 dB, 50 Ω | je 1–2 | 20 dB immer vor SPEKTRUM |
| DC-Sperre (Blocking Capacitor) | 1 | vor SPEKTRUM und ZÄHLER, wenn der Eingang DC-gekoppelt ist |
| Leistungsteiler 2-fach 50 Ω oder BNC-T | 1 | Eigenversatz der Phasenmessung (V4) |
| Koaxkabel **gleicher Länge** (Längenunterschied < 1 mm ≙ < 5 ps) | 2 + 2 | zwei für die Phase, zwei Reserve |
| Adapter vom Modulausgang (Pfostenstecker bzw. SMA) auf BNC | nach Modul | |
| Dupont-Kabel, Mini-Klemmen für CN1 (SDA, SCL, 3,3 V, GND) | je 4 | Umstecken und Fehlerzustände (J) |
| USB-Datenkabel Micro-USB und USB-C, USB-Netzteil 5 V/1 A, Powerbank | je 1 | F6 d |
| Diodenringmischer (z. B. SBL-1 oder ADE-1), LO-Pegel +7 dBm | 1 | G1, G2 |
| Soundkarte (Line-In) oder SDR mit FFT-Software | 1 | G2, D5 |
| QSD-Mischer (FST3253) oder zwei Mischer mit 90°-NF-Netzwerk | optional | E10 |
| I2C-Modul, einstellbar auf Adresse 0x60 (z. B. MCP4725 mit passender Adressbeschaltung) | optional | J15 |
| SI5351-Klon (MS5351M) | optional | J7 |
| Eingabestift (Kunststoff, Spitze ≈ 1 mm; der Touch ist resistiv) | 1 | B19 |
| Isolierunterlage, ESD-Armband | 1 | Abschnitt 3.1 |

### 2.3 Rechner und Software

| Posten | Hinweis |
|---|---|
| PC mit PlatformIO | **`~/.platformio/penv/Scripts/pio.exe` verwenden, nicht das alte `pio` 6.1.5 im PATH** (bricht ESP32-Builds, Audit P-1) |
| Python 3 mit `pyserial`, `pyvisa`, `pyvisa-py` | Skripte in Anhang D |
| `curl` (unter Windows `curl.exe`), `ncat`/`nc`, `esptool` (aus der PlatformIO-Umgebung) | L, L-S |
| Serieller Monitor 115 200 Baud | `pio device monitor -b 115200 --port COMx` |
| Browser (Chrome, Firefox) auf PC, Smartphones Android **und** iOS, optional Tablet | L5, L8, L16 bis L21 |
| WLAN-Router mit WPA2 (Test-WLAN, 2,4 GHz), ein zweiter Router oder AP | L1, L2 |
| Gerät zum Erzeugen von Test-SSIDs (zweiter ESP32 oder Linux-Rechner mit `hostapd`) | L-S4, nur im eigenen Labor und nur mit eigenen Geräten |
| Wireshark (optional) | L-S6, L-S13 |
| FFT-Software für die Soundkarte (z. B. Audacity, SpectrumLab) | G2 |
| Messprotokoll (Kopie dieser Datei, Anhang A) und Kamera für Aufbau-Fotos | alle |

### 2.4 Personen für Bedientests

5 Lernende (ohne Vorkenntnis des Geräts) und 2–3 Lehrende; je 30 min für B16, kurze Einzelsitzungen (10 min) für B8, B9, B11, B14; Einverständnis für Aufzeichnung der Touch-Daten und Notizen (keine Namen im Protokoll, nur Personen-Codes P1 … P8).

---

## 3. Gemeinsame Grundlagen

### 3.1 Sicherheit und Geräteschutz

- **Nie ein ausgeschaltetes Gerät mit `Off state` = `High` an SPEKTRUM oder ZÄHLER mit DC-Kopplung**: Der Ausgang führt dann 3,3 V Gleichspannung (K1). Pegelmessungen im Zustand OFF nur mit OSZI (DC, 1 MΩ oder 50 Ω) oder DMM, nie mit SPEKTRUM.
- **Immer erst 20 dB Dämpfung und DC-Sperre vor SPEKTRUM.** Die Grundwelle an 50 Ω kann bei 8 mA grob 0 … +10 dBm erreichen (genau misst F2); die Eingangsgrenze des Analysators steht im Datenblatt.
- Pegel und Frequenz am Gerät einstellen, **bevor** der Ausgang eingeschaltet wird; neue Aufbauten nur bei ausgeschalteten Ausgängen verbinden.
- SI5351-Modul, Kabel und Mischer nicht unter Spannung umstecken, außer in den Tests J1 bis J5, J14 (dort ist es der Test).
- ESD: Armband, Isolierunterlage; die Boards nicht auf Metall legen (Kurzschluss an der Rückseite).
- Masseschleifen: OSZI, ZÄHLER und SPEKTRUM sind über Netzleiter geerdet; das Board wird über USB vom PC gespeist. Bei Brummen oder Offsets ein USB-Netzteil statt PC-USB verwenden und im Protokoll vermerken.
- Netzwerktests (L-S) nur im eigenen, abgetrennten Laborsegment und nur mit eigenen Geräten; die Störtests (Slowloris, Beacon-Fluten) nicht in einem produktiven WLAN.

### 3.2 Standardbedingungen

Wenn bei einer Messung nichts anderes steht:

| Größe | Wert |
|---|---|
| Ausgangslast | 50 Ω abgeschlossen; am SPEKTRUM über DC-Sperre und 20 dB |
| Drive | 8 mA |
| Tuning, Spread, Off state | `Fractional`, `Off`, `Low` (Werkseinstellung) |
| Korrektur | nach C2 kalibriert und danach unverändert |
| WLAN | aus (außer bei L, L-S, F6, L13, L15, Q2) |
| Aufwärmzeit | 15 min mit eingeschaltetem CH1 (10 MHz) vor C, E, F |
| Umgebung | 20 … 25 °C, keine Zugluft, Temperatur im Protokoll |
| Spannungsversorgung | PC-USB; abweichend nur nach Vermerk |
| Firmware-Build | Release (`LOG_DEBUG 0`); Debug-Build nur dort, wo ausdrücklich gefordert |
| Boot | nach jedem Flashen oder Reset die Info-Karte prüfen (Version, Commit ohne `+`) |

### 3.3 Frequenzen messen: Klammermessung

Der Quarz driftet innerhalb von Minuten um bis zu einige 0,1 ppm (≙ 10 Hz bei 100 MHz). Die Synthese kann deshalb nur **relativ** zu einer Messung bei 10 MHz bewertet werden; die absolute Referenzgenauigkeit ist dafür unkritisch, solange C2 und alle weiteren Messungen mit demselben ZÄHLER und derselben REF laufen.

1. 10 MHz an CH1 einstellen, 10 Werte mit 1 s Torzeit mitteln: **f10a**.
2. Messfrequenz **f_soll** einstellen, 10 Werte mitteln: **f_mess**.
3. 10 MHz erneut einstellen, 10 Werte mitteln: **f10b**.
4. Abweichung: **δ = f_mess − f_soll · (f10a + f10b) / (2 · 10 MHz)** in Hz.

Dabei müssen Ausgang, Kabel und Zähler unverändert bleiben. Die Messunsicherheit u ist die Auflösung des Zählers (V3) plus die Streuung der 10 Mittelwerte.

### 3.4 Phase am Oszilloskop messen

Beide Ausgänge über **gleich lange** Kabel an zwei OSZI-Kanäle mit 50 Ω (Aufbau 3). Trigger auf Kanal 1. Zeitversatz **Δt** zwischen den steigenden Flanken an 50 % der Amplitude, ≥ 256 Mittelungen.

- **Phase: φ = Δt · f · 360°**, bei negativem Δt eine Periode addieren.
- **Eigenversatz φ₀** aus V4 abziehen (Messung mit demselben Signal über einen Teiler auf beide Kanäle).
- Umrechnung: 1° entspricht bei 4,73 MHz ≈ 587 ps, 10 MHz ≈ 278 ps, 30 MHz ≈ 93 ps, 99 MHz ≈ 28 ps. Bei hohen Frequenzen setzt die Zeitauflösung des OSZI die Grenze (V4 bestimmt u).
- Eine Phasenabweichung von 1° entspricht einer Seitenbandunterdrückung von ≈ 41 dB, 2° ≈ 35 dB (E10).

### 3.5 Spektren messen

- Aufbau 2 mit 20 dB und DC-Sperre; REF an den Referenzeingang des SPEKTRUM.
- Referenzpegel so, dass der Träger im oberen Drittel liegt (mit Dämpfungsglied meist −10 … 0 dBm); Detektor „Sample“ oder „RMS“, Mittelung ≥ 10; Eintrag der Einstellungen im Protokoll (Span, RBW, VBW, Mittelung, Referenzpegel).
- **Nebenlinien** in dBc zum Träger; Abstand in Hz notieren.
- **Phasenrauschen** mit dem Rauschmarker, auf 1 Hz normiert, Korrektur für Detektor und RBW laut Gerätehandbuch. Bei kleinen Ablagen (100 Hz, 1 kHz) begrenzt das Eigenrauschen des Analysators: das **Eigenrauschen aus V4 daneben eintragen**. Liegt der Messwert < 6 dB über dem Eigenrauschen, gilt er als obere Schranke (○).
- Ein Screenshot je Messung, Dateiname `<Nr>_<Lauf>_<Board>.png`.

### 3.6 Bewertung

Jede Messung erhält ein Ergebnis:

| Zeichen | Bedeutung |
|---|---|
| ✔ | bestanden: \|Abweichung\| + u ≤ Grenze |
| ✘ | nicht bestanden: \|Abweichung\| − u > Grenze |
| ○ | nicht entscheidbar (Unsicherheit zu groß); Grund und u eintragen |
| i | nur zu dokumentieren (Information für eine Entscheidung) |
| – | nicht anwendbar (Firmware-Stand, Gerät fehlt) |

Jedes ✘ und jedes unerwartete Verhalten wird mit der Vorlage aus Anhang E als **Befund M-n** erfasst (Test-Nr., Lauf, Board, Messwert, Foto, Vermutung) und nach Priorität in [todo.md](todo.md) eingetragen. Bei ○ wird, wenn möglich, mit besserem Aufbau wiederholt.

### 3.7 Aufbauten

```text
Aufbau 1 – Frequenz
  CLKx ──► 50-Ω-Eingang ZÄHLER           REF ──► ZÄHLER (Ref-In)
  (bei DC-gekoppeltem Eingang: DC-Sperre davor)

Aufbau 2 – Spektrum
  CLKx ──► DC-Sperre ──► 20 dB ──► SPEKTRUM (50 Ω)      REF ──► SPEKTRUM (Ref-In)

Aufbau 3 – Phase / Zeit
  CLK0 ──► Kabel 1 ──► OSZI Kanal 1 (50 Ω)
  CLK2 ──► Kabel 2 ──► OSZI Kanal 2 (50 Ω)       Kabel gleich lang, Masse gemeinsam

Aufbau 4 – Mischer
  CLK0 (LO) ──► Pad auf +7 dBm ──► LO ┐
  CLK1 (RF) ──► 30 dB ──────────► RF ├ Mischer ── IF ──► SPEKTRUM bzw. Soundkarte
                                       ┘ (Off state Low oder High-Z, nie High)

Aufbau 5 – I2C
  LOGIK Kanal 0 = SDA, Kanal 1 = SCL, Masse = GND (CN1), Dekoder I2C

Aufbau 6 – Gleichpegel
  CLKx ──► OSZI (DC, 1 MΩ und 50 Ω) oder DMM       **nie SPEKTRUM**

Aufbau 7 – Netz
  Router (WPA2, 2,4 GHz) ── PC (LAN/WLAN) ── Smartphones; Gerät als Client oder AP
```

---

## 4. Vorbereitung und Eintrittskriterien

### 4.1 Vorbereitung

#### V1 · Boards, Ports und Modul zuordnen

`Geräte` PC · `Dauer` 20 min

1. Beide Boards einzeln anstecken, `pio device list` zeigt die Ports (COM7 und COM13). Das Board **mit** zusätzlicher USB-C-Buchse ist Board B (CYD2USB), das mit nur Micro-USB Board A.
2. Je Board den Serial Monitor öffnen (`pio device monitor -b 115200 --port COMx`), EN-Taste drücken und die Zeilen `[BOOT]`, `[DISPLAY]` und `[APP]` ablesen. `[DISPLAY] ILI9341 …` ≙ Board A, `ST7789 …` ≙ Board B. Auf der Info-Karte (CONFIG) steht dasselbe.
3. `[APP] SI5351 at 0x60, SDA=… SCL=…` zeigt, welches Board das Modul hat (**Messboard**); beim anderen steht `[APP] SI5351 not found`.
4. MAC-Adressen notieren (Log oder `esptool.py chip_id`), Boards beschriften („A“, „B“, Messboard).

**Ergebnis:**

| Port | Board | Controller | SI5351 | MAC | Messboard |
|---|---|---|---|---|---|
| COM7 | | | | | |
| COM13 | | | | | |

#### V2 · Firmware sichern

`Geräte` PC · `Dauer` 20 min

1. Git-Stand prüfen: `git status` sauber, `git log -1` zeigt den zu prüfenden Commit.
2. Referenz: `git tag -a v1.3.0 8ab70bc -m "Firmware 1.3.0"` (lokal; Veröffentlichung nach eigener Entscheidung).
3. Referenzbuild: `git switch --detach v1.3.0`, `pio run`, `.pio/build/esp32-2432S028R/firmware.bin` und `firmware.elf` nach `messungen/firmware/1.3.0/` kopieren, SHA-256 notieren, zurück auf den Entwicklungsstand wechseln.
4. Release-Kandidat: `git tag -a v1.4.0-rc1 <Commit>` und Build nach `messungen/firmware/1.4.0-rc1/`; zusätzlich die Testbuilds aus Abschnitt 4.2.
5. Zum Flashen genügt `pio run -t upload --upload-port COMx` aus dem jeweiligen Checkout, alternativ `esptool` mit der gesicherten `firmware.bin` (Offsets laut `huge_app.csv`).

**Ergebnis:** SHA-256 R0 ______ · SHA-256 R1 ______ · Commit R1 ______

#### V3 · Messgeräte prüfen und aufwärmen

`Geräte` alle · `Dauer` 45 min (parallel zur Aufwärmzeit)

1. Alle Geräte einschalten; ZÄHLER und SPEKTRUM auf externe Referenz stellen, „Lock“ abwarten (≥ 30 min Aufwärmzeit laut Gerätehandbuch).
2. ZÄHLER: Eigenmessung der REF (10 MHz an Eingang), 10 Werte mit 1 s Torzeit; Streuung = Auflösung. Notieren.
3. OSZI: Tastköpfe kompensieren oder 50-Ω-Eingänge wählen; Eigenabgleich ausführen; Kanäle auf gleiche Vertikalempfindlichkeit.
4. SPEKTRUM: Eigenkalibrierung; Marker-Test mit −10-dBm-Signal (falls Kalibriersignal vorhanden).
5. DMM: Nullpunkt (Kurzschluss), Messbereich 20 V.

**Ergebnis:** u_ZÄHLER = ______ Hz · Datum der Kalibrierungen: ______

#### V4 · Eigenmessung des Aufbaus (Unsicherheiten)

`Geräte` OSZI, SPEKTRUM, ZÄHLER · `Dauer` 60 min · `Aufbau` 3 und 2

1. **Eigenversatz der Phasenmessung:** CH1 auf 10 MHz einschalten, über Teiler/T-Stück auf beide OSZI-Kanäle; Δt mit ≥ 256 Mittelungen messen, dann Kabel tauschen und erneut messen. Mittelwert der Beträge = Eigenversatz Δt₀ (ps), Differenz/2 = Unsicherheit der Kabel.
2. Dasselbe bei 30 MHz und 99 MHz (Frequenzabhängigkeit der Kanäle).
3. **Zeitauflösung:** Streuung von Δt über 10 Messungen (ps) = u_t. Daraus **u_φ = u_t · f · 360°** (Tabelle in Abschnitt 3.4).
4. **Eigenrauschen SPEKTRUM:** Eingang mit 50 Ω abschließen, Span 200 kHz, RBW 100 Hz, Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz Ablage (bei 10 MHz Mittenfrequenz) notieren.
5. **Dämpfung** der 20-dB-Glieder und der DC-Sperre: mit dem SPEKTRUM relativ zu einem bekannten Pegel (oder Herstellerangabe) bestimmen; ZÄHLER-Eingang: Empfindlichkeit ≥ Ausgangspegel mit 10 dB Reserve.
6. **SPEKTRUM-Oberwellen:** Das Eigenverzerrungsniveau bei −10 dBm am Eingang prüfen (Messung mit sauberem Signal oder Tiefpassfilter), damit F1 nicht die Analysator-Harmonischen misst.

**Ergebnis:** Δt₀ = ___ ps (10 / 30 / 99 MHz: ___ / ___ / ___) · u_t = ___ ps · u_φ bei 10 / 99 MHz = ___° / ___° · Eigenrauschen (dBc/Hz bei 100 Hz / 1 k / 10 k / 100 k) = ___ / ___ / ___ / ___

#### V5 · Protokoll anlegen

1. Diese Datei nach `messungen/<Datum>_<Board>_<Firmware>.md` kopieren (Anhang A ausfüllen).
2. Ordner `messungen/<Datum>/` für Screenshots, Logs, Fotos; Dateinamen `<Nr>_<Lauf>_<Board>.<ext>`.
3. Auf der Info-Karte Firmware, Branch, Commit und Build-Zeit fotografieren.

### 4.2 Eintrittskriterien (vor Beginn der Kampagne)

| Nr | Kriterium | Prüfung | ☐ |
|---|---|---|---|
| EK-1 | Host-Tests grün in `native-win` und `native` | `pio test -e native-win`, CI | ☐ |
| EK-2 | Build ohne Warnungen mit `-Wall -Wextra` in `src/` | `pio run` | ☐ |
| EK-3 | Phasen-Gates aus Audit 5.8 erfüllt (Review, Doku DE + EN, Spezifikation, README) | Review-Protokolle | ☐ |
| EK-4 | Fuzzing ≥ 10 Mio. Ausführungen ohne Fund (W-S25) | CI-Protokoll | ☐ |
| EK-5 | Abhängigkeiten exakt gepinnt, Advisory-Check je Release (W-S24) | `platformio.ini`, Notiz | ☐ |
| EK-6 | Release-Kandidat committet, Info-Karte ohne `+` | CONFIG | ☐ |
| EK-7 | Smoke-Test (4.3) auf beiden Boards bestanden | Protokoll | ☐ |
| EK-8 | Testbuilds vorhanden (Tabelle unten) | Dateien in `messungen/firmware/` | ☐ |
| EK-9 | Handbuch DE und EN zur Firmware passend; Abweichungen zum Plan notiert | Durchsicht | ☐ |

**Testbuilds** (ein Flag je Build; Release-Build bleibt `LOG_DEBUG 0`):

| Build | Flag | Zweck | Stand |
|---|---|---|---|
| Debug | `-DLOG_DEBUG=1` | J6, J9, J14, A10, Q | vorhanden |
| IQ-Fehler | `-DTEST_FAULT_IQ=1` | A8, A11 | vorhanden |
| Touch-Log | env `esp32-2432S028R-touchlog` (`-DTEST_TOUCH_LOG=1`): je Berührung `[TOUCH] x=… y=… to …,… z=… t=…ms gap=…ms drop=n/…ms target=…` | B8, B9, B12 | vorhanden (S28) |
| Trefferkarte | env `esp32-2432S028R-hitmap` (`-DTEST_HIT_MAP=1`, mit Touch-Log): 20 Zielkreuze, roter Punkt am Treffer, `[HITMAP] target n (x,y) hit (x,y) dx dy`; Verlassen durch Neustart | B8 | vorhanden (S28) |
| PLL ohne Reset | Testbuild (todo 2.1/2.2) | D5, K6 | zu erstellen |
| Assert/WDT | `-DTEST_LVGL_ASSERT=1`, `-DTEST_LOOP_HANG=1` | Q4, Q5 | vorhanden (S15) |

### 4.3 Smoke-Test (je Entwicklungsphase, beide Boards)

`Dauer` ≈ 45 min · wird nach jeder Phase des Audit-Plans ausgeführt; Ergebnis im Phasen-Review notieren.

| Schritt | Messung | Erwartung |
|---|---|---|
| 1 | Flashen, Boot, Info-Karte | Version und Commit stimmen; Hauptseite erscheint ohne Rauschen (A6, I1/I2) |
| 2 | A1, A2 | genau ein Dialog, danach Verbindung in ≤ 2,5 s |
| 3 | A5 | Einstellungen erhalten, Ausgänge nach Neustart aus |
| 4 | B6 | alle Seiten ohne Abschneiden, ohne Scrollbalken (CONFIG scrollt) |
| 5 | CH1 = 10 MHz an ZÄHLER oder OSZI | 10 MHz ± 100 Hz (unkalibriert), Ausgang an und aus |
| 6 | CH3 IQ 90° an OSZI | zwei Signale mit 25 ns Versatz |
| 7 | K8, K9 | Dropdowns vollständig, Optionen bleiben nach Neustart |
| 8 (ab WEB-HMI) | L1 (Client), `*IDN?` über TCP 5025 und USB, Webseite öffnet | siehe L-Gruppe |

---

## 5. Ablauf und Zeitplan

### 5.1 Reihenfolge und Abhängigkeiten

1. **Vorbereitung** (V1 bis V5) und **Eintrittskriterien**. Erst danach messen.
2. **Robustheit und Umschalten** (A, I, J) bilden die Grundlage: Läuft die Erkennung nicht, sind alle späteren Messungen unsicher.
3. **C2 (Kalibrieren) vor E, F und K**: Alle HF-Messungen nach der Kalibrierung, ohne die Korrektur danach zu verändern (Ausnahme: C4, C5, E7 setzen sie zurück).
4. **Aufwärmzeit**: 15 min mit CH1 an, vor C, E, F.
5. **Langzeittests** (C7, E8, J9, L14, Q) starten am Ende eines Tages und laufen über Nacht.
6. **Bedientests mit Personen** (B8 bis B20, L16 bis L21) zuletzt, wenn die Firmware stabil ist.

### 5.2 Zeitplan (Richtwert; ≈ 8–9 Arbeitstage + Auswertung)

| Tag | Inhalt | Geräte | Dauer |
|---|---|---|---|
| 1 | V1 bis V5, Eintrittskriterien, A (außer A7), K1, K2, A9, A10, A11 | OSZI, DMM, ZÄHLER | 6 h |
| 2 | **Modul umstecken**: A6, A7, B6, B7, I1 bis I8 auf beiden Boards, K10 | VIDEO, LOGIK | 6 h |
| 3 | J1 bis J15 (Board ohne Modul: J3, J12, J13); Start C7 (Kaltstart) am Morgen | LOGIK, ZÄHLER | 7 h |
| 4 | C1 bis C8, D1 bis D5 | ZÄHLER, REF, OSZI | 7 h |
| 5 | E1 bis E9 | OSZI | 7 h |
| 6 | F1 bis F9, G1, G2, K3 bis K7, H (falls umgesetzt) | SPEKTRUM, SONDE, MISCHER | 8 h |
| 7 | L1 bis L15 (Funktion), L22 bis L24; Start L14 (24 h) | Router, PC, Smartphones | 7 h |
| 8 | L-S1 bis L-S13 | PC, zweiter ESP32 | 7 h |
| 9 | Q1 bis Q5; Auswertung L14 und C7; Bedientests B8 bis B20, L16 bis L21 (mit Personen, Terminvergabe vorab) | Personen, LUX, VIDEO | 8 h |
| danach | Auswertung (Abschnitt 7), Befunde, Handbuch- und todo-Änderungen | – | 1–2 d |

### 5.3 Referenzmessungen R0 (Firmware 1.3.0)

Auf dem Messboard mit der gesicherten 1.3.0-Firmware ausführen (≈ 1 Tag); die Werte dienen dem Vergleich in Abschnitt 7.2:

A1, A2, A3, A5, A6, A10, B6, B7, C1 bis C3, C7, C8, D1, D2, E1 bis E3, F1 bis F4, F6, I3, J1 bis J4, K1, K2, K3, K5 bis K7, Q1 bis Q3.

Die Tests der Gruppen L, L-S sowie Q4, Q5 und die U-Punkte der Gruppe B gibt es erst im Release-Kandidaten.

---

## 6. Messungen

Aufbau der Einträge: Kopfzeile mit Geräten, Aufbau, Läufen, Bezug und Dauer; dann die Schritte; dann **Soll** und **Ergebnis** (✔ ✘ ○ i –, Messwert). „Board“ nennt, wo gemessen wird (Messboard = Board mit SI5351-Modul).

### 6.A Inbetriebnahme und Robustheit

#### A1 · Booten ohne SI5351 (alt 1)

`Geräte` VIDEO · `Board` ohne Modul (oder Modul abgezogen) · `Läufe` R0 R1 S · `Bezug` A-34, A-36, E-17 · `Dauer` 5 min

1. Board ohne Modul an CN1 an den PC anschließen, Serial Monitor starten, EN-Taste drücken.
2. Anzahl der Dialoge zählen; Text lesen; `OK` tippen.
3. MAIN, CH1-Seite und CH3-Seite ansehen; CONFIG-Info-Karte ansehen.
4. Einen Wert ändern (z. B. CH1 auf 7,100 000 MHz) und 10 s warten.

**Soll:** genau **ein** Dialog „SI5351 not found …“ mit dem Hinweis auf SDA/SCL in beliebiger Reihenfolge; `OK` schließt ihn und er kommt nicht wieder; MAIN zeigt `NO HW` bei allen drei Kanälen, die Kanalseiten `SI5351 not found`; Info-Karte `SI5351 not found`; die Änderung wird gespeichert (`[SETTINGS] Saved to NVS`); keine wiederholten Fehlerzeilen im Log.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A2 · Modul nachträglich anschließen (alt 1a)

`Geräte` VIDEO, ZÄHLER oder OSZI · `Board` Messboard · `Läufe` R0 R1 S · `Bezug` A-35, A-41 · `Dauer` 10 min

1. Wie A1 booten (SDA abgezogen), den Dialog **nicht** schließen.
2. SDA anstecken, mit VIDEO festhalten (Stoppuhr im Bild); Zeit vom Kontakt bis zum Verschwinden von `NO HW` messen (3 Wiederholungen).
3. Dialogzustand prüfen; CH1 = 10 MHz einschalten.

**Soll:** `NO HW` verschwindet nach **≤ 2,5 s**; der offene SI5351-Dialog schließt sich selbst; Log `[APP] SI5351 at 0x60, SDA=22 SCL=27`; CH1 liefert 10 MHz.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (3 Zeiten):

#### A3 · Verbindung im Betrieb unterbrechen (alt 1b)

`Geräte` ZÄHLER oder OSZI · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-35, E-17 · `Dauer` 10 min

1. CH1 = 10 MHz einschalten, Ausgang an ZÄHLER.
2. SDA kurz abziehen (≈ 5 s), danach wieder anstecken.
3. Dialog, Status und Zählerwert beobachten; Log mitlesen.

**Soll:** Dialog „Connection lost. Check the cable; SDA/SCL may be in either order …“, MAIN `NO HW`; das Modul bleibt versorgt, der Ausgang läuft weiter (Handbuch §11); nach dem Anstecken `[APP] Restoring outputs`, CH1 wieder `ON` mit derselben Frequenz (Zähler), der Dialog schließt sich selbst.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A4 · Versorgungsausfall des Moduls (alt 1c)

`Geräte` OSZI · `Aufbau` 3 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-35, A-18 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH3 an (Master CH1, 270°); beide Ausgänge an OSZI, Phase messen (Abschnitt 3.4).
2. 3,3 V am Modul kurz trennen (≈ 2 s), wieder anschließen.
3. Verhalten wie A3 beobachten; danach die Phase erneut messen.

**Soll:** wie A3; CH3 läuft danach wieder mit 270° (± 1° + u_φ); beide Ausgänge aus dem Rücksetzzustand wiederhergestellt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Phase vorher/nachher):

#### A5 · Speichern und Neustart (alt 14)

`Geräte` – · `Board` beide · `Läufe` R0 R1 S · `Bezug` A-4, A-20 bis A-23 · `Dauer` 10 min

1. Einstellungen ändern: CH1 = 7,100 000 MHz, CH2 = 14,200 000 MHz, CH3 Master CH2, Phase 90°, Drive 4 mA, Backlight 60 %, Tuning `Low noise`; alle drei Kanäle einschalten.
2. Auf CONFIG die Titelzeile beobachten (`Saving…` → `Saved` nach 5 s); Log `[SETTINGS] Saved to NVS`.
3. Strom aus (Stecker), wieder ein.

**Soll:** `Saved` nach ≈ 5 s; nach dem Neustart alle Werte erhalten; **alle Ausgänge OFF** (E-11).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A6 · Start ohne Rauschen (alt 14a)

`Geräte` VIDEO · `Board` beide (Modul umstecken) · `Läufe` R1 S · `Bezug` A-33 · `Dauer` 15 min

1. Je Board 10× einschalten (Stecker), die Anzeige mit Slow-Motion aufnehmen.
2. Bilder auf weiße oder verrauschte Frames prüfen; Zeit bis zur Hauptseite notieren (Q3).

**Soll:** Beleuchtung bleibt aus, bis das erste Bild gezeichnet ist; kein weißes oder verrauschtes Bild; Hauptseite erscheint direkt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehlstarts von 20):

#### A7 · Flashen beider Boards (alt 17)

`Geräte` PC · `Board` beide · `Läufe` R0 R1 · `Bezug` A-45, A-44 · `Dauer` 15 min

1. Im Checkout des Prüflings `pio run -t upload --upload-port COM7`, dann `--upload-port COM13` (ohne `-e`).
2. Build-Ausgabe lesen: Es darf nur `esp32-2432S028R` gebaut werden.
3. Je Board starten, Info-Karte und Log ansehen.

**Soll:** nur eine Firmware-Umgebung; Display läuft auf beiden Boards; Log `[DISPLAY] ILI9341 (detected)` bzw. `ST7789 (detected)` passend zu V1.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A8 · IQ-Fehlerzustand (alt 18b)

`Geräte` OSZI · `Aufbau` 3 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-19, A-37, E-12 · `Dauer` 10 min

1. Build mit `-DTEST_FAULT_IQ=1` flashen.
2. CH1 an (10 MHz), CH3 an (Master CH1), Phase 270°.
3. Status auf MAIN, CH3-Seite und Header lesen; Dialoge zählen; CH1 am OSZI prüfen.
4. Phase auf 90° stellen.

**Soll:** CH3 `ERR` (MAIN, Header, Infozeile `IQ setup failed, retrying`), **ein** Dialog „CH3 IQ setup failed“, CH1 läuft ungestört weiter; bei 90° nach ≤ 2 s CH3 `ON`, Log `[APP] Output error cleared`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A9 · Pegel im Zustand OFF, Werkseinstellung (neu)

`Geräte` OSZI, DMM · `Aufbau` 6 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-3, A-8, E-21 · `Dauer` 10 min

1. `Off state` = `Low` (Standard) prüfen. CH1 einmal einschalten und wieder ausschalten.
2. DC-Pegel an CLK0 messen: mit OSZI (DC, 1 MΩ), mit OSZI (50 Ω) und mit DMM.
3. Dasselbe an CLK1 und CLK2 (CH3 ein- und wieder ausschalten).

**Soll:** an allen drei Ausgängen ≈ 0 V (< 0,1 V) in jeder Last.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwerte (CLK0/1/2 an 1 MΩ, 50 Ω):

#### A10 · Ausgang beim Neustart (alt 18c, neu gefasst)

`Geräte` OSZI · `Aufbau` 6 mit Kanal 2 an dem EN-Pin des ESP32 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-34, 2.3 (todo) · `Dauer` 15 min

1. CH1 = 10 MHz einschalten. OSZI: Kanal 1 an CLK0, Kanal 2 an EN (Reset-Leitung, Masse gemeinsam); Zeitbasis 100 ms/div, Trigger auf fallende Flanke von EN, Single.
2. Reset durch die EN-Taste auslösen (alternativ Upload). Aufnahme speichern.
3. Zeit vom Reset (EN fällt) bis zum Ende des 10-MHz-Signals ablesen; zehn Wiederholungen.

**Soll:** zu dokumentieren; Erwartung: der Ausgang läuft bis zur Initialisierung weiter und endet **vor** der Anzeige der Hauptseite; die Dauer kommt als Hinweis ins Handbuch (Sicherheit für angeschlossene Schaltungen).
**Ergebnis:** i · Messwert (min / max / Mittel in ms):

#### A11 · CLK2 im IQ-Fehlerzustand (neu, Audit C-1)

`Geräte` OSZI · `Aufbau` 6 · `Board` Messboard · `Läufe` R1 · `Bezug` A-19, A-8, C-1 · `Dauer` 15 min

1. Build mit `-DTEST_FAULT_IQ=1`. `Off state` = `High` einstellen.
2. CH1 = 10 MHz an, CH3 an (Master CH1), Phase 270° → CH3 steht auf `ERR`.
3. CLK2 am OSZI (DC, 1 MΩ) beobachten, 1 min lang; danach Phase 90° (CH3 läuft) und wieder 270°.
4. `Off state` = `Low` und `High-Z` wiederholen.

**Soll:** bei `ERR` darf CLK2 **nicht** schwingen; nur der gewählte Ruhepegel (High: ≈ 3,3 V, Low: 0 V, High-Z: undefiniert, nicht oszillierend). Vor der Korrektur C-1 zeigte CLK2 im Fehlerzustand das alte Signal (Fehlerbild).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.B Bedienung

Für B8 bis B20 gelten die Bedingungen aus Abschnitt 2.4. Die Auswertung der Touch-Daten läuft mit dem Skript `touchstat.py` (Anhang D) bzw. von Hand.

#### B2 · Wiederholung beim Halten (alt 12)

`Geräte` VIDEO · `Board` beide · `Läufe` R0 R1 · `Bezug` A-30 · `Dauer` 10 min

1. CH1-Seite, Schritt 1 kHz, `+` halten, mit Slow-Motion die Anzeige filmen.
2. Zeit bis zum ersten Wiederholungsschritt und Abstand der folgenden Schritte aus dem Video lesen.
3. Auf Flackern achten.

**Soll:** erster Schritt nach ≈ 0,4 s, dann alle ≈ 0,1 s; kein Flackern.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (t₁, Δt):

#### B3 · Numpad: ungültige und gültige Eingaben (alt 13)

`Board` beide · `Läufe` R0 R1 · `Bezug` A-2, E-5 · `Dauer` 10 min

1. NUMPAD öffnen; „500“ und `MHz`; dann `C`, „0.5“ und `MHz`; dann `C`, „10.7“ und `MHz`.
2. Zusätzlich „1“ und `Hz` (unter 1 MHz) prüfen; Verhalten bei Bereich 8 kHz … 160 MHz nur, falls 2.4 umgesetzt ist (H).

**Soll:** zweimal `Allowed 1 - 100 MHz` in Rot mit rotem Rahmen, die Eingabe bleibt stehen (U-9, seit S26); dann 10.700 000 MHz.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B4 · Numpad schließen mit kurzem Finger-Nachhalten (alt 13a)

`Board` beide · `Läufe` R0 R1 · `Bezug` A-30, A-32 · `Dauer` 5 min

1. „5“ `kHz`: Bestätigen und den Finger ca. 0,5 s liegen lassen.
2. Danach „100“ `Hz` mit demselben Vorgehen.

**Soll:** Kanalseite erscheint; das Numpad öffnet **nicht** erneut; die Schrittweite bleibt unverändert.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B5 · Ziffer antippen (alt 18a)

`Board` beide · `Läufe` R0 R1 · `Bezug` A-30 · `Dauer` 5 min

1. CH1-Seite; auf die Ziffer der 1-kHz-Stelle tippen; danach auf `MHz`.

**Soll:** Schritt 1 kHz gewählt, die Ziffer ist in der Kanalfarbe markiert; Tipp auf `MHz` öffnet das NUMPAD.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B6 · Sichtprüfung aller Seiten (alt 15)

`Geräte` VIDEO (Fotos) · `Board` beide · `Läufe` R0 R1 S · `Bezug` A-25 · `Dauer` 15 min

1. Je Board MAIN, CH1, CH2, CH3 (alle Zustände: ON, OFF, WAIT, ERR, NO HW), CONFIG (oben, unten gescrollt), NUMPAD, Dialog fotografieren.
2. Zustände mit `TEST_FAULT_IQ` (ERR) und abgezogenem Modul (NO HW) erzeugen.
3. Auf abgeschnittene Texte, Überlappungen und Scrollbalken prüfen (CONFIG: nur senkrechte Leiste).
4. RGB-LED je Seite ablesen (A-27, E-3).

**Soll:** nichts abgeschnitten, nichts überlappt, kein Scrollbalken außer auf CONFIG; RGB-LED: CH1 rot, CH2 grün, CH3 blau, auf MAIN, CONFIG und NUMPAD aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert / Fotos:

#### B7 · Randtasten treffen

`Geräte` VIDEO · `Board` beide · `Läufe` R0 R1 · `Bezug` A-32, E-1 · `Dauer` 15 min

1. Je Element 10× mit dem Finger antippen: Schritt-Chips `1` und `1M`, `MAIN`, `CONFIG`, Dialog-`OK`, Schalter auf den Karten.
2. Fehltipps (keine Reaktion oder falsches Ziel) zählen.

**Soll:** 10/10 Treffer an jedem Ziel auf jedem Board (feste Touch-Kalibrierung).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehltipps je Ziel):

#### B8 · Trefferkarte (Audit U-5)

`Geräte` PC (Auswertung) · `Board` beide · `Läufe` R1 · `Bezug` A-32, A-26, E-1 · `Dauer` 45 min · Build: Touch-Log + Trefferkarte

1. Trefferkarte öffnen: 20 Zielkreuze, darunter die vier Ecken (0 px) und Positionen 4 px vom Rand.
2. P1 bis P3 tippen je Kreuz 5×, einmal mit dem Finger, einmal mit dem Stift (B19); Touch-Log je Berührung mitschreiben.
3. Auswertung: mittlere und maximale Abweichung (px) je Zielbereich (Ecken, Rand, Mitte) und je Board.

**Soll:** zu dokumentieren; daraus folgen die Mindestgrößen und der Randabstand für Tasten (U-4, U-5); Ecken sollen innerhalb 4 px liegen (Klemmung auf 319, 0 beachten).
**Ergebnis:** i · Messwert (mittel/max je Bereich): Vorlauf 2026-11-04 (ein Bediener, Finger, Firmware 2073e2d mit fester Kalibrierung): Original-CYD dx −11 … −19 px in der Mitte, −4 … −12 px am Rand, dy 0 … +6 px; rechts nur x ≈ 305–308 erreichbar, die rechte Ecke (319) nicht. CYD2USB (55 Tipps): dx bei x = 80 −14 … −19 px, bei 160 −10 … −15, bei 240 −5 … −8, am rechten Rand −1 … −7; dy −7 … +3 (ein Ausreißer +11). Beide Boards linear (Versatz und Steigung in x, Original-CYD ≈ 1,03, CYD2USB ≈ 1,06) → U-26 (Touch-Abgleich, S28a). Abgleich mit S28a am 2026-11-04: CYD2USB x 161…3778, y 245…3830, Prüfkreuz −2/+2 px; Original-CYD x 166…3716, y 280…3885, −2/+1 px (statt fest 300…3800). Trefferkarte mit Abgleich wiederholen.

#### B9 · Trefferquote echter Ziele (Audit U-4, U-6)

`Board` beide · `Läufe` R1 · `Bezug` A-26 · `Dauer` 45 min · Build: Touch-Log

1. Aufgabe je Person (P1 bis P5): je 20 Tipps auf jedes Ziel in zufälliger Reihenfolge: Dialog-`OK`, `MAIN`-Ecke, Phase `90` und `180`, Chips `1` und `1M`, Corr. `−`/`+`, Listeneintrag auf CONFIG, Numpad `◀` und `C`.
2. Treffer, Fehltipps, Nachbartreffer (falsche Taste) aus dem Touch-Log zählen.

**Soll:** ≤ 2 % Fehltipps; **0** Nachbartreffer bei Phase und Schaltern.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehltippquote je Ziel):

#### B10 · Karte öffnen ohne Schalten (Audit U-3)

`Board` beide · `Läufe` R1 · `Bezug` A-29 · `Dauer` 15 min

1. Kanal CH1 auf OFF. 20× die Karte über Tipp auf die Frequenz bzw. auf `MHz` öffnen (P1, P2 je 10×).
2. Anzahl der ungewollten Schaltvorgänge (Kanal wird `ON`) notieren.

**Soll:** 0 ungewollte Schaltvorgänge.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B11 · Ziffer-Antippen (Audit U-15)

`Board` beide · `Läufe` R1 · `Bezug` A-30 · `Dauer` 20 min

1. CH1 auf 10.000 000 MHz, dann auf 100.000 000 MHz stellen.
2. Jede Ziffer 10× antippen, die markierte Ziffer ablesen; P1 bis P3.

**Soll:** ≥ 95 % richtige Schrittwahl; sonst Funktion nur als Komfort beschreiben (Handbuch).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Trefferquote je Ziffer):

#### B12 · CONFIG wischen (Audit U-20, E-22)

`Board` beide · `Läufe` R1 · `Bezug` E-22 · `Dauer` 20 min · Build: Touch-Log

1. CONFIG: auf freier Fläche, auf Tasten (`Corr. +` mit Wiederholung) und auf Dropdowns wischen, je mit leichtem und mit festem Druck, je 10×.
2. Während der Einrast-Animation einmal tippen.
3. Druckaussetzer > 35 ms im Touch-Log zählen (ergänzt K10).

**Soll:** 0 ungewollte Auslösungen; keine Aussetzer > 35 ms bei normalem Druck.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B13 · Halten über 5 s (Audit U-20)

`Board` beide · `Läufe` R1 · `Bezug` A-30 · `Dauer` 10 min

1. Step 1 Hz, `+` 5 s lang halten (leicht und fest); Schritte aus der Frequenzänderung zählen.
2. Unterbrechungen im Touch-Log zählen.

**Soll:** ≈ 47 Schritte (0,4 s Verzögerung + 4,6 s / 0,1 s); keine Unterbrechungen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Schritte, Unterbrechungen):

#### B14 · Lesbarkeit aus der Entfernung (Audit U-8)

`Geräte` LUX, VIDEO · `Board` beide · `Läufe` R1 · `Bezug` A-26 · `Dauer` 60 min

1. Raumbeleuchtung ≈ 500 lx (mit LUX einstellen und notieren).
2. Das Gerät zeigt auf MAIN 10 zufällige Zustandskombinationen (ON/OFF/WAIT/ERR/NO HW je Kanal); die Testperson liest von ihrem Platz aus: Entfernung 0,4 / 0,7 / 1,0 / 1,5 m (Maßband); Blickwinkel 0° / 30° / 45°; Backlight 30 / 80 / 100 %.
3. Erkennungsrate je Kombination eintragen (P1 bis P5).

**Soll:** Erkennungsrate ≥ 95 % bei 1,0 m für ON/OFF und Frequenz (Unterricht am Laborplatz); Grenzentfernung je Element dokumentieren.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Rate je Entfernung/Winkel):

#### B15 · Farbsehschwäche (Audit U-7, U-19)

`Geräte` VIDEO · `Board` beide · `Läufe` R1 · `Dauer` 20 min

1. Entweder Testperson mit Rot-Grün-Schwäche oder Fotos der Seiten mit Simulationsfilter (Protanopie, Deuteranopie).
2. Prüfen: CH1/CH2 unterscheidbar, ON/ERR unterscheidbar, markierte Ziffer, gewählter Chip.

**Soll:** alle Zustände sind auch ohne Farbe über Form oder Text unterscheidbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B16 · Aufgabentest (Audit 2.3)

`Geräte` VIDEO, PC (Touch-Log) · `Board` Messboard · `Läufe` R1 · `Dauer` 5 × 30 min + Lehrende · Aufgaben und SUS: Anhang C

1. Person erhält die Aufgaben T1 bis T7 (Anhang C) nacheinander, zuerst **ohne** Handbuch, danach die nicht gelösten **mit** Handbuch; laut denken lassen.
2. Beobachter notiert je Aufgabe: Erfolg (ja/nein/mit Hilfe), Zeit, Tipps (Touch-Log), Fehlgriffe, Zitate.
3. Danach SUS-Fragebogen (Anhang C) und fünf Verständnisfragen (WAIT, NO HW, Out, Corr., Off state, Tuning).

**Soll:** Aufgabenerfolg ohne Handbuch ≥ 80 % (T1 bis T3), SUS ≥ 70; die Verständnisfragen von ≥ 80 % richtig beantwortet.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Erfolg, Zeit, SUS je Person):

#### B17 · Berührung beim Einschalten (Audit U-10)

`Geräte` VIDEO · `Board` beide · `Läufe` R1 · `Bezug` A-44 · `Dauer` 20 min

1. 20× das Gerät in natürlicher Haltung einschalten (Daumen auf dem Display, USB-Stecker einstecken), den Daumen 0,5 bis 3 s liegen lassen.
2. Zählen: Starts mit falschem Bild (Rauschen) und Tipps, die in der Bedienung landen.

**Soll:** 20/20 richtiges Bild, kein Tipp in der GUI (seit 1.3.2 gibt es keine Boot-Geste mehr, E-26).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B18 · Fehlerdialoge mit Testperson

`Board` Messboard · `Läufe` R1 · `Bezug` A-38 · `Dauer` 20 min

1. Wie A1 und A3 ausführen; die Testperson liest die Meldung vor und erklärt, was zu tun ist.
2. `OK` beim ersten Versuch getroffen? Zeit bis zur Reaktion.

**Soll:** Meldung verstanden (Abhilfe genannt); `OK` im ersten Versuch (U-21).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B19 · Eingabestift (Audit U-5)

`Board` beide · `Läufe` R1 · `Dauer` 30 min

1. B8 und B9 mit dem Stift wiederholen (P1 bis P3).

**Soll:** zu dokumentieren; Entscheidung, ob ein Stift am Laborplatz empfohlen wird.
**Ergebnis:** i · Messwert:

#### B20 · Spannungsausfall bemerken (Audit U-22)

`Board` Messboard · `Läufe` R1 · `Bezug` A-4, A-22 · `Dauer` 20 min

1. Während einer laufenden Messung (CH1 an) den USB-Stecker ziehen; die Testperson wird nicht vorgewarnt.
2. Zeit messen, bis sie merkt, dass alle Ausgänge aus sind; nach dem Anstecken: Weiß sie, dass sie die Ausgänge wieder einschalten muss?
3. Änderung < 5 s vor dem Ziehen vornehmen; geht sie verloren (Autosave 5 s)?

**Soll:** Verlust < 5 s alter Änderungen ist dokumentiert und im Handbuch genannt; Testperson bemerkt den Ausfall (Ausgänge aus) innerhalb 30 s.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.C Frequenzgenauigkeit und Kalibrierung

Alle Messungen: Messboard, Aufbau 1, ZÄHLER mit REF, Standardbedingungen, 15 min Aufwärmzeit. Auswertung mit der Klammermessung (Abschnitt 3.3).

#### C1 · Korrektur 0, Quarzfehler bestimmen (alt 3)

`Geräte` ZÄHLER, REF · `Läufe` R0 R1 · `Bezug` A-11 · `Dauer` 10 min

1. CONFIG: `Corr.` auf 0.00 ppm (Schrittweite 0.01, `−` halten bis 0.00 oder aus dem Reset-Zustand starten).
2. CH1 = 10 MHz, einschalten, Zähler 10 Werte mit 1 s Torzeit mitteln.
3. Auf der CH1-Seite die Zeile `Out` ablesen (Abweichung in Klammern).

**Soll:** zu dokumentieren: Abweichung f_mess − 10 MHz (Hz) = Quarzfehler; `Out` zeigt die Rundung der Register (bei 10 MHz mit ganzzahligem Teiler ±0,0 Hz).
**Ergebnis:** i · Messwert (Hz, ppm):

#### C2 · Kalibrieren (alt 4)

`Geräte` ZÄHLER, REF · `Läufe` R0 R1 · `Bezug` A-11, Audit U-11 · `Dauer` 15 min

1. Mit dem Wert aus C1: Korrektur in ppm = Abweichung (Hz) ÷ 10 (Handbuch §8). Mit der Schritttaste (`1`, `0.1`, `0.01`) und `−`/`+` einstellen. **Anzahl der Tipps und Zeit notieren** (Bedienaufwand, Audit U-11: ≈ 20 Tipps).
2. 10 MHz am Zähler erneut messen, nachstellen bis |Abweichung| < 0,2 Hz.
3. Die Korrektur nicht mehr ändern; Wert notieren.

**Soll:** danach < 1 Hz Abweichung bei 10 MHz (Auflösung der Korrektur 0,01 ppm ≙ 0,1 Hz); Wert im Bereich ±50 ppm.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Corr. in ppm, Tipps, Zeit):

#### C3 · Genauigkeit über den Bereich (Klammermessung)

`Geräte` ZÄHLER, REF · `Läufe` R0 R1 · `Bezug` A-12 (≤ 0,5 Hz in `Fractional`; Host-Test ≤ 0,375 Hz) · `Dauer` 45 min

1. Standardbedingungen, Korrektur nach C2.
2. Für jede Frequenz der Tabelle die Klammermessung (Abschnitt 3.3) ausführen; Frequenz über das NUMPAD eingeben, 10 s Einschwingzeit.
3. δ berechnen und eintragen.

| f_soll | f10a (Hz) | f_mess (Hz) | f10b (Hz) | δ (Hz) | Grenze | ✔/✘/○ |
|---|---|---|---|---|---|---|
| 1,000 000 MHz | | | | | 0,5 Hz + u | |
| 3,500 000 MHz | | | | | 0,5 Hz + u | |
| 7,100 000 MHz | | | | | 0,5 Hz + u | |
| 14,200 000 MHz | | | | | 0,5 Hz + u | |
| 28,400 000 MHz | | | | | 0,5 Hz + u | |
| 50,000 000 MHz | | | | | 0,5 Hz + u | |
| 99,999 999 MHz | | | | | 0,5 Hz + u | |

**Soll:** \|δ\| ≤ 0,5 Hz + u (früher „≤ 1 Hz + Zählerfehler“; mit der Klammermessung gilt die Grenze aus A-12).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### C4 · Korrektur +50 ppm bei kleinen Frequenzen (alt 19)

`Geräte` ZÄHLER, REF · `Läufe` R0 R1 · `Bezug` A-11, A-13 · `Dauer` 20 min

1. `Corr.` auf +50.00 ppm stellen.
2. CH1 = 1,9 MHz und 10,9 MHz; Klammermessung wie C3.
3. `Out`-Zeile (Abweichung) mit der Messung vergleichen; Korrektur danach auf den Wert aus C2 zurück.

**Soll:** Abweichung ≈ angezeigte `Out`-Abweichung zuzüglich des Korrektureffekts (Messung relativ zur 10-MHz-Klammer: δ ≤ 0,5 Hz + u); beide Frequenzen gleich gut.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (δ bei 1,9 / 10,9 MHz):

#### C5 · Korrektur um 1 ppm ändern (alt 11)

`Geräte` ZÄHLER, OSZI · `Läufe` R0 R1 · `Bezug` A-11 · `Dauer` 15 min

1. CH1 = 10 MHz; Zähler ablesen; OSZI auf CLK0 mit Timeout-Trigger (Flanke fehlt > 3 Perioden), Single.
2. `Corr.` um +1,00 ppm ändern; Zähler und OSZI beobachten; danach −1,00 ppm.

**Soll:** Frequenz sinkt um ≈ 10 Hz (±0,5 Hz); **kein** Aussetzer am OSZI.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Δf):

#### C6 · Hz-Schritte bei hoher Frequenz (alt 5)

`Geräte` ZÄHLER · `Läufe` R0 R1 · `Bezug` A-1 · `Dauer` 10 min

1. CH1 = 80 MHz, Schrittweite 1 Hz; Zähler ablesen (10 Werte).
2. 10× `+` tippen; Zähler erneut ablesen (10 Werte).

**Soll:** Zähler steigt um ≈ 10 Hz und bleibt nicht hängen (Abweichung ≤ 1 Hz).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Δf):

#### C7 · Aufwärmdrift

`Geräte` ZÄHLER, REF, THERMO · `Läufe` R0 R1 · `Bezug` todo 6 (TCXO) · `Dauer` 60 min (Start nach ≥ 2 h Ruhe, Kaltstart)

1. Board und Modul mindestens 2 h stromlos, Raumtemperatur notieren.
2. Einschalten, CH1 = 10 MHz einschalten (Korrektur nach C2), Zähler Torzeit 10 s, Aufzeichnung in `.csv` (Skript `counter_log.py`, Anhang D).
3. Alle 5 min den Wert notieren (60 min); Raumtemperatur alle 15 min.
4. Drift in ppm über der Zeit auftragen.

**Soll:** zu dokumentieren; Aufwärmzeit fürs Handbuch bestimmen (Zeit bis Drift < 0,1 ppm/10 min); Entscheidung TCXO (Abschnitt 1.4).
**Ergebnis:** i · Messwert (Drift 0–15 min / 15–60 min in ppm):

#### C8 · Kurzzeitstabilität

`Geräte` ZÄHLER, REF · `Läufe` R0 R1 · `Dauer` 15 min

1. Nach der Aufwärmzeit: CH1 = 10 MHz, Torzeit 1 s, 100 Werte aufzeichnen.
2. Standardabweichung (Hz) und Allan-Abweichung für τ = 1 s berechnen.

**Soll:** zu dokumentieren (Rauschen des Quarzes plus Zählerauflösung).
**Ergebnis:** i · Messwert:

---

### 6.D Abstimmen und Umschalten

Aufbau: OSZI Kanal 1 an CLK0 (CH1), Kanal 2 optional an CLK1/CLK2. **Aussetzer erkennen:** Trigger „Timeout“ (Dropout) mit Schwelle ≈ 3 Perioden, Single-Modus; jeder Trigger = ein Aussetzer, die Dauer mit den Cursorn messen. Wenn das OSZI keinen Timeout-Trigger hat: Persistenz und Triggerverlust beobachten, Zeitbasis ≈ 5 Perioden/div, Ergebnis auf „○“.

#### D1 · Kanal 1 bleibt beim Abstimmen von CH2 ungestört (alt 6)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-10 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH2 = 7,100 000 MHz an. OSZI: Timeout-Trigger auf CLK0.
2. CH2 über `+`/`−` mit 1 MHz, 1 kHz, 1 Hz mehrfach verstellen; außerdem per NUMPAD Sprünge zwischen 1 und 99 MHz (mind. 20 Wechsel).
3. Auslösungen am OSZI zählen.

**Soll:** **0** Aussetzer an CH1 (A-10: kein PLL-Reset der anderen PLL).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Aussetzer von 20):

#### D2 · Dauerabstimmen im `Fractional`-Modus (alt 18)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-6, A-10 · `Dauer` 20 min

1. CH1 = 10 MHz, Schritt 1 Hz, `+` 10 s lang halten; Timeout-Trigger auf CLK0.
2. Wiederholen mit CH1 = 99 MHz und Schritt 1 MHz (bis 100 MHz).

**Soll:** keine Aussetzer im Lauf; ein Aussetzer ist nur zulässig bei einem PLL-Wechsel 900 ↔ 887,5 MHz (A-6) und wird dokumentiert (Frequenz, Dauer).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### D3 · IQ: Master abstimmen (Realignment)

`Geräte` OSZI · `Aufbau` 3 · `Läufe` R0 R1 · `Bezug` A-17, todo 2.2 · `Dauer` 20 min

1. CH1 = 10 MHz an, CH3 an (Master CH1, 90°). OSZI: beide Kanäle, Timeout-Trigger auf CLK0, Single.
2. Schrittweite 1 kHz; `+` einmal tippen; Aussetzer beider Ausgänge messen (Dauer, Cursor). 10 Wiederholungen.

**Soll:** Aussetzer beider Ausgänge bei **jedem** Schritt (erwartet); Dauer notieren (Entscheidungsgrundlage für „IQ ohne Reset“, Abschnitt 1.4).
**Ergebnis:** i · Messwert (Dauer min / max in µs):

#### D4 · IQ: Korrektur ändern

`Geräte` OSZI · `Aufbau` 3 · `Läufe` R0 R1 · `Bezug` A-17 · `Dauer` 10 min

1. Wie D3; statt der Frequenz die Korrektur um 0,10 ppm ändern.

**Soll:** wie D3 (Aussetzer, Dauer notieren).
**Ergebnis:** i · Messwert:

#### D5 · PLL-Änderung ohne Reset (nur Testbuild)

`Geräte` OSZI, ZÄHLER, Soundkarte oder SDR · `Läufe` R1 (Testbuild) · `Bezug` todo 2.1, 2.2, E-19 · `Dauer` 30 min

1. Testbuild „PLL ohne Reset“ flashen; Tuning `Low noise`.
2. CH1 springt von 10,000 000 auf 10,001 000 MHz (über NUMPAD und Schritttaste); Messung per SDR-Wasserfall oder ZÄHLER mit kurzer Torzeit (1 ms).
3. Einschwingzeit und Überschwingen (Hz, ms) notieren; im IQ-Betrieb (CH3 90°) danach die Phase messen (Aufbau 3).

**Soll:** zu dokumentieren; die IQ-Phase ist nach der Änderung unverändert (±1°); die Einschwingzeit ist so kurz, dass sie bei Versuchen nicht stört.
**Ergebnis:** i · Messwert:

---

### 6.E IQ-Betrieb

Aufbau 3; Messmethode Abschnitt 3.4; Eigenversatz φ₀ und Unsicherheit u_φ aus V4 eintragen und abziehen. CH1 ist Master, Standardbedingungen, Korrektur nach C2, 15 min Aufwärmzeit. Phase jeweils **2 s nach** dem Umschalten messen.

#### E1 · Vier Phasen bei 10 MHz (alt 7)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-15, A-16 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH3 an (Master CH1); Phase `0`, `90`, `180`, `270` nacheinander einstellen; je Phase Δt messen (≥ 256 Mittelungen) und φ berechnen.

| Phase | Δt (ps) | φ gemessen (°) | Abweichung (°) | Grenze |
|---|---|---|---|---|
| 0° | | | | 1° + u_φ |
| 90° | | | | 1° + u_φ |
| 180° | | | | 1° + u_φ |
| 270° | | | | 1° + u_φ |

**Soll:** 0°, 90°, 180°, 270° ± 1° (+ u_φ).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### E2 · Alle Phasen über den Bereich (alt 8, erweitert)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-15, E-7 · `Dauer` 45 min

1. Für f = 4,73 / 5 / 14,2 / 30 / 99 MHz je vier Phasen messen wie E1 (20 Werte).
2. Der IQ-Teiler ist min(127, ⌊900 MHz / f⌋): 127, 127, 63, 30, 9 (ungerade bei 14,2 und 99 MHz).

| f (MHz) | Teiler | 0° | 90° | 180° | 270° |
|---|---|---|---|---|---|
| 4,73 | 127 | | | | |
| 5 | 127 | | | | |
| 14,2 | 63 | | | | |
| 30 | 30 | | | | |
| 99 | 9 | | | | |

**Soll:** alle vier Phasen innerhalb ±1° (+ u_φ). Bei 99 MHz entspricht 1° nur ≈ 28 ps: Bei u_φ > 0,5° das Ergebnis auf „○“ setzen und mit besserem Aufbau (OSZI höherer Bandbreite, Mittelung) wiederholen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### E3 · Ausgangs-Skew bei Phase 0° (neu aus alt 8)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-15 · `Dauer` 20 min

1. Phase 0°; für f = 5 / 10 / 14,2 / 30 / 50 / 99 MHz den Zeitversatz CLK0 → CLK2 Δt (ps) messen.
2. Prüfen, ob Δt unabhängig von f ist (konstanter Ausgangs-Skew) und φ = Δt · f · 360° die Abweichung erklärt.

**Soll:** konstanter Versatz (± u_t); Phasenfehler = Δt · f · 360° ≤ 1° bis 99 MHz, sonst Skew-Kompensation (Phasen-Feinabgleich, Abschnitt 1.4).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Δt je Frequenz):

#### E4 · 180° bei ungeradem Teiler (neu aus alt 8)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-16, todo 2.2 · `Dauer` 20 min

1. 180° bei 14,2 MHz (Teiler 63) und 99 MHz (Teiler 9) messen; als Vergleich 10 MHz (Teiler 90).
2. Abweichung des Phasenfehlers ungerader gegen gerader Teiler bilden.

**Soll:** Phasenfehler bei ungeradem Teiler höchstens 1° größer als bei geradem; sonst „ungerade Teiler“ umsetzen (todo 2.2).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### E5 · Tastverhältnis

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-1, Handbuch §1 · `Dauer` 20 min

1. Messung: Tastverhältnis = positive Impulsbreite / Periode, Schwelle 50 %, ≥ 100 Perioden gemittelt.
2. Messpunkte: CLK0 bei 7,1 MHz (fraktional), 10 MHz (ganzzahlig), CLK2 bei 14,2 MHz (IQ, Teiler 63) und 99 MHz (Teiler 9).

**Soll:** zu dokumentieren; die Dokumentation verspricht 50 %. Abweichung > 2 % → Handbuch und A-1 präzisieren.
**Ergebnis:** i · Messwert (%, je Punkt):

#### E6 · CH3 wartet unter 4,730 MHz (alt 9)

`Geräte` – (Display) · `Läufe` R0 R1 · `Bezug` A-18 · `Dauer` 10 min

1. CH1 an, CH3 an (Master CH1). Masterfrequenz auf 4,72 MHz, dann auf 4,73 MHz stellen.
2. Statuszeilen der CH3-Seite und von MAIN lesen; OSZI zeigt CLK2.

**Soll:** bei 4,72 MHz `WAIT` / `Waiting: CH1 below 4.730 MHz`, CLK2 aus, kein Dialog; bei 4,73 MHz startet CH3 **automatisch** (`ON`).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### E7 · Korrektur −1 ppm bei 100 MHz (alt 13b)

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-15, A-17 · `Dauer` 10 min

1. `Corr.` auf −1,00 ppm; CH1 = 100 MHz an, CH3 an (Master CH1).
2. Status und Meldungen beobachten; Korrektur zurück auf den Wert aus C2.

**Soll:** Master und CH3 laufen; keine Meldung „CH3 IQ setup failed“.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### E8 · Phasendrift (30 min)

`Geräte` OSZI, REF, THERMO · `Läufe` R0 R1 · `Bezug` A-15 · `Dauer` 35 min (Langzeit)

1. CH1 = 10 MHz, CH3 90°, nach der Aufwärmzeit.
2. Alle 5 min φ messen (Mittelung), Raumtemperatur notieren.

**Soll:** Drift < 0,5° in 30 min.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### E9 · Wiederholgenauigkeit nach Realignment

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-17 · `Dauer` 25 min

1. CH1 = 10 MHz, CH3 90°. Master 20× um 1 kHz verstellen (`+`, danach `−` usw.).
2. Nach jedem Schritt (2 s Wartezeit) φ messen; Mittelwert und Standardabweichung berechnen.

**Soll:** nach jedem Realignment wieder 90° (Mittelwert ± 1° + u_φ), Streuung < 0,5°.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Mittel, σ):

#### E10 · Ausbildungsaufbau mit Mischern (optional)

`Geräte` SPEKTRUM, QSD oder zwei Mischer mit 90°-NF-Netzwerk · `Läufe` R1 · `Bezug` E1 bis E3 · `Dauer` 45 min

1. IQ an QSD (FST3253) oder zwei Mischer; Testsignal einmal oberhalb und einmal unterhalb des LO (Offset z. B. +1 kHz / −1 kHz).
2. Seitenbandunterdrückung messen.

**Soll:** passt zu E1 bis E3 (1° ≈ 41 dB, 2° ≈ 35 dB Unterdrückung).
**Ergebnis:** i · Messwert:

---

### 6.F Spektrale Reinheit und Phasenrauschen

Aufbau 2, Messboard, Standardbedingungen, WLAN aus, Korrektur nach C2, 15 min Aufwärmzeit. Einstellungen des SPEKTRUM je Messung im Protokoll festhalten (Abschnitt 3.5). Eigenrauschen und Eigenverzerrung des Analysators aus V4 danebenhalten.

#### F1 · Oberwellen in Abhängigkeit von Drive (A-5)

`Geräte` SPEKTRUM · `Läufe` R0 R1 · `Bezug` A-1, A-5 · `Dauer` 30 min

1. CH1 = 10 MHz an. Drive 2 / 4 / 6 / 8 mA (CONFIG, `Drive all`).
2. Je Drive: Grundwelle (dBm) und 2. bis 5. Harmonische (dBc), Span 0 … 60 MHz, RBW 1 kHz; die Dämpfung der 20-dB-Glieder korrigieren.

| Drive | Grundwelle (dBm) | 2. (dBc) | 3. (dBc) | 4. (dBc) | 5. (dBc) |
|---|---|---|---|---|---|
| 2 mA | | | | | |
| 4 mA | | | | | |
| 6 mA | | | | | |
| 8 mA | | | | | |

**Soll:** zu dokumentieren; ein Rechteck mit 50 % Tastverhältnis hat 3. ≈ −9,5 dBc, 5. ≈ −14 dBc, 2. und 4. sehr klein; größere 2. Harmonische deutet auf Tastverhältnis ≠ 50 % (E5).
**Ergebnis:** i · Messwert:

#### F2 · Ausgangspegel an 50 Ω

`Geräte` SPEKTRUM · `Läufe` R1 · `Bezug` Handbuch (Pegeltabelle), G1 · `Dauer` 30 min

1. CH1 an; f = 1 / 10 / 30 / 100 MHz; Drive 2 / 4 / 6 / 8 mA; Pegel der Grundwelle an 50 Ω (mit Dämpfungskorrektur).

| f (MHz) | 2 mA (dBm) | 4 mA | 6 mA | 8 mA |
|---|---|---|---|---|
| 1 | | | | |
| 10 | | | | |
| 30 | | | | |
| 100 | | | | |

**Soll:** zu dokumentieren (Pegeltabelle fürs Handbuch; LO-Pegel für Mischer).
**Ergebnis:** i · Messwert:

#### F3 · Nebenlinien um den Träger (alt: F3)

`Geräte` SPEKTRUM · `Läufe` R0 R1 · `Bezug` E-19 · `Dauer` 45 min

1. Span 200 kHz, RBW 100 Hz, Mittelung 10, Träger bei Mittenfrequenz, Marker-Delta für Nebenlinien.
2. Drei Zustände (alle `Fractional`, Korrektur 0):
   - a) 10,000 000 MHz (ganzzahliger Teiler);
   - b) 7,100 000 MHz (fraktionaler Teiler);
   - c) 7,100 000 MHz mit CH3 an (IQ: ganzzahliger Teiler, fraktionale PLL).
3. Größte Nebenlinien innerhalb ±100 kHz in dBc und Abstand (Hz) eintragen.

| Zustand | Nebenlinien (dBc @ Abstand) | Eigenrauschen-Reserve (dB) | ✔/i |
|---|---|---|---|
| a) 10 MHz ganzzahlig | | | |
| b) 7,1 MHz fraktional | | | |
| c) 7,1 MHz + CH3 | | | |

**Soll:** zu dokumentieren; Vergleich der Tuning-Modi in K5.
**Ergebnis:** i

#### F4 · Wechsel ganzzahlig → fraktional

`Geräte` SPEKTRUM · `Läufe` R0 R1 · `Bezug` E-19 · `Dauer` 15 min

1. Wie F3 bei 10 MHz mit Korrektur 0 und danach +0,01 ppm (macht den Teiler fraktional).

**Soll:** Zeigt den Wechsel (Nebenlinien erscheinen); dokumentieren.
**Ergebnis:** i · Messwert:

#### F5 · Phasenrauschen

`Geräte` SPEKTRUM, REF · `Läufe` R0 R1 · `Bezug` E-19 · `Dauer` 45 min

1. CH1 = 10 MHz, danach 14,2 MHz; Span 200 kHz, RBW 10 Hz (bei 100 Hz Ablage 3 Hz, falls möglich), Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz Ablage.
2. Eigenrauschen aus V4 daneben eintragen.

| f | 100 Hz | 1 kHz | 10 kHz | 100 kHz |
|---|---|---|---|---|
| 10 MHz (dBc/Hz) | | | | |
| Eigenrauschen | | | | |
| 14,2 MHz (dBc/Hz) | | | | |

**Soll:** zu dokumentieren; Werte < 6 dB über dem Eigenrauschen sind obere Schranken (○).
**Ergebnis:** i

#### F6 · Störungen durch die Plattform (Audit 2.1)

`Geräte` SPEKTRUM, STROM optional · `Board` beide (Modul umstecken) · `Läufe` R0 R1 · `Bezug` todo 2.1, E-23 · `Dauer` 60 min

1. CH1 = 10 MHz; Span 20 kHz, RBW 10 Hz, Max-Hold 60 s.
2. **a) Backlight:** CONFIG `Backlight` 100 / 50 / 10 % (Minimum 10 %, E-23); Seitenlinien bei ±5 kHz (PWM) und Vielfachen suchen.
3. **b) Neuzeichnen:** auf der CH1-Seite `+` halten (ständiges Neuzeichnen); Spektrum vor und während.
4. **c) Überwachungstakt:** Zero-Span auf dem Träger (RBW 1 kHz, 10 s): periodische Störungen im 2-s-Takt (I2C-Überwachung)?
5. **d) Versorgung:** PC-USB, USB-Netzteil und Powerbank vergleichen.
6. Alles auf beiden Boards (Modul umstecken); WLAN aus.

| Fall | Nebenlinien (Abstand, dBc) | Periodische Störung? | Board |
|---|---|---|---|
| 100 % | | | |
| 50 % | | | |
| 10 % | | | |
| `+` gehalten | | | |
| 2-s-Takt | | | |
| PC / Netzteil / Powerbank | | | |

**Soll:** zu dokumentieren; Entscheidung über Abhilfen (Abschnitt 1.4).
**Ergebnis:** i

#### F7 · Übersprechen CH1 ↔ CH2

`Geräte` SPEKTRUM · `Läufe` R0 R1 · `Dauer` 20 min

1. CH1 = 10 MHz an; CH2 = 10,1 MHz zuerst aus, dann an.
2. Spektrum an CLK0: Linie bei 10,1 MHz in dBc.

**Soll:** zu dokumentieren (Übersprechen mit CH2 aus bzw. an, z. B. durch gemeinsame Versorgung).
**Ergebnis:** i · Messwert:

#### F8 · Spektrum CLK2 gegen CLK0 (alt 10)

`Geräte` SPEKTRUM · `Läufe` R0 R1 · `Bezug` A-15 · `Dauer` 20 min

1. CH1 an, CH3 an (IQ). f = 4,73 MHz und 10 MHz. CLK0 und CLK2 nacheinander am SPEKTRUM.

**Soll:** an CLK2 keine zusätzlichen Nebenlinien gegenüber CLK0.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### F9 · Störlinien der Plattform im Nahfeld

`Geräte` SPEKTRUM, SONDE · `Board` beide · `Läufe` R1 · `Dauer` 45 min

1. SI5351-Ausgänge aus (OFF), WLAN aus; Sonde ≈ 1 cm über dem Display, über dem ESP32-Modul und über den Ausgangsleitungen; Span 1–500 MHz.
2. Störlinien mit Frequenz und Pegel notieren; Zuordnung vermuten (40-MHz-Quarz, SPI-Takt 40 MHz und Vielfache, 25-MHz-SI5351-Quarz).
3. Wiederholen mit eingeschaltetem CH1 (10 MHz): Mischprodukte?

**Soll:** zu dokumentieren; Abstand für Empfängerversuche und Hinweis im Handbuch.
**Ergebnis:** i · Messwert:

---

### 6.G Mischer-Versuche (Ausbildung)

#### G1 · Diodenringmischer

`Geräte` SPEKTRUM, MISCHER · `Aufbau` 4 · `Läufe` R1 · `Bezug` F2, E-21 · `Dauer` 45 min

1. `Off state` = `Low` (nicht `High`). LO = CH1 10 MHz mit Drive und Pad auf +7 dBm (Pegel nach F2). RF = CH2 10,455 MHz über 30 dB Dämpfung.
2. Pegel RF am Mischer (Messung), ZF-Linien bei 455 kHz und 20,455 MHz mit dem SPEKTRUM.
3. Konversionsverlust = P(RF) − P(ZF 455 kHz); LO-Durchschlag am ZF-Anschluss in dBc.

**Soll:** ZF bei 455 kHz und 20,455 MHz, Konversionsverlust ≈ 6–7 dB; LO-Durchschlag dokumentieren.
**Ergebnis:** i · Messwert:

#### G2 · Zwei Ausgänge, Schwebung an der Soundkarte

`Geräte` MISCHER, Soundkarte oder SDR · `Aufbau` 4 · `Läufe` R1 · `Dauer` 30 min

1. CH1 = 10,000 MHz und CH2 = 10,001 MHz an den Mischer; ZF an die Soundkarte (Line-In, ggf. Dämpfung).
2. FFT (≥ 96 kHz Abtastrate): 1-kHz-Ton, Seitenlinien, Rauschsockel.

**Soll:** sauberer 1-kHz-Ton; Seitenlinien und Rauschsockel dokumentieren (Phasenrauschen beider Kanäle).
**Ergebnis:** i · Messwert:

---

### 6.H Nach der Bereichserweiterung (nur wenn todo 2.4 umgesetzt ist)

Sonst „–“. ZÄHLER, Klammermessung wie C3; SPEKTRUM bis 1 GHz.

#### H1 · Untergrenze und kHz-Anzeige (alt 20)

`Geräte` ZÄHLER · `Läufe` R1 · `Bezug` E-16 · `Dauer` 15 min

1. CH1 = 8 kHz, 455 kHz, 999,999 kHz, 1 MHz; Zähler und Anzeige.

**Soll:** Frequenz stimmt (≤ 0,5 Hz + u); Anzeige in kHz bzw. MHz laut Handbuch.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### H2 · Oberes Ende und 112,5 MHz (alt 21)

`Geräte` ZÄHLER, SPEKTRUM · `Läufe` R1 · `Bezug` E-16, todo 2.4 · `Dauer` 20 min

1. CH1 = 112,499 996 / 112,500 004 / 120 / 160 MHz; Frequenz und Spektrum.

**Soll:** Abweichung ≤ 1,5 / 2 / 2 / 3 Hz (todo 2.4); Ausgang vorhanden (Pegel geringer).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### H3 · CH3 über 100 MHz (alt 22)

`Geräte` OSZI · `Läufe` R1 · `Bezug` A-18 · `Dauer` 10 min

1. CH3 an (Master CH1); Master von 100 MHz auf 100,000 001 MHz stellen.

**Soll:** CH3 wechselt auf `WAIT … above 100 MHz`, CLK2 aus, Master läuft weiter.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### H4 · Oberwellen bei 455 kHz

`Geräte` SPEKTRUM · `Läufe` R1 · `Dauer` 10 min

1. CH1 = 455 kHz; Spektrum 0 … 5 MHz.

**Soll:** zu dokumentieren; daraus folgt ein Tiefpass-Hinweis im Handbuch.
**Ergebnis:** i · Messwert:

---

### 6.I Display-Erkennung (eine Firmware, beide Boards)

I0 bis I2 sind am 2026-10-28 gelaufen (Messwerte in Spezifikation E-18); die Ergebnisse bleiben stehen. Nach dem Flashen der Einheitsfirmware: 5/5 EN-Resets je Board richtig erkannt (COM13 Original-CYD: ILI9341, ID 000000; COM7 CYD2USB: ST7789, ID 8181B3, Stand 2026-10-28; die Zuordnung der Ports ist neu zu prüfen, V1).

| Nr | Test | Ergebnis |
|---|---|---|
| I0 | Diagnose-Sketch: MADCTL-Rücklesen, RDDID, Pull an GPIO12, SPI-Modus 0/3, Schreiben mit 40 MHz | ✔ erledigt 2026-10-28, siehe Spezifikation E-18 |
| I1 | Original-CYD einschalten: Log „ILI9341 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B7 |
| I2 | CYD2USB einschalten: Log „ST7789 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B7 |

#### I3 · Erkennung im Wiederholtest

`Geräte` VIDEO · `Board` beide (Modul umstecken) · `Läufe` R0 R1 · `Bezug` A-44, E-18 · `Dauer` 30 min

1. Je Board 20× Kaltstart (USB-Stecker ziehen und stecken) und 20× EN-Taste.
2. Je Start: Log `[DISPLAY] … (detected)` und Bild kontrollieren.

**Soll:** 40/40 richtig erkannt, nie Rauschen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehlerkennungen je Board):

#### I4 · Gespeicherte Wahl der alten Boot-Geste wird gelöscht

`Board` CYD2USB · `Läufe` R0 · `Bezug` A-44, E-26 · `Dauer` 15 min

1. Firmware 1.3.1 flashen, eine Boot-Geste ausführen (Display beim Einschalten 1,5 s halten): Rauschen, `disp_panel` = 1 gespeichert.
2. Firmware ab 1.3.2 flashen (ohne Löschen des NVS), einschalten.
3. Log und Info-Karte ablesen; ein zweites Mal einschalten.

**Soll:** beim ersten Start Log `[SETTINGS] Stored display choice removed; the controller is detected` und `[DISPLAY] ST7789 (detected)`, Bild normal; beim zweiten Start keine Löschmeldung mehr.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### I5 · Lange Berührung beim Einschalten

`Board` beide · `Läufe` R0 R1 · `Bezug` A-44 · `Dauer` 10 min

1. Display beim Einschalten länger als 3 s halten, dann loslassen; auf Fehl-Tipps in der GUI achten.
2. Nur 1 s berühren und weiter halten, bis die Hauptseite erscheint.

**Soll:** kein Tipp landet in der GUI.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### I6 · Kurze Berührung beim Einschalten

`Board` beide · `Läufe` R0 R1 · `Bezug` A-44 · `Dauer` 5 min

1. Beim Einschalten kurz (< 1,5 s) berühren, 10×.

**Soll:** keine Umschaltung des Controllers.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### I7 · Langzeit-Anzeige auf dem CYD2USB

`Board` CYD2USB · `Läufe` R0 R1 · `Bezug` E-18 · `Dauer` 30 min

1. 30 min Bedienung: Seitenwechsel, NUMPAD, `+` halten, CONFIG scrollen.

**Soll:** keine Pixelfehler (SPI-Modus 0 mit 40 MHz).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### I8 · A und B sowie F6 mit der Einheitsfirmware

`Board` beide · `Läufe` R0 R1 · `Dauer` siehe A, B, F6

1. Messplan A, B und F6 auf beiden Boards ausführen (Modul umstecken).

**Soll:** unverändert gegenüber dem Messboard.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.J SDA/SCL-Erkennung

Die Identitätsprüfung (Register 0, 183, Rücklesen von Register 3) folgt AN619; J7 prüft sie am eigenen Modul und, wenn möglich, an einem Klon (MS5351M). Lehnt J7 ein echtes Modul ab, die harten Prüfungen in `SI5351Manager::begin()` lockern (E-17). Debug-Build (`-DLOG_DEBUG=1`) bei J6, J7, J9, J14.

#### J1 · Kabel gerade

`Geräte` ZÄHLER · `Board` Messboard · `Läufe` R0 R1 S · `Bezug` A-41 · `Dauer` 5 min

1. SDA an GPIO22, SCL an GPIO27 (CN1), booten; Log und Info-Karte lesen; CH1 = 10 MHz an den Zähler.

**Soll:** verbunden, Log `SDA=22 SCL=27`, kein Dialog.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J2 · Kabel vertauscht

`Geräte` ZÄHLER · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-41, E-17 · `Dauer` 5 min

1. SDA und SCL vertauschen, booten; Log und Info-Karte; CH1 = 10 MHz.

**Soll:** verbunden, Log `SDA=27 SCL=22 (swapped)`, Info-Karte mit `(swapped)`; 10 MHz am Zähler.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J3 · Anstecken nach dem Boot (gerade und vertauscht)

`Geräte` VIDEO · `Board` beide Varianten · `Läufe` R0 R1 · `Bezug` A-41 · `Dauer` 15 min

1. Booten ohne Modul; danach gerade anstecken; Zeit bis zur Verbindung messen (3×).
2. Neu booten ohne Modul; vertauscht anstecken; Zeit messen (3×).

**Soll:** Dialog schließt sich selbst; Ausgänge einschaltbar ≤ 2,5 s nach dem Anstecken.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Zeiten):

#### J4 · Im Betrieb abziehen und vertauscht wieder anstecken

`Geräte` OSZI · `Aufbau` 3 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-35, A-41 · `Dauer` 15 min

1. CH1 an, CH3 an (IQ 270°); Phase messen.
2. SDA/SCL abziehen, vertauscht wieder anstecken.
3. Dialoge zählen; Phase erneut messen.

**Soll:** ein Dialog „Connection lost“; danach beide Ausgänge laufen, Phase 270° ± 1°.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J5 · Wackelkontakt

`Geräte` – · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-34, E-17 · `Dauer` 10 min

1. CH1 an; 30 s lang das SDA-Kabel wackeln lassen (Kontakt auf/zu).
2. Log lesen, Frequenz am Zähler prüfen.

**Soll:** keine Fehlprogrammierung (Frequenz nach Beruhigung richtig), eine Logzeile je Zustandswechsel.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J6 · Leitung dauerhaft auf GND

`Geräte` LOGIK · `Aufbau` 5 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-43, E-17 · `Dauer` 20 min

1. Debug-Build. SDA nach GND legen (Widerstand ≤ 100 Ω), dann SCL nach GND.
2. Rückgabecode im Debug-Log lesen; GUI-Bedienung prüfen (Tipps, Frame-Zeit).
3. Fehler entfernen.

**Soll:** Code 5; Dialog „I2C line stays low …“; die GUI stockt < 100 ms; nach Entfernen verbunden. Liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen (E-17).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J7 · Registerdump und Klonwarnung

`Geräte` LOGIK, PC · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-42, E-17 · `Dauer` 20 min

1. Debug-Build; Register 0, 3, 183 nach dem Einschalten aus dem Log lesen (eigenes Modul; wenn vorhanden auch MS5351M).

**Soll:** Werte notieren; das Modul wird angenommen; Log ohne „Unexpected reg183“ (sonst Klonwarnung und Info-Karte `(clone?)`).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J8 · Modul ohne Pull-ups

`Geräte` LOGIK · `Board` Messboard · `Läufe` R0 R1 · `Bezug` E-17 · `Dauer` 20 min

1. Modul ohne (bzw. mit abgetrennten) Pull-ups anschließen; booten.
2. Log lesen; CH1 an, 10 min Betrieb.

**Soll:** Erkennung bei 100 kHz; Betrieb mit 400 kHz stabil **oder** Log „I2C fails at 400 kHz; staying at 100 kHz“ und Betrieb mit 100 kHz stabil.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J9 · Dauer ohne Modul

`Geräte` – · `Board` Board ohne Modul · `Läufe` R1 · `Bezug` A-36, E-17, V-8 · `Dauer` 15 min

1. Debug-Build mit Heap-Ausgabe; 10 min ohne Modul laufen lassen; freien Heap alle 30 s notieren.

**Soll:** freier Heap konstant (ein `Wire.end()`/`begin()` pro 2 s), keine wiederholten Logzeilen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Heap min/max):

#### J10 · CN1-Belegung auf dem CYD2USB

`Geräte` DMM · `Board` CYD2USB · `Läufe` R0 R1 · `Bezug` Spezifikation 2.2 · `Dauer` 15 min

1. Mit dem DMM (Durchgang) CN1-Pins gegen GPIO22 und GPIO27 am ESP32 prüfen.
2. J1 und J2 auf dem CYD2USB wiederholen.

**Soll:** CN1 führt GPIO22/27; J1/J2 wie auf Board A.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J11 · Vertauschte Adern: Aktivität am Chip

`Geräte` LOGIK · `Aufbau` 5 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-42, E-17 · `Dauer` 15 min

1. LOGIK direkt am Modul (SDA/SCL); Kabel vertauscht; booten und Suche beobachten.
2. Dekodierte Transfers prüfen.

**Soll:** am Chip nur START/STOP und Fragmente, **keine Registeränderung** (keine Schreibzugriffe auf Register 16 bis 183).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J12 · 3,3 V offen

`Geräte` LOGIK, DMM · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-42, E-17 · `Dauer` 10 min

1. Modul nur mit GND und SDA/SCL, 3,3 V offen; booten, Log lesen.
2. 3,3 V anschließen, ohne abzuziehen.

**Soll:** „not found“, „line stays low“ oder „not an SI5351“, **kein Teilschreiben**; nach Anschluss von 3,3 V verbunden (ohne weiteres Abziehen).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J13 · GND offen

`Geräte` LOGIK, DMM · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-42 · `Dauer` 10 min

1. Wie J12, aber GND offen, 3,3 V und SDA/SCL angeschlossen.

**Soll:** wie J12.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J14 · Reset während I2C-Zugriff

`Geräte` LOGIK · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-43 · `Dauer` 20 min

1. Debug-Build mit Dauerlesen; während der Lesezugriffe 20× EN drücken.
2. Nach jedem Start prüfen: verbunden?

**Soll:** nach jedem Boot verbunden (Leitungsfreigabe beim Start greift, `[I2C] Line held low; releasing bus` ist erlaubt).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehlstarts von 20):

#### J15 · Fremdgerät auf Adresse 0x60

`Geräte` LOGIK · `Board` Board ohne Modul · `Läufe` R0 R1 · `Bezug` A-42 · `Dauer` 15 min

1. Fremdes I2C-Modul mit Adresse 0x60 an CN1 anstecken; booten; Dialoge und Bus beobachten.

**Soll:** Dialog „The device at I2C address 0x60 is not an SI5351“; höchstens ein Schreibzugriff (Register 3), keine weiteren Dialoge.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.K CONFIG-Optionen (1.3.0)

#### K1 · Pegel im Zustand OFF je `Off state`

`Geräte` OSZI, DMM · `Aufbau` 6 (**nie SPEKTRUM**) · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-8, E-21 · `Dauer` 20 min

1. CH1 ein- und wieder ausschalten, `Off state` = `Low`, `High`, `High-Z` nacheinander.
2. Je Zustand DC-Pegel an CLK0 messen: OSZI 1 MΩ, OSZI 50 Ω, DMM; Hochohmigkeit bei `High-Z` zusätzlich mit einem 10-kΩ-Widerstand nach 3,3 V bzw. GND prüfen.

| Off state | 1 MΩ (V) | 50 Ω (V) | DMM (V) | 10 kΩ gegen 3,3 V / GND (V) |
|---|---|---|---|---|
| Low | | | | |
| High | | | | |
| High-Z | | | | |

**Soll:** `Low` ≈ 0 V; `High` ≈ 3,3 V an 1 MΩ, an 50 Ω durch den Drive-Strom begrenzt (Messwert notieren); `High-Z` nimmt den Pegel der Last an.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### K2 · Pegel nach `CLKx_PDN`

`Geräte` OSZI, DMM · `Aufbau` 6 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` E-21, todo 2.1 · `Dauer` 20 min

1. `Off state` = `Low`, CH3 aus (CLK2 mit `CLK2_PDN`): Pegel an CLK2 mit OSZI 1 MΩ, 50 Ω und DMM.
2. Wie K1 mit der 10-kΩ-Last (gegen 3,3 V und gegen GND): Zieht der Pin auf Low oder ist er hochohmig?

**Soll:** Pegel notieren; entscheidet, ob `High-Z` alle abgeschalteten Ausgänge mit `CLKx_PDN` abschalten darf (E-21, Abschnitt 1.4).
**Ergebnis:** i · Messwert:

#### K3 · Spread −0,5 % bei 10 MHz

`Geräte` SPEKTRUM, ZÄHLER · `Aufbau` 2 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-7, E-20 · `Dauer` 30 min

1. `Spread CH1` = `-0.5 %`; CH1 = 10 MHz an; Span 100 kHz um 9,975 MHz, RBW 1 kHz, Max-Hold.
2. Bandbreite ablesen; CH2 = 7,1 MHz an den Zähler: unverändert?
3. Modulationsrate: Zero-Span an der Bandkante (RBW 30 kHz) oder SDR-FM-Demodulation; Periodendauer aus dem Zeitverlauf (≈ 31,7 µs).
4. CH1-Seite: Zeile `Out` lesen.

**Soll:** Band 9,95 … 10,00 MHz; Modulation ≈ 31,5 kHz; CH2 unverändert (±0,5 Hz + u); `Out 9.975 000 MHz mean, SSC -0.5 %`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K4 · Spread mit IQ

`Geräte` OSZI, SPEKTRUM · `Aufbau` 3 und 2 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-7, E-20 · `Dauer` 30 min

1. `Spread CH1` = `+/-0.5 %`; CH1 = 10 MHz an, CH3 an (Master CH1), 90°.
2. Δt zwischen CLK0 und CLK2 mit Spread aus und ein messen; Spektren beider Ausgänge.

**Soll:** Phase bleibt (Differenz ≤ 1° + u_φ); beide Ausgänge gleich breit.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K5 · Tuning-Modi im Vergleich

`Geräte` SPEKTRUM · `Aufbau` 2 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` E-19, todo 9.2 · `Dauer` 45 min

1. CH1 = 7,1 MHz; Span 200 kHz, RBW 100 Hz, Mittelung 10 (wie F3); nacheinander `Fractional`, `Low noise`, `Integer` (bei `Integer` auf die Frequenz laut `Out` zentrieren, 7,102 273 MHz).
2. Größte Nebenlinien in dBc und Abstand; zusätzlich Phasenrauschen bei 1 kHz Ablage.

| Modus | Nebenlinien (dBc @ Abstand) | L(1 kHz) | Abweichung `Out` (Hz) |
|---|---|---|---|
| Fractional | | | |
| Low noise | | | |
| Integer | | | |

**Soll:** zu dokumentieren; Grundlage für den Standardmodus (Abschnitt 1.4).
**Ergebnis:** i

#### K6 · Abstimmen in `Low noise`

`Geräte` OSZI · `Läufe` R0 R1 · `Bezug` A-6, E-19 · `Dauer` 25 min

1. `Tuning` = `Low noise`; OSZI mit Timeout-Trigger auf CLK0.
2. CH1 von 9,990 bis 10,010 MHz in 1-kHz-Schritten (je `+` einmal); außerdem 7,0 bis 7,2 MHz.
3. Aussetzer zählen und Frequenz notieren.

**Soll:** kein Aussetzer, solange der Teiler bleibt (Teiler 90 deckt 6,67 … 10 MHz ab); ein Aussetzer beim Übergang über 10,000 MHz (Teilerwechsel 90 → 88); dokumentieren.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Aussetzer, Dauer):

#### K7 · `Integer`: tatsächliche Frequenz

`Geräte` ZÄHLER, REF · `Aufbau` 1 · `Board` Messboard · `Läufe` R0 R1 · `Bezug` A-6, A-14, E-19 · `Dauer` 20 min

1. `Tuning` = `Integer`; CH1 = 7,1 MHz an; Zähler mit Klammermessung (10 MHz im Raster ist exakt 25 MHz × 24 / 60).
2. `Out` auf der CH1-Seite ablesen.

**Soll:** Zähler = angezeigte tatsächliche Frequenz (`Out 7.102 273 MHz`, +2272,7 Hz) ± u; MAIN zeigt `~7.100 000 MHz`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K8 · CONFIG-Dropdowns und Info-Karte

`Geräte` VIDEO · `Board` beide · `Läufe` R0 R1 S · `Bezug` A-31, E-24 · `Dauer` 15 min

1. Alle Dropdowns öffnen, auch ganz unten gescrollt; Listen vollständig sichtbar?
2. Info-Karte: Firmware, Branch, Commit, Build-Zeit mit `git log -1 --format="%h %cd"` vergleichen.

**Soll:** Listen vollständig sichtbar (öffnen nach oben, wenn unten kein Platz ist); Angaben der Info-Karte stimmen mit `git` überein.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K9 · Optionen bleiben gespeichert

`Board` Messboard · `Läufe` R0 R1 S · `Bezug` A-20 bis A-22 · `Dauer` 10 min

1. `Tuning` = `Low noise`, `Spread CH1` = `-0.5 %`, `Off state` = `High-Z`, `Drive all` = 4 mA; 5 s warten (`Saved`); aus- und einschalten.

**Soll:** Optionen erhalten, alle Ausgänge aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K10 · CONFIG scrollen

`Geräte` VIDEO · `Board` beide · `Läufe` R0 R1 · `Bezug` E-22 · `Dauer` 20 min

1. Auf freier Fläche, auf Tasten (`Corr. +` mit Wiederholung) und auf Dropdowns wischen; danach einzelne Tipps.
2. Zu MAIN wechseln und zurück: Scrollposition?
3. Eine Person, die CONFIG nicht kennt, öffnet die Seite und soll die Einstellung `Off state` finden, ohne Hinweis (U-24).

**Soll:** rastet an Zeilenkanten ein; Wischen löst keine Aktion aus; Tipps treffen; Scrollposition nach MAIN → CONFIG erhalten; die Person erkennt ohne Hinweis, dass die Seite scrollt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K11 · `Integer` sperrt Spread

`Board` Messboard · `Läufe` R0 R1 · `Bezug` A-7, A-14 · `Dauer` 10 min

1. `Spread CH1` = `-0.5 %`, danach `Tuning` = `Integer`; `Spread CH1` lesen; zurück auf `Fractional`.
2. MAIN mit CH1 = 7,1 MHz: Anzeige der Frequenz.

**Soll:** `Spread CH1` steht auf `Off` und ist gesperrt (ausgegraut); nach dem Zurückwechseln muss es neu gewählt werden; MAIN zeigt `~` vor abweichenden Frequenzen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K12 · Backlight (ersetzt B1)

`Geräte` LUX optional · `Board` beide · `Läufe` R0 R1 · `Bezug` E-23 · `Dauer` 15 min

1. `−` bis 10 %, `+` bis 100 % (je Tipp eine Stufe); `−` gedrückt halten (keine Wiederholung).
2. Lesbarkeit bei 10 % (Raumlicht ≈ 500 lx); Wert nach Neustart.

**Soll:** Stufen 10 … 100 %; bei 10 % noch lesbar; Halten wiederholt nicht; Wert bleibt nach Neustart.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K13 · „~“ nach dem Neustart (neu, Audit C-2)

`Board` Messboard · `Läufe` R1 · `Bezug` A-14, C-2 · `Dauer` 5 min

1. `Tuning` = `Integer`, CH1 = 7,1 MHz; 5 s warten; Neustart.
2. MAIN direkt nach dem Start ansehen.

**Soll:** MAIN zeigt `~7.100 000 MHz` sofort nach dem Start (mit 1.3.0 fehlte das `~` bis zur nächsten Änderung).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K14 · Unnötige Flash-Schreibvorgänge (neu, Audit C-5)

`Geräte` PC (Log) · `Board` Messboard · `Läufe` R1 · `Bezug` W-E17, C-5 · `Dauer` 15 min

1. Serial Monitor; 10× CH1 ein- und ausschalten, 10 s warten; Zeilen `[SETTINGS] Saved to NVS` zählen.
2. Dasselbe mit einer Schrittweitenwahl auf demselben Wert.
3. Eine Frequenz ändern; 10 s warten.

**Soll:** nach nur ON/OFF und nach unveränderter Schrittweite **kein** Speichern; nach der Frequenzänderung genau ein Speichern nach 5 s (bzw. 30 s bei Fernbefehl, L11).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Anzahl Speicherungen):

---

### 6.L WEB-HMI (nur Release-Kandidat, Konzept Abschnitt 12)

Aufbau 7. Die Bezeichnungen der Bedienelemente (WiFi, Setup…, Regenerate, Identify …) gelten laut [KONZEPT_WEB_HMI.md](KONZEPT_WEB_HMI.md) Abschnitt 7 und 9; maßgeblich ist das Handbuch der geprüften Firmware. Test-WLAN: WPA2, 2,4 GHz, eigenes Segment. SCPI-Skripte siehe Anhang D. Gerätename und IP aus der Info-Karte.

#### L1 · WLAN-Modi und Rückfall-AP

`Geräte` Router, Smartphone · `Läufe` R1 · `Bezug` W-E, Konzept 7.2 · `Dauer` 45 min

1. CONFIG `WiFi` = `Off`: Smartphone sucht Netze; SCPI und Web nicht erreichbar.
2. `Client`: Test-WLAN auswählen, Passwort eingeben; Rückmeldungen (`Connecting…`, `Connected <IP>`) und Zeit bis zur Verbindung (3×).
3. Router abschalten: nach 60 s startet der Rückfall-AP (WPA2), Statusleiste und Rückmeldung `Fallback: AP active`; die Suche läuft weiter (Router wieder einschalten).
4. Router ein: Gerät verbindet sich, der AP schaltet sofort ab.
5. `Access point`: AP erscheint am Smartphone.
6. Falsches Passwort und „Network not found“ ausprobieren: Rückmeldungen `Wrong password`, `Network not found`.

**Soll:** wie beschrieben; Verbindung ≤ 15 s; Rückfall-AP nach 60 s (± 5 s).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Zeiten):

#### L2 · Zwei WLANs

`Geräte` zwei Router · `Läufe` R1 · `Dauer` 30 min

1. Zwei Netze speichern (Test-WLAN A, B); Gerät in Reichweite beider (A stärker).
2. Router A abschalten; danach A einschalten und B abschalten.
3. Signalstärke (RSSI auf der Info-Karte) und das gewählte Netz notieren.

**Soll:** das stärkere Netz gewinnt; bei Misserfolg Wechsel auf das andere.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L3 · AP mit Zufallspasswort und QR, offener AP

`Geräte` Smartphones Android und iOS · `Läufe` R1 · `Bezug` W-E3, W-S1, W-S2 · `Dauer` 45 min

1. `Access point`: SSID und Passwort auf dem Display; QR-Code „Join“ mit der Kamera beider Smartphones scannen und beitreten.
2. `Regenerate`: neues Passwort; alte Verbindung wird getrennt?
3. `Open (not recommended)`: Bestätigung am Touch erforderlich; Statusleiste zeigt `AP open` in Warnfarbe; maximal 2 Stationen (dritte abgelehnt); ohne Station nach 15 min aus.
4. Über den offenen AP ein WLAN-Passwort per REST/Web setzen versuchen (L-S1).

**Soll:** QR funktioniert auf beiden Systemen; offener AP nur nach Bestätigung, mit Warnung, Zeitlimit und Stationslimit.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L4 · Statische IP

`Läufe` R1 · `Bezug` W-S12 · `Dauer` 20 min

1. `Device` → statische IP: gültige Werte (im Subnetz, Gateway ≠ IP) eintragen; Verbindung prüfen.
2. Ungültige: IP = Gateway, Broadcast-Adresse, Netzadresse, außerhalb des Subnetzes.

**Soll:** gültige wird übernommen, ungültige abgelehnt (Klartextmeldung).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L5 · mDNS und Captive-DNS

`Geräte` Windows-PC, Android, iOS, optional Schulnetz · `Läufe` R1 · `Bezug` W-E6, W-S4 · `Dauer` 45 min

1. Client-Modus: `ping <name>.local`, `http://<name>.local` auf Windows, Android, iOS (und im Schulnetz, falls mDNS dort durchgeleitet wird).
2. AP-Modus: Smartphone verbindet; die Einrichtungsseite öffnet sich automatisch (Captive-Portal-Erkennung); jeder Hostname beantwortet mit der AP-Adresse.
3. Client-Modus: Captive-DNS darf nicht laufen (`nslookup example.com <Geräte-IP>` keine Antwort).

**Soll:** Name löst auf; im AP-Modus öffnet sich die Seite; im Client-Modus kein Captive-DNS.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L6 · SCPI über USB und TCP

`Geräte` PC (Skript Anhang D) · `Läufe` R1 · `Bezug` W-S22, C-16 · `Dauer` 45 min

1. USB: serielle Verbindung ohne DTR/RTS-Wechsel öffnen (Skript `scpi_usb.py`); das Board darf nicht neu starten. 1000× `*IDN?`; Antworten prüfen.
2. TCP: `TCPIP0::<IP>::5025::SOCKET`; 1000× `*IDN?`; Antwortzeiten (Median, 99-%-Perzentil).
3. Während der USB-Sitzung auf fremde Zeilen (Log) im Datenstrom prüfen; `SYSTem:LOG?` liefert die Logzeilen.

**Soll:** kein Neustart beim Öffnen; 1000/1000 gültige Antworten `CYD,HF-Generator,<MAC>,1.4.0`; keine Logzeilen im Datenstrom; Antwortzeit < 50 ms (Median).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L7 · Zweite SCPI-Verbindung und Leerlauf

`Läufe` R1 · `Bezug` W-S20 · `Dauer` 30 min

1. Eine SCPI-Verbindung offen; zweite Verbindung (`nc <IP> 5025`) öffnen: wird sie abgelehnt? Info-Karte: `connected <IP>` / `rejected <IP>`.
2. Erste Verbindung 10 min ohne Befehl; danach prüfen.
3. `Disconnect all` am Gerät drücken.

**Soll:** zweite Verbindung wird abgelehnt; Leerlauf-Trennung nach 10 min; `Disconnect all` trennt SCPI und Web.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L8 · Web auf mehreren Geräten

`Geräte` PC, 2 Smartphones, weiterer Browser · `Läufe` R1 · `Dauer` 30 min

1. Drei Clients öffnen die Seite; eine Änderung (Frequenz, ON/OFF, Phase) in jedem Client und am Gerät.
2. Ein Feld mit Eingabefokus: wird es überschrieben?
3. Vierter Client: Seite „Too many clients (max. 3)“.
4. Netz trennen: Seite wird grau.

**Soll:** Änderungen live überall; Feld mit Fokus wird nicht überschrieben; vierter Client abgewiesen; Verbindungsabbruch sichtbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L9 · Tastaturbedienung im Browser

`Läufe` R1 · `Bezug` Konzept 9.1 · `Dauer` 20 min

1. Alle Tasten aus der Tabelle prüfen: `↑`/`↓`, `←`/`→`, `Bild↑`/`Bild↓`, Mausrad, `1`/`2`/`3`, `F`/`Enter`/`Esc`, `O`, `?`.
2. `Space` drücken: darf nichts schalten.

**Soll:** Belegung wie im Konzept; `Space` schaltet nichts.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L10 · Zweistufige Netzänderung

`Läufe` R1 · `Bezug` W-E9, W-S12 · `Dauer` 20 min

1. Über die Weboberfläche eine Netzänderung (z. B. anderes WLAN) auslösen, **nicht** bestätigen: nach 120 s Rückfall auf die alten Einstellungen (Countdown beobachten).
2. Erneut mit Bestätigung innerhalb 120 s.
3. Ohne Web-Passwort: zusätzlich Bestätigung am Touch gefordert?

**Soll:** Rückfall nach 120 s (± 5 s); mit Bestätigung wird übernommen; ohne Web-Passwort Touch-Bestätigung.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L11 · Speicherfristen

`Geräte` PC (Log) · `Läufe` R1 · `Bezug` W-E17, C-5 · `Dauer` 20 min

1. Eine Änderung am Touch: Zeit bis `[SETTINGS] Saved to NVS`.
2. Dieselbe über Web (REST) und SCPI.
3. Nur ON/OFF per Touch, Web, SCPI: wird gespeichert?

**Soll:** Touch und Web nach 5 s, SCPI nach 30 s (bzw. sofort mit `SYSTem:SAVE`); ON/OFF schreibt nichts.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L12 · Ohne SI5351

`Board` Board ohne Modul · `Läufe` R1 · `Dauer` 15 min

1. Booten ohne Modul; Web öffnen, SCPI verbinden.
2. `OUTPut1:STATus?`, `SYSTem:SI5351?`, Einstellungen ändern; Web-Zustand ansehen.

**Soll:** Web und SCPI funktionieren; Kanäle zeigen `NO HW` bzw. `NOHW`; Einstellungen werden gespeichert; keine wiederholten Fehler.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L13 · WLAN-Störungen im Spektrum

`Geräte` SPEKTRUM · `Aufbau` 2 · `Läufe` R1 · `Bezug` Konzept Schritt 0, F3 · `Dauer` 45 min

1. CH1 = 10 MHz an; Spektrum Span 200 kHz, RBW 100 Hz, zusätzlich Span 2 kHz, RBW 10 Hz (WLAN-Aktivität wie Beacons im Abstand ≈ 102,4 ms kann Seitenlinien im Abstand von ≈ 10 Hz erzeugen).
2. WLAN `Off`; `Client` im Leerlauf; `Client` mit Datenverkehr (Ping-Flut, drei Browser mit Live-Updates, SCPI-Skript 10 Befehle/s); `Access point` mit einer Station.
3. Neue Nebenlinien (Abstand, dBc) gegenüber `Off` notieren.

**Soll:** zu dokumentieren; bei Linien > −60 dBc im nutzbaren Bereich Hinweis im Handbuch (WLAN für Messungen abschalten).
**Ergebnis:** i · Messwert:

#### L14 · 24-h-Dauertest

`Geräte` PC (Skript) · `Läufe` R1 · `Bezug` W-S23, Konzept 7.3 · `Dauer` 24 h

1. Drei Browser (Live-Updates) und ein SCPI-Skript mit 10 Befehlen/s (gemischt: Frequenz ändern, Abfragen) laufen 24 h; die Frequenz bleibt innerhalb 8–12 MHz.
2. `SYSTem:HEAP?` jede Minute protokollieren (freier und minimaler Heap, größter Block, LVGL-Heap, Stack-Reserve).
3. Nach 24 h: Neustart-Zähler (Log), Heap-Verlauf, Fehlerzähler.

**Soll:** kein Neustart; minimaler Heap sinkt nicht stetig; Stack-Reserve ≥ 1 KB; keine Warnung „Heap < 40 KB“ in Dauerlast.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Heap min Start / nach 24 h):

#### L15 · WLAN aus entspricht 1.3.0

`Geräte` SPEKTRUM · `Läufe` R1 · `Dauer` 30 min

1. WLAN `Off`; F3 und F6 wiederholen; mit den R0-Messungen vergleichen.

**Soll:** Spektrum wie 1.3.0 (Linienpegel ± 3 dB, keine neuen Linien).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L16 · Einrichtung durch Novizen (Audit U)

`Geräte` Smartphone, VIDEO · `Läufe` R1 · `Dauer` 5 × 15 min

1. Aufgaben W1 bis W3 (Anhang C): Gerät ins Test-WLAN einbuchen; per QR am AP beitreten und Webseite öffnen; Frequenz im Browser ändern.
2. Zweimal: mit und ohne QR-Hilfe. Zeit, Fehler, Abbrüche; wurde „mobile Daten ausschalten“ nötig?

**Soll:** ≥ 80 % Erfolg ohne Hilfe, Zeit ≤ 5 min je Aufgabe.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L17 · 16-stelliges Passwort mit der Bildschirmtastatur

`Geräte` VIDEO · `Läufe` R1 · `Dauer` 5 × 10 min

1. 5 Personen geben ein 16-stelliges Passwort (Groß-/Kleinbuchstaben, Ziffern, 2 Sonderzeichen) mit Finger und mit Stift ein.
2. Zeit, Tippfehler, Abbrüche.

**Soll:** zu dokumentieren; Abbruchquote < 10 %, mittlere Zeit < 3 min (Vorschlag).
**Ergebnis:** i · Messwert:

#### L18 · Fremde Änderungen bemerken

`Läufe` R1 · `Dauer` 20 min

1. Drei Browser und das Gerät ändern abwechselnd Einstellungen; eine Testperson soll angeben, wann eine Änderung von außen kam (Hervorhebung, `REMOTE <Quelle>`).

**Soll:** Änderungen werden bemerkt (≥ 90 %).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L19 · Tastaturbedienung ohne Maus

`Läufe` R1 · `Dauer` 30 min

1. Aufgaben T1 bis T3 (Anhang C) im Browser nur mit der Tastatur; Fehlbedienungen zählen.

**Soll:** zu dokumentieren; ≤ 1 Fehlbedienung je Aufgabe.
**Ergebnis:** i · Messwert:

#### L20 · Identify mit mehreren Geräten

`Läufe` R1 · `Bezug` W-E28 · `Dauer` 20 min

1. Mit mindestens 3 Geräten im Raum die Web-Taste `Identify` (bzw. `SYSTem:IDENtify`) für ein Gerät auslösen; die Testperson nennt das Gerät.

**Soll:** richtiges Gerät in ≥ 95 % der Versuche.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L21 · Großansicht am Beamer

`Läufe` R1 · `Dauer` 15 min

1. Weboberfläche (Großansicht) am Beamer, Betrachter in 5 m Entfernung: Frequenzen, Status ON/OFF und Phasen lesen.

**Soll:** Frequenzen und Zustände aus 5 m lesbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L22 · SCPI-Befehlsbaum vollständig (neu)

`Geräte` PC (Skript), ZÄHLER · `Läufe` R1 · `Bezug` Konzept 8, W-S17, W-S19 · `Dauer` 60 min

1. Skript `scpi_selftest.py` führt alle Befehle der Tabelle in Konzept 8 mit gültigen und ungültigen Parametern aus (Kurz-/Langform, Einheiten HZ/KHZ/MHZ, MIN/MAX/DEF, Grenzwerte, `NAN`, `INF`, Hex, Exponenten, Überlänge, 17 Befehle in einer Zeile).
2. `SYSTem:ERRor?` nach jedem Befehl; Fehlercodes mit der Erwartung vergleichen (−222, −224, −221, −363 …).
3. Wirkung am Ausgang mit ZÄHLER stichprobenartig prüfen (`FREQ`, `OUTP1 ON`, `SOUR3:PHAS`).

**Soll:** alle Erwartungen des Skripts erfüllt; keine Abstürze; Frequenzen nur als Ganzzahlen akzeptiert.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (bestanden / gesamt):

#### L23 · REST und Weboberfläche (neu)

`Geräte` PC (curl) · `Läufe` R1 · `Bezug` Konzept 9.2 · `Dauer` 30 min

1. `GET /api/state` gegen die Anzeige am Gerät prüfen; alle Schlüssel in `POST /api/config` und `POST /api/channel/<n>` ändern.
2. Unbekannte Schlüssel und falsche Typen (`{"drive":"4"}`, `{"drive":4.5}`) senden.

**Soll:** Zustand stimmt mit dem Gerät überein; gültige Änderungen wirken; unbekannte Schlüssel und falsche Typen ergeben 400 mit `{"error":"-2xx,…"}` (dieselben Codes wie SCPI).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L24 · `*RST` und Speichern (neu)

`Läufe` R1 · `Bezug` W-E19, C-17 · `Dauer` 15 min

1. Einstellungen ändern, speichern; per SCPI `*RST`: Zustand (Ausgänge aus, Standardwerte) prüfen.
2. Stromlos, wieder ein: sind die zuvor gespeicherten Werte wieder da (kein Speichern durch `*RST`)?
3. `*RST`, danach `SYSTem:SAVE`, Neustart.

**Soll:** `*RST` schaltet Ausgänge aus und setzt Standardwerte, **ohne** zu speichern; nach `SYSTem:SAVE` und Neustart gelten die Standardwerte.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.L-S WEB-HMI Sicherheit

Nur im eigenen Labornetz und mit eigenen Geräten. `<IP>`: Geräteadresse, `<TOKEN>`: Token von der Info-Karte. Alle Befehle mit `curl.exe` unter Windows. Vor jedem Test den Zustand notieren (Kanal aus).

#### L-S1 · curl-Matrix (W-S1, W-S4, W-S5, W-S10)

`Läufe` R1 · `Bezug` W-S1, W-S4, W-S5, W-S10 · `Dauer` 45 min

1. `curl -i -H "Host: evil.example" http://<IP>/api/state` → **421**.
2. `curl -i -X POST -H "Content-Type: text/plain" -H "X-CYD-Token: <TOKEN>" --data '{"enabled":true}' http://<IP>/api/channel/1` → **415**.
3. Ohne Header `X-CYD-Token` → **403**.
4. `-H "Origin: http://evil.example"` → **403**; `-H "Sec-Fetch-Site: cross-site"` → **403**.
5. Body 100 KB (`head -c 102400 /dev/zero | tr '\0' 'a' > big.json`, `--data-binary @big.json`) → **413**.
6. Über einen **offenen** AP `POST /api/wifi` mit Passwortfeld → **403**.
7. `curl -i -X OPTIONS http://<IP>/api/channel/1` → abgelehnt; keine `Access-Control-Allow-*`-Header.
8. Nach jedem Schritt: Zustand der Kanäle unverändert.

**Soll:** wie angegeben; Zustand bleibt unverändert.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S2 · Fremde Webseite (W-S5, W-S6)

`Geräte` PC, Browser · `Läufe` R1 · `Dauer` 45 min

1. Testseite mit `python -m http.server 8000` (andere Herkunft als das Gerät); darauf drei Skripte: `fetch('http://<IP>/api/channel/1', {method:'POST', mode:'no-cors', body:'{"enabled":true}'})`, ein `<form enctype="text/plain" method="POST">` mit JSON-Teil im Feldnamen und ein `new WebSocket('ws://<IP>/ws')`.
2. Seite im Browser öffnen; Zustand des Geräts prüfen.
3. **DNS-Rebinding:** in der `hosts`-Datei `evil.example` auf `<IP>` legen; `http://evil.example/` im Browser öffnen.

**Soll:** Zustand unverändert; WebSocket-Upgrade abgelehnt; `evil.example` → 421.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S3 · SCPI-Port aus dem Browser (W-S17, W-S18)

`Läufe` R1 · `Dauer` 20 min

1. `curl -X POST --data-binary $'OUTP1 ON\n' http://<IP>:5025/` → Verbindung wird geschlossen, CH1 bleibt aus.
2. `python -c "import sys; sys.stdout.write('A'*20000 + 'OUTP1 ON\n')" | ncat <IP> 5025` (eine Zeile, Überlänge, **kein** Zeilenumbruch vor `OUTP1 ON`) → CH1 bleibt aus; `SYSTem:ERRor?` danach: −363.
3. Zeile mit HTTP-Header-Muster (`Host: x`) senden → Verbindung wird geschlossen.

**Soll:** CH1 bleibt in allen Fällen aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S4 · Fremde SSIDs (W-S8, W-S9)

`Geräte` zweiter ESP32 oder Linux/hostapd · `Läufe` R1 · `Dauer` 45 min

1. Test-AP mit den SSIDs `<svg onload=alert(1)>`, `a"b\c`, `x\ny`, UTF-8-Fehler (`\xF0A`, `\xC3`), 32 Byte lang und 200 Beacons mit verschiedenen BSSIDs (nur im eigenen Labor, Sendeleistung klein).
2. Scan am Gerät (Seite WIFI) und im Web.

**Soll:** keine Skriptausführung; SSIDs bereinigt dargestellt; höchstens 15 Einträge; Gerät bleibt bedienbar, kein Neustart.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S5 · Lasten (W-S10)

`Geräte` PC · `Läufe` R1 · `Dauer` 45 min

1. Slowloris mit 30 Sockets (z. B. Skript `slowloris.py` oder `slowhttptest`), WebSocket-Flut, Multipart- und Chunked-Posts.
2. Während der Tests: Touch am Gerät bedienen; Heap (`SYSTem:HEAP?`) vor, während, nach 2 min.

**Soll:** Touch reagiert (< 200 ms); der Heap erholt sich auf ≥ 95 % des Ausgangswerts; kein Neustart.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S6 · Kein Routing zwischen AP und STA (W-S3)

`Geräte` Smartphone, Laptop, Wireshark optional · `Läufe` R1 · `Dauer` 30 min

1. AP-Modus: Smartphone verbinden, in den Netzwerkdetails Gateway/DNS ablesen (DHCP ohne Gateway-Option).
2. Falls AP und STA gleichzeitig laufen (Rückfall-AP, solange Router fehlt und kurz danach): Laptop am AP mit statischer Route `route add <Heimnetz> mask <Maske> 192.168.4.1` und Zugriff auf einen Host im Heimnetz versuchen.
3. Mit Verbindung zum Router: AP schaltet ab (L1).

**Soll:** kein Gateway per DHCP; kein Host im Heimnetz erreichbar; AP aus bei STA-IP.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S7 · Keine Klartext-Passwörter im Flash (W-S14)

`Geräte` PC (`esptool`) · `Läufe` R1 · `Dauer` 30 min

1. Eindeutige Test-Passwörter setzen (WLAN `Testpass-7f3a91`, Web-Passwort `Webpass-2c8d55`).
2. NVS-Partition lesen (Offset und Größe aus der Partitionstabelle): `esptool --port COMx read_flash <offset> <size> nvs.bin`; zusätzlich ganzer Flash `read_flash 0 0x400000 full.bin`.
3. `strings nvs.bin full.bin | findstr /i "Testpass Webpass"`.
4. Werksreset auslösen; Dump wiederholen.

**Soll:** keine Passphrase im Klartext (WLAN als PMK gespeichert, Web-Passwort als Hash); nach Werksreset weder `hfnet` noch `nvs.net80211` noch Coredump mit Inhalt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S8 · `Remote outputs: local only` (W-S21)

`Läufe` R1 · `Bezug` W-E25, W-S21 · `Dauer` 20 min

1. CONFIG `Remote outputs` = `local only`.
2. Per SCPI `OUTP1 ON`, per REST `{"enabled":true}`, über die Weboberfläche: Einschalten.
3. `OUTP1 OFF` und Touch-Einschalten.
4. Statusleiste `REMOTE <Quelle>`; Option per SCPI/Web nicht änderbar.

**Soll:** Einschalten aus der Ferne abgelehnt (−221 bzw. 403); Ausschalten erlaubt; Touch schaltet weiter; Option nur am Touch änderbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S9 · Antwort-Header (W-S7)

`Läufe` R1 · `Dauer` 15 min

1. `curl -i http://<IP>/`, `curl -i http://<IP>/api/state`, `curl -i http://<IP>/` mit zugehöriger JS/CSS-Datei.
2. Header lesen: CSP, `X-Content-Type-Options`, `Referrer-Policy`, `Cache-Control`, `Access-Control-*`.

**Soll:** CSP genau `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`; `nosniff`; `no-referrer`; `/api/*` mit `Cache-Control: no-store`; keine `Access-Control-*`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S10 · Grenzen und Rate-Limits (W-S10, W-S11)

`Läufe` R1 · `Dauer` 45 min

1. URL mit 300 B; Header mit 3 KB; Body 1 KB + 1 B → 413; Multipart und Chunked → 415; Anfrage mit 12 s Pause → Timeout.
2. Fünfte HTTP-Verbindung und vierter WebSocket gleichzeitig.
3. 50 Schreibbefehle in 1 s → 429 ab dem 21.; Scan `GET /api/wifi/scan` zweimal in 10 s (zweite Antwort aus dem Zwischenspeicher).
4. 5 falsche Logins → Sperre 30 s, dann verdoppelt.

**Soll:** wie in W-S10 und W-S11.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S11 · Web-Passwort und Sitzung (W-S13)

`Läufe` R1 · `Dauer` 20 min

1. Passwort mit 7 Zeichen setzen → abgelehnt; mit 8 Zeichen → angenommen.
2. Login: `Set-Cookie` lesen (HttpOnly, SameSite=Strict, Ablauf 12 h); kaputte `Authorization`- und `Cookie`-Header senden.
3. Antworten und Log auf das Passwort prüfen.

**Soll:** wie W-S13; Passwort erscheint nie in einer Ausgabe.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S12 · Gerätename, mDNS, IPv6 (W-S4, W-S16)

`Geräte` PC (`dns-sd` bzw. `avahi-browse`) · `Läufe` R1 · `Dauer` 20 min

1. Ungültige Gerätenamen (`ab_c`, 64 Zeichen, Leerzeichen, Großbuchstaben falls nicht erlaubt) → abgelehnt; AP-SSID mit nicht druckbaren Zeichen → abgelehnt.
2. mDNS-Einträge (`dns-sd -B _http._tcp`, `-L`): TXT ohne Version und Commit.
3. `Host: <name>.local`, `Host: <IP>` → akzeptiert; anderer Host → 421; `curl -6` → keine IPv6-Verbindung.

**Soll:** wie W-S16 und W-S4.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S13 · WPA2 und PMF (W-S15)

`Geräte` Router-Log, Wireshark/Monitor-Adapter optional · `Läufe` R1 · `Dauer` 30 min

1. Client-Modus: Router-Log bzw. Wireshark: Gerät verbindet mit WPA2-PSK; PMF „capable“.
2. AP-Modus: RSN-Informationselement der Beacons (WPA2 oder WPA2/WPA3-Übergang).
3. Verbindung mit einem Router, der nur WPA/TKIP anbietet: wird sie abgelehnt?

**Soll:** Mindestsicherheit WPA2; PMF capable; WPA/TKIP wird nicht akzeptiert.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

### 6.Q Qualität und Dauerbetrieb

#### Q1 · Flash, RAM und Heap gegen die Basis

`Geräte` PC · `Läufe` R0 R1 · `Bezug` Audit 5.8 (Build-Gate) · `Dauer` 30 min

1. `pio run`: Flash- und RAM-Angaben (Bytes, %) aus der Build-Ausgabe notieren.
2. Debug-Build: freier Heap, minimaler Heap, größter freier Block, LVGL-Heap (V-8) nach dem Boot und nach 5 min Bedienung (Seitenwechsel, NUMPAD, CONFIG).

| Größe | R0 | R1 | Abweichung |
|---|---|---|---|
| Flash (B / %) | | | |
| RAM statisch (B / %) | | | |
| Heap frei nach Boot | – | | |
| Heap minimal nach 5 min | – | | |
| größter Block | – | | |
| LVGL-Heap frei/minimal | – | | |

**Soll:** Abweichung > 10 % begründen; minimaler Heap ≥ 40 KB, größter Block ≥ 16 KB (Konzept 7.3).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### Q2 · Stromaufnahme

`Geräte` STROM · `Läufe` R0 R1 · `Dauer` 30 min

1. STROM zwischen USB-Quelle und Board; Mittelwert über 10 s je Zustand: (a) Boot, Ausgänge aus, Backlight 100 %; (b) Backlight 10 %; (c) CH1, CH2, CH3 an (10 MHz, 8 mA, 50 Ω); (d) WLAN `Client` im Leerlauf; (e) `Access point` ohne Station; (f) 3 Web-Clients mit Live-Updates; (g) Spitzenwerte (Maximum).

**Soll:** zu dokumentieren; Hinweis für Netzteil oder Powerbank (Dauerstrom, Spitzen bei WLAN-Sendung).
**Ergebnis:** i · Messwert:

#### Q3 · Bootzeit

`Geräte` VIDEO · `Läufe` R0 R1 · `Dauer` 15 min

1. 10× einschalten; Zeit bis zum ersten Bild, bis zur Hauptseite, bis zum Verbinden des Moduls.

**Soll:** zu dokumentieren; R1 höchstens 1 s länger als R0 (Vorschlag).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### Q4 · Assert-Handler (Audit C-3, S-23)

`Geräte` OSZI, PC (Log) · `Läufe` R1 · `Bezug` C-3, W-S9 · `Dauer` 20 min · Build: `TEST_LVGL_ASSERT`

1. CH1 = 10 MHz an; OSZI auf CLK0 (Timeout-Trigger) und auf EN.
2. Der Testbuild füllt 20 s nach dem Boot den LVGL-Speicher und löst damit einen Malloc-Assert aus.
3. Log und Zeit bis zum Neustart; Zustand der Ausgänge danach.

**Soll:** Logzeile mit Ursache und Neustart innerhalb 5 s (kein dauerhaftes Einfrieren); nach dem Neustart alle Ausgänge aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Zeit bis Neustart):

#### Q5 · Loop-Watchdog (Audit S-23, W-S23)

`Geräte` OSZI, PC (Log) · `Läufe` R1 · `Bezug` W-S23 · `Dauer` 20 min · Build: `TEST_LOOP_HANG`

1. CH1 = 10 MHz an (innerhalb von 20 s nach dem Boot); der Testbuild blockiert danach `loop()` (Endlosschleife).
2. Zeit bis zum Watchdog-Reset (Log: Reset-Grund), Ausgang an CLK0 währenddessen.

**Soll:** Watchdog-Reset 5 s nach dem Blockieren (Task-Watchdog des Cores); Ausgänge nach dem Neustart aus; Dauer des Weiterlaufens im Hängezustand dokumentieren.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

## 7. Auswertung und Entscheidungen

### 7.1 Zusammenfassung

| Gruppe | Tests | ✔ | ✘ | ○ | i | – | Befunde |
|---|---|---|---|---|---|---|---|
| A | A1 bis A11 | | | | | | |
| B | B2 bis B20 | | | | | | |
| C | C1 bis C8 | | | | | | |
| D | D1 bis D5 | | | | | | |
| E | E1 bis E10 | | | | | | |
| F | F1 bis F9 | | | | | | |
| G, H | G1, G2, H1 bis H4 | | | | | | |
| I | I0 bis I8 | | | | | | |
| J | J1 bis J15 | | | | | | |
| K | K1 bis K14 | | | | | | |
| L | L1 bis L24 | | | | | | |
| L-S | L-S1 bis L-S13 | | | | | | |
| Q | Q1 bis Q5 | | | | | | |

### 7.2 Vergleich R0 gegen R1

Gleiches Messboard, gleiche Geräte, am selben Tag oder mit Messung der Drift (C7). Abweichungen über den Grenzen sind Befunde (Abschnitt 3.6).

| Größe | Messung | Zulässige Abweichung R1 gegen R0 |
|---|---|---|
| Frequenzgenauigkeit | C1, C3 | ≤ 0,5 Hz (Klammermessung) |
| IQ-Phasen | E1 bis E3 | ≤ 1° (+ u_φ) |
| Nebenlinien | F3, F4, K5 | ± 3 dB |
| Phasenrauschen | F5 | ± 3 dB |
| Störungen Plattform | F6, F9 | keine neuen Linien über −70 dBc |
| Aussetzer beim Abstimmen | D1 bis D4, K6 | keine zusätzlichen |
| Erkennung und Verbindung | I3, J1 bis J4 | 100 % wie R0 |
| Boot und Heap | Q1, Q3 | ± 10 % (Q1), ≤ +1 s (Q3) |
| Bedienung | B6, B7 | unverändert oder besser |

### 7.3 Entscheidungsblatt

Die Tabelle aus Abschnitt 1.4 wird mit den Messwerten ausgefüllt; die Entscheidung kommt als E-n in die Spezifikation und als Aufgabe in todo.md:

| Entscheidung | Messwerte | Ergebnis | Aufgabe / E-n |
|---|---|---|---|
| Standard-Tuning | | | |
| IQ ohne Reset | | | |
| ungerade IQ-Teiler | | | |
| `High-Z` über PDN | | | |
| TCXO | | | |
| Tastverhältnis-Aussage | | | |
| Pegeltabelle / Mischer | | | |
| Störquellen-Abhilfen | | | |
| Stift / Mindestgrößen | | | |
| WLAN bei Messungen | | | |
| Heap-Grenzen | | | |
| Klon-Module | | | |
| Phasen-Feinabgleich | | | |

### 7.4 Freigabekriterien für 1.4.0

Die Version wird freigegeben, wenn

1. alle Tests der Gruppen **L-S** und **A** mit ✔ bestanden sind (Sicherheit und Robustheit; ein ✘ bei L-S stoppt die Freigabe),
2. I3, J1 bis J4, J6, K9, K10 mit ✔ bestanden sind,
3. C3 und E1 bis E3 mit ✔ oder begründetem ○ bestanden sind,
4. L14 ohne Neustart und mit stabilem Heap bestanden ist,
5. B16 SUS ≥ 70 und B7, B9, B10 ✔,
6. alle Befunde priorisiert in todo.md stehen, offene P0/P1-Befunde die Freigabe verhindern,
7. Handbuch DE und EN zu den Messwerten passen (Pegeltabelle, Tastverhältnis, Aufwärmzeit, WLAN-Hinweis).

### 7.5 Wiederholungen

Nach Korrekturen nur die betroffenen Gruppen wiederholen; zusätzlich der Smoke-Test (4.3). Befunde werden mit dem Datum des Wiederholungstests geschlossen.

---

## Anhang A · Protokollkopf (je Durchlauf ausfüllen)

| Feld | Wert |
|---|---|
| Datum, Prüfer | |
| Lauf (R0 / R1 / S) | |
| Firmware: Version, Branch, Commit, Build-Zeit (Info-Karte, Foto) | |
| SHA-256 der Binärdatei | |
| Board (A / B), Port, MAC, Controller | |
| SI5351-Modul (Typ, Bezugsquelle, Klon?), Quarz | |
| Raumtemperatur / Luftfeuchte (Anfang, Ende) | |
| Versorgung (PC-USB, Netzteil, Powerbank) | |
| Korrektur nach C2 (ppm) | |
| Abweichungen vom Plan | |

| Gerät | Modell | Seriennr. | Kalibrierdatum | Firmware/Einstellung |
|---|---|---|---|---|
| ZÄHLER | | | | |
| REF | | | | |
| OSZI | | | | |
| SPEKTRUM | | | | |
| LOGIK | | | | |
| DMM | | | | |
| weitere | | | | |

---

## Anhang B · Rückverfolgbarkeit

### B.1 Anforderungen (A-n) und Entscheidungen

| Anforderung | Messung |
|---|---|
| A-1 | C3, C6, E5, F1, H1 |
| A-2 | B3 |
| A-3 | A9, K1 |
| A-4 | A5, B20 |
| A-5 | F1, F2 |
| A-6 | D2, D5, K5, K6, K7 |
| A-7 | K3, K4, K11 |
| A-8, A-9 | A9, A11, K1, K2 |
| A-10 | D1, D2 |
| A-11 | C1 bis C5 |
| A-12, A-13 | C3, C4, C6, K7 |
| A-14 | K7, K11, K13 |
| A-15 bis A-17 | D3, D4, E1 bis E4, E7, E9 |
| A-18 | E6, H3 |
| A-19 | A8, A11 |
| A-20 bis A-22 | A5, K9, K14 |
| A-23 | Host-Test; am Gerät optional (NVS gelöscht) |
| A-24, A-25 | B6, K8, K10 |
| A-26 | B9, B14 |
| A-27 | B6 (RGB-LED je Seite) |
| A-28, A-29 | B3, B6, B10 |
| A-30 | B2 bis B5, B11, B13 |
| A-31 | K8, K9, K12 |
| A-32 | B7, B8, K10 |
| A-33 | A6 |
| A-34 bis A-38 | A1 bis A4, A8, B18 |
| A-39 | A5, K14 (Logzeilen) |
| A-40 | Host-Test, J-Gruppe |
| A-41 bis A-43 | J1 bis J15 |
| A-44 | I3 bis I8 |
| A-45 bis A-49 | A7, K8 (Host-Tests, Build) |
| E-18 | I0 bis I8 |
| E-19, E-20 | K3 bis K7, F3 bis F5 |
| E-21 | K1, K2, A9, A11 |
| E-22 | K10, B12 |
| E-23 | K12, F6 |
| E-24 | K8 |

### B.2 Sicherheitsanforderungen der WEB-HMI

| Anforderung | Messung |
|---|---|
| W-S1, W-S2 | L3, L-S1 |
| W-S3 | L-S6 |
| W-S4 | L-S1, L-S2, L-S12 |
| W-S5, W-S6 | L-S1, L-S2, L-S10 |
| W-S7 | L-S9 |
| W-S8, W-S9 | L-S4, Q4 |
| W-S10, W-S11 | L-S5, L-S10 |
| W-S12 | L4, L10 |
| W-S13 | L-S11 |
| W-S14 | L-S7 |
| W-S15 | L-S13 |
| W-S16 | L-S12 |
| W-S17, W-S18 | L-S3, L22 |
| W-S19 | L22, L23 |
| W-S20 | L7 |
| W-S21 | L-S8 |
| W-S22 | L6 |
| W-S23 | L14, Q5 |
| W-S24, W-S25 | Eintrittskriterien EK-4, EK-5 |

---

## Anhang C · Aufgaben und SUS-Fragebogen

### C.1 Aufgaben für B16 (Reihenfolge fest, erst ohne, dann mit Handbuch)

| Nr | Aufgabe | Erfolg, wenn … |
|---|---|---|
| T1 | „Stelle CH1 auf 7,1 MHz ein und schalte den Ausgang ein.“ | MAIN zeigt CH1 `ON`, 7.100 000 MHz |
| T2 | „Stelle CH2 auf 14,2 MHz ein; erhöhe sie danach um 3 kHz.“ | 14.203 000 MHz |
| T3 | „CH3 soll CH1 mit 90° folgen; schalte CH3 ein.“ | CH3 `ON`, Master CH1, 90° |
| T4 | „Ein Frequenzzähler zeigt bei 10 MHz 10 000 097 Hz. Stelle die Frequenzkorrektur passend ein.“ | `Corr.` ≈ +9,70 ppm |
| T5 | „Stelle das Teilerverfahren auf `Low noise`.“ | `Tuning` = `Low noise` |
| T6 | „Ausgeschaltete Ausgänge sollen hochohmig sein.“ | `Off state` = `High-Z` |
| T7 | Mündlich: „Das Gerät wurde ausgesteckt und wieder angesteckt. Was ist mit den Ausgängen? Was musst du tun?“ | nennt: alle Ausgänge aus, Einstellungen erhalten, einschalten |

Verständnisfragen: „Was bedeutet `WAIT` bei CH3? `NO HW`? Die Zeile `Out`? Was macht `Corr.`? Was ist `Off state`? Was ändert `Tuning`?“

WEB-HMI (L16): **W1** „Buche das Gerät in das Test-WLAN ein.“ · **W2** „Verbinde dein Smartphone mit dem Gerät (Access point) und öffne die Weboberfläche.“ · **W3** „Ändere im Browser die Frequenz von CH1 auf 10 MHz und schalte ihn ein.“

Je Aufgabe protokollieren: Erfolg (ja / nein / mit Hilfe), Zeit (s), Tipps (Touch-Log), Fehlgriffe, Zitate.

### C.2 SUS-Fragebogen (System Usability Scale)

Antwort je Aussage: 1 (stimme gar nicht zu) bis 5 (stimme voll zu).

1. Ich denke, dass ich das System gerne häufig benutzen würde.
2. Ich fand das System unnötig komplex.
3. Ich fand das System einfach zu benutzen.
4. Ich glaube, ich würde die Hilfe einer technisch versierten Person benötigen, um das System benutzen zu können.
5. Ich fand, die verschiedenen Funktionen in diesem System waren gut integriert.
6. Ich denke, das System enthielt zu viele Inkonsistenzen.
7. Ich kann mir vorstellen, dass die meisten Leute den Umgang mit diesem System sehr schnell lernen.
8. Ich fand das System sehr umständlich zu nutzen.
9. Ich fühlte mich bei der Benutzung des Systems sehr sicher.
10. Ich musste eine Menge Dinge lernen, bevor ich mit dem System arbeiten konnte.

**Auswertung:** ungerade Aussagen: Antwort − 1; gerade Aussagen: 5 − Antwort; Summe × 2,5 = SUS-Wert (0 … 100); Mittelwert über die Personen.

---

## Anhang D · Skripte (Skizzen)

Die Skripte entstehen beim Aufbau; sie gehören zum Messprotokoll (Ordner `messungen/skripte/`).

```python
# scpi_tcp.py – 1000x *IDN? über TCP (L6)
import time, pyvisa
rm = pyvisa.ResourceManager("@py")
inst = rm.open_resource("TCPIP0::<IP>::5025::SOCKET")
inst.read_termination = inst.write_termination = "\n"
t = []
for _ in range(1000):
    t0 = time.perf_counter(); r = inst.query("*IDN?"); t.append(time.perf_counter() - t0)
    assert r.startswith("CYD,HF-Generator,"), r
t.sort(); print("median %.1f ms, p99 %.1f ms" % (t[500] * 1e3, t[990] * 1e3))
```

```python
# scpi_usb.py – serielle Verbindung ohne Reset-Impuls (L6); vor Gebrauch am Board prüfen
import serial
s = serial.Serial(); s.port = "COM7"; s.baudrate = 115200; s.timeout = 1
s.dtr = False; s.rts = False; s.open()
s.write(b"*IDN?\n"); print(s.readline())
```

- `scpi_selftest.py` (L22): Tabelle aus Konzept 8 mit gültigen und ungültigen Parametern, `SYST:ERR?` nach jedem Befehl, Soll-Fehlercode je Zeile.
- `counter_log.py` (C7, C8): liest den ZÄHLER per SCPI/USB im Takt der Torzeit und schreibt `Zeit;Frequenz;Temperatur` in eine CSV.
- `touchstat.py` (B8 bis B13): wertet das Touch-Log (x, y, Druck, Dauer, Ziel-ID) aus: Abweichung je Zielbereich, Fehltippquote, Aussetzer > 35 ms.
- `slowloris.py` (L-S5): 30 offene, nie beendete HTTP-Anfragen (nur im eigenen Labor).

---

## Anhang E · Befundvorlage

| Feld | Inhalt |
|---|---|
| Befund-Nr. | M-n |
| Test, Lauf, Board | z. B. E2, R1, Board B |
| Beobachtung / Messwert | |
| Soll | |
| Aufbau, Einstellungen, Foto/Screenshot | |
| Reproduzierbar? (n von m) | |
| Vermutete Ursache | |
| Priorität (P0 Sicherheit/Blocker, P1 Fehler, P2 Verbesserung, P3 Hinweis) | |
| Verweis (todo.md, Audit C/S/U, Spezifikation A/E) | |
| Status, Datum Wiederholungstest | |
