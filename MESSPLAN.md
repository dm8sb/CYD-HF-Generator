# Messplan – CYD HF Generator

Stand: 2026-11-04 · Plan-Version 3.0 (nach Priorität geordnet) · Prüflinge: Firmware 1.3.2 ff. und Release-Kandidat 1.4.0 (WEB-HMI)

Dieses Dokument beschreibt die Messungen und Bedientests am Gerät mit Einzelschritten, Messgeräten, Sollwerten und Bewertung. Es ist nach Priorität in Stufen geordnet: zuerst die notwendigen manuellen Messungen (Stufe 1), dann die Messungen für offene Entscheidungen (Stufe 2), zuletzt alles Optionale (Stufe 3). Die Nummern A1 … Q5 und L-S1 … L-S13 sind gegenüber Plan-Version 2.0 unverändert, damit Verweise gültig bleiben; „(alt n)“ nennt die Nummer im Plan vom 2026-10-26. Messungen, die nicht mehr nötig sind, stehen mit Grund in Abschnitt 8.

Quellen der Sollwerte: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md) (A-n Anforderungen, E-n Entscheidungen), [USER_MANUAL_DE.md](USER_MANUAL_DE.md), [ARBEITSPLAN.md](ARBEITSPLAN.md) (Befunde C, S, U, P, T aus dem Audit; WEB-HMI-Konzept mit W-E, W-S, V-n; Schritte S-n und Folgearbeiten N-n). Bei einem Widerspruch zwischen diesem Plan und dem Handbuch der geprüften Firmware gilt das Handbuch; der Plan wird dann nachgezogen.

## Inhalt

1. Ziel, Stufen, Aufwand
2. Messgeräte und Material
3. Gemeinsame Grundlagen: Sicherheit, Bedingungen, Methoden, Bewertung, Aufbauten
4. Vorbereitung, Eintrittskriterien, Smoke-Test
5. **Stufe 1 – Pflicht** (5.1 Ablauf · 5.2 bis 5.8 Blöcke · 5.9 bedingte Messungen · 5.10 Wiederholung je Release · 5.11 Stufe 1W ab WEB-HMI)
6. **Stufe 2 – Entscheidungsmessungen**
7. **Stufe 3 – Optional**
8. Entfallen und erledigt
9. Auswertung und Freigabe
- Anhang A Protokollkopf · B Rückverfolgbarkeit · C Aufgaben und SUS-Fragebogen · D Skripte · E Befundvorlage

---

## 1. Ziel, Stufen, Aufwand

### 1.1 Ziel

1. Am Gerät belegen, was nur die Hardware zeigen kann und was bei einem Fehler echten Schaden anrichtet: falsche Frequenz, falsche IQ-Phase, ein Ausgang treibt unerwartet, Verbindung oder Einstellungen gehen verloren, falsches Display, Touch trifft nicht (Stufe 1).
2. Messwerte für offene Entscheidungen liefern (Stufe 2, Tabelle 6.1).
3. Charakterisierung, Bedientests mit Personen und seltene Fehlerfälle nur bei Anlass und Zeit (Stufe 3).

**Was die Host-Tests prüfen, wird am Gerät nicht wiederholt:** Registermodell und Teilerplanung des SI5351 (Genauigkeit, Teilerwahl, Spread, Off state, kein PLL-Reset beim Abstimmen), `AppController` (Wiederverbinden, gerade und vertauschte Adern, Fremdgerät, Leitung auf Low, Speichern mit Wiederholung, IQ-Fehlerzustand), Display-Erkennung gegen ein Panel-Modell, Touch-Filter und Touch-Abgleich, Numpad- und Eingabelogik, Statustexte und Kontraste. Am Gerät wird nur geprüft, dass der echte Chip, das echte Display und der echte Touch sich wie die Modelle verhalten.

### 1.2 Stufen

| Stufe | Inhalt | Wann | Messungen | Aufwand |
|---|---|---|---|---|
| **1 Pflicht** | Verbindung, Frequenz und Kalibrierung, Abstimmen ohne Aussetzer, IQ-Phasen, Pegel im Zustand OFF, Speichern, Display-Erkennung, Touch | **sobald Zähler und Oszilloskop verfügbar sind** (auf 1.3.2 bzw. 1.3.3, vor der Migration; dieser Lauf ist die Referenz R0), danach vor jeder Weitergabe und jedem Release, dann nur die geänderten Blöcke (5.10) | 25 + 4 bedingte | ≈ 7 h (1 Arbeitstag) |
| **1W Pflicht ab WEB-HMI** | WLAN-Grundfunktion, SCPI, Web-Sicherheit, 24-h-Lauf | vor 1.4.0 und jedem Release mit WEB-HMI | 16 | ≈ 8 h + 24 h unbeaufsichtigt |
| **2 Entscheidung** | Messungen, deren Ergebnis eine offene Entscheidung trägt | vor dem Folgeschritt (N1 bis N8, S33, S39, S63); am besten im selben Zeitraum wie der erste Stufe-1-Lauf | 16 + 3 ab WEB-HMI | ≈ 6 h + 1,5 h ab WEB-HMI |
| **3 Optional** | Charakterisierung, Ausbildungsversuche, Bedientests mit Personen, seltene Fehlerfälle, WEB-HMI-Komfort | nach Bedarf, kein Freigabekriterium | 56 | ≈ 25,5 h + Personen |
| Entfallen | Host-Test, Doppelung, Funktion entfernt | – | 25 | – |
| Erledigt | I0 bis I2 (2026-10-28) | – | 3 | – |

Der **Smoke-Test** (4.3, ≈ 30 min) bleibt die kurze Prüfung nach jeder Entwicklungsphase.

### 1.3 Aufwand gegenüber Plan-Version 2.0

| | Plan 2.0 | Plan 3.0 |
|---|---|---|
| Pflicht für die Freigabe 1.4.0 | alle 148 Messungen in einer Kampagne nach der WEB-HMI: 9 Messtage, Referenz R0 mit 1.3.0 (1 d), Auswertung (1–2 d) ≈ 10–11 d | Stufe 1 + 1W: 45 Messungen ≈ 15 h (2 Tage) + 24-h-Lauf |
| Entscheidungsmessungen | Teil der Kampagne | Stufe 2: 19 Messungen ≈ 7,5 h (1 Tag) |
| Optional | – | Stufe 3 nach Bedarf |
| Referenz R0 | 1.3.0 nach der WEB-HMI nachflashen und nachmessen | entfällt als eigener Lauf: der erste Stufe-1/2-Lauf auf 1.3.x vor der Migration ist R0; die 1.3.0-Binärdatei bleibt gesichert (S03) und wird nur bei Verdacht gemessen |
| Modul umstecken | eigener Messtag | nicht nötig (1.4) |

### 1.4 Prüflinge und Boards

| Code | Prüfling | Zweck |
|---|---|---|
| R0 | erster Lauf von Stufe 1 (und möglichst Stufe 2) auf Firmware 1.3.2 oder 1.3.3, **vor** der Migration auf Core 3.x (S37 bis S41) | Referenz für den Vergleich in 9.2 |
| R1 | Release-Kandidat **1.4.0** (sauberer Commit, kein `+` hinter dem Commit auf der Info-Karte) | Stufe 1 und 1W, Vergleich mit R0 |
| S | Zwischenstand je Entwicklungsphase | nur Smoke-Test (4.3) |

**Boards** (nach Spezifikation 2.1; beide laufen mit derselben Firmware):

| Code | Board | Erkennung | Controller |
|---|---|---|---|
| Board A | Original-CYD | nur Micro-USB | ILI9341 |
| Board B | CYD2USB | zusätzlich USB-C | ST7789 |

Stand 2026-11-05 (Smoke-Tests 1.3.1 bis 1.3.3): **COM7** = Original-CYD (ILI9341) **ohne** SI5351, **COM13** = CYD2USB (ST7789) **mit** SI5351. V1 bestätigt die Zuordnung vor jedem Lauf. Folgen:

- Alle HF-Messungen laufen auf dem Board mit Modul („**Messboard**“, derzeit das CYD2USB). Das Modul bestimmt die HF-Werte, das Board nur Störungen (F6, F9).
- Display, Touch und Boot ohne Modul brauchen kein Modul: I3, B6, B7, B8 laufen auf beiden Boards ohne Umstecken, A1 auf dem Board ohne Modul. Umstecken ist nur noch für die optionale Wiederholung von F6 am zweiten Board nötig.

---

## 2. Messgeräte und Material

Spalte „Vorhanden“ vor Beginn ausfüllen (Modell, Seriennummer, Kalibrierdatum). **Stufe 1 braucht nur ZÄHLER, REF, OSZI, DMM und einen PC**; der SPEKTRUM kommt erst in Stufe 2.

### 2.1 Messgeräte

| Code | Gerät | Mindestanforderung | Wofür (Stufe) | Vorhanden | Ohne das Gerät |
|---|---|---|---|---|---|
| **ZÄHLER** | Frequenzzähler | Eingang ≥ 200 MHz, 50 Ω, Auflösung ≤ 0,1 Hz bei 100 MHz in 1 s Torzeit, Eingang für externe 10-MHz-Referenz | 1: C1 bis C3, K7, D2 · 2: C7, D5 · 3: C8, H | Ja | C nur mit OSZI-Frequenzmessung (≈ 1 ppm), C2 und C3 dann ○ |
| **REF** | 10-MHz-Referenz | GPS-gestützt oder Rubidium; für relative Prüfungen genügt ≤ 0,1 ppm (3.3); Verteiler für ZÄHLER und SPEKTRUM | 1: C · 2: C7, F | Ja, GPS-TCXO 10 MHz | absolute Genauigkeit unbekannt; C3 gilt nur relativ |
| **OSZI** | Oszilloskop | ≥ 2 Kanäle, ≥ 500 MHz, ≥ 2 GS/s, Persistenz, Single-Trigger, **Timeout-/Dropout-Trigger**, Δt-Messung mit Mittelung, 50-Ω-Eingang | 1: A4, A10, D1, D2, E1, E2, K1, K2 · 2: D3, D5, E3 bis E5, E9, K6 · 3: A11, E8, K4 | Ja, R&S RTO 2 GHz | Stufe 1 nicht möglich |
| **SPEKTRUM** | Spektrumanalysator | bis ≥ 1 GHz, RBW ≤ 10 Hz, Rauschmarker (dBc/Hz), 50 Ω, Dauerpegel ≥ +20 dBm, Referenzeingang | 2: F2, F3, F6, F7, K5, L13, L15 · 3: F1, F4, F5, F8, F9, G, H4, K3, K4 | Ja, Siglent 3,2 GHz | Stufe 2 nur teilweise (SDR für Nebenlinien, kein Phasenrauschen) |
| **LOGIK** | Logikanalysator | ≥ 8 Kanäle, ≥ 24 MS/s, I2C-Dekoder, 3,3 V | 2: J6 (optional) · 3: J11 bis J15 | Ja, Saleae | Debug-Log statt Busmitschnitt |
| **DMM** | Multimeter | DC-Spannung, Eingang 10 MΩ | 1: K1, K2 | Ja, Agilent 34401A | mit OSZI (1 MΩ) möglich |
| **STROM** | USB-Leistungsmesser | 5 V, Auflösung ≤ 1 mA, Mittelwert ≥ 10 s | 3: Q2 | nein | Q2 entfällt |
| **LUX** | Luxmeter | 10 … 1000 lx | 3: B14, K12 | nein | Beleuchtung nur beschreiben |
| **SONDE** | Nahfeldsonde (H-Feld, ≈ 1 cm) | 1 MHz … 1 GHz | 3: F9 | Ja | F9 entfällt |
| **THERMO** | Thermometer | Raumtemperatur, Auflösung 0,1 K | 2: C7 · 3: E8 | Ja | Temperatur nur schätzen |
| **VIDEO** | Smartphone mit Slow-Motion und Stoppuhr | ≥ 120 Bilder/s | 1: I3 (optional) · 3: B14, B16, Q3 | eventuell | Zeiten nur mit Stoppuhr |

### 2.2 Zubehör

| Material | Menge | Hinweis |
|---|---|---|
| 50-Ω-Durchgangsabschlüsse (BNC oder SMA) | 3 | Abschluss an jedem gemessenen Ausgang |
| Dämpfungsglieder 10 / 20 / 30 dB, 50 Ω | je 1–2 | 20 dB immer vor SPEKTRUM |
| DC-Sperre | 1 | vor SPEKTRUM und ZÄHLER, wenn der Eingang DC-gekoppelt ist |
| Leistungsteiler 2-fach 50 Ω oder BNC-T | 1 | Eigenversatz der Phasenmessung (V4) |
| Koaxkabel **gleicher Länge** (Unterschied < 1 mm ≙ < 5 ps) | 2 + 2 | zwei für die Phase, zwei Reserve |
| Adapter vom Modulausgang auf BNC | nach Modul | |
| Dupont-Kabel, Mini-Klemmen für CN1 (SDA, SCL, 3,3 V, GND) | je 4 | A2 bis A4, J |
| Widerstände 10 kΩ und ≤ 100 Ω | je 2 | K1, K2 (Last gegen 3,3 V / GND), J6 |
| USB-Kabel Micro-USB und USB-C, USB-Netzteil 5 V/1 A, Powerbank | je 1 | F6 d |
| Diodenringmischer (z. B. SBL-1, ADE-1), LO +7 dBm | 1 | G1, G2 (Stufe 3) |
| Soundkarte (Line-In) oder SDR mit FFT-Software | 1 | D5, G2 |
| QSD-Mischer (FST3253) oder zwei Mischer mit 90°-NF-Netzwerk | optional | E10 |
| I2C-Modul auf Adresse 0x60 (z. B. MCP4725 mit passender Adressbeschaltung) | optional | J15 |
| SI5351-Klon (MS5351M) | optional | J7 |
| Eingabestift (Kunststoff, Spitze ≈ 1 mm; der Touch ist resistiv) | 1 | B19 |
| Isolierunterlage, ESD-Armband | 1 | 3.1 |

### 2.3 Rechner und Software

| Posten | Hinweis |
|---|---|
| PC mit PlatformIO | `~/.platformio/penv/Scripts/pio.exe` (6.2.0); `pio` steht nicht mehr im PATH (S02) |
| Python 3 mit `pyserial`, `pyvisa`, `pyvisa-py` | Skripte in Anhang D |
| `curl` (unter Windows `curl.exe`), `ncat`/`nc`, `esptool` (aus der PlatformIO-Umgebung) | 1W, L, L-S |
| Serieller Monitor 115 200 Baud | `pio device monitor -b 115200 --port COMx` |
| Browser (Chrome, Firefox) auf PC, Smartphones Android **und** iOS | L1, L3, L5, L8, L16 bis L21 |
| WLAN-Router mit WPA2 (Test-WLAN, 2,4 GHz), zweiter Router oder AP | L1, L2 |
| Gerät für Test-SSIDs (zweiter ESP32 oder Linux mit `hostapd`) | L-S4, nur im eigenen Labor mit eigenen Geräten |
| Wireshark (optional) | L-S6, L-S13 |
| FFT-Software für die Soundkarte (Audacity, SpectrumLab) | G2 |
| Messprotokoll (Anhang A) und Kamera für Aufbau-Fotos | alle |

### 2.4 Personen für Bedientests (nur Stufe 3)

5 Lernende (ohne Vorkenntnis des Geräts) und 2–3 Lehrende; je 30 min für B16, kurze Einzelsitzungen (10 min) für B9, B14, B18, B19; Einverständnis für die Aufzeichnung der Touch-Daten und Notizen (keine Namen im Protokoll, nur Personen-Codes P1 … P8). Stufe 1 und 2 braucht nur einen Bediener.

---

## 3. Gemeinsame Grundlagen

### 3.1 Sicherheit und Geräteschutz

- **Nie ein ausgeschaltetes Gerät mit `Off state` = `High` an SPEKTRUM oder ZÄHLER mit DC-Kopplung**: Der Ausgang führt dann 3,3 V Gleichspannung (K1). Pegelmessungen im Zustand OFF nur mit OSZI (DC, 1 MΩ oder 50 Ω) oder DMM, nie mit SPEKTRUM.
- **Immer erst 20 dB Dämpfung und DC-Sperre vor SPEKTRUM.** Die Grundwelle an 50 Ω kann bei 8 mA grob 0 … +10 dBm erreichen (genau misst F2).
- Pegel und Frequenz am Gerät einstellen, **bevor** der Ausgang eingeschaltet wird; neue Aufbauten nur bei ausgeschalteten Ausgängen verbinden.
- SI5351-Modul, Kabel und Mischer nicht unter Spannung umstecken, außer in A2 bis A4, J5, J12, J13 (dort ist es der Test).
- ESD: Armband, Isolierunterlage; die Boards nicht auf Metall legen (Kurzschluss an der Rückseite).
- Masseschleifen: OSZI, ZÄHLER und SPEKTRUM sind über Netzleiter geerdet; das Board wird über USB vom PC gespeist. Bei Brummen oder Offsets ein USB-Netzteil statt PC-USB verwenden und im Protokoll vermerken.
- Netzwerktests (L-S) nur im eigenen, abgetrennten Laborsegment und nur mit eigenen Geräten; Störtests (Slowloris, Beacon-Fluten) nie in einem produktiven WLAN.

### 3.2 Standardbedingungen

Wenn bei einer Messung nichts anderes steht:

| Größe | Wert |
|---|---|
| Ausgangslast | 50 Ω abgeschlossen; am SPEKTRUM über DC-Sperre und 20 dB |
| Drive | 8 mA |
| Tuning, Spread, Off state | `Fractional`, `Off`, `Low` (Werkseinstellung) |
| Korrektur | nach C2 kalibriert und danach unverändert |
| WLAN | aus (außer bei 1W, L, L-S, L13, L15, Q2) |
| Aufwärmzeit | 15 min mit eingeschaltetem CH1 (10 MHz) vor C, E, F |
| Umgebung | 20 … 25 °C, keine Zugluft, Temperatur im Protokoll |
| Spannungsversorgung | PC-USB; abweichend nur nach Vermerk |
| Firmware-Build | Release (`LOG_DEBUG 0`); Testbuilds nur, wo gefordert (4.2) |
| Boot | nach jedem Flashen oder Reset die Info-Karte prüfen (Version, Commit ohne `+`) |

### 3.3 Frequenzen messen: Klammermessung

Der Quarz driftet innerhalb von Minuten um bis zu einige 0,1 ppm (≙ 10 Hz bei 100 MHz). Die Synthese kann deshalb nur **relativ** zu einer Messung bei 10 MHz bewertet werden; die absolute Referenzgenauigkeit ist dafür unkritisch, solange C2 und alle weiteren Messungen mit demselben ZÄHLER und derselben REF laufen.

1. 10 MHz an CH1 einstellen, 10 Werte mit 1 s Torzeit mitteln: **f10a**.
2. Messfrequenz **f_soll** einstellen, 10 Werte mitteln: **f_mess**.
3. 10 MHz erneut einstellen, 10 Werte mitteln: **f10b**.
4. Abweichung: **δ = f_mess − f_soll · (f10a + f10b) / (2 · 10 MHz)** in Hz.

Ausgang, Kabel und Zähler bleiben dabei unverändert. Die Messunsicherheit u ist die Auflösung des Zählers (V3) plus die Streuung der 10 Mittelwerte.

### 3.4 Phase am Oszilloskop messen

Beide Ausgänge über **gleich lange** Kabel an zwei OSZI-Kanäle mit 50 Ω (Aufbau 3). Trigger auf Kanal 1. Zeitversatz **Δt** zwischen den steigenden Flanken an 50 % der Amplitude, ≥ 256 Mittelungen.

- **Phase: φ = Δt · f · 360°**, bei negativem Δt eine Periode addieren.
- **Eigenversatz φ₀** aus V4 abziehen.
- Umrechnung: 1° entspricht bei 4,73 MHz ≈ 587 ps, 10 MHz ≈ 278 ps, 30 MHz ≈ 93 ps, 99 MHz ≈ 28 ps. Bei hohen Frequenzen setzt die Zeitauflösung des OSZI die Grenze (V4 bestimmt u).
- 1° Phasenfehler entspricht einer Seitenbandunterdrückung von ≈ 41 dB, 2° ≈ 35 dB (E10).

### 3.5 Spektren messen (Stufe 2 und 3)

- Aufbau 2 mit 20 dB und DC-Sperre; REF an den Referenzeingang des SPEKTRUM.
- Referenzpegel so, dass der Träger im oberen Drittel liegt; Detektor „Sample“ oder „RMS“, Mittelung ≥ 10; Einstellungen ins Protokoll (Span, RBW, VBW, Mittelung, Referenzpegel).
- **Nebenlinien** in dBc zum Träger; Abstand in Hz notieren.
- **Phasenrauschen** mit dem Rauschmarker, auf 1 Hz normiert. Eigenrauschen aus V4 daneben eintragen; liegt der Messwert < 6 dB darüber, gilt er als obere Schranke (○).
- Ein Screenshot je Messung, Dateiname `<Nr>_<Lauf>_<Board>.png`.

### 3.6 Bewertung

| Zeichen | Bedeutung |
|---|---|
| ✔ | bestanden: \|Abweichung\| + u ≤ Grenze |
| ✘ | nicht bestanden: \|Abweichung\| − u > Grenze |
| ○ | nicht entscheidbar (Unsicherheit zu groß); Grund und u eintragen |
| i | nur zu dokumentieren (Information für eine Entscheidung) |
| – | nicht anwendbar (Firmware-Stand, Gerät fehlt) |

Jedes ✘ und jedes unerwartete Verhalten wird mit der Vorlage aus Anhang E als **Befund M-n** erfasst (Test-Nr., Lauf, Board, Messwert, Foto, Vermutung) und mit Priorität in [ARBEITSPLAN.md](ARBEITSPLAN.md) eingetragen. Bei ○ wird, wenn möglich, mit besserem Aufbau wiederholt.

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

## 4. Vorbereitung, Eintrittskriterien, Smoke-Test

### 4.1 Vorbereitung

#### V1 · Boards, Ports und Modul zuordnen

`Geräte` PC · `Dauer` 10 min

1. Beide Boards anstecken, `pio device list` zeigt die Ports. Board **mit** zusätzlicher USB-C-Buchse = Board B (CYD2USB).
2. Je Board den Serial Monitor öffnen, EN drücken, `[BOOT]`, `[DISPLAY]` und `[APP]` ablesen: `[DISPLAY] ILI9341 …` ≙ Board A, `ST7789 …` ≙ Board B.
3. `[APP] SI5351 at 0x60, SDA=… SCL=…` zeigt das **Messboard**; beim anderen steht `[APP] SI5351 not found`.
4. Boards beschriften („A“, „B“, Messboard); MAC nur beim ersten Lauf notieren.

**Ergebnis:**

| Port | Board | Controller | SI5351 | MAC | Messboard |
|---|---|---|---|---|---|
| COM7 | | | | | |
| COM13 | | | | | |

#### V2 · Prüfling sichern

`Geräte` PC · `Dauer` 10 min

1. `git status` sauber, `git log -1` zeigt den Prüfling; Info-Karte ohne `+`.
2. Lokaler Tag des Prüflings (z. B. `v1.3.2` liegt schon, für R1 `v1.4.0-rc1`); `firmware.bin` und `firmware.elf` nach `messungen/firmware/<Version>/`, SHA-256 notieren. Die Referenz 1.3.0 ist seit S03 gesichert.
3. Benötigte Testbuilds (4.2) bauen und daneben ablegen.

**Ergebnis:** Version ______ · Commit ______ · SHA-256 ______

#### V3 · Messgeräte prüfen und aufwärmen

`Geräte` alle für die Stufe · `Dauer` 15 min aktiv (Aufwärmzeit läuft parallel)

1. Geräte einschalten; ZÄHLER (und SPEKTRUM) auf externe Referenz, „Lock“ abwarten (≥ 30 min Aufwärmzeit laut Gerätehandbuch). Gleichzeitig CH1 = 10 MHz einschalten (Aufwärmzeit des Prüflings).
2. ZÄHLER: REF an den Eingang, 10 Werte mit 1 s Torzeit; Streuung = Auflösung.
3. OSZI: Eigenabgleich, 50-Ω-Eingänge, gleiche Vertikalempfindlichkeit.
4. DMM: Nullpunkt, Bereich 20 V. SPEKTRUM (nur Stufe 2): Eigenkalibrierung.

**Ergebnis:** u_ZÄHLER = ______ Hz · Kalibrierdaten: ______

#### V4 · Eigenmessung des Aufbaus (Unsicherheiten)

`Geräte` OSZI (Teil a, Stufe 1), SPEKTRUM (Teil b, Stufe 2) · `Aufbau` 3 und 2 · `Dauer` a 20 min, b 30 min

**Teil a (vor E1, E2):**

1. CH1 auf 10 MHz über Teiler/T-Stück auf beide OSZI-Kanäle; Δt mit ≥ 256 Mittelungen messen, Kabel tauschen, erneut messen. Mittelwert der Beträge = Eigenversatz Δt₀ (ps), Differenz/2 = Unsicherheit der Kabel.
2. Dasselbe bei 99 MHz (Frequenzabhängigkeit der Kanäle).
3. Streuung von Δt über 10 Messungen = u_t; daraus u_φ = u_t · f · 360°.

**Teil b (vor Stufe 2):**

4. Eigenrauschen SPEKTRUM: Eingang mit 50 Ω abschließen, Span 200 kHz, RBW 100 Hz, Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz Ablage (Mitte 10 MHz).
5. Dämpfung der 20-dB-Glieder und der DC-Sperre bestimmen.
6. Eigenverzerrung bei −10 dBm am Eingang prüfen (sonst misst F1/F2 die Analysator-Harmonischen).

**Ergebnis:** Δt₀ = ___ ps (10 / 99 MHz: ___ / ___) · u_t = ___ ps · u_φ bei 10 / 99 MHz = ___° / ___° · Eigenrauschen (dBc/Hz bei 100 Hz / 1 k / 10 k / 100 k) = ___ / ___ / ___ / ___

#### V5 · Protokoll anlegen

1. Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` aus Anhang A anlegen; nur die Abschnitte der gemessenen Stufe übernehmen.
2. Ordner `messungen/<Datum>/` für Screenshots, Logs, Fotos; Dateinamen `<Nr>_<Lauf>_<Board>.<ext>`.
3. Info-Karte (Firmware, Branch, Commit, Build-Zeit) fotografieren.

### 4.2 Eintrittskriterien

| Nr | Kriterium | Prüfung | Gilt für | ☐ |
|---|---|---|---|---|
| EK-1 | Host-Tests grün in `native-win` und `native` | `pio test -e native-win`, CI | alle | ☐ |
| EK-2 | Build ohne Warnungen mit `-Wall -Wextra` in `src/` | `pio run` | alle | ☐ |
| EK-3 | Gates der Phase erfüllt (Review, Doku DE + EN, Spezifikation, README) | [ARBEITSPLAN.md](ARBEITSPLAN.md) | alle | ☐ |
| EK-4 | Fuzzing ≥ 10 Mio. Ausführungen ohne Fund (W-S25) | CI-Protokoll | 1W | ☐ |
| EK-5 | Abhängigkeiten exakt gepinnt, Advisory-Check je Release (W-S24) | `platformio.ini`, Notiz | 1W | ☐ |
| EK-6 | Prüfling committet, Info-Karte ohne `+` | CONFIG | alle | ☐ |
| EK-7 | Smoke-Test (4.3) auf beiden Boards bestanden | Protokoll | alle | ☐ |
| EK-8 | benötigte Testbuilds vorhanden (Tabelle unten) | `messungen/firmware/` | alle | ☐ |
| EK-9 | Handbuch DE und EN passen zur Firmware; Abweichungen zum Plan notiert | Durchsicht | alle | ☐ |

**Testbuilds** (ein Flag je Build; der Release-Build bleibt `LOG_DEBUG 0`):

| Build | Flag | Für | Stand |
|---|---|---|---|
| Debug | `-DLOG_DEBUG=1` | J6, J7 (Stufe 2) · J9, J14, Q1 | vorhanden |
| IQ-Fehler | `-DTEST_FAULT_IQ=1` | A8, A11 (Stufe 3) | vorhanden |
| Touch-Log | env `esp32-2432S028R-touchlog` (`-DTEST_TOUCH_LOG=1`): je Berührung `[TOUCH] x=… y=… to …,… z=… t=…ms gap=…ms drop=n/…ms target=…` | B12 (Stufe 2) · B9 | vorhanden (S28) |
| Trefferkarte | env `esp32-2432S028R-hitmap` (`-DTEST_HIT_MAP=1`, mit Touch-Log): 20 Zielkreuze, roter Punkt am Treffer, `[HITMAP] target n (x,y) hit (x,y) dx dy`; Verlassen durch Neustart | **B8 (Stufe 1)** | vorhanden (S28) |
| PLL ohne Reset | Testbuild „PLL-Änderung ohne Reset“ (N2) | D5 (Stufe 2) | zu erstellen |
| Assert/WDT | `-DTEST_LVGL_ASSERT=1`, `-DTEST_LOOP_HANG=1` | Q4, Q5 (bedingt) | vorhanden (S09) |

### 4.3 Smoke-Test (je Entwicklungsphase, beide Boards)

`Dauer` ≈ 30 min · nach jeder Phase des Arbeitsplans; das Ergebnis steht im Release-Schritt des [ARBEITSPLAN.md](ARBEITSPLAN.md) (zuletzt S17 für 1.3.1, S30 für 1.3.2, S36 für 1.3.3 und S41b für Core 3).

| Schritt | Prüfung | Erwartung |
|---|---|---|
| 1 | Beide Boards flashen, booten, Info-Karte | Version und Commit stimmen; Display `detected`; Touch kalibriert; Hauptseite ohne Rauschen |
| 2 | A1, A2 kurz: Board ohne Modul booten; am Messboard SDA ab- und wieder anstecken | je genau ein Dialog; Verbindung ≤ 2,5 s nach dem Anstecken |
| 3 | A5 kurz: eine Frequenz und eine CONFIG-Option ändern, `Saved` abwarten, Stecker ziehen | Werte erhalten, alle Ausgänge aus |
| 4 | B6 kurz: MAIN, CH1 bis CH3, CONFIG (gescrollt, Dropdowns geöffnet, K8), NUMPAD | nichts abgeschnitten oder überlappt; Dropdowns vollständig |
| 5 | CH1 = 10 MHz an ZÄHLER oder OSZI, ein und aus | 10 MHz ± 100 Hz (unkalibriert); aus = kein Signal |
| 6 | CH3 IQ 90° an OSZI | zwei Signale mit 25 ns Versatz |
| 7 (ab WEB-HMI) | L1 (Client), `*IDN?` über TCP 5025 und USB, Webseite öffnet | siehe 5.11 |

Bei 1.3.1 bis 1.3.3 und beim Core-3-Stand (S41b, 2026-11-06) blieben die Schritte 5 und 6 offen (kein Messgerät verfügbar); der erste Stufe-1-Lauf schließt sie (C3, E1).

---

## 5. Stufe 1 – Pflicht

Die notwendigen manuellen Messungen. Sie decken ab, was kein Host-Test zeigen kann und was bei einem Fehler Schaden anrichtet. Alles läuft an einem Arbeitstag mit ZÄHLER, REF, OSZI und DMM; der SPEKTRUM wird nicht gebraucht.

### 5.1 Ablauf

| Block | Messungen | Board | Geräte | Dauer |
|---|---|---|---|---|
| 0 Vorbereitung | V1, V2, V3, V4 a, V5; Aufwärmzeit 15 min (CH1 10 MHz) parallel | beide | alle | 45 min |
| V Verbindung | J1, J2, A1, A2 (mit J3), A3, A4 | Messboard; A1 am Board ohne Modul | ZÄHLER oder OSZI | 45 min |
| F Frequenz | C1, C2, C3 mit K7 (ab S35 zusätzlich H1, H2) | Messboard | ZÄHLER, REF | 55 min |
| T Abstimmen | D1, D2 (mit C5) | Messboard | OSZI, ZÄHLER | 30 min |
| IQ Phase | E1, E2 | Messboard | OSZI | 40 min |
| P Pegel | K1 (mit A9), K2, A10 | Messboard | OSZI, DMM | 45 min |
| S Speichern | A5 mit K9 | Messboard | – | 15 min |
| D Display und Touch | I3 (mit A6), B6 (mit B3, K8), B7, B8; I7 läuft über den ganzen Tag mit | beide | PC, VIDEO optional | 90 min |
| bedingt | H1, H2 (ab S35); Q4, Q5 (nach der Migration, S41) | Messboard | ZÄHLER, OSZI | 15 + 30 min |
| | **Summe** | | | **≈ 6 h + Protokoll ≈ 7 h** |

Die Reihenfolge ist fest: V zuerst (ohne Verbindung sind alle späteren Werte unsicher), C2 vor allen HF-Messungen (die Korrektur bleibt danach unverändert), S erst nach den HF-Blöcken (A5 verstellt Drive und Tuning).

### 5.2 Block V · Verbindung

#### J1 · Kabel gerade

`Geräte` ZÄHLER oder OSZI · `Board` Messboard · `Bezug` A-41 · `Dauer` 5 min

1. SDA an GPIO22, SCL an GPIO27 (CN1), booten; Log und Info-Karte lesen; CH1 = 10 MHz einschalten.

**Soll:** verbunden, Log `[APP] SI5351 at 0x60, SDA=22 SCL=27`, kein Dialog; 10 MHz am Ausgang.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### J2 · Kabel vertauscht

`Geräte` ZÄHLER oder OSZI · `Board` Messboard · `Bezug` A-41, E-17 · `Dauer` 5 min

1. SDA und SCL vertauschen, booten; Log und Info-Karte lesen; CH1 = 10 MHz.

**Soll:** verbunden, Log `SDA=27 SCL=22 (swapped)`, Info-Karte mit `(swapped)`; 10 MHz am Ausgang. Danach wieder gerade stecken.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A1 · Booten ohne SI5351 (alt 1)

`Board` ohne Modul · `Bezug` A-34, A-36, E-17 · `Dauer` 5 min

1. Board ohne Modul an CN1 anschließen, Serial Monitor starten, EN drücken.
2. Dialoge zählen, Text lesen, `OK` tippen.
3. MAIN, CH1- und CH3-Seite, Info-Karte ansehen.
4. CH1 auf 7,100 000 MHz ändern und 10 s warten.

**Soll:** genau **ein** Dialog „SI5351 not found …“ mit dem Hinweis auf SDA/SCL in beliebiger Reihenfolge; `OK` schließt ihn, er kommt nicht wieder; MAIN und Header zeigen `NO HW` bei allen Kanälen, die Kanalseiten `SI5351 not found`; Info-Karte `SI5351 not found`; die Änderung wird gespeichert (`[SETTINGS] Saved to NVS`); keine wiederholten Fehlerzeilen im Log.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A2 · Modul nachträglich anschließen (alt 1a, enthält J3)

`Geräte` Stoppuhr oder VIDEO, ZÄHLER oder OSZI · `Board` Messboard · `Bezug` A-35, A-41 · `Dauer` 15 min

1. Messboard mit abgezogenem SDA booten, den Dialog **nicht** schließen.
2. SDA anstecken; Zeit vom Kontakt bis zum Verschwinden von `NO HW` messen (3×).
3. Dialogzustand prüfen; CH1 = 10 MHz einschalten.
4. (J3) Ohne SDA/SCL neu booten, dann **vertauscht** anstecken; Zeit einmal messen. Danach wieder gerade stecken.

**Soll:** `NO HW` verschwindet nach **≤ 2,5 s**; der offene SI5351-Dialog schließt sich selbst; Log `[APP] SI5351 at 0x60, SDA=22 SCL=27` bzw. `SDA=27 SCL=22 (swapped)`; CH1 liefert 10 MHz.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Zeiten gerade / vertauscht):

#### A3 · Verbindung im Betrieb unterbrechen (alt 1b)

`Geräte` ZÄHLER oder OSZI · `Board` Messboard · `Bezug` A-35, E-17 · `Dauer` 10 min

1. CH1 = 10 MHz einschalten, Ausgang an ZÄHLER.
2. SDA ≈ 5 s abziehen, wieder anstecken.
3. Dialog, Status und Zählerwert beobachten; Log mitlesen.

**Soll:** Dialog „Connection lost. Check the cable; SDA/SCL may be in either order …“, MAIN `NO HW`; das Modul bleibt versorgt, der Ausgang läuft weiter (Handbuch §11); nach dem Anstecken `[APP] Restoring outputs`, CH1 wieder `ON` mit derselben Frequenz (Zähler), der Dialog schließt sich selbst.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### A4 · Versorgungsausfall des Moduls (alt 1c)

`Geräte` OSZI · `Aufbau` 3 · `Board` Messboard · `Bezug` A-35, A-18 · `Dauer` 10 min

1. CH1 = 10 MHz an, CH3 an (Master CH1, 270°); beide Ausgänge an OSZI.
2. 3,3 V am Modul ≈ 2 s trennen, wieder anschließen.
3. Verhalten wie A3 beobachten; danach die Phase messen (3.4).

**Soll:** wie A3; beide Ausgänge aus dem Rücksetzzustand des Chips wiederhergestellt, CH3 wieder 270° (± 1° + u_φ).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Phase nachher):

### 5.3 Block F · Frequenz und Kalibrierung

Messboard, Aufbau 1, ZÄHLER mit REF, Standardbedingungen, 15 min Aufwärmzeit. Auswertung mit der Klammermessung (3.3).

#### C1 · Korrektur 0, Quarzfehler bestimmen (alt 3)

`Geräte` ZÄHLER, REF · `Bezug` A-11 · `Dauer` 10 min

1. CONFIG: `Corr.` auf 0.00 ppm.
2. CH1 = 10 MHz einschalten, 10 Werte mit 1 s Torzeit mitteln.
3. Auf der CH1-Seite die Zeile `Out` ablesen.

**Soll:** zu dokumentieren: f_mess − 10 MHz (Hz) = Quarzfehler; `Out` zeigt die Rundung der Register (bei 10 MHz ±0,0 Hz).
**Ergebnis:** i · Messwert (Hz, ppm):

#### C2 · Kalibrieren (alt 4)

`Geräte` ZÄHLER, REF · `Bezug` A-11, U-11 · `Dauer` 15 min

1. Korrektur in ppm = Abweichung aus C1 (Hz) ÷ 10 (Handbuch §8) mit der Schritttaste (`1`, `0.1`, `0.01`) und `−`/`+` einstellen; **Anzahl der Tipps und Zeit notieren** (Bedienaufwand U-11, Grundlage für S32).
2. 10 MHz erneut messen, nachstellen bis |Abweichung| < 0,2 Hz.
3. Korrektur nicht mehr ändern; Wert ins Protokoll (Anhang A).

**Soll:** danach < 1 Hz Abweichung bei 10 MHz (Auflösung 0,01 ppm ≙ 0,1 Hz); Wert im Bereich ±50 ppm; das Vorzeichen stimmt (positive Korrektur senkt die Frequenz, Handbuch §8).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Corr. in ppm, Tipps, Zeit):

#### C3 · Genauigkeit über den Bereich (Klammermessung)

`Geräte` ZÄHLER, REF · `Bezug` A-12 (≤ 0,5 Hz in `Fractional`; Host-Test ≤ 0,375 Hz), A-14 · `Dauer` 30 min (+ 15 min mit H1, H2)

1. Standardbedingungen, Korrektur nach C2.
2. Je Zeile die Klammermessung (3.3); Frequenz über das NUMPAD, 10 s Einschwingzeit; δ eintragen.
3. Zeile K7: `Tuning` = `Integer`, f_soll = die tatsächliche Frequenz laut `Out`; danach zurück auf `Fractional`.
4. Ab Firmware mit S35 zusätzlich die Zeilen H1 und H2.

| f_soll | f10a (Hz) | f_mess (Hz) | f10b (Hz) | δ (Hz) | Grenze | ✔/✘/○ |
|---|---|---|---|---|---|---|
| 1,000 000 MHz | | | | | 0,5 Hz + u | |
| 7,100 000 MHz | | | | | 0,5 Hz + u | |
| 28,400 000 MHz | | | | | 0,5 Hz + u | |
| 99,999 999 MHz | | | | | 0,5 Hz + u | |
| K7: 7,1 MHz `Integer` → 7,102 273 MHz laut `Out` | | | | | Anzeige ± u | |
| H1 (ab S35): 8 kHz / 455 kHz | | | | | 0,5 Hz + u | |
| H2 (ab S35): 112,499 996 / 112,500 004 / 160 MHz | | | | | 1,5 / 2 / 3 Hz + u | |

**Soll:** \|δ\| ≤ Grenze; die Zwischenpunkte (3,5 / 14,2 / 50 MHz aus Plan 2.0) deckt der Host-Test ab.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### K7 · `Integer`: tatsächliche Frequenz

`Geräte` ZÄHLER, REF · `Bezug` A-6, A-14, E-19 · `Dauer` in C3 (5 min)

1. Zeile K7 der Tabelle in C3; zusätzlich MAIN ansehen.

**Soll:** Zähler = angezeigte tatsächliche Frequenz (`Out 7.102 273 MHz`, +2272,7 Hz) ± u; MAIN zeigt `~7.100 000 MHz`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### H1 · Untergrenze und kHz-Anzeige (alt 20, bedingt ab S35)

`Geräte` ZÄHLER · `Bezug` E-16 · `Dauer` in C3 (5 min)

1. Zeile H1 der Tabelle in C3 (8 kHz, 455 kHz); Anzeige bei 999,999 kHz und 1 MHz ablesen.

**Soll:** Frequenz ≤ 0,5 Hz + u; Anzeige in kHz bzw. MHz laut Handbuch.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ ☐ – · Messwert:

#### H2 · Oberes Ende und 112,5 MHz (alt 21, bedingt ab S34/S35)

`Geräte` ZÄHLER · `Bezug` E-16, S34 · `Dauer` in C3 (10 min)

1. Zeile H2 der Tabelle in C3; bei 160 MHz zusätzlich prüfen, dass ein Ausgangssignal vorhanden ist (Pegel geringer).

**Soll:** Abweichung ≤ 1,5 / 2 / 3 Hz + u (Grenzen aus S34).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ ☐ – · Messwert:

### 5.4 Block T · Abstimmen ohne Aussetzer

OSZI Kanal 1 an CLK0 (CH1). **Aussetzer erkennen:** Trigger „Timeout“ (Dropout) mit Schwelle ≈ 3 Perioden, Single-Modus; jeder Trigger = ein Aussetzer, Dauer mit den Cursorn messen. Ohne Timeout-Trigger: Persistenz und Triggerverlust beobachten, Ergebnis „○“.

#### D1 · Kanal 1 bleibt beim Abstimmen von CH2 ungestört (alt 6)

`Geräte` OSZI · `Bezug` A-10 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH2 = 7,100 000 MHz an. OSZI: Timeout-Trigger auf CLK0.
2. CH2 über `+`/`−` mit 1 MHz, 1 kHz, 1 Hz verstellen; per NUMPAD mindestens 20 Sprünge zwischen 1 und 99 MHz.
3. Auslösungen zählen.

**Soll:** **0** Aussetzer an CH1 (A-10: kein Reset der anderen PLL).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Aussetzer von 20):

#### D2 · Dauerabstimmen im `Fractional`-Modus (alt 18, enthält C5)

`Geräte` OSZI, ZÄHLER · `Bezug` A-6, A-10, A-11 · `Dauer` 15 min

1. CH1 = 10 MHz, Schritt 1 Hz, `+` 10 s halten; Timeout-Trigger auf CLK0.
2. CH1 = 99 MHz, Schritt 1 MHz, bis 100 MHz.
3. (C5) CH1 = 10 MHz; `Corr.` um +1,00 ppm ändern, Zähler und OSZI beobachten; danach zurück auf den Wert aus C2.

**Soll:** keine Aussetzer beim Abstimmen; zulässig ist nur ein Aussetzer bei einem PLL-Wechsel 900 ↔ 887,5 MHz (A-6), der dokumentiert wird (Frequenz, Dauer). C5: Frequenz sinkt um ≈ 10 Hz (± 0,5 Hz), **kein** Aussetzer.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Aussetzer, Δf bei +1 ppm):

### 5.5 Block IQ · Phasen

Aufbau 3, Methode 3.4; φ₀ und u_φ aus V4 a. CH1 ist Master, Standardbedingungen, Korrektur nach C2. Phase jeweils **2 s nach** dem Umschalten messen. Die Werte aus E1 und E2 dienen zugleich als Daten für E3 und E4 (Stufe 2).

#### E1 · Vier Phasen bei 10 MHz (alt 7)

`Geräte` OSZI · `Bezug` A-15, A-16 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH3 an (Master CH1); Phase `0`, `90`, `180`, `270` nacheinander; je Phase Δt (≥ 256 Mittelungen) und φ.

| Phase | Δt (ps) | φ gemessen (°) | Abweichung (°) | Grenze |
|---|---|---|---|---|
| 0° | | | | 1° + u_φ |
| 90° | | | | 1° + u_φ |
| 180° | | | | 1° + u_φ |
| 270° | | | | 1° + u_φ |

**Soll:** 0°, 90°, 180°, 270° ± 1° (+ u_φ).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### E2 · Phasen an den Bereichsgrenzen (alt 8, gekürzt)

`Geräte` OSZI · `Bezug` A-15, A-16, E-7 · `Dauer` 25 min

1. Für f = 4,73 / 14,2 / 99 MHz je vier Phasen wie E1. Der IQ-Teiler ist min(127, ⌊900 MHz / f⌋): 127, 63, 9 (größter Teiler, zwei ungerade Teiler).

| f (MHz) | Teiler | 0° | 90° | 180° | 270° |
|---|---|---|---|---|---|
| 4,73 | 127 | | | | |
| 14,2 | 63 | | | | |
| 99 | 9 | | | | |

**Soll:** alle Phasen ±1° (+ u_φ). Bei 99 MHz entspricht 1° nur ≈ 28 ps: bei u_φ > 0,5° das Ergebnis „○“ und mit mehr Mittelung wiederholen. Die Punkte 5 und 30 MHz aus Plan 2.0 entfallen (Host-Test der Phasenregister, keine neue Grenze).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

### 5.6 Block P · Pegel im Zustand OFF und beim Neustart

Aufbau 6, **nie SPEKTRUM**.

#### K1 · Pegel im Zustand OFF je `Off state` (enthält A9)

`Geräte` OSZI, DMM · `Aufbau` 6 · `Board` Messboard · `Bezug` A-3, A-8, E-21 · `Dauer` 20 min

1. (A9) `Off state` = `Low` (Werkseinstellung): CH1, CH2 und CH3 je einmal ein- und wieder ausschalten; DC-Pegel an CLK0, CLK1 und CLK2 mit OSZI 1 MΩ, OSZI 50 Ω und DMM.
2. `Off state` = `High` (seit 1.3.2 mit Bestätigung) und `High-Z`: CH1 ein- und ausschalten, Pegel an CLK0; bei `High-Z` zusätzlich mit 10 kΩ nach 3,3 V bzw. nach GND.
3. Danach `Off state` wieder `Low`.

| Off state | Ausgang | 1 MΩ (V) | 50 Ω (V) | DMM (V) | 10 kΩ gegen 3,3 V / GND (V) |
|---|---|---|---|---|---|
| Low | CLK0 | | | | – |
| Low | CLK1 | | | | – |
| Low | CLK2 | | | | – |
| High | CLK0 | | | | – |
| High-Z | CLK0 | | | | |

**Soll:** `Low` ≈ 0 V (< 0,1 V) an allen drei Ausgängen in jeder Last; `High` ≈ 3,3 V an 1 MΩ, an 50 Ω durch den Drive-Strom begrenzt (notieren); `High-Z` nimmt den Pegel der Last an.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○

#### K2 · Pegel nach `CLKx_PDN` (entscheidet N5)

`Geräte` OSZI, DMM · `Aufbau` 6 · `Board` Messboard · `Bezug` E-21, N5 · `Dauer` 15 min

Seit 1.3.0 schaltet die Firmware CLK2 bei ausgeschaltetem CH3 nur mit `Off state` = `Low` über `CLK2_PDN` ab; bei `High` und `High-Z` bleibt der Ausgangstreiber versorgt (Handbuch, „bis zur Hardwareprüfung K2“).

1. `Off state` = `Low`, CH3 aus (CLK2 mit `CLK2_PDN`): Pegel an CLK2 mit OSZI 1 MΩ, 50 Ω und DMM.
2. Mit 10 kΩ gegen 3,3 V und gegen GND: zieht der Pin auf Low oder ist er hochohmig?

**Soll:** Pegel notieren. Ist der Pin mit `PDN` hochohmig, darf `High-Z` alle abgeschalteten Ausgänge mit `CLKx_PDN` abschalten (N5, E-21); zieht er auf Low, bleibt es bei der heutigen Lösung. Ergebnis in Handbuch DE/EN nachziehen (Hinweis „bis zur Hardwareprüfung K2“).
**Ergebnis:** i · Messwert (CLK2 an 1 MΩ / 50 Ω / DMM / 10 kΩ gegen 3,3 V / 10 kΩ gegen GND):

#### A10 · Ausgang beim Neustart (alt 18c)

`Geräte` OSZI · `Aufbau` 6, Kanal 2 an EN des ESP32 · `Board` Messboard · `Bezug` A-34, S63 · `Dauer` 10 min

1. CH1 = 10 MHz einschalten. OSZI: Kanal 1 an CLK0, Kanal 2 an EN; 100 ms/div, Trigger auf fallende Flanke von EN, Single.
2. 5× Reset mit der EN-Taste, 1× Upload (`pio run -t upload`); Zeit vom Reset bis zum Ende des 10-MHz-Signals ablesen.

**Soll:** zu dokumentieren; erwartet: der Ausgang läuft bis zur Initialisierung weiter (beim Upload einige Sekunden) und endet **vor** der Hauptseite. Die Dauer kommt als Hinweis ins Handbuch (Schutz angeschlossener Schaltungen, S63).
**Ergebnis:** i · Messwert (EN: min / max in ms; Upload in s):

### 5.7 Block S · Speichern

#### A5 · Speichern und Neustart (alt 14, enthält K9)

`Board` Messboard · `Bezug` A-4, A-20 bis A-23 · `Dauer` 15 min

1. Einstellen: CH1 = 7,100 000 MHz, CH2 = 14,200 000 MHz, CH3 Master CH2, Phase 90°, Backlight 60 %; (K9) `Drive all` = 4 mA, `Tuning` = `Low noise`, `Spread CH1` = `-0.5 %`, `Off state` = `High-Z`; alle drei Kanäle einschalten.
2. Auf CONFIG die Titelzeile beobachten (`Saving…` → `Saved` nach 5 s); Log `[SETTINGS] Saved to NVS`.
3. Stecker ziehen, wieder ein.
4. Danach die Standardbedingungen wiederherstellen (8 mA, `Fractional`, Spread `Off`, `Low`).

**Soll:** `Saved` nach ≈ 5 s; nach dem Neustart alle Werte erhalten, die Detailzeile auf MAIN und den Kanalseiten zeigt die Sondermodi (z. B. `SSC -0.5 % · 8 mA` bzw. `4 mA`); **alle Ausgänge OFF** (E-11).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### K9 · Optionen bleiben gespeichert

`Board` Messboard · `Bezug` A-20 bis A-22 · `Dauer` in A5

1. CONFIG-Optionen aus A5 Schritt 1 nach dem Neustart lesen.

**Soll:** `Drive all` 4 mA, `Tuning` `Low noise`, `Spread CH1` `-0.5 %`, `Off state` `High-Z` erhalten, alle Ausgänge aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

### 5.8 Block D · Display und Touch (beide Boards)

#### I3 · Erkennung im Wiederholtest (enthält A6)

`Geräte` VIDEO optional · `Board` beide · `Bezug` A-33, A-44, E-18 · `Dauer` 20 min

1. Je Board 10× Kaltstart (USB-Stecker ziehen und stecken) und 10× EN-Taste.
2. Je Start: Log `[DISPLAY] … (detected)` und Bild kontrollieren.
3. (A6) Bei den Kaltstarts auf das Einschalten achten (3 davon mit Slow-Motion aufnehmen): Beleuchtung bleibt aus, bis das erste Bild gezeichnet ist.

**Soll:** 20/20 je Board richtig erkannt; nie weißes oder verrauschtes Bild; Hauptseite erscheint direkt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehlerkennungen / Fehlstarts je Board):

#### B6 · Sichtprüfung aller Seiten (alt 15, enthält B3)

`Geräte` Kamera · `Board` beide · `Bezug` A-2, A-24, A-25, A-27 · `Dauer` 20 min

1. Je Board fotografieren: MAIN mit Statusleiste, Header, CH1, CH2, CH3 in den Zuständen ON, OFF, WAIT (Master auf 4,72 MHz) und NO HW (Board ohne Modul), CONFIG oben und unten gescrollt (Reihenfolge Drive, Off state, Tuning, Spread, Corr., Backlight, Touch), Dialog, Touch-Abgleich (CONFIG `Touch`, danach abbrechen bzw. zu Ende führen).
2. (B3) NUMPAD: „500“ `MHz` und „0.5“ `MHz` eingeben, dann `C`, „10.7“ `MHz`.
3. Auf abgeschnittene Texte, Überlappungen und Scrollbalken prüfen (nur CONFIG hat eine senkrechte Leiste).
4. RGB-LED je Seite ablesen.

**Soll:** nichts abgeschnitten, nichts überlappt, kein Scrollbalken außer auf CONFIG; Header zeigt den Ist-Zustand wie MAIN; RGB-LED CH1 rot, CH2 grün, CH3 blau, auf MAIN, CONFIG und NUMPAD aus. NUMPAD: zweimal die Grenze laut Handbuch (heute `Allowed 1 - 100 MHz`) in Rot mit rotem Rahmen, die Eingabe bleibt stehen (U-9); dann 10.700 000 MHz. Den Zustand ERR zeigt nur A8 (Stufe 3); seine Texte sind host-getestet.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert / Fotos:

#### K8 · CONFIG-Dropdowns und Info-Karte

`Board` beide · `Bezug` A-31, E-24 · `Dauer` in B6 (5 min)

1. Alle Dropdowns öffnen, auch ganz unten gescrollt.
2. Info-Karte: Firmware, Branch, Commit, Build-Zeit mit `git log -1 --format="%h %cd"` vergleichen; Display-Controller, I2C-Pins.

**Soll:** Listen vollständig sichtbar (öffnen nach oben, wenn unten kein Platz ist); Angaben der Info-Karte stimmen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B7 · Randtasten treffen

`Board` beide · `Bezug` A-32, E-1, U-5 · `Dauer` 10 min

1. Nach dem Touch-Abgleich je Ziel 5× mit dem Finger: Schritt-Chips `1M` (links) und `1` (rechts), `MAIN`, `CONFIG` in der Statusleiste, Dialog-`OK`, Schalter auf den Karten, unterste CONFIG-Zeile.
2. Fehltipps (keine Reaktion, falsches Ziel) zählen.

**Soll:** 5/5 Treffer an jedem Ziel auf jedem Board.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Fehltipps je Ziel):

#### B8 · Trefferkarte nach dem Touch-Abgleich (U-5, U-26)

`Geräte` PC (Log) · `Board` beide · `Bezug` A-26, A-32, E-1 · `Dauer` 40 min · Build: Trefferkarte

1. Trefferkarte flashen (der gespeicherte Abgleich bleibt im NVS); 20 Zielkreuze, darunter die vier Ecken (0 px) und Positionen 4 px vom Rand.
2. Ein Bediener tippt jedes Kreuz 3× mit dem Finger; `[HITMAP]`-Zeilen mitschreiben.
3. Auswertung je Board: mittlere und maximale Abweichung (px) je Bereich (Ecken, Rand, Mitte).
4. Danach den Release-Build zurückflashen.

**Soll:** im Mittel ≤ 4 px, höchstens 8 px (Prüfgrenze des Abgleichs) je Bereich; alle Ecken erreichbar (auch x = 319). Größere Abweichungen sind ein Befund zum Abgleich (U-26); ob ein Stift empfohlen wird, entscheidet B8 mit dem optionalen B19.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (mittel/max je Bereich und Board): Vorlauf 2026-11-04 (ein Bediener, Finger, Firmware 2073e2d mit fester Kalibrierung): Original-CYD dx −11 … −19 px in der Mitte, −4 … −12 px am Rand, dy 0 … +6 px; rechts nur x ≈ 305–308 erreichbar, die rechte Ecke (319) nicht. CYD2USB (55 Tipps): dx bei x = 80 −14 … −19 px, bei 160 −10 … −15, bei 240 −5 … −8, am rechten Rand −1 … −7; dy −7 … +3 (ein Ausreißer +11). Beide Boards linear (Versatz und Steigung in x, Original-CYD ≈ 1,03, CYD2USB ≈ 1,06) → U-26 (Touch-Abgleich, S28a). Abgleich mit S28a am 2026-11-04: CYD2USB x 161…3778, y 245…3830, Prüfkreuz −2/+2 px; Original-CYD x 166…3716, y 280…3885, −2/+1 px (statt fest 300…3800). Trefferkarte mit Abgleich wiederholen (dieser Test).

#### I7 · Langzeit-Anzeige auf dem CYD2USB

`Board` CYD2USB (= Messboard) · `Bezug` E-18 · `Dauer` 0 min (läuft über den ganzen Stufe-1-Tag mit)

1. Während aller Blöcke von Stufe 1 (≈ 5 h Bedienung mit Seitenwechseln, NUMPAD, `+` halten, CONFIG scrollen) auf Pixelfehler achten.

**Soll:** keine Pixelfehler (SPI-Modus 0 mit 40 MHz).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

### 5.9 Bedingte Messungen

#### Q4 · Assert-Handler (C-3, S-23)

`Geräte` OSZI, PC (Log) · `Board` Messboard · `Bezug` C-3, W-S9 · `Dauer` 15 min · Build: `TEST_LVGL_ASSERT`

**Erledigt 2026-11-04** (S09, COM7 = Original-CYD ohne Modul): Assert → Neustart nach < 1 s (`SW_CPU_RESET`). **Wiederholen** nur nach Änderungen am Assert-Handler oder an der Plattform, also einmal nach der Migration auf Core 3.x (S41), dann auf dem Messboard:

1. CH1 = 10 MHz an; OSZI auf CLK0 (Timeout-Trigger) und auf EN.
2. Der Testbuild füllt 20 s nach dem Boot den LVGL-Speicher und löst einen Malloc-Assert aus.
3. Log und Zeit bis zum Neustart; Zustand der Ausgänge danach.

**Soll:** Logzeile mit Ursache, Neustart innerhalb 5 s (kein dauerhaftes Einfrieren); nach dem Neustart alle Ausgänge aus (bis dahin läuft CLK0 weiter wie in A10).
**Ergebnis:** ✔ 2026-11-04 (< 1 s, ohne Modul) · nach S41: ☒ ✔ ☐ ✘ ☐ ○ · Messwert: 2026-11-05, Core 3.3.12 vor der Abnahme (S41a), COM13 mit Modul, nur Log: Assert → `[ERROR][FATAL] LVGL assert; restarting`, `SW_CPU_RESET` nach < 0,1 s, danach `[SI5351] Init OK; all outputs off`; CLK0 am OSZI noch offen

#### Q5 · Loop-Watchdog (S-23, W-S23)

`Geräte` OSZI, PC (Log) · `Board` Messboard · `Bezug` W-S23 · `Dauer` 15 min · Build: `TEST_LOOP_HANG`

**Erledigt 2026-11-04** (S09, COM7 ohne Modul): blockiertes `loop()` → Task-Watchdog nach 5 s. **Wiederholen** wie Q4 nach S41 (Core 3.x hat eine neue Watchdog-API):

1. CH1 = 10 MHz an (innerhalb von 20 s nach dem Boot); der Testbuild blockiert danach `loop()`.
2. Zeit bis zum Watchdog-Reset (Log: Reset-Grund), Ausgang an CLK0 währenddessen.

**Soll:** Watchdog-Reset 5 s nach dem Blockieren; Ausgänge nach dem Neustart aus; Dauer des Weiterlaufens im Hängezustand dokumentieren.
**Ergebnis:** ✔ 2026-11-04 (5 s, ohne Modul) · nach S41: ☒ ✔ ☐ ✘ ☐ ○ · Messwert: 2026-11-05, Core 3.3.12 vor der Abnahme (S41a), COM13 mit Modul, nur Log: `task_wdt` meldet `loopTask` nach 5,0 s, `SW_CPU_RESET`, danach `all outputs off`; CLK0 im Hängezustand am OSZI noch offen

H1 und H2 stehen in Block F (C3).

### 5.10 Wiederholung je Release

Stufe 1 läuft einmal vollständig (R0). Vor einer Weitergabe oder einem Release werden nur die Blöcke wiederholt, deren Code sich seit dem letzten vollständigen Lauf geändert hat; der Smoke-Test läuft immer.

| Geänderter Bereich | Blöcke |
|---|---|
| `drivers/si5351_manager.*`, `drivers/si5351_plan.*` | F, T, IQ, P |
| `core/app_controller.*` | V, T, IQ, P, S |
| `hal/i2c_bus.*` | V (mit J6 aus Stufe 2) |
| `services/settings.*`, `storage.*` | S |
| `drivers/display_*`, Touch, `gui/` | D |
| **Plattform** (Core, Bibliotheken, Compiler, z. B. Migration S41) | alle Blöcke, Q4, Q5, J6 |
| nur Doku | keiner (nur Smoke-Test) |

**Auswahl nach der Migration (S41):** alle Blöcke, außer in F nur die Zeilen 7,1 und 99,999 999 MHz von C3 und in D nur I3 und B7; dazu Q4, Q5 und J6 (≈ 4 h).

### 5.11 Stufe 1W – Pflicht ab WEB-HMI (vor 1.4.0)

Nur im Release-Kandidaten mit WEB-HMI ([ARBEITSPLAN.md](ARBEITSPLAN.md), Phase 6). Aufbau 7, Test-WLAN WPA2, 2,4 GHz, eigenes Segment. Die Bezeichnungen der Bedienelemente stammen aus dem WEB-HMI-Konzept im Arbeitsplan; maßgeblich ist das Handbuch der geprüften Firmware. Gerätename, IP und Token aus der Info-Karte; `<IP>` = Geräteadresse, `<TOKEN>` = Token. Vor jedem Sicherheitstest den Zustand notieren (Kanal aus). Was die Host-Suites (`test_http_guard`, `test_scpi`, `test_line_framer`, `test_sanitize`, `test_rest_json`, `test_net_settings`, `test_command`) abdecken, prüft 1W nur stichprobenartig am Gerät.

| Messung | Inhalt | Dauer |
|---|---|---|
| L1, L3, L12 | WLAN-Modi, AP mit QR, Betrieb ohne SI5351 | 1 h 45 min |
| L6 | SCPI über USB und TCP | 45 min |
| L14 | 24-h-Dauertest (unbeaufsichtigt) | 30 min aktiv |
| L-S1, L-S9, L-S10, L-S11 | ein Skriptlauf (`lsec_check.py`, Anhang D) | 60 min |
| L-S2, L-S3 | fremde Webseite, SCPI-Port aus dem Browser | 65 min |
| L-S6, L-S7, L-S8 | kein Routing, keine Klartext-Passwörter, `Remote outputs` | 65 min |
| L-S4, L-S5 | fremde SSIDs, Last (Tabelle unten) | 90 min |
| | **Summe** | **≈ 8 h + 24 h** |

#### L-S4, L-S5 · Fremde SSIDs und Last

Pflicht, weil sie die Befunde S-7 (XSS über SSIDs, hoch) und S-13 (DoS/Heap) am Gerät prüfen; nur im eigenen Labor.

| Nr | Prüfung | Bezug | Soll | Dauer |
|---|---|---|---|---|
| L-S4 | Fremde SSIDs (W-S8, W-S9): Test-AP mit `<svg onload=alert(1)>`, `a"b\c`, `x\ny`, `\xF0A`, `\xC3`, 32 Byte und 200 Beacons; Scan am Gerät und im Web (nur im eigenen Labor, kleine Sendeleistung) | `test_sanitize` | keine Skriptausführung; SSIDs bereinigt; höchstens 15 Einträge; kein Neustart | 45 min |
| L-S5 | Lasten (W-S10): Slowloris mit 30 Sockets, WebSocket-Flut, Multipart und Chunked; Touch bedienen; Heap vor, während, nach 2 min | – | Touch reagiert < 200 ms; Heap erholt sich auf ≥ 95 %; kein Neustart | 45 min |

#### L1 · WLAN-Modi und Rückfall-AP

`Geräte` Router, Smartphone · `Bezug` W-E (WEB-HMI-Konzept) · `Dauer` 45 min

1. CONFIG `WiFi` = `Off`: Smartphone sucht Netze; SCPI und Web nicht erreichbar.
2. `Client`: Test-WLAN auswählen, Passwort eingeben; Rückmeldungen (`Connecting…`, `Connected <IP>`) und Zeit bis zur Verbindung (3×).
3. Router abschalten: nach 60 s startet der Rückfall-AP (WPA2), Statusleiste und Rückmeldung `Fallback: AP active`; Router wieder ein: Gerät verbindet sich, der AP schaltet sofort ab.
4. `Access point`: AP erscheint am Smartphone.
5. Falsches Passwort und unbekanntes Netz: Rückmeldungen `Wrong password`, `Network not found`.

**Soll:** wie beschrieben; Verbindung ≤ 15 s; Rückfall-AP nach 60 s (± 5 s).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Zeiten):

#### L3 · AP mit Zufallspasswort und QR, offener AP

`Geräte` Smartphones Android und iOS · `Bezug` W-E3, W-S1, W-S2 · `Dauer` 45 min

1. `Access point`: SSID und Passwort auf dem Display; QR-Code „Join“ mit beiden Smartphones scannen und beitreten.
2. `Regenerate`: neues Passwort; die alte Verbindung wird getrennt.
3. `Open (not recommended)`: Bestätigung am Touch erforderlich; Statusleiste `AP open` in Warnfarbe; höchstens 2 Stationen (dritte abgelehnt); ohne Station nach 15 min aus.

**Soll:** QR funktioniert auf beiden Systemen; offener AP nur nach Bestätigung, mit Warnung, Zeit- und Stationslimit.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L6 · SCPI über USB und TCP

`Geräte` PC (Skripte Anhang D) · `Bezug` W-S22, C-16 · `Dauer` 45 min

1. USB: Port ohne DTR/RTS-Wechsel öffnen (`scpi_usb.py`); das Board darf nicht neu starten. 1000× `*IDN?`.
2. TCP: `TCPIP0::<IP>::5025::SOCKET`; 1000× `*IDN?` (`scpi_tcp.py`), Median und 99-%-Perzentil der Antwortzeit.
3. Während der USB-Sitzung auf Logzeilen im Datenstrom achten; `SYSTem:LOG?` liefert die Logzeilen.

**Soll:** kein Neustart beim Öffnen; 1000/1000 gültige Antworten `CYD,HF-Generator,<MAC>,1.4.0`; keine Logzeilen im Datenstrom; Median < 50 ms.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L12 · Ohne SI5351

`Board` ohne Modul · `Dauer` 15 min

1. Booten ohne Modul; Web öffnen, SCPI verbinden.
2. `OUTPut1:STATus?`, `SYSTem:SI5351?`, Einstellungen ändern; Web-Zustand ansehen.

**Soll:** Web und SCPI funktionieren; Kanäle zeigen `NO HW` bzw. `NOHW`; Einstellungen werden gespeichert; keine wiederholten Fehler.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L14 · 24-h-Dauertest

`Geräte` PC (Skript) · `Bezug` W-S23, Heap-Grenzen des WEB-HMI-Konzepts · `Dauer` 24 h (30 min aktiv)

1. Drei Browser (Live-Updates) und ein SCPI-Skript mit 10 Befehlen/s (Frequenz ändern innerhalb 8–12 MHz, Abfragen) laufen 24 h.
2. `SYSTem:HEAP?` jede Minute protokollieren (freier und minimaler Heap, größter Block, LVGL-Heap, Stack-Reserve).
3. Nach 24 h: Neustart-Zähler (Log), Heap-Verlauf, Fehlerzähler.

**Soll:** kein Neustart; minimaler Heap sinkt nicht stetig; Stack-Reserve ≥ 1 KB; keine Warnung „Heap < 40 KB“.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Heap min Start / nach 24 h):

#### L-S1 · curl-Matrix (W-S1, W-S4, W-S5, W-S10)

`Bezug` W-S1, W-S4, W-S5, W-S10 · `Dauer` im Skriptlauf `lsec_check.py`

1. `curl -i -H "Host: evil.example" http://<IP>/api/state` → **421**.
2. `curl -i -X POST -H "Content-Type: text/plain" -H "X-CYD-Token: <TOKEN>" --data '{"enabled":true}' http://<IP>/api/channel/1` → **415**.
3. Ohne Header `X-CYD-Token` → **403**.
4. `-H "Origin: http://evil.example"` → **403**; `-H "Sec-Fetch-Site: cross-site"` → **403**.
5. Body 100 KB (`--data-binary @big.json`) → **413**.
6. Über einen **offenen** AP `POST /api/wifi` mit Passwortfeld → **403**.
7. `curl -i -X OPTIONS http://<IP>/api/channel/1` → abgelehnt; keine `Access-Control-Allow-*`-Header.
8. Nach jedem Schritt: Zustand der Kanäle unverändert.

**Soll:** wie angegeben; Zustand bleibt unverändert.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S2 · Fremde Webseite (W-S5, W-S6)

`Geräte` PC, Browser · `Dauer` 45 min

1. Testseite mit `python -m http.server 8000` (andere Herkunft); darauf `fetch('http://<IP>/api/channel/1', {method:'POST', mode:'no-cors', body:'{"enabled":true}'})`, ein `<form enctype="text/plain" method="POST">` mit JSON im Feldnamen und `new WebSocket('ws://<IP>/ws')`.
2. Seite öffnen; Zustand des Geräts prüfen.
3. **DNS-Rebinding:** in der `hosts`-Datei `evil.example` auf `<IP>`; `http://evil.example/` öffnen.

**Soll:** Zustand unverändert; WebSocket-Upgrade abgelehnt; `evil.example` → 421.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S3 · SCPI-Port aus dem Browser (W-S17, W-S18)

`Dauer` 20 min

1. `curl -X POST --data-binary $'OUTP1 ON\n' http://<IP>:5025/` → Verbindung wird geschlossen, CH1 bleibt aus.
2. `python -c "import sys; sys.stdout.write('A'*20000 + 'OUTP1 ON\n')" | ncat <IP> 5025` (eine überlange Zeile, **kein** Zeilenumbruch vor `OUTP1 ON`) → CH1 bleibt aus; `SYSTem:ERRor?` danach −363.
3. Zeile mit HTTP-Header-Muster (`Host: x`) → Verbindung wird geschlossen.

**Soll:** CH1 bleibt in allen Fällen aus.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S6 · Kein Routing zwischen AP und STA (W-S3)

`Geräte` Smartphone, Laptop · `Dauer` 15 min

1. AP-Modus: Smartphone verbinden, Gateway/DNS in den Netzwerkdetails ablesen (DHCP ohne Gateway-Option).
2. Mit Verbindung zum Router: der AP schaltet ab (L1); Zeit von der STA-IP (Log) bis zum Verschwinden des AP am Smartphone messen.
3. Optional (Stufe 3): Laptop am Rückfall-AP mit statischer Route `route add <Heimnetz> mask <Maske> 192.168.4.1`, Zugriff auf einen Host im Heimnetz versuchen.

**Soll:** kein Gateway per DHCP; AP aus ≤ 1 s nach der STA-IP (D14: das IDF leitet weiter, solange beide eine Adresse haben); optional: kein Host im Heimnetz erreichbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S7 · Keine Klartext-Passwörter im Flash (W-S14)

`Geräte` PC (`esptool`) · `Dauer` 30 min

1. Eindeutige Test-Passwörter setzen (WLAN `Testpass-7f3a91`, Web-Passwort `Webpass-2c8d55`).
2. NVS-Partition lesen (Offset und Größe aus der Partitionstabelle): `esptool --port COMx read_flash <offset> <size> nvs.bin`; zusätzlich `read_flash 0 0x400000 full.bin`.
3. `strings nvs.bin full.bin | findstr /i "Testpass Webpass"`.
4. Werksreset auslösen; Dump wiederholen.

**Soll:** keine Passphrase im Klartext (WLAN als PMK, Web-Passwort als Hash); nach Werksreset weder `hfnet` noch `nvs.net80211` noch Coredump mit Inhalt.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S8 · `Remote outputs: local only` (W-S21)

`Bezug` W-E25, W-S21 · `Dauer` 20 min

1. CONFIG `Remote outputs` = `local only`.
2. Per SCPI `OUTP1 ON`, per REST `{"enabled":true}` und über die Weboberfläche einschalten.
3. `OUTP1 OFF` und Einschalten am Touch.
4. Statusleiste `REMOTE <Quelle>`; Option per SCPI/Web nicht änderbar.

**Soll:** Einschalten aus der Ferne abgelehnt (−221 bzw. 403); Ausschalten erlaubt; Touch schaltet weiter; Option nur am Touch änderbar.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S9 · Antwort-Header (W-S7)

`Dauer` im Skriptlauf `lsec_check.py`

1. `curl -i` auf `/`, `/api/state` und eine JS/CSS-Datei; Header lesen.

**Soll:** CSP genau `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`; `nosniff`; `no-referrer`; `/api/*` mit `Cache-Control: no-store`; keine `Access-Control-*`.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S10 · Grenzen und Rate-Limits (W-S10, W-S11)

`Dauer` im Skriptlauf `lsec_check.py`

1. URL mit 300 B; Header mit 3 KB; Body 1 KB + 1 B → 413; Multipart und Chunked → 415; Anfrage mit 12 s Pause → Timeout.
2. Fünfte HTTP-Verbindung und vierter WebSocket gleichzeitig.
3. 50 Schreibbefehle in 1 s → 429 ab dem 21.; `GET /api/wifi/scan` zweimal in 10 s (zweite Antwort aus dem Zwischenspeicher).
4. 5 falsche Logins → Sperre 30 s, dann verdoppelt.

**Soll:** wie in W-S10 und W-S11.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### L-S11 · Web-Passwort und Sitzung (W-S13)

`Dauer` im Skriptlauf `lsec_check.py`

1. Passwort mit 7 Zeichen → abgelehnt; mit 8 Zeichen → angenommen.
2. Login: `Set-Cookie` lesen (HttpOnly, SameSite=Strict, Ablauf 12 h); kaputte `Authorization`- und `Cookie`-Header senden.
3. Antworten und Log auf das Passwort prüfen.

**Soll:** wie W-S13; das Passwort erscheint nie in einer Ausgabe.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

## 6. Stufe 2 – Entscheidungsmessungen

Jede Messung dieser Stufe trägt eine offene Entscheidung oder einen Handbuchwert. Sie läuft vor dem Folgeschritt im [ARBEITSPLAN.md](ARBEITSPLAN.md), am besten im selben Zeitraum wie der erste Stufe-1-Lauf (dann sind die Werte zugleich Referenz für L15 und den Vergleich nach der Migration). Vorher V4 b.

### 6.1 Entscheidungen und Messungen

Die Schwellen sind **Vorschläge**; sie werden vor der Messung bestätigt oder geändert.

| Entscheidung | Messungen | Vorgeschlagenes Kriterium | Folgeschritt |
|---|---|---|---|
| Standard-Tuning `Fractional` oder `Low noise` (E-19) | K5, F3, K6 (K7 in Stufe 1) | `Low noise` wird Standard, wenn die Nebenlinien innerhalb ±10 kHz um den Träger im Mittel ≥ 10 dB niedriger sind, die Abweichung ≤ 1,5 Hz bleibt (A-12) und die Aussetzer beim Abstimmen (K6) selten und kurz sind | N1 |
| IQ-Abstimmen ohne Aussetzer | D3, D5, E9 | umsetzen, wenn die PLL-Änderung ohne Reset die Phase hält (D5, E9) und der Aussetzer in D3 > 1 ms stört | N2 |
| Ungerade IQ-Teiler bei 180°/270° | E4 (Auswertung von E1, E2) | umsetzen, wenn der Phasenfehler bei ungeradem Teiler um > 1° größer ist als bei geradem | N3 |
| Phasen-Feinabgleich | E3 (Auswertung von E1, E2), E9 | umsetzen, wenn der Phasenfehler systematisch > 1° ist | N4 |
| `High-Z` über `CLKx_PDN`, MultiSynth abgeschalteter Kanäle abschalten (E-21) | K2 (Stufe 1), F7 | `PDN` für alle abgeschalteten Ausgänge, wenn der Pin dabei hochohmig ist (K2); MultiSynth abschalten, wenn ein abgeschalteter Kanal in F7 als Linie > −70 dBc sichtbar ist | N5 |
| Abhilfen für Störquellen | F6 (F9 optional) | Backlight-PWM, I2C-Takt oder Neuzeichnen ändern, wenn ein Störer > −60 dBc im Frequenzbereich der Anwendung liegt | N6 |
| TCXO-Option | C7 | empfehlen, wenn die Drift nach 15 min Aufwärmzeit > 0,5 ppm/h beträgt | N7 |
| Aussage „50 % Tastverhältnis“ (A-1) | E5 | Handbuch und A-1 präzisieren, wenn die Abweichung > 2 % beträgt | S63 |
| Pegeltabelle und LO-Hinweise im Handbuch | F2 (G1 optional) | Tabelle übernehmen | S63, N8 |
| Aufwärmzeit im Handbuch | C7 | Zeit bis Drift < 0,1 ppm/10 min | S63 |
| Leitung auf GND: Rückgabecode der IDF (E-17) | J6 | liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen; nach der Migration wiederholen | S39, S41 |
| Klon-Module (MS5351M) akzeptieren (E-17) | J7 | Prüfregeln lockern, wenn ein echtes Modul abgelehnt wird | E-17 |
| Druckaussetzer überbrücken, Wiederholung (U-20) | B12 | Überbrückungszeit aus der Verteilung der Aussetzer (heute bis 30 ms überbrückt, geplant 60–80 ms) | S33 |
| Stift am Laborplatz (U-5) | B8 (Stufe 1), B19 optional | Stift empfehlen, wenn der Finger am Rand > 8 px abweicht | Handbuch |
| WLAN während Messungen aus (ab WEB-HMI) | L13, L15 | Hinweis, wenn durch WLAN Nebenlinien > −60 dBc sichtbar sind | S58 |
| Heap-Grenzen (W-S10, ab WEB-HMI) | Q1, L14 | Grenzwerte anpassen, wenn der minimale Heap im Dauertest sinkt | S57 |

### 6.2 Ablauf

| Block | Messungen | Geräte | Dauer |
|---|---|---|---|
| 0 | V4 b; C7 am Morgen nach ≥ 2 h Ruhe starten (läuft 60 min unbeaufsichtigt) | SPEKTRUM, ZÄHLER | 30 min |
| Tuning | K5, F3, K6 | SPEKTRUM, OSZI | 1 h 30 min |
| IQ | D3, D5, E9, E5; Auswertung E3, E4 | OSZI, ZÄHLER | 1 h 30 min |
| Störungen | F6, F7, F2 | SPEKTRUM | 1 h 25 min |
| Bus und Touch | J6, J7, B12 | PC (Log), LOGIK optional | 45 min |
| ab WEB-HMI | L13, L15, Q1 | SPEKTRUM, PC | 1 h 25 min |
| | **Summe** | | **≈ 6 h + 1,5 h** |

### 6.3 Tuning-Modi

#### K5 · Tuning-Modi im Vergleich

`Geräte` SPEKTRUM · `Aufbau` 2 · `Board` Messboard · `Bezug` E-19, N1 · `Dauer` 45 min

1. CH1 = 7,1 MHz; Span 200 kHz, RBW 100 Hz, Mittelung 10; nacheinander `Fractional`, `Low noise`, `Integer` (bei `Integer` auf die Frequenz laut `Out` zentrieren, 7,102 273 MHz).
2. Größte Nebenlinien in dBc und Abstand; Phasenrauschen bei 1 kHz Ablage.

| Modus | Nebenlinien (dBc @ Abstand) | L(1 kHz) | Abweichung `Out` (Hz) |
|---|---|---|---|
| Fractional | | | |
| Low noise | | | |
| Integer | | | |

**Soll:** zu dokumentieren; Grundlage für den Standardmodus (6.1). Die Zeile `Fractional` ersetzt F3 b.
**Ergebnis:** i

#### F3 · Nebenlinien um den Träger

`Geräte` SPEKTRUM · `Aufbau` 2 · `Bezug` E-19, N1 · `Dauer` 20 min

1. Einstellungen wie K5, `Fractional`, Korrektur 0.
2. a) 10,000 000 MHz (ganzzahliger Teiler); c) 7,100 000 MHz mit CH3 an (IQ: ganzzahliger Teiler, fraktionale PLL). Fall b (7,1 MHz fraktional) ist die Zeile `Fractional` in K5.
3. Größte Nebenlinien innerhalb ±100 kHz in dBc und Abstand.

| Zustand | Nebenlinien (dBc @ Abstand) | Eigenrauschen-Reserve (dB) |
|---|---|---|
| a) 10 MHz ganzzahlig | | |
| c) 7,1 MHz + CH3 | | |

**Soll:** zu dokumentieren.
**Ergebnis:** i

#### K6 · Abstimmen in `Low noise`

`Geräte` OSZI · `Bezug` A-6, E-19, N1 · `Dauer` 25 min

1. `Tuning` = `Low noise`; OSZI mit Timeout-Trigger auf CLK0.
2. CH1 von 9,990 bis 10,010 MHz in 1-kHz-Schritten (je `+` einmal); außerdem 7,0 bis 7,2 MHz.
3. Aussetzer zählen, Frequenz notieren.

**Soll:** kein Aussetzer, solange der Teiler bleibt (Teiler 90 deckt 6,67 … 10 MHz ab); ein Aussetzer beim Übergang über 10,000 MHz (Teilerwechsel 90 → 88); dokumentieren.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Aussetzer, Dauer):

### 6.4 IQ-Abstimmen und Phase

#### D3 · IQ: Master abstimmen (Realignment)

`Geräte` OSZI · `Aufbau` 3 · `Bezug` A-17, N2 · `Dauer` 15 min

1. CH1 = 10 MHz an, CH3 an (Master CH1, 90°). OSZI: beide Kanäle, Timeout-Trigger auf CLK0, Single.
2. Schrittweite 1 kHz, `+` einmal tippen; Aussetzer beider Ausgänge messen (Dauer). 10 Wiederholungen.

**Soll:** Aussetzer beider Ausgänge bei **jedem** Schritt (erwartet, auch bei einer Korrekturänderung); Dauer notieren.
**Ergebnis:** i · Messwert (Dauer min / max in µs):

#### D5 · PLL-Änderung ohne Reset (nur Testbuild)

`Geräte` OSZI, ZÄHLER, Soundkarte oder SDR · `Bezug` N2, E-19 · `Dauer` 30 min · Build: PLL ohne Reset (zu erstellen)

1. Testbuild flashen; Tuning `Low noise`.
2. CH1 springt von 10,000 000 auf 10,001 000 MHz (NUMPAD und Schritttaste); SDR-Wasserfall oder ZÄHLER mit 1 ms Torzeit.
3. Einschwingzeit und Überschwingen (Hz, ms) notieren; im IQ-Betrieb (CH3 90°) danach die Phase messen (Aufbau 3).
4. Ab S35: auch ein Schritt über 112,5 MHz (dort setzt heute jeder Schritt die PLL zurück).

**Soll:** zu dokumentieren; die IQ-Phase ist nach der Änderung unverändert (±1°); die Einschwingzeit ist so kurz, dass sie bei Versuchen nicht stört.
**Ergebnis:** i · Messwert:

#### E9 · Wiederholgenauigkeit nach Realignment

`Geräte` OSZI · `Aufbau` 3 · `Bezug` A-17, N2, N4 · `Dauer` 15 min

1. CH1 = 10 MHz, CH3 90°. Master 10× um 1 kHz verstellen (`+`, `−` abwechselnd).
2. Nach jedem Schritt (2 s Wartezeit) φ messen; Mittelwert und Standardabweichung.

**Soll:** nach jedem Realignment wieder 90° (Mittelwert ± 1° + u_φ), Streuung < 0,5°.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Mittel, σ):

#### E3 · Ausgangs-Skew bei Phase 0° (Auswertung)

`Geräte` OSZI · `Bezug` A-15, N4 · `Dauer` 10 min

1. Δt bei 0° aus E1 (10 MHz) und E2 (4,73 / 14,2 / 99 MHz) übernehmen; optional 30 und 50 MHz bei 0° nachmessen.
2. Prüfen, ob Δt unabhängig von f ist (konstanter Skew) und φ = Δt · f · 360° die Abweichungen in E1 und E2 erklärt.

**Soll:** konstanter Versatz (± u_t); Phasenfehler ≤ 1° bis 99 MHz, sonst Skew-Kompensation (N4).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Δt je Frequenz):

#### E4 · 180° bei ungeradem Teiler (Auswertung)

`Bezug` A-16, N3 · `Dauer` 5 min

1. Phasenfehler bei 180° und 270° aus E2 (14,2 MHz, Teiler 63; 99 MHz, Teiler 9) gegen E1 (10 MHz, Teiler 90) vergleichen.

**Soll:** Phasenfehler bei ungeradem Teiler höchstens 1° größer als bei geradem; sonst N3.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### E5 · Tastverhältnis

`Geräte` OSZI · `Bezug` A-1, S63 · `Dauer` 15 min

1. Tastverhältnis = positive Impulsbreite / Periode, Schwelle 50 %, ≥ 100 Perioden gemittelt.
2. CLK0 bei 7,1 MHz (fraktional) und 10 MHz (ganzzahlig), CLK2 bei 14,2 MHz (IQ, Teiler 63) und 99 MHz (Teiler 9).

**Soll:** zu dokumentieren; die Doku verspricht 50 %. Abweichung > 2 % → Handbuch und A-1 präzisieren.
**Ergebnis:** i · Messwert (% je Punkt):

### 6.5 Störungen und Pegel

#### F6 · Störungen durch die Plattform

`Geräte` SPEKTRUM · `Board` Messboard (zweites Board optional, Stufe 3) · `Bezug` E-23, N6 · `Dauer` 45 min

1. CH1 = 10 MHz; Span 20 kHz, RBW 10 Hz, Max-Hold 60 s; WLAN aus.
2. **a) Backlight:** 100 / 50 / 10 %; Seitenlinien bei ±5 kHz (PWM) und Vielfachen.
3. **b) Neuzeichnen:** auf der CH1-Seite `+` halten; Spektrum vor und während.
4. **c) Überwachungstakt:** Zero-Span auf dem Träger (RBW 1 kHz, 10 s): Störungen im 2-s-Takt (I2C-Überwachung)?
5. **d) Versorgung:** PC-USB, USB-Netzteil, Powerbank.

| Fall | Nebenlinien (Abstand, dBc) | Periodische Störung? |
|---|---|---|
| 100 % | | |
| 50 % | | |
| 10 % | | |
| `+` gehalten | | |
| 2-s-Takt | | |
| PC / Netzteil / Powerbank | | |

**Soll:** zu dokumentieren; Entscheidung über Abhilfen (6.1).
**Ergebnis:** i

#### F7 · Übersprechen CH1 ↔ CH2

`Geräte` SPEKTRUM · `Bezug` N5 · `Dauer` 20 min

1. CH1 = 10 MHz an; CH2 = 10,1 MHz zuerst aus (MultiSynth läuft weiter), dann an.
2. Spektrum an CLK0: Linie bei 10,1 MHz in dBc.

**Soll:** zu dokumentieren (Übersprechen mit CH2 aus bzw. an).
**Ergebnis:** i · Messwert:

#### F2 · Ausgangspegel an 50 Ω (gekürzt)

`Geräte` SPEKTRUM · `Bezug` S63, N8 · `Dauer` 20 min

1. CH1 an; Pegel der Grundwelle an 50 Ω (mit Dämpfungskorrektur): bei 10 MHz für Drive 2 / 4 / 6 / 8 mA, bei 1 / 30 / 100 MHz für 8 mA. Die volle Tabelle (alle Frequenzen × alle Drives) ist optional.

| f (MHz) | 2 mA (dBm) | 4 mA | 6 mA | 8 mA |
|---|---|---|---|---|
| 1 | – | – | – | |
| 10 | | | | |
| 30 | – | – | – | |
| 100 | – | – | – | |

**Soll:** zu dokumentieren (Pegeltabelle fürs Handbuch; LO-Pegel für Mischer).
**Ergebnis:** i

#### C7 · Aufwärmdrift

`Geräte` ZÄHLER, REF, THERMO · `Bezug` N7, S63 · `Dauer` 60 min (15 min aktiv; Start nach ≥ 2 h Ruhe)

1. Board und Modul mindestens 2 h stromlos, Raumtemperatur notieren.
2. Einschalten, CH1 = 10 MHz (Korrektur nach C2), Torzeit 10 s, Aufzeichnung als `.csv` (`counter_log.py`, Anhang D).
3. 60 min laufen lassen; Raumtemperatur alle 15 min.
4. Drift in ppm über der Zeit auftragen.

**Soll:** zu dokumentieren; Aufwärmzeit fürs Handbuch (Zeit bis Drift < 0,1 ppm/10 min); TCXO-Entscheidung (6.1). C8 lässt sich aus derselben Aufzeichnung auswerten.
**Ergebnis:** i · Messwert (Drift 0–15 min / 15–60 min in ppm):

### 6.6 Bus und Touch

#### J6 · Leitung dauerhaft auf GND

`Geräte` PC (Log), LOGIK optional · `Aufbau` 5 · `Board` Messboard · `Bezug` A-43, E-17, S39 · `Dauer` 15 min · Build: Debug

1. SDA über ≤ 100 Ω nach GND legen, dann SCL.
2. Rückgabecode im Debug-Log lesen; GUI-Bedienung prüfen (Tipps, Frame-Zeit).
3. Fehler entfernen.

**Soll:** Code 5; Dialog „I2C line stays low …“; die GUI stockt < 100 ms; nach Entfernen verbunden. Liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen (E-17). Nach der Migration (S41) wiederholen; der Fake-Test aus S39 bildet die Annahme ab.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Code vor / nach Migration):

#### J7 · Registerdump und Klonwarnung

`Geräte` PC (Log) · `Board` Messboard · `Bezug` A-42, E-17 · `Dauer` 10 min (+ 10 min je Klon) · Build: Debug

1. Register 0, 3, 183 nach dem Einschalten aus dem Log lesen (eigenes Modul; wenn vorhanden auch MS5351M).

**Soll:** Werte notieren; das Modul wird angenommen; Log ohne „Unexpected reg183“ (sonst Klonwarnung und Info-Karte `(clone?)`).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### B12 · CONFIG wischen, Druckaussetzer (U-20, E-22)

`Board` beide · `Bezug` E-22, S33 · `Dauer` 20 min · Build: Touch-Log

1. CONFIG: auf freier Fläche, auf Tasten (`Corr. +` mit Wiederholung) und auf Dropdowns wischen, je leicht und fest, je 10×.
2. Während der Einrast-Animation einmal tippen.
3. `+` auf der CH1-Seite 5 s halten (leicht und fest); Druckaussetzer (`gap`, `drop`) im Touch-Log zählen und ihre Dauer notieren.

**Soll:** 0 ungewollte Auslösungen; Verteilung der Aussetzer dokumentiert (Grundlage für die Überbrückungszeit in S33); keine Aussetzer > 70 ms bei normalem Druck (Überbrückung seit S33 70 ms).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert (Auslösungen, Aussetzer min / max / Anzahl):

### 6.7 Ab WEB-HMI

#### L13 · WLAN-Störungen im Spektrum

`Geräte` SPEKTRUM · `Aufbau` 2 · `Bezug` WLAN-Vorabmessung (S49), F3 · `Dauer` 45 min

1. CH1 = 10 MHz an; Span 200 kHz, RBW 100 Hz, zusätzlich Span 2 kHz, RBW 10 Hz (Beacons im Abstand ≈ 102,4 ms können Seitenlinien im Abstand ≈ 10 Hz erzeugen).
2. WLAN `Off`; `Client` im Leerlauf; `Client` mit Datenverkehr (Ping-Flut, drei Browser mit Live-Updates, SCPI-Skript 10 Befehle/s); `Access point` mit einer Station.
3. Neue Nebenlinien (Abstand, dBc) gegenüber `Off` notieren.

**Soll:** zu dokumentieren; bei Linien > −60 dBc im nutzbaren Bereich Hinweis im Handbuch.
**Ergebnis:** i · Messwert:

#### L15 · WLAN aus entspricht 1.3.x

`Geräte` SPEKTRUM · `Dauer` 20 min

1. WLAN `Off`; F3 a und F6 a wiederholen; mit den R0-Werten vergleichen.

**Soll:** Spektrum wie R0 (Linienpegel ± 3 dB, keine neuen Linien).
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

#### Q1 · Flash, RAM und Heap gegen die Basis

`Geräte` PC · `Bezug` Build-Gate, Heap-Grenzen des WEB-HMI-Konzepts · `Dauer` 20 min · Build: Release und Debug

1. `pio run`: Flash und RAM (Bytes, %) aus der Build-Ausgabe.
2. Debug-Build: freier Heap, minimaler Heap, größter Block, LVGL-Heap nach dem Boot und nach 5 min Bedienung.

| Größe | R0 (Vorwert S05/S06, 2026-11-04) | R1 | Abweichung |
|---|---|---|---|
| Flash (B / %) | 623 045 / 19,8 % | | |
| RAM statisch (B / %) | 90 236 / 27,5 % | | |
| Heap frei nach Boot | 277 412 | | |
| Heap minimal | 271 820 | | |
| größter Block | 110 580 | | |
| LVGL-Heap frei / max. belegt | 38 484 von 49 152 / 4 789 | | |

**Soll:** Abweichung > 10 % begründen; minimaler Heap ≥ 40 KB, größter Block ≥ 16 KB (WEB-HMI-Konzept). Die R0-Vorwerte stammen vom Original-CYD ohne Modul mit dem Stand von S05/S06; beim ersten Stufe-1-Lauf durch Werte von 1.3.x ersetzen, wenn sie abweichen.
**Ergebnis:** ☐ ✔ ☐ ✘ ☐ ○ · Messwert:

---

## 7. Stufe 3 – Optional

Diese Messungen sind **kein Freigabekriterium**. Sie laufen bei Anlass (Befund, Ausbildungsversuch, Rückfrage) oder wenn Zeit ist. Die Einträge sind knapp gehalten; Aufbauten, Methoden und Bewertung wie in Abschnitt 3. Ergebnis je Zeile: ✔ ✘ ○ i –.

### 7.1 Robustheit und Fehlerfälle

| Nr | Prüfung | Geräte, Build | Soll | Dauer |
|---|---|---|---|---|
| A8 | IQ-Fehlerzustand (alt 18b): CH1 10 MHz an, CH3 an (Master CH1), 270°; Status auf MAIN, Header, CH3-Seite; Dialoge zählen; dann 90° | OSZI, Build IQ-Fehler | CH3 `ERR` (Infozeile `IQ setup failed, retrying`), **ein** Dialog mit Kanal und Abhilfe, CH1 läuft ungestört; bei 90° nach ≤ 2 s `ON`, Log `[APP] Output error cleared` (host-getestet: `test_fault_iq`) | 10 min |
| A11 | CLK2 im IQ-Fehlerzustand (C-1), im selben Flash wie A8: `Off state` `High`, `Low`, `High-Z` je 1 min bei `ERR` beobachten | OSZI (DC, 1 MΩ), Build IQ-Fehler | CLK2 schwingt **nicht**, nur der Ruhepegel (High ≈ 3,3 V, Low 0 V, High-Z undefiniert); Register host-getestet | 15 min |
| J5 | Wackelkontakt: CH1 an, 30 s SDA-Kabel wackeln lassen; Log und Zähler | ZÄHLER | Frequenz nach Beruhigung richtig; eine Logzeile je Zustandswechsel | 10 min |
| J8 | Modul ohne (bzw. mit abgetrennten) Pull-ups; booten, 10 min CH1 an | LOGIK | Erkennung bei 100 kHz; Betrieb mit 400 kHz stabil **oder** Log „I2C fails at 400 kHz; staying at 100 kHz“ und Betrieb mit 100 kHz stabil (host-getestet) | 20 min |
| J9 | Dauer ohne Modul: Board ohne Modul, Debug-Build, 10 min; Heap alle 30 s; kann nebenbei während Stufe 1 laufen | Build Debug | Heap konstant (ein `Wire.end()`/`begin()` pro 2 s), keine wiederholten Logzeilen | 15 min |
| J11 | Vertauschte Adern, Aktivität am Chip: LOGIK direkt am Modul, Kabel vertauscht, booten | LOGIK, Aufbau 5 | nur START/STOP und Fragmente, keine Schreibzugriffe auf Register 16 bis 183 (host-getestet) | 15 min |
| J12 | 3,3 V offen: Modul nur mit GND und SDA/SCL booten; danach 3,3 V anschließen | LOGIK, DMM | „not found“, „line stays low“ oder „not an SI5351“, **kein Teilschreiben**; nach Anschluss von 3,3 V verbunden | 10 min |
| J13 | GND offen, sonst wie J12 | LOGIK, DMM | wie J12 | 10 min |
| J14 | Reset während I2C-Zugriff: Debug-Build mit Dauerlesen, 20× EN | LOGIK, Build Debug | nach jedem Boot verbunden (`[I2C] Line held low; releasing bus` erlaubt; host-getestet) | 20 min |
| J15 | Fremdgerät auf 0x60 an CN1, booten | LOGIK, I2C-Modul 0x60 | Dialog „The device at I2C address 0x60 is not an SI5351“; höchstens ein Schreibzugriff (Register 3); keine weiteren Dialoge (host-getestet) | 15 min |

### 7.2 HF-Charakterisierung und Ausbildung

| Nr | Prüfung | Geräte | Soll | Dauer |
|---|---|---|---|---|
| C8 | Kurzzeitstabilität: nach der Aufwärmzeit CH1 10 MHz, Torzeit 1 s, 100 Werte (oder aus C7); Standardabweichung und Allan-Abweichung für τ = 1 s | ZÄHLER, REF | zu dokumentieren | 15 min |
| E8 | Phasendrift: CH1 10 MHz, CH3 90°, 30 min alle 5 min φ, Raumtemperatur | OSZI, THERMO | Drift < 0,5° in 30 min | 35 min |
| E10 | Ausbildungsaufbau: IQ an QSD (FST3253) oder zwei Mischer; Testsignal ±1 kHz um den LO; Seitenbandunterdrückung | SPEKTRUM, QSD | passt zu E1 bis E3 (1° ≈ 41 dB, 2° ≈ 35 dB) | 45 min |
| F1 | Oberwellen: CH1 10 MHz, Drive 2 / 4 / 6 / 8 mA; Grundwelle (dBm) und 2. bis 5. Harmonische (dBc), Span 0 … 60 MHz, RBW 1 kHz | SPEKTRUM | zu dokumentieren; Rechteck mit 50 %: 3. ≈ −9,5 dBc, 5. ≈ −14 dBc, 2. und 4. sehr klein (sonst E5 prüfen) | 30 min |
| F4 | Wechsel ganzzahlig → fraktional: wie F3 a mit Korrektur 0 und +0,01 ppm | SPEKTRUM | Nebenlinien erscheinen; dokumentieren | 15 min |
| F5 | Phasenrauschen: CH1 10 und 14,2 MHz, Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz; Eigenrauschen aus V4 b daneben | SPEKTRUM, REF | zu dokumentieren; < 6 dB über Eigenrauschen = obere Schranke (○) | 45 min |
| F8 | Spektrum CLK2 gegen CLK0 (alt 10): CH3 IQ bei 4,73 und 10 MHz | SPEKTRUM | an CLK2 keine zusätzlichen Nebenlinien | 20 min |
| F9 | Nahfeld: Ausgänge aus, WLAN aus; Sonde ≈ 1 cm über Display, ESP32-Modul, Ausgangsleitungen, Span 1–500 MHz; danach mit CH1 10 MHz (Mischprodukte?) | SPEKTRUM, SONDE | zu dokumentieren (40-MHz-Quarz, SPI 40 MHz, 25-MHz-Quarz des SI5351); ergänzt N6 und N10 | 45 min |
| F6 (Board 2) | F6 am zweiten Board (Modul umstecken) | SPEKTRUM | wie F6 am Messboard | 45 min |
| G1 | Diodenringmischer: `Off state` `Low`; LO = CH1 10 MHz auf +7 dBm (nach F2), RF = CH2 10,455 MHz über 30 dB; ZF bei 455 kHz und 20,455 MHz; Konversionsverlust, LO-Durchschlag | SPEKTRUM, MISCHER, Aufbau 4 | ZF bei 455 kHz und 20,455 MHz, Konversionsverlust ≈ 6–7 dB; LO-Durchschlag dokumentieren (N8) | 45 min |
| G2 | Schwebung an der Soundkarte: CH1 10,000 MHz und CH2 10,001 MHz an den Mischer, ZF an Line-In, FFT ≥ 96 kHz | MISCHER, Soundkarte | sauberer 1-kHz-Ton; Seitenlinien und Rauschsockel dokumentieren (N8) | 30 min |
| H4 | Oberwellen bei 455 kHz (ab S35): Spektrum 0 … 5 MHz | SPEKTRUM | zu dokumentieren; Tiefpass-Hinweis im Handbuch | 10 min |
| K3 | Spread −0,5 % bei 10 MHz: Span 100 kHz um 9,975 MHz, RBW 1 kHz, Max-Hold; CH2 7,1 MHz am Zähler; Modulationsrate (Zero-Span an der Bandkante oder SDR-FM); `Out`-Zeile | SPEKTRUM, ZÄHLER | Band 9,95 … 10,00 MHz; Modulation ≈ 31,5 kHz; CH2 unverändert (±0,5 Hz + u); `Out 9.975 000 MHz mean, SSC -0.5 %` (Register host-getestet) | 30 min |
| K4 | Spread mit IQ: `+/-0.5 %`, CH3 90°; Δt mit Spread aus und ein; Spektren beider Ausgänge | OSZI, SPEKTRUM | Phase bleibt (≤ 1° + u_φ); beide Ausgänge gleich breit | 30 min |

### 7.3 Bedienung

Für die Tests mit Personen gelten die Bedingungen aus 2.4; Auswertung des Touch-Logs mit `touchstat.py` (Anhang D).

| Nr | Prüfung | Personen, Build | Soll | Dauer |
|---|---|---|---|---|
| B4 | Numpad schließen mit Finger-Nachhalten (alt 13a): „5“ `kHz` bestätigen und Finger 0,5 s liegen lassen; dann „100“ `Hz` | Bediener | Kanalseite erscheint; das Numpad öffnet **nicht** erneut; Schrittweite unverändert | 5 min |
| B9 | Trefferquote echter Ziele (U-4, U-6): P1 bis P5 je 20 Tipps auf Dialog-`OK`, `MAIN`, Phase `90`/`180`, Chips `1`/`1M`, `Corr. −`/`+`, CONFIG-Listeneintrag, Numpad `◀`/`C` | 5 Personen, Touch-Log | ≤ 2 % Fehltipps; **0** Nachbartreffer bei Phase und Schaltern | 45 min |
| B10 | Karte öffnen ohne Schalten (U-3): CH1 OFF, 20× Karte über Frequenz bzw. `MHz` öffnen | P1, P2 | 0 ungewollte Schaltvorgänge | 15 min |
| B13 | Halten über 5 s (U-20, enthält B2): Step 1 Hz, `+` 5 s halten (leicht und fest); Schritte zählen, erster Schritt und Takt aus der Frequenzänderung bzw. Video | Bediener, Touch-Log | heute erster Schritt nach ≈ 0,4 s, dann alle ≈ 0,1 s, ≈ 47 Schritte, kein Flackern, keine Unterbrechung; nach S33 die dort festgelegten Werte | 10 min |
| B14 | Lesbarkeit aus der Entfernung (U-8): ≈ 500 lx; MAIN mit 10 Zustandskombinationen; 0,4 / 0,7 / 1,0 / 1,5 m, 0° / 30° / 45°, Backlight 30 / 80 / 100 % | P1 bis P5, LUX, VIDEO | ≥ 95 % bei 1,0 m für ON/OFF und Frequenz; Grenzentfernung je Element dokumentieren | 60 min |
| B15 | Farbsehschwäche (U-7, U-19): Testperson mit Rot-Grün-Schwäche oder Fotos aus B6 mit Simulationsfilter | 1 Person oder Fotos | alle Zustände auch ohne Farbe unterscheidbar (Kontraste host-getestet: `test_contrast`) | 10 min |
| B16 | Aufgabentest (Anhang C): T1 bis T7 erst ohne, dann mit Handbuch; laut denken; SUS und Verständnisfragen | 5 Lernende + Lehrende, VIDEO | Erfolg ohne Handbuch ≥ 80 % (T1 bis T3), SUS ≥ 70; Verständnisfragen ≥ 80 % richtig | 5 × 30 min |
| B17 | Berührung beim Einschalten (U-10, enthält I5): 20× einschalten mit Daumen auf dem Display (0,5 bis 3 s, einmal > 3 s), einmal 1 s berühren und halten, bis die Hauptseite erscheint | Bediener, VIDEO | 20/20 richtiges Bild; kein Tipp landet in der GUI (seit 1.3.2 keine Boot-Geste, E-26) | 15 min |
| B18 | Fehlerdialoge mit Testperson: A1 und A3 ausführen; Person liest vor und erklärt die Abhilfe | 1–2 Personen | Meldung verstanden; `OK` im ersten Versuch (U-21) | 20 min |
| B19 | Eingabestift (U-5): B8 und B9 mit dem Stift (P1 bis P3) | 3 Personen, Trefferkarte | zu dokumentieren; Entscheidung „Stift am Laborplatz“ (6.1) | 30 min |
| B20 | Spannungsausfall bemerken (U-22): CH1 an, Stecker unangekündigt ziehen; Zeit bis zum Bemerken; nach dem Anstecken: weiß die Person, dass sie einschalten muss? Änderung < 5 s vor dem Ziehen | 1–2 Personen | Ausfall innerhalb 30 s bemerkt; Verlust < 5 s alter Änderungen steht im Handbuch (Autosave host-getestet) | 20 min |
| K10 | CONFIG scrollen (U-24): Einrasten, Scrollposition nach MAIN → CONFIG; eine Person, die CONFIG nicht kennt, sucht `Off state` ohne Hinweis | 1 Person, VIDEO | rastet an Zeilenkanten ein; Wischen löst nichts aus (B12); Scrollposition erhalten; Person erkennt ohne Hinweis, dass die Seite scrollt | 20 min |
| K12 | Backlight (ersetzt B1): `−` bis 10 %, `+` bis 100 %; Halten wiederholt nicht; Lesbarkeit bei 10 % (≈ 500 lx); Wert nach Neustart | Bediener, LUX optional | Stufen 10 … 100 % (host-getestet); bei 10 % noch lesbar; Halten wiederholt nicht; Wert bleibt | 15 min |

### 7.4 Qualität

| Nr | Prüfung | Geräte | Soll | Dauer |
|---|---|---|---|---|
| Q2 | Stromaufnahme, Mittel über 10 s: (a) Boot, Ausgänge aus, Backlight 100 %; (b) Backlight 10 %; (c) CH1 bis CH3 an (10 MHz, 8 mA, 50 Ω); (d) WLAN `Client` im Leerlauf; (e) `Access point` ohne Station; (f) 3 Web-Clients; (g) Spitzenwerte | STROM | zu dokumentieren; Hinweis für Netzteil oder Powerbank | 30 min |
| Q3 | Bootzeit: Zeit bis zum ersten Bild, zur Hauptseite und zum Verbinden des Moduls, aus den Videos von I3 | VIDEO | zu dokumentieren; R1 höchstens 1 s länger als R0 | 10 min |

### 7.5 WEB-HMI (optional, erst wenn die WEB-HMI existiert)

Was hier steht, ist großteils durch die Host-Suites der WEB-HMI abgedeckt oder Komfort. Aufbau 7; nur im eigenen Labornetz.

| Nr | Prüfung | Bezug | Soll | Dauer |
|---|---|---|---|---|
| L2 | Zwei WLANs speichern (A stärker); A ab, dann A ein und B ab; RSSI und gewähltes Netz | WiFiMulti | das stärkere Netz gewinnt; bei Misserfolg Wechsel | 30 min |
| L4 | Statische IP: gültige Werte; ungültige (IP = Gateway, Broadcast, Netzadresse, außerhalb des Subnetzes) | W-S12, `test_net_settings` | gültige übernommen, ungültige mit Klartextmeldung abgelehnt | 20 min |
| L5 | mDNS und Captive-DNS: `ping <name>.local` und `http://<name>.local` auf Windows, Android, iOS; im AP-Modus öffnet sich die Einrichtungsseite; im Client-Modus `nslookup example.com <Geräte-IP>` ohne Antwort | W-E6, W-S4 | Name löst auf; Captive-DNS nur im AP-Modus | 45 min |
| L7 | Zweite SCPI-Verbindung (`nc <IP> 5025`) wird abgelehnt (Info-Karte `connected` / `rejected <IP>`); Leerlauf-Trennung nach 10 min; `Disconnect all` | W-S20 | wie angegeben | 30 min |
| L8 | Web auf drei Clients: Änderungen live; Feld mit Fokus wird nicht überschrieben; vierter Client „Too many clients (max. 3)“; Netz trennen → Seite grau | – | wie angegeben | 30 min |
| L9 | Tastaturbedienung im Browser: `↑`/`↓`, `←`/`→`, `Bild↑`/`Bild↓`, Mausrad, `1`/`2`/`3`, `F`/`Enter`/`Esc`, `O`, `?`; `Space` | WEB-HMI-Konzept | Belegung wie im Konzept; `Space` schaltet nichts | 20 min |
| L10 | Zweistufige Netzänderung: ohne Bestätigung Rückfall nach 120 s; mit Bestätigung übernommen; ohne Web-Passwort zusätzlich Touch-Bestätigung | W-E9, W-S12 | Rückfall nach 120 s (± 5 s) | 20 min |
| L16 | Einrichtung durch Novizen: W1 bis W3 (Anhang C), mit und ohne QR-Hilfe; war „mobile Daten aus“ nötig? | 5 Personen | ≥ 80 % Erfolg ohne Hilfe, ≤ 5 min je Aufgabe | 5 × 15 min |
| L17 | 16-stelliges Passwort mit der Bildschirmtastatur (Finger und Stift) | 5 Personen | Abbruchquote < 10 %, mittlere Zeit < 3 min (Vorschlag) | 5 × 10 min |
| L18 | Fremde Änderungen bemerken (Hervorhebung, `REMOTE <Quelle>`) | 1 Person | ≥ 90 % bemerkt | 20 min |
| L19 | T1 bis T3 (Anhang C) im Browser nur mit der Tastatur | 1 Person | ≤ 1 Fehlbedienung je Aufgabe | 30 min |
| L20 | `Identify` (bzw. `SYSTem:IDENtify`) mit ≥ 3 Geräten im Raum | W-E28 | richtiges Gerät in ≥ 95 % | 20 min |
| L21 | Großansicht am Beamer aus 5 m | – | Frequenzen und Zustände lesbar | 15 min |
| L22 | SCPI-Befehlsbaum vollständig: `scpi_selftest.py` mit gültigen und ungültigen Parametern, `SYSTem:ERRor?` nach jedem Befehl, Stichproben am ZÄHLER | W-S17, W-S19, `test_scpi` | alle Erwartungen erfüllt; keine Abstürze; Frequenzen nur als Ganzzahlen | 60 min |
| L23 | REST: `GET /api/state` gegen die Anzeige; alle Schlüssel in `POST /api/config` und `/api/channel/<n>`; unbekannte Schlüssel und falsche Typen (`{"drive":"4"}`, `{"drive":4.5}`) | `test_rest_json` | Zustand stimmt; Fehler 400 mit `{"error":"-2xx,…"}` | 30 min |
| L24 | `*RST` und Speichern: `*RST` → Ausgänge aus, Standardwerte; stromlos → alte Werte zurück; `*RST` + `SYSTem:SAVE` + Neustart → Standardwerte | W-E19, C-17, `test_command` | `*RST` speichert nicht; nach `SYSTem:SAVE` gelten die Standardwerte | 15 min |
| L-S12 | Gerätename, mDNS, IPv6 (W-S4, W-S16): ungültige Namen und SSIDs abgelehnt; TXT-Einträge ohne Version und Commit; `Host: <name>.local` und `<IP>` akzeptiert, anderer Host 421; `curl -6` ohne Verbindung | `dns-sd` / `avahi-browse` | wie W-S16 und W-S4 | 20 min |
| L-S13 | WPA2 und PMF (W-S15): Router-Log bzw. Wireshark; RSN-Element der AP-Beacons; Router nur mit WPA/TKIP | Wireshark optional | WPA2 mindestens; PMF capable; WPA/TKIP abgelehnt | 30 min |

---

## 8. Entfallen und erledigt

### 8.1 Entfallen

Die Nummern bleiben reserviert, damit Verweise auflösbar bleiben.

| Nr | Titel | Grund |
|---|---|---|
| A6 | Start ohne Rauschen | in I3 aufgegangen (Kaltstarts mit Blick auf Beleuchtung und erstes Bild) |
| A7 | Flashen beider Boards | jeder Smoke-Test flasht beide Boards mit derselben Firmware; CI baut die Umgebungen; die Erkennung prüft I3 |
| A9 | Pegel im Zustand OFF, Werkseinstellung | in K1 aufgegangen (K1 misst `Low` jetzt an CLK0 bis CLK2) |
| B2 | Wiederholung beim Halten | in B13 aufgegangen (B13 misst ersten Schritt und Takt mit) |
| B3 | Numpad: ungültige und gültige Eingaben | Logik host-getestet (`test_input_logic`: Grenzen, Fehlertexte); Anzeige in B6 Schritt 2 |
| B5 | Ziffer antippen | host-getestet (`charIndexAt`, Tap-Ziel, `stepMarkup`); Treffsicherheit zeigt B8 |
| B11 | Trefferquote Ziffer-Antippen | entschieden in S26 (Ziffer-Antippen ist Komfortfunktion); Zuordnung host-getestet, Touch-Genauigkeit in B8 |
| C4 | Korrektur +50 ppm bei kleinen Frequenzen | Korrekturrechnung und VCO-Grenzen host-getestet (S14, C-7); am Gerät nichts Neues gegenüber C2, C3 |
| C5 | Korrektur um 1 ppm ändern | in D2 Schritt 3 aufgegangen; kein PLL-Reset host-getestet (`test_correction_change_does_not_reset_pll`) |
| C6 | Hz-Schritte bei hoher Frequenz | 1-Hz-Raster bis 100 MHz host-getestet (`test_standalone_accuracy_*`); C3 misst 99,999 999 MHz |
| D4 | IQ: Korrektur ändern | derselbe Realignment-Mechanismus wie D3 (host-getestet); D3 reicht für N2 |
| E6 | CH3 wartet unter 4,730 MHz | host-getestet (`test_ch3_below_iq_range_waits_and_resumes`); WAIT-Anzeige in B6 |
| E7 | Korrektur −1 ppm bei 100 MHz mit IQ | host-getestet (`test_iq_vco_stays_in_range_at_iq_limits`, VCO-Grenzen mit Korrektur seit S14) |
| H3 | CH3 über 100 MHz | Testfall `FreqTooHigh` mit Master > 100 MHz kommt mit S35 als Host-Test |
| I4 | Gespeicherte Wahl der alten Boot-Geste wird gelöscht | host-getestet (`test_stored_display_choice_is_removed_once`); beide Boards zeigen seit 1.3.2 `detected` (Smoke-Test S30); 1.3.1 dafür nachzuflashen lohnt nicht |
| I5 | Lange Berührung beim Einschalten | in B17 aufgegangen |
| I6 | Kurze Berührung beim Einschalten | Boot-Geste entfernt (S27, E-26); es gibt nichts mehr umzuschalten |
| I8 | A, B und F6 mit der Einheitsfirmware | Doppelung: die board-abhängigen Prüfungen (I3, B6, B7, B8, optional F6) laufen ohnehin auf beiden Boards |
| J3 | Anstecken nach dem Boot (gerade und vertauscht) | in A2 aufgegangen; host-getestet (`test_hot_plug_straight_then_swapped`) |
| J4 | Im Betrieb abziehen und vertauscht wieder anstecken | host-getestet (`test_rewiring_while_running_restores_iq`); am Gerät decken A3 (Abziehen), A4 (IQ nach Chip-Neustart) und J2 (vertauscht) das Verhalten ab |
| J10 | CN1-Belegung auf dem CYD2USB | durch Betrieb belegt: das CYD2USB verbindet sich über CN1 (Smoke-Tests 1.3.1 und 1.3.2); J1 und J2 laufen auf ihm |
| K11 | `Integer` sperrt Spread | host-getestet (`test_integer_mode_turns_spread_off`); gesperrte Zeile in B6 sichtbar |
| K13 | „~“ nach dem Neustart (C-2) | host-getestet (Bootreihenfolge S08, `test_actual_frequency_known_only_after_stored_settings`) |
| K14 | Unnötige Flash-Schreibvorgänge (C-5) | host-getestet (`test_unchanged_settings_do_not_write_flash`, S11) |
| L11 | Speicherfristen je Quelle | wird mit S45 host-getestet (`test_command`: Fristen 5 s / 30 s, ON/OFF speichert nicht), analog K14 |

### 8.2 Erledigt

I0 bis I2 sind am 2026-10-28 gelaufen (Messwerte in Spezifikation E-18). Nach dem Flashen der Einheitsfirmware: 5/5 EN-Resets je Board richtig erkannt (Original-CYD: ILI9341, ID 000000; CYD2USB: ST7789, ID 8181B3). Die Ports waren damals vertauscht (COM13 Original-CYD, COM7 CYD2USB); seit 2026-11-04 gilt die Zuordnung aus 1.4.

| Nr | Test | Ergebnis |
|---|---|---|
| I0 | Diagnose-Sketch: MADCTL-Rücklesen, RDDID, Pull an GPIO12, SPI-Modus 0/3, Schreiben mit 40 MHz | ✔ erledigt 2026-10-28, siehe Spezifikation E-18 |
| I1 | Original-CYD einschalten: Log „ILI9341 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B8 |
| I2 | CYD2USB einschalten: Log „ST7789 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe B8 |

Q4 und Q5 sind ebenfalls am Gerät geprüft (2026-11-04, S09); sie stehen als bedingte Messungen in Stufe 1 (5.9), weil sie nach der Migration einmal wiederholt werden.

---

## 9. Auswertung und Freigabe

### 9.1 Zusammenfassung

| Stufe | Messungen | ✔ | ✘ | ○ | i | – | Befunde |
|---|---|---|---|---|---|---|---|
| 1 | J1, J2, A1 bis A5, A10, C1 bis C3, K7, D1, D2, E1, E2, K1, K2, K8, K9, I3, I7, B6 bis B8; bedingt H1, H2, Q4, Q5 | | | | | | |
| 1W | L1, L3, L6, L12, L14, L-S1 bis L-S11 | | | | | | |
| 2 | K5, F3, K6, D3, D5, E9, E3, E4, E5, F6, F7, F2, C7, J6, J7, B12; L13, L15, Q1 | | | | | | |
| 3 | nach Lauf | | | | | | |

### 9.2 Vergleich R0 gegen R1 (vor und nach der Migration)

Gleiches Messboard, gleiche Geräte; die Drift des Quarzes über die Klammermessung herausrechnen. Abweichungen über den Grenzen sind Befunde (3.6).

| Größe | Messung | Zulässige Abweichung R1 gegen R0 |
|---|---|---|
| Frequenzgenauigkeit | C1, C3 | ≤ 0,5 Hz (Klammermessung) |
| IQ-Phasen | E1, E2 | ≤ 1° (+ u_φ) |
| Aussetzer beim Abstimmen | D1, D2 (K6, D3 falls gemessen) | keine zusätzlichen |
| Pegel im Zustand OFF | K1, K2 | ± 0,1 V |
| Erkennung und Verbindung | I3, J1, J2, A2 | 100 % wie R0 |
| Touch | B7, B8 | gleich oder besser |
| Nebenlinien, Störungen (falls Stufe 2 auf R0 lief) | F3, K5, F6 | ± 3 dB; keine neuen Linien über −70 dBc |
| Boot und Heap | Q1, Q3 | ± 10 % (Q1), ≤ +1 s (Q3) |

### 9.3 Entscheidungsblatt

Die Tabelle aus 6.1 wird mit den Messwerten ausgefüllt; jede Entscheidung kommt als E-n in die Spezifikation und als bestätigter oder gestrichener Folgeschritt in den [ARBEITSPLAN.md](ARBEITSPLAN.md):

| Entscheidung | Messwerte | Ergebnis | Folgeschritt / E-n |
|---|---|---|---|
| Standard-Tuning | | | N1 |
| IQ ohne Reset | | | N2 |
| ungerade IQ-Teiler | | | N3 |
| Phasen-Feinabgleich | | | N4 |
| `High-Z` über PDN, MultiSynth aus | | | N5 |
| Störquellen-Abhilfen | | | N6 |
| TCXO | | | N7 |
| Tastverhältnis, Pegeltabelle, Aufwärmzeit, Neustart-Hinweis | | | S63, N8 |
| IDF-Rückgabecode, Klon-Module | | | S39, E-17 |
| Druckaussetzer | | | S33 |
| Stift am Laborplatz | | | Handbuch |
| WLAN bei Messungen | | | S58 |
| Heap-Grenzen | | | S57 |

### 9.4 Freigabekriterien

**Release 1.3.x und jede Weitergabe:**

1. Smoke-Test (4.3) auf beiden Boards bestanden,
2. die nach 5.10 betroffenen Blöcke von Stufe 1 mit ✔ (beim ersten Release nach dem Eintreffen der Messgeräte: Stufe 1 vollständig),
3. keine offenen P0/P1-Befunde.

**Release 1.4.0:**

1. Stufe 1 auf R1 vollständig mit ✔ (C3, E1, E2 mit ✔ oder begründetem ○), dazu Q4 und Q5 nach der Migration,
2. Stufe 1W mit ✔; ein ✘ bei L-S stoppt die Freigabe; L14 ohne Neustart und mit stabilem Heap,
3. Vergleich R0/R1 (9.2) innerhalb der Grenzen,
4. Stufe-2-Entscheidungen getroffen oder bewusst vertagt (Eintrag im Arbeitsplan),
5. alle Befunde priorisiert im [ARBEITSPLAN.md](ARBEITSPLAN.md), keine offenen P0/P1,
6. Handbuch DE und EN passen zu den Messwerten (K2-Hinweis, Pegeltabelle, Tastverhältnis, Aufwärmzeit, Neustart-Hinweis, WLAN-Hinweis, soweit gemessen).

Stufe 3 ist kein Freigabekriterium.

### 9.5 Wiederholungen

Nach Korrekturen nur die betroffenen Messungen bzw. Blöcke (5.10) wiederholen, dazu der Smoke-Test. Befunde werden mit dem Datum des Wiederholungstests geschlossen.

---

## Anhang A · Protokollkopf (je Lauf ausfüllen)

| Feld | Wert |
|---|---|
| Datum, Prüfer | |
| Lauf (R0 / R1 / S), Stufe(n) | |
| Firmware: Version, Branch, Commit, Build-Zeit (Info-Karte, Foto) | |
| SHA-256 der Binärdatei | |
| Board (A / B), Port, MAC, Controller | |
| SI5351-Modul (Typ, Bezugsquelle, Klon?), Quarz | |
| Touch-Abgleich (Werte von der Info-Karte bzw. aus dem Log) | |
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

„Host“ heißt: durch Host-Tests abgedeckt, am Gerät nicht gemessen (Nummern in Klammern = entfallene Messungen).

| Anforderung | Gerät: Stufe 1 | Stufe 2 / 3 | Host |
|---|---|---|---|
| A-1 | C3, H1 | E5 (2), F1 (3) | Genauigkeit (C6) |
| A-2 | B6 | – | `test_input_logic` (B3) |
| A-3 | K1 | – | – |
| A-4 | A5 | B20 (3) | Autosave |
| A-5 | – | F2 (2), F1 (3) | – |
| A-6 | D2, K7 | D5, K5, K6 (2) | Teilerplanung |
| A-7 | – | K3, K4 (3) | Spread (K11) |
| A-8, A-9 | K1, K2 | A11 (3) | Off state |
| A-10 | D1, D2 | – | kein PLL-Reset |
| A-11 | C1, C2, D2 | – | Korrektur (C4, C5) |
| A-12, A-13 | C3, K7 | – | Genauigkeit (C4, C6) |
| A-14 | K7 | – | (K11, K13) |
| A-15 bis A-17 | E1, E2 | D3, E3, E4, E9 (2), E8 (3) | Phasenregister, Realignment (D4, E7) |
| A-18 | A4 | – | (E6, H3) |
| A-19 | – | A8, A11 (3) | `test_fault_iq` |
| A-20 bis A-22 | A5, K9 | – | Speichern (K14) |
| A-23 | – | – | Host |
| A-24, A-25 | B6, K8 | K10 (3) | – |
| A-26 | B8 | B9, B14 (3) | – |
| A-27 | B6 | – | – |
| A-28, A-29 | B6 | B10 (3) | – |
| A-30 | – | B12 (2), B4, B13 (3) | `test_input_logic` (B2, B5, B11) |
| A-31 | K8, K9 | K12 (3) | – |
| A-32 | B7, B8 | K10 (3) | `test_touch_cal`, `test_touch_filter` |
| A-33 | I3 | – | (A6) |
| A-34 bis A-38 | A1 bis A4, A10 | A8, B18 (3) | Wiederverbinden |
| A-39 | A5 (Logzeilen) | – | (K14) |
| A-40 | – | J-Gruppe (3) | Host |
| A-41 bis A-43 | J1, J2, A2 | J6, J7 (2), J5, J8, J11 bis J15 (3) | Adern, Fremdgerät, Leitung low (J3, J4) |
| A-44 | I3 | B17 (3) | `test_display_detect` (I4 bis I6) |
| A-45 bis A-49 | K8 | – | Build, CI (A7) |
| E-17 | J1, J2 | J6, J7 (2) | Identitätsprüfung |
| E-18 | I3, I7 (I0 bis I2 erledigt) | – | `test_display_detect` (I8) |
| E-19, E-20 | K7 | K5, K6, F3 (2), K3, K4, F4, F5 (3) | Teilerplanung, Spread |
| E-21 | K1, K2 | A11 (3) | Off state |
| E-22 | – | B12 (2), K10 (3) | – |
| E-23 | – | F6 (2), K12 (3) | Backlight-Stufen |
| E-24 | K8 | – | – |

### B.2 Sicherheitsanforderungen der WEB-HMI

| Anforderung | Messung (Stufe) |
|---|---|
| W-S1, W-S2 | L3, L-S1 (1W) |
| W-S3 | L-S6 (1W) |
| W-S4 | L-S1, L-S2 (1W), L-S12 (3) |
| W-S5, W-S6 | L-S1, L-S2, L-S10 (1W) |
| W-S7 | L-S9 (1W) |
| W-S8, W-S9 | Q4 (1), L-S4 (1W) |
| W-S10, W-S11 | L-S10 (1W), L-S5 (1W) |
| W-S12 | L4, L10 (3), `test_net_settings` |
| W-S13 | L-S11 (1W) |
| W-S14 | L-S7 (1W) |
| W-S15 | L-S13 (3) |
| W-S16 | L-S12 (3) |
| W-S17, W-S18 | L-S3 (1W), L22 (3) |
| W-S19 | L22, L23 (3) |
| W-S20 | L7 (3) |
| W-S21 | L-S8 (1W) |
| W-S22 | L6 (1W) |
| W-S23 | L14 (1W), Q5 (1) |
| W-S24, W-S25 | Eintrittskriterien EK-4, EK-5 |

---

## Anhang C · Aufgaben und SUS-Fragebogen (Stufe 3)

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

- `counter_log.py` (C7, C8): liest den ZÄHLER per SCPI/USB im Takt der Torzeit und schreibt `Zeit;Frequenz;Temperatur` in eine CSV.
- `touchstat.py` (B8, B9, B12, B13): wertet Touch-Log und `[HITMAP]`-Zeilen aus: Abweichung je Zielbereich, Fehltippquote, Aussetzer.
- `lsec_check.py` (L-S1, L-S9, L-S10, L-S11): führt die curl-Fälle der vier Tests nacheinander aus, vergleicht Statuscodes und Header mit der Erwartung und prüft nach jedem Fall den Kanalzustand über `GET /api/state`.
- `scpi_selftest.py` (L22): Tabelle des SCPI-Befehlsbaums (WEB-HMI-Konzept im Arbeitsplan) mit gültigen und ungültigen Parametern, `SYST:ERR?` nach jedem Befehl, Soll-Fehlercode je Zeile.
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
| Verweis (Arbeitsplan: Befund C/S/U, Schritt S-n/N-n; Spezifikation A/E) | |
| Status, Datum Wiederholungstest | |
