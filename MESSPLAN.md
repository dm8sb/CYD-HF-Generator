# Messplan – CYD HF Generator

Stand: 2026-11-23 · Plan-Version 4.0 (einheitliche Mess-IDs) · Prüflinge: Firmware 1.3.2 ff. und Release-Kandidat 1.4.0 (WEB-HMI)

Dieses Dokument beschreibt die Messungen und Bedientests am Gerät mit Einzelschritten, Messgeräten, Sollwerten und Bewertung. Es ist nach Priorität in Stufen geordnet: zuerst die notwendigen manuellen Messungen (Stufe 1), dann die Messungen für offene Entscheidungen (Stufe 2), zuletzt alles Optionale (Stufe 3). Messungen, die nicht mehr nötig sind, stehen mit Grund in Abschnitt 8.

**Mess-IDs:** `M-<Stufe><nn>`, die Hunderterstelle folgt der Stufe: **M-0nn** Vorbereitung (4.1), **M-1nn** Stufe 1 (Pflicht), **M-2nn** Stufe 1W (Pflicht ab WEB-HMI), **M-3nn** Stufe 2 (Entscheidungen), **M-4nn** Stufe 3 (optional), **M-5nn** entfallen oder erledigt (Abschnitt 8; die Nummern bleiben reserviert, damit Verweise auflösbar bleiben). Die Nummern sind stabil und werden nicht neu vergeben. Befunde aus Messungen heißen **MB-n** (3.6), Eintrittskriterien EK-n (4.2), Aufgaben der Bedientests T1 … T7 und W1 … W3, Personen P1 … P8 (Anhang C).

Quellen der Sollwerte: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md) (Anforderungen A-nnn, Entscheidungen E-nnn, nach Kapiteln nummeriert; das frühere WEB-HMI-Konzept steht dort als Entscheidungen E-5nn/E-6nn und Sicherheitsanforderungen A-7nn), [USER_MANUAL_DE.md](USER_MANUAL_DE.md), [ARBEITSPLAN.md](ARBEITSPLAN.md) (offene Befunde und Aufgabenliste mit Bereichs-IDs FW-nn, DOC-nn, REL-nn, MES-nn, USR-nn; Audit-IDs C, S, U, P, T und Schrittnamen wie S35 oder AF39 sind historische Namen aus der Git-Historie). Bei einem Widerspruch zwischen diesem Plan und dem Handbuch der geprüften Firmware gilt das Handbuch; der Plan wird dann nachgezogen.

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
| **2 Entscheidung** | Messungen, deren Ergebnis eine offene Entscheidung trägt | vor dem Folgeschritt (FW-01 bis FW-06, DOK-01, DOK-02, S33, S39); am besten im selben Zeitraum wie der erste Stufe-1-Lauf | 16 + 3 ab WEB-HMI | ≈ 6 h + 1,5 h ab WEB-HMI |
| **3 Optional** | Charakterisierung, Ausbildungsversuche, Bedientests mit Personen, seltene Fehlerfälle, WEB-HMI-Komfort | nach Bedarf, kein Freigabekriterium | 56 | ≈ 25,5 h + Personen |
| Entfallen | Host-Test, Doppelung, Funktion entfernt | – | 25 | – |
| Erledigt | M-526 bis M-528 (2026-10-28) | – | 3 | – |

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

**Boards** (nach Spezifikation 1.2; beide laufen mit derselben Firmware):

| Code | Board | Erkennung | Controller |
|---|---|---|---|
| Board A | Original-CYD | nur Micro-USB | ILI9341 |
| Board B | CYD2USB | zusätzlich USB-C | ST7789 |

Stand 2026-11-05 (Smoke-Tests 1.3.1 bis 1.3.3): **COM7** = Original-CYD (ILI9341) **ohne** SI5351, **COM13** = CYD2USB (ST7789) **mit** SI5351. M-001 bestätigt die Zuordnung vor jedem Lauf. Folgen:

- Alle HF-Messungen laufen auf dem Board mit Modul („**Messboard**“, derzeit das CYD2USB). Das Modul bestimmt die HF-Werte, das Board nur Störungen (M-310, M-418).
- Display, Touch und Boot ohne Modul brauchen kein Modul: M-122, M-123, M-125, M-126 laufen auf beiden Boards ohne Umstecken, M-103 auf dem Board ohne Modul. Umstecken ist nur noch für die optionale Wiederholung von M-310 am zweiten Board nötig.

---

## 2. Messgeräte und Material

Spalte „Vorhanden“ vor Beginn ausfüllen (Modell, Seriennummer, Kalibrierdatum). **Stufe 1 braucht nur ZÄHLER, REF, OSZI, DMM und einen PC**; der SPEKTRUM kommt erst in Stufe 2.

### 2.1 Messgeräte

| Code | Gerät | Mindestanforderung | Wofür (Stufe) | Vorhanden | Ohne das Gerät |
|---|---|---|---|---|---|
| **ZÄHLER** | Frequenzzähler | Eingang ≥ 200 MHz, 50 Ω, Auflösung ≤ 0,1 Hz bei 100 MHz in 1 s Torzeit, Eingang für externe 10-MHz-Referenz | 1: M-107 bis M-109, M-110, M-114 · 2: M-313, M-305 · 3: M-411, M-421 | Ja | M-107 bis M-109 nur mit OSZI-Frequenzmessung (≈ 1 ppm), M-108 und M-109 dann ○ |
| **REF** | 10-MHz-Referenz | GPS-gestützt oder Rubidium; für relative Prüfungen genügt ≤ 0,1 ppm (3.3); Verteiler für ZÄHLER und SPEKTRUM | 1: M-107 bis M-109 · 2: M-313, M-302, M-310, M-311, M-312 | Ja, GPS-TCXO 10 MHz | absolute Genauigkeit unbekannt; M-109 gilt nur relativ |
| **OSZI** | Oszilloskop | ≥ 2 Kanäle, ≥ 500 MHz, ≥ 2 GS/s, Persistenz, Single-Trigger, **Timeout-/Dropout-Trigger**, Δt-Messung mit Mittelung, 50-Ω-Eingang | 1: M-106, M-119, M-113, M-114, M-115, M-116, M-117, M-118 · 2: M-304, M-305, M-307 bis M-309, M-306, M-303 · 3: M-402, M-412, M-423 | Ja, R&S RTO 2 GHz | Stufe 1 nicht möglich |
| **SPEKTRUM** | Spektrumanalysator | bis ≥ 1 GHz, RBW ≤ 10 Hz, Rauschmarker (dBc/Hz), 50 Ω, Dauerpegel ≥ +20 dBm, Referenzeingang | 2: M-312, M-302, M-310, M-311, M-301, M-317, M-318 · 3: M-414, M-415, M-416, M-417, M-418, M-419, M-420, M-421, M-422, M-423 | Ja, Siglent 3,2 GHz | Stufe 2 nur teilweise (SDR für Nebenlinien, kein Phasenrauschen) |
| **LOGIK** | Logikanalysator | ≥ 8 Kanäle, ≥ 24 MS/s, I2C-Dekoder, 3,3 V | 2: M-314 (optional) · 3: M-406 bis M-410 | Ja, Saleae | Debug-Log statt Busmitschnitt |
| **DMM** | Multimeter | DC-Spannung, Eingang 10 MΩ | 1: M-117, M-118 | Ja, Agilent 34401A | mit OSZI (1 MΩ) möglich |
| **STROM** | USB-Leistungsmesser | 5 V, Auflösung ≤ 1 mA, Mittelwert ≥ 10 s | 3: M-437 | nein | M-437 entfällt |
| **LUX** | Luxmeter | 10 … 1000 lx | 3: M-428, M-436 | nein | Beleuchtung nur beschreiben |
| **SONDE** | Nahfeldsonde (H-Feld, ≈ 1 cm) | 1 MHz … 1 GHz | 3: M-418 | Ja | M-418 entfällt |
| **THERMO** | Thermometer | Raumtemperatur, Auflösung 0,1 K | 2: M-313 · 3: M-412 | Ja | Temperatur nur schätzen |
| **VIDEO** | Smartphone mit Slow-Motion und Stoppuhr | ≥ 120 Bilder/s | 1: M-122 (optional) · 3: M-428, M-430, M-438 | eventuell | Zeiten nur mit Stoppuhr |

### 2.2 Zubehör

| Material | Menge | Hinweis |
|---|---|---|
| 50-Ω-Durchgangsabschlüsse (BNC oder SMA) | 3 | Abschluss an jedem gemessenen Ausgang |
| Dämpfungsglieder 10 / 20 / 30 dB, 50 Ω | je 1–2 | 20 dB immer vor SPEKTRUM |
| DC-Sperre | 1 | vor SPEKTRUM und ZÄHLER, wenn der Eingang DC-gekoppelt ist |
| Leistungsteiler 2-fach 50 Ω oder BNC-T | 1 | Eigenversatz der Phasenmessung (M-004) |
| Koaxkabel **gleicher Länge** (Unterschied < 1 mm ≙ < 5 ps) | 2 + 2 | zwei für die Phase, zwei Reserve |
| Adapter vom Modulausgang auf BNC | nach Modul | |
| Dupont-Kabel, Mini-Klemmen für CN1 (SDA, SCL, 3,3 V, GND) | je 4 | M-104 bis M-106, I2C-Messungen (M-101, M-102, M-403, M-314, M-315, M-404, M-405, M-406 bis M-410) |
| Widerstände 10 kΩ und ≤ 100 Ω | je 2 | M-117, M-118 (Last gegen 3,3 V / GND), M-314 |
| USB-Kabel Micro-USB und USB-C, USB-Netzteil 5 V/1 A, Powerbank | je 1 | M-310 d |
| Diodenringmischer (z. B. SBL-1, ADE-1), LO +7 dBm | 1 | M-419, M-420 (Stufe 3) |
| Soundkarte (Line-In) oder SDR mit FFT-Software | 1 | M-305, M-420 |
| QSD-Mischer (FST3253) oder zwei Mischer mit 90°-NF-Netzwerk | optional | M-413 |
| I2C-Modul auf Adresse 0x60 (z. B. MCP4725 mit passender Adressbeschaltung) | optional | M-410 |
| SI5351-Klon (MS5351M) | optional | M-315 |
| Eingabestift (Kunststoff, Spitze ≈ 1 mm; der Touch ist resistiv) | 1 | M-433 |
| Isolierunterlage, ESD-Armband | 1 | 3.1 |

### 2.3 Rechner und Software

| Posten | Hinweis |
|---|---|
| PC mit PlatformIO | `~/.platformio/penv/Scripts/pio.exe` (6.2.0); `pio` steht nicht mehr im PATH (S02) |
| Python 3 mit `pyserial`, `pyvisa`, `pyvisa-py` | Skripte in Anhang D |
| `curl` (unter Windows `curl.exe`), `ncat`/`nc`, `esptool` (aus der PlatformIO-Umgebung) | Stufe 1W und WEB-HMI-Messungen der Stufen 2 und 3 |
| Serieller Monitor 115 200 Baud | `pio device monitor -b 115200 --port COMx` |
| Browser (Chrome, Firefox) auf PC, Smartphones Android **und** iOS | M-201, M-202, M-441, M-443, M-446 bis M-451 |
| WLAN-Router mit WPA2 (Test-WLAN, 2,4 GHz), zweiter Router oder AP | M-201, M-439 |
| Gerät für Test-SSIDs (zweiter ESP32 oder Linux mit `hostapd`) | M-209, nur im eigenen Labor mit eigenen Geräten |
| Wireshark (optional) | M-211, M-456 |
| FFT-Software für die Soundkarte (Audacity, SpectrumLab) | M-420 |
| Messprotokoll (Anhang A) und Kamera für Aufbau-Fotos | alle |

### 2.4 Personen für Bedientests (nur Stufe 3)

5 Lernende (ohne Vorkenntnis des Geräts) und 2–3 Lehrende; je 30 min für M-430, kurze Einzelsitzungen (10 min) für M-425, M-428, M-432, M-433; Einverständnis für die Aufzeichnung der Touch-Daten und Notizen (keine Namen im Protokoll, nur Personen-Codes P1 … P8). Stufe 1 und 2 braucht nur einen Bediener.

---

## 3. Gemeinsame Grundlagen

### 3.1 Sicherheit und Geräteschutz

- **Nie ein ausgeschaltetes Gerät mit `Off state` = `High` an SPEKTRUM oder ZÄHLER mit DC-Kopplung**: Der Ausgang führt dann 3,3 V Gleichspannung (M-117). Pegelmessungen im Zustand OFF nur mit OSZI (DC, 1 MΩ oder 50 Ω) oder DMM, nie mit SPEKTRUM.
- **Immer erst 20 dB Dämpfung und DC-Sperre vor SPEKTRUM.** Die Grundwelle an 50 Ω kann bei 8 mA grob 0 … +10 dBm erreichen (genau misst M-312).
- Pegel und Frequenz am Gerät einstellen, **bevor** der Ausgang eingeschaltet wird; neue Aufbauten nur bei ausgeschalteten Ausgängen verbinden.
- SI5351-Modul, Kabel und Mischer nicht unter Spannung umstecken, außer in M-104 bis M-106, M-403, M-407, M-408 (dort ist es der Test).
- ESD: Armband, Isolierunterlage; die Boards nicht auf Metall legen (Kurzschluss an der Rückseite).
- Masseschleifen: OSZI, ZÄHLER und SPEKTRUM sind über Netzleiter geerdet; das Board wird über USB vom PC gespeist. Bei Brummen oder Offsets ein USB-Netzteil statt PC-USB verwenden und im Protokoll vermerken.
- Netzwerk-Sicherheitstests (M-206 bis M-216, M-455, M-456) nur im eigenen, abgetrennten Laborsegment und nur mit eigenen Geräten; Störtests (Slowloris, Beacon-Fluten) nie in einem produktiven WLAN.

### 3.2 Standardbedingungen

Wenn bei einer Messung nichts anderes steht:

| Größe | Wert |
|---|---|
| Ausgangslast | 50 Ω abgeschlossen; am SPEKTRUM über DC-Sperre und 20 dB |
| Drive | 8 mA |
| Tuning, Spread, Off state | `Fractional`, `Off`, `Low` (Werkseinstellung) |
| Korrektur | nach M-108 kalibriert und danach unverändert |
| WLAN | aus (außer bei Stufe 1W, den WEB-HMI-Messungen der Stufe 3, M-317, M-318, M-437) |
| Aufwärmzeit | 15 min mit eingeschaltetem CH1 (10 MHz) vor den Frequenz-, Phasen- und Spektrummessungen |
| Umgebung | 20 … 25 °C, keine Zugluft, Temperatur im Protokoll |
| Spannungsversorgung | PC-USB; abweichend nur nach Vermerk |
| Firmware-Build | Release (`LOG_DEBUG 0`); Testbuilds nur, wo gefordert (4.2) |
| Boot | nach jedem Flashen oder Reset die Info-Karte prüfen (Version, Commit ohne `+`) |

### 3.3 Frequenzen messen: Klammermessung

Der Quarz driftet innerhalb von Minuten um bis zu einige 0,1 ppm (≙ 10 Hz bei 100 MHz). Die Synthese kann deshalb nur **relativ** zu einer Messung bei 10 MHz bewertet werden; die absolute Referenzgenauigkeit ist dafür unkritisch, solange M-108 und alle weiteren Messungen mit demselben ZÄHLER und derselben REF laufen.

1. 10 MHz an CH1 einstellen, 10 Werte mit 1 s Torzeit mitteln: **f10a**.
2. Messfrequenz **f_soll** einstellen, 10 Werte mitteln: **f_mess**.
3. 10 MHz erneut einstellen, 10 Werte mitteln: **f10b**.
4. Abweichung: **δ = f_mess − f_soll · (f10a + f10b) / (2 · 10 MHz)** in Hz.

Ausgang, Kabel und Zähler bleiben dabei unverändert. Die Messunsicherheit u ist die Auflösung des Zählers (M-003) plus die Streuung der 10 Mittelwerte.

### 3.4 Phase am Oszilloskop messen

Beide Ausgänge über **gleich lange** Kabel an zwei OSZI-Kanäle mit 50 Ω (Aufbau 3). Trigger auf Kanal 1. Zeitversatz **Δt** zwischen den steigenden Flanken an 50 % der Amplitude, ≥ 256 Mittelungen.

- **Phase: φ = Δt · f · 360°**, bei negativem Δt eine Periode addieren.
- **Eigenversatz φ₀** aus M-004 abziehen.
- Umrechnung: 1° entspricht bei 4,73 MHz ≈ 587 ps, 10 MHz ≈ 278 ps, 30 MHz ≈ 93 ps, 99 MHz ≈ 28 ps. Bei hohen Frequenzen setzt die Zeitauflösung des OSZI die Grenze (M-004 bestimmt u).
- 1° Phasenfehler entspricht einer Seitenbandunterdrückung von ≈ 41 dB, 2° ≈ 35 dB (M-413).

### 3.5 Spektren messen (Stufe 2 und 3)

- Aufbau 2 mit 20 dB und DC-Sperre; REF an den Referenzeingang des SPEKTRUM.
- Referenzpegel so, dass der Träger im oberen Drittel liegt; Detektor „Sample“ oder „RMS“, Mittelung ≥ 10; Einstellungen ins Protokoll (Span, RBW, VBW, Mittelung, Referenzpegel).
- **Nebenlinien** in dBc zum Träger; Abstand in Hz notieren.
- **Phasenrauschen** mit dem Rauschmarker, auf 1 Hz normiert. Eigenrauschen aus M-004 daneben eintragen; liegt der Messwert < 6 dB darüber, gilt er als obere Schranke (○).
- Ein Screenshot je Messung, Dateiname `<Nr>_<Lauf>_<Board>.png`.

### 3.6 Bewertung

| Zeichen | Bedeutung |
|---|---|
| ✔ | bestanden: \|Abweichung\| + u ≤ Grenze |
| ✘ | nicht bestanden: \|Abweichung\| − u > Grenze |
| ○ | nicht entscheidbar (Unsicherheit zu groß); Grund und u eintragen |
| i | nur zu dokumentieren (Information für eine Entscheidung) |
| – | nicht anwendbar (Firmware-Stand, Gerät fehlt) |

Jedes ✘ und jedes unerwartete Verhalten wird mit der Vorlage aus Anhang E als **Messbefund MB-n** erfasst (Mess-ID, Lauf, Board, Messwert, Foto, Vermutung) und mit Priorität und einer Aufgabe in [ARBEITSPLAN.md](ARBEITSPLAN.md) eingetragen. Bei ○ wird, wenn möglich, mit besserem Aufbau wiederholt.

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

#### M-001 · Boards, Ports und Modul zuordnen

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

#### M-002 · Prüfling sichern

`Geräte` PC · `Dauer` 10 min

1. `git status` sauber, `git log -1` zeigt den Prüfling; Info-Karte ohne `+`.
2. Lokaler Tag des Prüflings (z. B. `v1.3.2` liegt schon, für R1 `v1.4.0-rc1`); `firmware.bin` und `firmware.elf` nach `messungen/firmware/<Version>/`, SHA-256 notieren. Die Referenz 1.3.0 ist seit S03 gesichert.
3. Benötigte Testbuilds (4.2) bauen und daneben ablegen.

**Ergebnis:** Version ______ · Commit ______ · SHA-256 ______

#### M-003 · Messgeräte prüfen und aufwärmen

`Geräte` alle für die Stufe · `Dauer` 15 min aktiv (Aufwärmzeit läuft parallel)

1. Geräte einschalten; ZÄHLER (und SPEKTRUM) auf externe Referenz, „Lock“ abwarten (≥ 30 min Aufwärmzeit laut Gerätehandbuch). Gleichzeitig CH1 = 10 MHz einschalten (Aufwärmzeit des Prüflings).
2. ZÄHLER: REF an den Eingang, 10 Werte mit 1 s Torzeit; Streuung = Auflösung.
3. OSZI: Eigenabgleich, 50-Ω-Eingänge, gleiche Vertikalempfindlichkeit.
4. DMM: Nullpunkt, Bereich 20 V. SPEKTRUM (nur Stufe 2): Eigenkalibrierung.

**Ergebnis:** u_ZÄHLER = ______ Hz · Kalibrierdaten: ______

#### M-004 · Eigenmessung des Aufbaus (Unsicherheiten)

`Geräte` OSZI (Teil a, Stufe 1), SPEKTRUM (Teil b, Stufe 2) · `Aufbau` 3 und 2 · `Dauer` a 20 min, b 30 min

**Teil a (vor M-115, M-116):**

1. CH1 auf 10 MHz über Teiler/T-Stück auf beide OSZI-Kanäle; Δt mit ≥ 256 Mittelungen messen, Kabel tauschen, erneut messen. Mittelwert der Beträge = Eigenversatz Δt₀ (ps), Differenz/2 = Unsicherheit der Kabel.
2. Dasselbe bei 99 MHz (Frequenzabhängigkeit der Kanäle).
3. Streuung von Δt über 10 Messungen = u_t; daraus u_φ = u_t · f · 360°.

**Teil b (vor Stufe 2):**

4. Eigenrauschen SPEKTRUM: Eingang mit 50 Ω abschließen, Span 200 kHz, RBW 100 Hz, Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz Ablage (Mitte 10 MHz).
5. Dämpfung der 20-dB-Glieder und der DC-Sperre bestimmen.
6. Eigenverzerrung bei −10 dBm am Eingang prüfen (sonst misst M-414/M-312 die Analysator-Harmonischen).

**Ergebnis:** Δt₀ = ___ ps (10 / 99 MHz: ___ / ___) · u_t = ___ ps · u_φ bei 10 / 99 MHz = ___° / ___° · Eigenrauschen (dBc/Hz bei 100 Hz / 1 k / 10 k / 100 k) = ___ / ___ / ___ / ___

#### M-005 · Protokoll anlegen

1. Protokoll `messungen/<Datum>_<Board>_<Firmware>.md` aus Anhang A anlegen; nur die Abschnitte der gemessenen Stufe übernehmen.
2. Ordner `messungen/<Datum>/` für Screenshots, Logs, Fotos; Dateinamen `<Nr>_<Lauf>_<Board>.<ext>`.
3. Info-Karte (Firmware, Branch, Commit, Build-Zeit) fotografieren.

### 4.2 Eintrittskriterien

| Nr | Kriterium | Prüfung | Gilt für | ☐ |
|---|---|---|---|---|
| EK-1 | Host-Tests grün in `native-win` und `native` | `pio test -e native-win`, CI | alle | ☐ |
| EK-2 | Build ohne Warnungen mit `-Wall -Wextra` in `src/` | `pio run` | alle | ☐ |
| EK-3 | Gate des Arbeitsplans erfüllt (Regel 0.2: Review, Doku DE + EN, Spezifikation, README) | [ARBEITSPLAN.md](ARBEITSPLAN.md) | alle | ☐ |
| EK-4 | Fuzzing ≥ 10 Mio. Ausführungen ohne Fund (A-725) | CI-Protokoll | 1W | ☐ |
| EK-5 | Abhängigkeiten exakt gepinnt, Advisory-Check je Release (A-724) | `platformio.ini`, Notiz | 1W | ☐ |
| EK-6 | Prüfling committet, Info-Karte ohne `+` | CONFIG | alle | ☐ |
| EK-7 | Smoke-Test (4.3) auf beiden Boards bestanden | Protokoll | alle | ☐ |
| EK-8 | benötigte Testbuilds vorhanden (Tabelle unten) | `messungen/firmware/` | alle | ☐ |
| EK-9 | Handbuch DE und EN passen zur Firmware; Abweichungen zum Plan notiert | Durchsicht | alle | ☐ |

**Testbuilds** (ein Flag je Build; der Release-Build bleibt `LOG_DEBUG 0`):

| Build | Flag | Für | Stand |
|---|---|---|---|
| Debug | `-DLOG_DEBUG=1` | M-314, M-315 (Stufe 2) · M-405, M-409, M-319 | vorhanden |
| IQ-Fehler | env `esp32-2432S028R-faultiq` (`-DTEST_FAULT_IQ=1`) | M-401, M-402 (Stufe 3) | vorhanden (AF39) |
| Touch-Log | env `esp32-2432S028R-touchlog` (`-DTEST_TOUCH_LOG=1`): je Berührung `[TOUCH] x=… y=… to …,… z=… t=…ms gap=…ms drop=n/…ms target=…` | M-316 (Stufe 2) · M-425 | vorhanden (S28) |
| Trefferkarte | env `esp32-2432S028R-hitmap` (`-DTEST_HIT_MAP=1`, mit Touch-Log): 20 Zielkreuze, roter Punkt am Treffer, `[HITMAP] target n (x,y) hit (x,y) dx dy`; Verlassen durch Neustart | **M-126 (Stufe 1)** | vorhanden (S28) |
| PLL ohne Reset | Testbuild „PLL-Änderung ohne Reset“ (FW-02) | M-305 (Stufe 2) | zu erstellen |
| Assert/WDT | `-DTEST_LVGL_ASSERT=1`, `-DTEST_LOOP_HANG=1` | M-128, M-129 (bedingt) | vorhanden (S09) |

### 4.3 Smoke-Test (je Entwicklungsphase, beide Boards)

`Dauer` ≈ 30 min · nach jedem größeren Block des Arbeitsplans ([ARBEITSPLAN.md](ARBEITSPLAN.md), Regel 0.8); das Ergebnis steht im Release-Commit.

| Schritt | Prüfung | Erwartung |
|---|---|---|
| 1 | Beide Boards flashen, booten, Info-Karte | Version und Commit stimmen; Display `detected`; Touch kalibriert; Hauptseite ohne Rauschen |
| 2 | M-103, M-104 kurz: Board ohne Modul booten; am Messboard SDA ab- und wieder anstecken | je genau ein Dialog; Verbindung ≤ 2,5 s nach dem Anstecken |
| 3 | M-120 kurz: eine Frequenz und eine CONFIG-Option ändern, `Saved` abwarten, Stecker ziehen | Werte erhalten, alle Ausgänge aus |
| 4 | M-123 kurz: MAIN, CH1 bis CH3, CONFIG (gescrollt, Dropdowns geöffnet, M-124), NUMPAD | nichts abgeschnitten oder überlappt; Dropdowns vollständig |
| 5 | CH1 = 10 MHz an ZÄHLER oder OSZI, ein und aus | 10 MHz ± 100 Hz (unkalibriert); aus = kein Signal |
| 6 | CH3 IQ 90° an OSZI | zwei Signale mit 25 ns Versatz |
| 7 (ab WEB-HMI) | M-201 (Client), `*IDN?` über TCP 5025 und USB, Webseite öffnet | siehe 5.11 |

Bei 1.3.1 bis 1.3.3 und beim Core-3-Stand (S41b, 2026-11-06) blieben die Schritte 5 und 6 offen (kein Messgerät verfügbar); der erste Stufe-1-Lauf schließt sie (M-109, M-115).

---

## 5. Stufe 1 – Pflicht

Die notwendigen manuellen Messungen. Sie decken ab, was kein Host-Test zeigen kann und was bei einem Fehler Schaden anrichtet. Alles läuft an einem Arbeitstag mit ZÄHLER, REF, OSZI und DMM; der SPEKTRUM wird nicht gebraucht.

### 5.1 Ablauf

| Block | Messungen | Board | Geräte | Dauer |
|---|---|---|---|---|
| 0 Vorbereitung | M-001, M-002, M-003, M-004 a, M-005; Aufwärmzeit 15 min (CH1 10 MHz) parallel | beide | alle | 45 min |
| V Verbindung | M-101, M-102, M-103, M-104 (mit M-519), M-105, M-106 | Messboard; M-103 am Board ohne Modul | ZÄHLER oder OSZI | 45 min |
| F Frequenz | M-107, M-108, M-109 mit M-110 (ab S35 zusätzlich M-111, M-112) | Messboard | ZÄHLER, REF | 55 min |
| T Abstimmen | M-113, M-114 (mit M-509) | Messboard | OSZI, ZÄHLER | 30 min |
| IQ Phase | M-115, M-116 | Messboard | OSZI | 40 min |
| P Pegel | M-117 (mit M-503), M-118, M-119 | Messboard | OSZI, DMM | 45 min |
| S Speichern | M-120 mit M-121 | Messboard | – | 15 min |
| D Display und Touch | M-122 (mit M-501), M-123 (mit M-505, M-124), M-125, M-126; M-127 läuft über den ganzen Tag mit | beide | PC, VIDEO optional | 90 min |
| bedingt | M-111, M-112 (ab S35); M-128, M-129 (nach der Migration, S41) | Messboard | ZÄHLER, OSZI | 15 + 30 min |
| | **Summe** | | | **≈ 6 h + Protokoll ≈ 7 h** |

Die Reihenfolge ist fest: V zuerst (ohne Verbindung sind alle späteren Werte unsicher), M-108 vor allen HF-Messungen (die Korrektur bleibt danach unverändert), S erst nach den HF-Blöcken (M-120 verstellt Drive und Tuning).

### 5.2 Block V · Verbindung

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-101 | Kabel gerade | ZÄHLER oder OSZI | verbunden, Log `[APP] SI5351 at 0x60, SDA=22 SCL=27`, kein Dialog; 10 MHz am Ausgang. | 5 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-102 | Kabel vertauscht | ZÄHLER oder OSZI | verbunden, Log `SDA=27 SCL=22 (swapped)`, Info-Karte mit `(swapped)`; 10 MHz am Ausgang. Danach wieder gerade stecken. | 5 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-103 | Booten ohne SI5351 | – | genau **ein** Dialog „SI5351 not found …“ mit dem Hinweis auf SDA/SCL in beliebiger Reihenfolge; `OK` schließt ihn, er kommt nicht wieder; MAIN und Header zeigen `NO HW` bei allen Kanälen, die Kanalseiten `SI5351 not found`; Info-Karte `SI5351 not found`; die Änderung wird gespeichert (`[SETTINGS] Saved to NVS`); keine wiederholten Fehlerzeilen im Log. | 5 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-104 | Modul nachträglich anschließen (enthält M-519) | Stoppuhr oder VIDEO, ZÄHLER oder OSZI | `NO HW` verschwindet nach **≤ 2,5 s**; der offene SI5351-Dialog schließt sich selbst; Log `[APP] SI5351 at 0x60, SDA=22 SCL=27` bzw. `SDA=27 SCL=22 (swapped)`; CH1 liefert 10 MHz. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-105 | Verbindung im Betrieb unterbrechen | ZÄHLER oder OSZI | Dialog „Connection lost. Check the cable; SDA/SCL may be in either order …“, MAIN `NO HW`; das Modul bleibt versorgt, der Ausgang läuft weiter (Handbuch §11); nach dem Anstecken `[APP] Restoring outputs`, CH1 wieder `ON` mit derselben Frequenz (Zähler), der Dialog schließt sich selbst. | 10 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-106 | Versorgungsausfall des Moduls | OSZI; Aufbau 3 | wie M-105; beide Ausgänge aus dem Rücksetzzustand des Chips wiederhergestellt, CH3 wieder 270° (± 1° + u_φ). | 10 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-101 · Kabel gerade

`Board` Messboard · `Bezug` A-221

1. SDA an GPIO22, SCL an GPIO27 (CN1), booten; Log und Info-Karte lesen; CH1 = 10 MHz einschalten.

**Messwert:**

#### M-102 · Kabel vertauscht

`Board` Messboard · `Bezug` A-221, E-212

1. SDA und SCL vertauschen, booten; Log und Info-Karte lesen; CH1 = 10 MHz.

**Messwert:**

#### M-103 · Booten ohne SI5351

`Board` ohne Modul · `Bezug` A-225, A-227, E-212

1. Board ohne Modul an CN1 anschließen, Serial Monitor starten, EN drücken.
2. Dialoge zählen, Text lesen, `OK` tippen.
3. MAIN, CH1- und CH3-Seite, Info-Karte ansehen.
4. CH1 auf 7,100 000 MHz ändern und 10 s warten.

**Messwert:**

#### M-104 · Modul nachträglich anschließen (enthält M-519)

`Board` Messboard · `Bezug` A-226, A-221

1. Messboard mit abgezogenem SDA booten, den Dialog **nicht** schließen.
2. SDA anstecken; Zeit vom Kontakt bis zum Verschwinden von `NO HW` messen (3×).
3. Dialogzustand prüfen; CH1 = 10 MHz einschalten.
4. (M-519) Ohne SDA/SCL neu booten, dann **vertauscht** anstecken; Zeit einmal messen. Danach wieder gerade stecken.

**Messwert (Zeiten gerade / vertauscht):**

#### M-105 · Verbindung im Betrieb unterbrechen

`Board` Messboard · `Bezug` A-226, E-212

1. CH1 = 10 MHz einschalten, Ausgang an ZÄHLER.
2. SDA ≈ 5 s abziehen, wieder anstecken.
3. Dialog, Status und Zählerwert beobachten; Log mitlesen.

**Messwert:**

#### M-106 · Versorgungsausfall des Moduls

`Board` Messboard · `Bezug` A-226, A-219

1. CH1 = 10 MHz an, CH3 an (Master CH1, 270°); beide Ausgänge an OSZI.
2. 3,3 V am Modul ≈ 2 s trennen, wieder anschließen.
3. Verhalten wie M-105 beobachten; danach die Phase messen (3.4).

**Messwert (Phase nachher):**

### 5.3 Block F · Frequenz und Kalibrierung

Messboard, Aufbau 1, ZÄHLER mit REF, Standardbedingungen, 15 min Aufwärmzeit. Auswertung mit der Klammermessung (3.3).

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-107 | Korrektur 0, Quarzfehler bestimmen | ZÄHLER, REF | zu dokumentieren: f_mess − 10 MHz (Hz) = Quarzfehler; `Out` zeigt die Rundung der Register (bei 10 MHz ±0,0 Hz). | 10 min | i |
| M-108 | Kalibrieren | ZÄHLER, REF | danach < 1 Hz Abweichung bei 10 MHz (Auflösung 0,01 ppm ≙ 0,1 Hz); Wert im Bereich ±50 ppm; das Vorzeichen stimmt (positive Korrektur senkt die Frequenz, Handbuch §8). | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-109 | Genauigkeit über den Bereich (Klammermessung) | ZÄHLER, REF | \|δ\| ≤ Grenze; die Zwischenpunkte (3,5 / 14,2 / 50 MHz aus Plan 2.0) deckt der Host-Test ab. | 30 min (+ 15 min mit M-111, M-112) | ☐ ✔ ☐ ✘ ☐ ○ |
| M-110 | `Integer`: tatsächliche Frequenz | ZÄHLER, REF | Zähler = angezeigte tatsächliche Frequenz (`Out 7.102 273 MHz`, +2272,7 Hz) ± u; MAIN zeigt `~7.100 000 MHz`. | in M-109 (5 min) | ☐ ✔ ☐ ✘ ☐ ○ |
| M-111 | Untergrenze und kHz-Anzeige (bedingt ab S35) | ZÄHLER | Frequenz ≤ 0,5 Hz + u; Anzeige in kHz bzw. MHz laut Handbuch. | in M-109 (5 min) | ☐ ✔ ☐ ✘ ☐ ○ ☐ – |
| M-112 | Oberes Ende und 112,5 MHz (bedingt ab S34/S35) | ZÄHLER | Abweichung ≤ 1,5 / 2 / 3 Hz + u (Grenzen aus S34). | in M-109 (10 min) | ☐ ✔ ☐ ✘ ☐ ○ ☐ – |

#### M-107 · Korrektur 0, Quarzfehler bestimmen

`Bezug` A-212

1. CONFIG: `Corr.` auf 0.00 ppm.
2. CH1 = 10 MHz einschalten, 10 Werte mit 1 s Torzeit mitteln.
3. Auf der CH1-Seite die Zeile `Out` ablesen.

**Messwert (Hz, ppm):**

#### M-108 · Kalibrieren

`Bezug` A-212, U-11

1. Korrektur in ppm = Abweichung aus M-107 (Hz) ÷ 10 (Handbuch §8) mit der Schritttaste (`1`, `0.1`, `0.01`) und `−`/`+` einstellen; **Anzahl der Tipps und Zeit notieren** (Bedienaufwand U-11, Grundlage für S32).
2. 10 MHz erneut messen, nachstellen bis |Abweichung| < 0,2 Hz.
3. Korrektur nicht mehr ändern; Wert ins Protokoll (Anhang A).

**Messwert (Corr. in ppm, Tipps, Zeit):**

#### M-109 · Genauigkeit über den Bereich (Klammermessung)

`Bezug` A-213 (≤ 0,5 Hz in `Fractional`; Host-Test ≤ 0,375 Hz), A-215

1. Standardbedingungen, Korrektur nach M-108.
2. Je Zeile die Klammermessung (3.3); Frequenz über das NUMPAD, 10 s Einschwingzeit; δ eintragen.
3. Zeile M-110: `Tuning` = `Integer`, f_soll = die tatsächliche Frequenz laut `Out`; danach zurück auf `Fractional`.
4. Ab Firmware mit S35 zusätzlich die Zeilen M-111 und M-112.

| f_soll | f10a (Hz) | f_mess (Hz) | f10b (Hz) | δ (Hz) | Grenze | ✔/✘/○ |
|---|---|---|---|---|---|---|
| 1,000 000 MHz | | | | | 0,5 Hz + u | |
| 7,100 000 MHz | | | | | 0,5 Hz + u | |
| 28,400 000 MHz | | | | | 0,5 Hz + u | |
| 99,999 999 MHz | | | | | 0,5 Hz + u | |
| M-110: 7,1 MHz `Integer` → 7,102 273 MHz laut `Out` | | | | | Anzeige ± u | |
| M-111 (ab S35): 8 kHz / 455 kHz | | | | | 0,5 Hz + u | |
| M-112 (ab S35): 112,499 996 / 112,500 004 / 160 MHz | | | | | 1,5 / 2 / 3 Hz + u | |

**Messwert:** in der Tabelle oben

#### M-110 · `Integer`: tatsächliche Frequenz

`Bezug` A-206, A-215, E-206

1. Zeile M-110 der Tabelle in M-109; zusätzlich MAIN ansehen.

**Messwert:**

#### M-111 · Untergrenze und kHz-Anzeige (bedingt ab S35)

`Bezug` E-203

1. Zeile M-111 der Tabelle in M-109 (8 kHz, 455 kHz); Anzeige bei 999,999 kHz und 1 MHz ablesen.

**Messwert:**

#### M-112 · Oberes Ende und 112,5 MHz (bedingt ab S34/S35)

`Bezug` E-203, S34

1. Zeile M-112 der Tabelle in M-109; bei 160 MHz zusätzlich prüfen, dass ein Ausgangssignal vorhanden ist (Pegel geringer).

**Messwert:**

### 5.4 Block T · Abstimmen ohne Aussetzer

OSZI Kanal 1 an CLK0 (CH1). **Aussetzer erkennen:** Trigger „Timeout“ (Dropout) mit Schwelle ≈ 3 Perioden, Single-Modus; jeder Trigger = ein Aussetzer, Dauer mit den Cursorn messen. Ohne Timeout-Trigger: Persistenz und Triggerverlust beobachten, Ergebnis „○“.

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-113 | Kanal 1 bleibt beim Abstimmen von CH2 ungestört | OSZI | **0** Aussetzer an CH1 (A-210: kein Reset der anderen PLL). | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-114 | Dauerabstimmen im `Fractional`-Modus (enthält M-509) | OSZI, ZÄHLER | keine Aussetzer beim Abstimmen; zulässig ist nur ein Aussetzer bei einem PLL-Wechsel 900 ↔ 887,5 MHz (A-206), der dokumentiert wird (Frequenz, Dauer). M-509: Frequenz sinkt um ≈ 10 Hz (± 0,5 Hz), **kein** Aussetzer. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-113 · Kanal 1 bleibt beim Abstimmen von CH2 ungestört

`Bezug` A-210

1. CH1 = 10 MHz an, CH2 = 7,100 000 MHz an. OSZI: Timeout-Trigger auf CLK0.
2. CH2 über `+`/`−` mit 1 MHz, 1 kHz, 1 Hz verstellen; per NUMPAD mindestens 20 Sprünge zwischen 1 und 99 MHz.
3. Auslösungen zählen.

**Messwert (Aussetzer von 20):**

#### M-114 · Dauerabstimmen im `Fractional`-Modus (enthält M-509)

`Bezug` A-206, A-210, A-212

1. CH1 = 10 MHz, Schritt 1 Hz, `+` 10 s halten; Timeout-Trigger auf CLK0.
2. CH1 = 99 MHz, Schritt 1 MHz, bis 100 MHz.
3. (M-509) CH1 = 10 MHz; `Corr.` um +1,00 ppm ändern, Zähler und OSZI beobachten; danach zurück auf den Wert aus M-108.

**Messwert (Aussetzer, Δf bei +1 ppm):**

### 5.5 Block IQ · Phasen

Aufbau 3, Methode 3.4; φ₀ und u_φ aus M-004 a. CH1 ist Master, Standardbedingungen, Korrektur nach M-108. Phase jeweils **2 s nach** dem Umschalten messen. Die Werte aus M-115 und M-116 dienen zugleich als Daten für M-307 und M-308 (Stufe 2).

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-115 | Vier Phasen bei 10 MHz | OSZI | 0°, 90°, 180°, 270° ± 1° (+ u_φ). | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-116 | Phasen an den Bereichsgrenzen (gekürzt) | OSZI | alle Phasen ±1° (+ u_φ). Bei 99 MHz entspricht 1° nur ≈ 28 ps: bei u_φ > 0,5° das Ergebnis „○“ und mit mehr Mittelung wiederholen. Die Punkte 5 und 30 MHz aus Plan 2.0 entfallen (Host-Test der Phasenregister, keine neue Grenze). | 25 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-115 · Vier Phasen bei 10 MHz

`Bezug` A-216, A-217

1. CH1 = 10 MHz an, CH3 an (Master CH1); Phase `0`, `90`, `180`, `270` nacheinander; je Phase Δt (≥ 256 Mittelungen) und φ.

| Phase | Δt (ps) | φ gemessen (°) | Abweichung (°) | Grenze |
|---|---|---|---|---|
| 0° | | | | 1° + u_φ |
| 90° | | | | 1° + u_φ |
| 180° | | | | 1° + u_φ |
| 270° | | | | 1° + u_φ |

**Messwert:** in der Tabelle oben

#### M-116 · Phasen an den Bereichsgrenzen (gekürzt)

`Bezug` A-216, A-217, E-209

1. Für f = 4,73 / 14,2 / 99 MHz je vier Phasen wie M-115. Der IQ-Teiler ist min(127, ⌊900 MHz / f⌋): 127, 63, 9 (größter Teiler, zwei ungerade Teiler).

| f (MHz) | Teiler | 0° | 90° | 180° | 270° |
|---|---|---|---|---|---|
| 4,73 | 127 | | | | |
| 14,2 | 63 | | | | |
| 99 | 9 | | | | |

**Messwert:** in der Tabelle oben

### 5.6 Block P · Pegel im Zustand OFF und beim Neustart

Aufbau 6, **nie SPEKTRUM**.

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-117 | Pegel im Zustand OFF je `Off state` (enthält M-503) | OSZI, DMM; Aufbau 6 | `Low` ≈ 0 V (< 0,1 V) an allen drei Ausgängen in jeder Last; `High` ≈ 3,3 V an 1 MΩ, an 50 Ω durch den Drive-Strom begrenzt (notieren); `High-Z` nimmt den Pegel der Last an. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-118 | Pegel nach `CLKx_PDN` (entscheidet FW-05) | OSZI, DMM; Aufbau 6 | Pegel notieren. Ist der Pin mit `PDN` hochohmig, darf `High-Z` alle abgeschalteten Ausgänge mit `CLKx_PDN` abschalten (FW-05, E-208); zieht er auf Low, bleibt es bei der heutigen Lösung. Ergebnis in Handbuch DE/EN nachziehen (Hinweis „bis zur Hardwareprüfung M-118“). | 15 min | i |
| M-119 | Ausgang beim Neustart | OSZI; Aufbau 6, Kanal 2 an EN des ESP32 | zu dokumentieren; erwartet: der Ausgang läuft bis zur Initialisierung weiter (beim Upload einige Sekunden) und endet **vor** der Hauptseite. Die Dauer kommt als Hinweis ins Handbuch (Schutz angeschlossener Schaltungen, DOK-01). | 10 min | i |

#### M-117 · Pegel im Zustand OFF je `Off state` (enthält M-503)

`Board` Messboard · `Bezug` A-203, A-208, E-208

1. (M-503) `Off state` = `Low` (Werkseinstellung): CH1, CH2 und CH3 je einmal ein- und wieder ausschalten; DC-Pegel an CLK0, CLK1 und CLK2 mit OSZI 1 MΩ, OSZI 50 Ω und DMM.
2. `Off state` = `High` (seit 1.3.2 mit Bestätigung) und `High-Z`: CH1 ein- und ausschalten, Pegel an CLK0; bei `High-Z` zusätzlich mit 10 kΩ nach 3,3 V bzw. nach GND.
3. Danach `Off state` wieder `Low`.

| Off state | Ausgang | 1 MΩ (V) | 50 Ω (V) | DMM (V) | 10 kΩ gegen 3,3 V / GND (V) |
|---|---|---|---|---|---|
| Low | CLK0 | | | | – |
| Low | CLK1 | | | | – |
| Low | CLK2 | | | | – |
| High | CLK0 | | | | – |
| High-Z | CLK0 | | | | |

**Messwert:** in der Tabelle oben

#### M-118 · Pegel nach `CLKx_PDN` (entscheidet FW-05)

`Board` Messboard · `Bezug` E-208, FW-05

Seit 1.3.0 schaltet die Firmware CLK2 bei ausgeschaltetem CH3 nur mit `Off state` = `Low` über `CLK2_PDN` ab; bei `High` und `High-Z` bleibt der Ausgangstreiber versorgt (Handbuch, „bis zur Hardwareprüfung M-118“).

1. `Off state` = `Low`, CH3 aus (CLK2 mit `CLK2_PDN`): Pegel an CLK2 mit OSZI 1 MΩ, 50 Ω und DMM.
2. Mit 10 kΩ gegen 3,3 V und gegen GND: zieht der Pin auf Low oder ist er hochohmig?

**Messwert (CLK2 an 1 MΩ / 50 Ω / DMM / 10 kΩ gegen 3,3 V / 10 kΩ gegen GND):**

#### M-119 · Ausgang beim Neustart

`Board` Messboard · `Bezug` A-225, DOK-01

1. CH1 = 10 MHz einschalten. OSZI: Kanal 1 an CLK0, Kanal 2 an EN; 100 ms/div, Trigger auf fallende Flanke von EN, Single.
2. 5× Reset mit der EN-Taste, 1× Upload (`pio run -t upload`); Zeit vom Reset bis zum Ende des 10-MHz-Signals ablesen.

**Messwert (EN: min / max in ms; Upload in s):**

### 5.7 Block S · Speichern

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-120 | Speichern und Neustart (enthält M-121) | – | `Saved` nach ≈ 5 s; nach dem Neustart alle Werte erhalten, die Detailzeile auf MAIN und den Kanalseiten zeigt die Sondermodi (z. B. `SSC -0.5 % · 8 mA` bzw. `4 mA`); **alle Ausgänge OFF** (E-205). | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-121 | Optionen bleiben gespeichert | – | `Drive all` 4 mA, `Tuning` `Low noise`, `Spread CH1` `-0.5 %`, `Off state` `High-Z` erhalten, alle Ausgänge aus. | in M-120 | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-120 · Speichern und Neustart (enthält M-121)

`Board` Messboard · `Bezug` A-204, A-401 bis A-403, A-405

1. Einstellen: CH1 = 7,100 000 MHz, CH2 = 14,200 000 MHz, CH3 Master CH2, Phase 90°, Backlight 60 %; (M-121) `Drive all` = 4 mA, `Tuning` = `Low noise`, `Spread CH1` = `-0.5 %`, `Off state` = `High-Z`; alle drei Kanäle einschalten.
2. Auf CONFIG die Titelzeile beobachten (`Saving…` → `Saved` nach 5 s); Log `[SETTINGS] Saved to NVS`.
3. Stecker ziehen, wieder ein.
4. Danach die Standardbedingungen wiederherstellen (8 mA, `Fractional`, Spread `Off`, `Low`).

**Messwert:**

#### M-121 · Optionen bleiben gespeichert

`Board` Messboard · `Bezug` A-401 bis A-403

1. CONFIG-Optionen aus M-120 Schritt 1 nach dem Neustart lesen.

**Messwert:**

### 5.8 Block D · Display und Touch (beide Boards)

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-122 | Erkennung im Wiederholtest (enthält M-501) | VIDEO optional | 20/20 je Board richtig erkannt; nie weißes oder verrauschtes Bild; Hauptseite erscheint direkt. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-123 | Sichtprüfung aller Seiten (enthält M-505) | Kamera | nichts abgeschnitten, nichts überlappt, kein Scrollbalken außer auf CONFIG; Header zeigt den Ist-Zustand wie MAIN; RGB-LED CH1 rot, CH2 grün, CH3 blau, auf MAIN, CONFIG und NUMPAD aus. NUMPAD: zweimal die Grenze laut Handbuch (heute `Allowed 1 - 100 MHz`) in Rot mit rotem Rahmen, die Eingabe bleibt stehen (U-9); dann 10.700 000 MHz. Den Zustand ERR zeigt nur M-401 (Stufe 3); seine Texte sind host-getestet. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-124 | CONFIG-Dropdowns und Info-Karte | – | Listen vollständig sichtbar (öffnen nach oben, wenn unten kein Platz ist); Angaben der Info-Karte stimmen. | in M-123 (5 min) | ☐ ✔ ☐ ✘ ☐ ○ |
| M-125 | Randtasten treffen | – | 5/5 Treffer an jedem Ziel auf jedem Board. | 10 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-126 | Trefferkarte nach dem Touch-Abgleich (U-5, U-26) | PC (Log) | im Mittel ≤ 4 px, höchstens 8 px (Prüfgrenze des Abgleichs) je Bereich; alle Ecken erreichbar (auch x = 319). Größere Abweichungen sind ein Befund zum Abgleich (U-26); ob ein Stift empfohlen wird, entscheidet M-126 mit dem optionalen M-433. | 40 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-127 | Langzeit-Anzeige auf dem CYD2USB | – | keine Pixelfehler (SPI-Modus 0 mit 40 MHz). | 0 min (läuft über den ganzen Stufe-1-Tag mit) | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-122 · Erkennung im Wiederholtest (enthält M-501)

`Board` beide · `Bezug` A-302, A-301, E-301

1. Je Board 10× Kaltstart (USB-Stecker ziehen und stecken) und 10× EN-Taste.
2. Je Start: Log `[DISPLAY] … (detected)` und Bild kontrollieren.
3. (M-501) Bei den Kaltstarts auf das Einschalten achten (3 davon mit Slow-Motion aufnehmen): Beleuchtung bleibt aus, bis das erste Bild gezeichnet ist.

**Messwert (Fehlerkennungen / Fehlstarts je Board):**

#### M-123 · Sichtprüfung aller Seiten (enthält M-505)

`Board` beide · `Bezug` A-202, A-304, A-305, A-307

1. Je Board fotografieren: MAIN mit Statusleiste, Header, CH1, CH2, CH3 in den Zuständen ON, OFF, WAIT (Master auf 4,72 MHz) und NO HW (Board ohne Modul), CONFIG oben und unten gescrollt (Reihenfolge Drive, Off state, Tuning, Spread, Corr., Backlight, Touch), Dialog, Touch-Abgleich (CONFIG `Touch`, danach abbrechen bzw. zu Ende führen).
2. (M-505) NUMPAD: „500“ `MHz` und „0.5“ `MHz` eingeben, dann `C`, „10.7“ `MHz`.
3. Auf abgeschnittene Texte, Überlappungen und Scrollbalken prüfen (nur CONFIG hat eine senkrechte Leiste).
4. RGB-LED je Seite ablesen.

**Messwert / Fotos:**

#### M-124 · CONFIG-Dropdowns und Info-Karte

`Board` beide · `Bezug` A-314, E-805

1. Alle Dropdowns öffnen, auch ganz unten gescrollt.
2. Info-Karte: Firmware, Branch, Commit, Build-Zeit mit `git log -1 --format="%h %cd"` vergleichen; Display-Controller, I2C-Pins.

**Messwert:**

#### M-125 · Randtasten treffen

`Board` beide · `Bezug` A-319, E-303, U-5

1. Nach dem Touch-Abgleich je Ziel 5× mit dem Finger: Schritt-Chips `1M` (links) und `1` (rechts), `MAIN`, `CONFIG` in der Statusleiste, Dialog-`OK`, Schalter auf den Karten, unterste CONFIG-Zeile.
2. Fehltipps (keine Reaktion, falsches Ziel) zählen.

**Messwert (Fehltipps je Ziel):**

#### M-126 · Trefferkarte nach dem Touch-Abgleich (U-5, U-26)

`Board` beide · `Bezug` A-306, A-319, E-303 · Build: Trefferkarte

1. Trefferkarte flashen (der gespeicherte Abgleich bleibt im NVS); 20 Zielkreuze, darunter die vier Ecken (0 px) und Positionen 4 px vom Rand.
2. Ein Bediener tippt jedes Kreuz 3× mit dem Finger; `[HITMAP]`-Zeilen mitschreiben.
3. Auswertung je Board: mittlere und maximale Abweichung (px) je Bereich (Ecken, Rand, Mitte).
4. Danach den Release-Build zurückflashen.

**Messwert (mittel/max je Bereich und Board):** Vorlauf 2026-11-04 (ein Bediener, Finger, Firmware e3d0349 mit fester Kalibrierung): Original-CYD dx −11 … −19 px in der Mitte, −4 … −12 px am Rand, dy 0 … +6 px; rechts nur x ≈ 305–308 erreichbar, die rechte Ecke (319) nicht. CYD2USB (55 Tipps): dx bei x = 80 −14 … −19 px, bei 160 −10 … −15, bei 240 −5 … −8, am rechten Rand −1 … −7; dy −7 … +3 (ein Ausreißer +11). Beide Boards linear (Versatz und Steigung in x, Original-CYD ≈ 1,03, CYD2USB ≈ 1,06) → U-26 (Touch-Abgleich, S28a). Abgleich mit S28a am 2026-11-04: CYD2USB x 161…3778, y 245…3830, Prüfkreuz −2/+2 px; Original-CYD x 166…3716, y 280…3885, −2/+1 px (statt fest 300…3800). Trefferkarte mit Abgleich wiederholen (dieser Test).

#### M-127 · Langzeit-Anzeige auf dem CYD2USB

`Board` CYD2USB (= Messboard) · `Bezug` E-301

1. Während aller Blöcke von Stufe 1 (≈ 5 h Bedienung mit Seitenwechseln, NUMPAD, `+` halten, CONFIG scrollen) auf Pixelfehler achten.

**Messwert:**

### 5.9 Bedingte Messungen

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-128 | Assert-Handler (C-3, S-23) | OSZI, PC (Log) | Logzeile mit Ursache, Neustart innerhalb 5 s (kein dauerhaftes Einfrieren); nach dem Neustart alle Ausgänge aus (bis dahin läuft CLK0 weiter wie in M-119). | 15 min | ✔ 2026-11-04 (< 1 s, ohne Modul) · nach S41: ☒ ✔ ☐ ✘ ☐ ○ |
| M-129 | Loop-Watchdog (S-23, A-723) | OSZI, PC (Log) | Watchdog-Reset 5 s nach dem Blockieren; Ausgänge nach dem Neustart aus; Dauer des Weiterlaufens im Hängezustand dokumentieren. | 15 min | ✔ 2026-11-04 (5 s, ohne Modul) · nach S41: ☒ ✔ ☐ ✘ ☐ ○ |

#### M-128 · Assert-Handler (C-3, S-23)

`Board` Messboard · `Bezug` C-3, A-709 · Build: `TEST_LVGL_ASSERT`

**Erledigt 2026-11-04** (S09, COM7 = Original-CYD ohne Modul): Assert → Neustart nach < 1 s (`SW_CPU_RESET`). **Wiederholen** nur nach Änderungen am Assert-Handler oder an der Plattform, also einmal nach der Migration auf Core 3.x (S41), dann auf dem Messboard:

1. CH1 = 10 MHz an; OSZI auf CLK0 (Timeout-Trigger) und auf EN.
2. Der Testbuild füllt 20 s nach dem Boot den LVGL-Speicher und löst einen Malloc-Assert aus.
3. Log und Zeit bis zum Neustart; Zustand der Ausgänge danach.

**Messwert:** 2026-11-05, Core 3.3.12 vor der Abnahme (S41a), COM13 mit Modul, nur Log: Assert → `[ERROR][FATAL] LVGL assert; restarting`, `SW_CPU_RESET` nach < 0,1 s, danach `[SI5351] Init OK; all outputs off`; CLK0 am OSZI noch offen

#### M-129 · Loop-Watchdog (S-23, A-723)

`Board` Messboard · `Bezug` A-723 · Build: `TEST_LOOP_HANG`

**Erledigt 2026-11-04** (S09, COM7 ohne Modul): blockiertes `loop()` → Task-Watchdog nach 5 s. **Wiederholen** wie M-128 nach S41 (Core 3.x hat eine neue Watchdog-API):

1. CH1 = 10 MHz an (innerhalb von 20 s nach dem Boot); der Testbuild blockiert danach `loop()`.
2. Zeit bis zum Watchdog-Reset (Log: Reset-Grund), Ausgang an CLK0 währenddessen.

**Messwert:** 2026-11-05, Core 3.3.12 vor der Abnahme (S41a), COM13 mit Modul, nur Log: `task_wdt` meldet `loopTask` nach 5,0 s, `SW_CPU_RESET`, danach `all outputs off`; CLK0 im Hängezustand am OSZI noch offen

M-111 und M-112 stehen in Block F (bei M-109).

### 5.10 Wiederholung je Release

Stufe 1 läuft einmal vollständig (R0). Vor einer Weitergabe oder einem Release werden nur die Blöcke wiederholt, deren Code sich seit dem letzten vollständigen Lauf geändert hat; der Smoke-Test läuft immer.

| Geänderter Bereich | Blöcke |
|---|---|
| `drivers/si5351_manager.*`, `drivers/si5351_plan.*` | F, T, IQ, P |
| `core/app_controller.*` | V, T, IQ, P, S |
| `hal/i2c_bus.*` | V (mit M-314 aus Stufe 2) |
| `services/settings.*`, `storage.*` | S |
| `drivers/display_*`, Touch, `gui/` | D |
| **Plattform** (Core, Bibliotheken, Compiler, z. B. Migration S41) | alle Blöcke, M-128, M-129, M-314 |
| nur Doku | keiner (nur Smoke-Test) |

**Auswahl nach der Migration (S41):** alle Blöcke, außer in F nur die Zeilen 7,1 und 99,999 999 MHz von M-109 und in D nur M-122 und M-125; dazu M-128, M-129 und M-314 (≈ 4 h).

### 5.11 Stufe 1W – Pflicht ab WEB-HMI (vor 1.4.0)

Nur im Release-Kandidaten mit WEB-HMI (seit 1.4.0-rc1). Aufbau 7, Test-WLAN WPA2, 2,4 GHz, eigenes Segment. Die Bezeichnungen der Bedienelemente stammen aus dem früheren WEB-HMI-Konzept (heute Spezifikation Kapitel 5 bis 7); maßgeblich ist das Handbuch der geprüften Firmware. Gerätename, IP und Token aus der Info-Karte; `<IP>` = Geräteadresse, `<TOKEN>` = Token. Vor jedem Sicherheitstest den Zustand notieren (Kanal aus). Was die Host-Suites (`test_http_guard`, `test_scpi`, `test_line_framer`, `test_sanitize`, `test_rest_json`, `test_net_settings`, `test_command`) abdecken, prüft 1W nur stichprobenartig am Gerät.

| Messung | Inhalt | Dauer |
|---|---|---|
| M-201, M-202, M-204 | WLAN-Modi, AP mit QR, Betrieb ohne SI5351 | 1 h 45 min |
| M-203 | SCPI über USB und TCP | 45 min |
| M-205 | 24-h-Dauertest (unbeaufsichtigt) | 30 min aktiv |
| M-206, M-214, M-215, M-216 | ein Skriptlauf (`tools/web_check.py` und `tools/contract_check.py`, Anhang D) | 60 min |
| M-207, M-208 | fremde Webseite, SCPI-Port aus dem Browser | 65 min |
| M-211, M-212, M-213 | kein Routing, keine Klartext-Passwörter, `Remote outputs` | 65 min |
| M-209, M-210 | fremde SSIDs, Last (Tabelle unten) | 90 min |
| | **Summe** | **≈ 8 h + 24 h** |

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-201 | WLAN-Modi und Rückfall-AP | Router, Smartphone | wie beschrieben; Verbindung ≤ 15 s; Rückfall-AP nach 60 s (± 5 s). | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-202 | AP mit Zufallspasswort und QR, offener AP | Smartphones Android und iOS | QR funktioniert auf beiden Systemen; offener AP nur nach Bestätigung, mit Warnung, Zeit- und Stationslimit. | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-203 | SCPI über USB und TCP | PC (Skripte Anhang D) | kein Neustart beim Öffnen; 1000/1000 gültige Antworten `CYD,HF-Generator,<MAC>,1.4.0`; keine Logzeilen im Datenstrom; Median < 50 ms. | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-204 | Ohne SI5351 | – | Web und SCPI funktionieren; Kanäle zeigen `NO HW` bzw. `NOHW`; Einstellungen werden gespeichert; keine wiederholten Fehler. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-205 | 24-h-Dauertest | PC (Skript) | kein Neustart; minimaler Heap sinkt nicht stetig; Stack-Reserve ≥ 1 KB; keine Warnung „Heap < 40 KB“. | 24 h (30 min aktiv) | ☐ ✔ ☐ ✘ ☐ ○ |
| M-206 | curl-Matrix (A-701, A-704, A-705, A-710) | – | wie angegeben; Zustand bleibt unverändert. | im Skriptlauf `web_check.py` / `contract_check.py` | ☐ ✔ ☐ ✘ ☐ ○ |
| M-207 | Fremde Webseite (A-705, A-706) | PC, Browser | Zustand unverändert; WebSocket-Upgrade abgelehnt; `evil.example` → 421. | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-208 | SCPI-Port aus dem Browser (A-717, A-718) | – | CH1 bleibt in allen Fällen aus. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-209 | Fremde SSIDs | Aufbau 7; Test-AP | keine Skriptausführung; SSIDs bereinigt; höchstens 15 Einträge; kein Neustart | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-210 | Lasten | Aufbau 7; PC (`tools/slowloris.py`) | Touch reagiert < 200 ms; Heap erholt sich auf ≥ 95 %; kein Neustart | 45 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-211 | Kein Routing zwischen AP und STA (A-703) | Smartphone, Laptop | kein Gateway per DHCP; AP aus ≤ 1 s nach der STA-IP (E-511: das IDF leitet weiter, solange beide eine Adresse haben); optional: kein Host im Heimnetz erreichbar. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-212 | Keine Klartext-Passwörter im Flash (A-714) | PC (`esptool`) | Web-Passwort nur als Hash; die WLAN-Passphrase liegt nach E-510 bewusst im Klartext in `hfnet` und erscheint sonst nirgends (nicht in `nvs.net80211`, nicht im Log); nach Werksreset weder `hfnet` noch `nvs.net80211` noch Coredump mit Inhalt. | 30 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-213 | `Remote outputs: local only` (A-721) | – | Einschalten aus der Ferne abgelehnt (−221 bzw. 403); Ausschalten erlaubt; Touch schaltet weiter; Option nur am Touch änderbar. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-214 | Antwort-Header (A-707) | – | CSP genau `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`; `nosniff`; `no-referrer`; `/api/*` mit `Cache-Control: no-store`; keine `Access-Control-*`. | im Skriptlauf `web_check.py` / `contract_check.py` | ☐ ✔ ☐ ✘ ☐ ○ |
| M-215 | Grenzen und Rate-Limits (A-710, A-711) | – | wie in A-710 und A-711. | im Skriptlauf `web_check.py` / `contract_check.py` | ☐ ✔ ☐ ✘ ☐ ○ |
| M-216 | Web-Passwort und Sitzung (A-713) | – | wie A-713; das Passwort erscheint nie in einer Ausgabe. | im Skriptlauf `web_check.py` / `contract_check.py` | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-201 · WLAN-Modi und Rückfall-AP

`Bezug` E-501 bis E-507 (Spezifikation Kapitel 5)

1. CONFIG `WiFi` = `Off`: Smartphone sucht Netze; SCPI und Web nicht erreichbar.
2. `Client`: Test-WLAN auswählen, Passwort eingeben; Rückmeldungen (`Connecting…`, `Connected <IP>`) und Zeit bis zur Verbindung (3×).
3. Router abschalten: nach 60 s startet der Rückfall-AP (WPA2), Statusleiste und Rückmeldung `Fallback: AP active`; Router wieder ein: Gerät verbindet sich, der AP schaltet sofort ab.
4. `Access point`: AP erscheint am Smartphone.
5. Falsches Passwort und unbekanntes Netz: Rückmeldungen `Wrong password`, `Network not found`.

**Messwert (Zeiten):**

#### M-202 · AP mit Zufallspasswort und QR, offener AP

`Bezug` E-503, A-701, A-702

1. `Access point`: SSID und Passwort auf dem Display; QR-Code „Join“ mit beiden Smartphones scannen und beitreten.
2. `Regenerate`: neues Passwort; die alte Verbindung wird getrennt.
3. `Open (not recommended)`: Bestätigung am Touch erforderlich; Statusleiste `AP open` in Warnfarbe; höchstens 2 Stationen (dritte abgelehnt); ohne Station nach 15 min aus.

**Messwert:**

#### M-203 · SCPI über USB und TCP

`Bezug` A-722, C-16

1. USB: Port ohne DTR/RTS-Wechsel öffnen (`scpi_usb.py` oder `tools/device_check.py --usb <PORT> --profile M-203`); das Board darf nicht neu starten. 1000× `*IDN?`.
2. TCP: `TCPIP0::<IP>::5025::SOCKET`; 1000× `*IDN?` (`scpi_tcp.py` oder `tools/device_check.py --tcp <IP> --profile M-203`), Median und 99-%-Perzentil der Antwortzeit.
3. Während der USB-Sitzung auf Logzeilen im Datenstrom achten; `SYSTem:LOG?` liefert die Logzeilen.

**Messwert:**

#### M-204 · Ohne SI5351

`Board` ohne Modul

1. Booten ohne Modul; Web öffnen, SCPI verbinden.
2. `OUTPut1:STATus?`, `SYSTem:SI5351?`, Einstellungen ändern; Web-Zustand ansehen.

**Messwert:**

#### M-205 · 24-h-Dauertest

`Bezug` A-723, Heap-Grenzen (A-710)

1. Drei Browser (Live-Updates) und ein SCPI-Skript mit 10 Befehlen/s (Frequenz ändern innerhalb 8–12 MHz, Abfragen) laufen 24 h.
2. `SYSTem:HEAP?` jede Minute protokollieren (freier und minimaler Heap, größter Block, LVGL-Heap, Stack-Reserve).
3. Nach 24 h: Neustart-Zähler (Log), Heap-Verlauf, Fehlerzähler.

**Messwert (Heap min Start / nach 24 h):**

#### M-206 · curl-Matrix (A-701, A-704, A-705, A-710)

`Bezug` A-701, A-704, A-705, A-710

1. `curl -i -H "Host: evil.example" http://<IP>/api/state` → **421**.
2. `curl -i -X POST -H "Content-Type: text/plain" -H "X-CYD-Token: <TOKEN>" --data '{"enabled":true}' http://<IP>/api/channel/1` → **415**.
3. Ohne Header `X-CYD-Token` → **403**.
4. `-H "Origin: http://evil.example"` → **403**; `-H "Sec-Fetch-Site: cross-site"` → **403**.
5. Body 100 KB (`--data-binary @big.json`) → **413**.
6. Über einen **offenen** AP `POST /api/wifi` mit Passwortfeld → **403**.
7. `curl -i -X OPTIONS http://<IP>/api/channel/1` → abgelehnt; keine `Access-Control-Allow-*`-Header.
8. Nach jedem Schritt: Zustand der Kanäle unverändert.

**Messwert:**

#### M-207 · Fremde Webseite (A-705, A-706)

1. Testseite mit `python -m http.server 8000` (andere Herkunft); darauf `fetch('http://<IP>/api/channel/1', {method:'POST', mode:'no-cors', body:'{"enabled":true}'})`, ein `<form enctype="text/plain" method="POST">` mit JSON im Feldnamen und `new WebSocket('ws://<IP>/ws')`.
2. Seite öffnen; Zustand des Geräts prüfen.
3. **DNS-Rebinding:** in der `hosts`-Datei `evil.example` auf `<IP>`; `http://evil.example/` öffnen.

**Messwert:**

#### M-208 · SCPI-Port aus dem Browser (A-717, A-718)

1. `curl -X POST --data-binary $'OUTP1 ON\n' http://<IP>:5025/` → Verbindung wird geschlossen, CH1 bleibt aus.
2. `python -c "import sys; sys.stdout.write('A'*20000 + 'OUTP1 ON\n')" | ncat <IP> 5025` (eine überlange Zeile, **kein** Zeilenumbruch vor `OUTP1 ON`) → CH1 bleibt aus; `SYSTem:ERRor?` danach −363.
3. Zeile mit HTTP-Header-Muster (`Host: x`) → Verbindung wird geschlossen.

**Messwert:**

#### M-209 · Fremde SSIDs

`Bezug` `test_sanitize`

Pflicht, weil sie den Befund S-7 (XSS über SSIDs, hoch) am Gerät prüft; nur im eigenen Labor.

1. Fremde SSIDs (A-708, A-709): Test-AP mit `<svg onload=alert(1)>`, `a"b\c`, `x\ny`, `\xF0A`, `\xC3`, 32 Byte und 200 Beacons; Scan am Gerät und im Web (nur im eigenen Labor, kleine Sendeleistung)

**Messwert:**

#### M-210 · Lasten

Pflicht, weil sie den Befund S-13 (DoS/Heap) am Gerät prüft; nur im eigenen Labor.

1. Lasten (A-710): Slowloris mit 30 Sockets (`tools/slowloris.py`), WebSocket-Flut, Multipart und Chunked; Touch bedienen; Heap vor, während, nach 2 min

**Messwert:**

#### M-211 · Kein Routing zwischen AP und STA (A-703)

1. AP-Modus: Smartphone verbinden, Gateway/DNS in den Netzwerkdetails ablesen (DHCP ohne Gateway-Option).
2. Mit Verbindung zum Router: der AP schaltet ab (M-201); Zeit von der STA-IP (Log) bis zum Verschwinden des AP am Smartphone messen. Vorab S52b (2026-11-06, Log): AP 0,1 s nach `Connected` aus.
3. Optional (Stufe 3): Laptop am Rückfall-AP mit statischer Route `route add <Heimnetz> mask <Maske> 192.168.4.1`, Zugriff auf einen Host im Heimnetz versuchen.

**Messwert:**

#### M-212 · Keine Klartext-Passwörter im Flash (A-714)

1. Eindeutige Test-Passwörter setzen (WLAN `Testpass-7f3a91`, Web-Passwort `Webpass-2c8d55`).
2. NVS-Partition lesen (Offset und Größe aus der Partitionstabelle): `esptool --port COMx read_flash <offset> <size> nvs.bin`; zusätzlich `read_flash 0 0x400000 full.bin`.
3. `strings nvs.bin full.bin | findstr /i "Testpass Webpass"`.
4. Werksreset auslösen; Dump wiederholen.

**Messwert:**

#### M-213 · `Remote outputs: local only` (A-721)

`Bezug` E-610, A-721

1. CONFIG `Remote outputs` = `local only`.
2. Per SCPI `OUTP1 ON`, per REST `{"enabled":true}` und über die Weboberfläche einschalten.
3. `OUTP1 OFF` und Einschalten am Touch.
4. Statusleiste `REMOTE <Quelle>`; Option per SCPI/Web nicht änderbar.

**Messwert:**

#### M-214 · Antwort-Header (A-707)

1. `curl -i` auf `/`, `/api/state` und eine JS/CSS-Datei; Header lesen.

**Messwert:**

#### M-215 · Grenzen und Rate-Limits (A-710, A-711)

1. URL mit 300 B; Header mit 3 KB; Body 1 KB + 1 B → 413; Multipart und Chunked → 415; Anfrage mit 12 s Pause → Timeout.
2. Fünfte HTTP-Verbindung und vierter WebSocket gleichzeitig.
3. 50 Schreibbefehle in 1 s → 429 ab dem 21.; `GET /api/wifi/scan` zweimal in 10 s (zweite Antwort aus dem Zwischenspeicher).
4. 5 falsche Logins → Sperre 30 s, dann verdoppelt.

**Messwert:**

#### M-216 · Web-Passwort und Sitzung (A-713)

1. Passwort mit 7 Zeichen → abgelehnt; mit 8 Zeichen → angenommen.
2. Login: `Set-Cookie` lesen (HttpOnly, SameSite=Strict, Ablauf 12 h); kaputte `Authorization`- und `Cookie`-Header senden.
3. Antworten und Log auf das Passwort prüfen.

**Messwert:**

---

## 6. Stufe 2 – Entscheidungsmessungen

Jede Messung dieser Stufe trägt eine offene Entscheidung oder einen Handbuchwert. Sie läuft vor der Folgeaufgabe im [ARBEITSPLAN.md](ARBEITSPLAN.md), am besten im selben Zeitraum wie der erste Stufe-1-Lauf (dann sind die Werte zugleich Referenz für M-318 und den Vergleich nach der Migration). Vorher M-004 b.

### 6.1 Entscheidungen und Messungen

Die Schwellen sind **Vorschläge**; sie werden vor der Messung bestätigt oder geändert.

| Entscheidung | Messungen | Vorgeschlagenes Kriterium | Folgeschritt |
|---|---|---|---|
| Standard-Tuning `Fractional` oder `Low noise` (E-206) | M-301, M-302, M-303 (M-110 in Stufe 1) | `Low noise` wird Standard, wenn die Nebenlinien innerhalb ±10 kHz um den Träger im Mittel ≥ 10 dB niedriger sind, die Abweichung ≤ 1,5 Hz bleibt (A-213) und die Aussetzer beim Abstimmen (M-303) selten und kurz sind | FW-01 |
| IQ-Abstimmen ohne Aussetzer | M-304, M-305, M-306 | umsetzen, wenn die PLL-Änderung ohne Reset die Phase hält (M-305, M-306) und der Aussetzer in M-304 > 1 ms stört | FW-02 |
| Ungerade IQ-Teiler bei 180°/270° | M-308 (Auswertung von M-115, M-116) | umsetzen, wenn der Phasenfehler bei ungeradem Teiler um > 1° größer ist als bei geradem | FW-03 |
| Phasen-Feinabgleich | M-307 (Auswertung von M-115, M-116), M-306 | umsetzen, wenn der Phasenfehler systematisch > 1° ist | FW-04 |
| `High-Z` über `CLKx_PDN`, MultiSynth abgeschalteter Kanäle abschalten (E-208) | M-118 (Stufe 1), M-311 | `PDN` für alle abgeschalteten Ausgänge, wenn der Pin dabei hochohmig ist (M-118); MultiSynth abschalten, wenn ein abgeschalteter Kanal in M-311 als Linie > −70 dBc sichtbar ist | FW-05 |
| Abhilfen für Störquellen | M-310 (M-418 optional) | Backlight-PWM, I2C-Takt oder Neuzeichnen ändern, wenn ein Störer > −60 dBc im Frequenzbereich der Anwendung liegt | FW-06 |
| TCXO-Option | M-313 | empfehlen, wenn die Drift nach 15 min Aufwärmzeit > 0,5 ppm/h beträgt | DOK-02 |
| Aussage „50 % Tastverhältnis“ (A-201) | M-309 | Handbuch und A-201 präzisieren, wenn die Abweichung > 2 % beträgt | DOK-01 |
| Pegeltabelle und LO-Hinweise im Handbuch | M-312 (M-419 optional) | Tabelle übernehmen | DOK-01 |
| Aufwärmzeit im Handbuch | M-313 | Zeit bis Drift < 0,1 ppm/10 min | DOK-01 |
| Leitung auf GND: Rückgabecode der IDF (E-212) | M-314 | liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen; nach der Migration wiederholen | S39, S41 |
| Klon-Module (MS5351M) akzeptieren (E-212) | M-315 | Prüfregeln lockern, wenn ein echtes Modul abgelehnt wird | E-212 |
| Druckaussetzer überbrücken, Wiederholung (U-20) | M-316 | Überbrückungszeit aus der Verteilung der Aussetzer (heute bis 30 ms überbrückt, geplant 60–80 ms) | S33 |
| Stift am Laborplatz (U-5) | M-126 (Stufe 1), M-433 optional | Stift empfehlen, wenn der Finger am Rand > 8 px abweicht | Handbuch |
| WLAN während Messungen aus (ab WEB-HMI) | M-317, M-318 | Hinweis, wenn durch WLAN Nebenlinien > −60 dBc sichtbar sind | S58 |
| Heap-Grenzen (A-710, ab WEB-HMI) | M-319, M-205 | Grenzwerte anpassen, wenn der minimale Heap im Dauertest sinkt | MES-04 |

### 6.2 Ablauf

| Block | Messungen | Geräte | Dauer |
|---|---|---|---|
| 0 | M-004 b; M-313 am Morgen nach ≥ 2 h Ruhe starten (läuft 60 min unbeaufsichtigt) | SPEKTRUM, ZÄHLER | 30 min |
| Tuning | M-301, M-302, M-303 | SPEKTRUM, OSZI | 1 h 30 min |
| IQ | M-304, M-305, M-306, M-309; Auswertung M-307, M-308 | OSZI, ZÄHLER | 1 h 30 min |
| Störungen | M-310, M-311, M-312 | SPEKTRUM | 1 h 25 min |
| Bus und Touch | M-314, M-315, M-316 | PC (Log), LOGIK optional | 45 min |
| ab WEB-HMI | M-317, M-318, M-319 | SPEKTRUM, PC | 1 h 25 min |
| | **Summe** | | **≈ 6 h + 1,5 h** |

### 6.3 Tuning-Modi

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-301 | Tuning-Modi im Vergleich | SPEKTRUM; Aufbau 2 | zu dokumentieren; Grundlage für den Standardmodus (6.1). Die Zeile `Fractional` ersetzt M-302 b. | 45 min | i |
| M-302 | Nebenlinien um den Träger | SPEKTRUM; Aufbau 2 | zu dokumentieren. | 20 min | i |
| M-303 | Abstimmen in `Low noise` | OSZI | kein Aussetzer, solange der Teiler bleibt (Teiler 90 deckt 6,67 … 10 MHz ab); ein Aussetzer beim Übergang über 10,000 MHz (Teilerwechsel 90 → 88); dokumentieren. | 25 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-301 · Tuning-Modi im Vergleich

`Board` Messboard · `Bezug` E-206, FW-01

1. CH1 = 7,1 MHz; Span 200 kHz, RBW 100 Hz, Mittelung 10; nacheinander `Fractional`, `Low noise`, `Integer` (bei `Integer` auf die Frequenz laut `Out` zentrieren, 7,102 273 MHz).
2. Größte Nebenlinien in dBc und Abstand; Phasenrauschen bei 1 kHz Ablage.

| Modus | Nebenlinien (dBc @ Abstand) | L(1 kHz) | Abweichung `Out` (Hz) |
|---|---|---|---|
| Fractional | | | |
| Low noise | | | |
| Integer | | | |

**Messwert:** in der Tabelle oben

#### M-302 · Nebenlinien um den Träger

`Bezug` E-206, FW-01

1. Einstellungen wie M-301, `Fractional`, Korrektur 0.
2. a) 10,000 000 MHz (ganzzahliger Teiler); c) 7,100 000 MHz mit CH3 an (IQ: ganzzahliger Teiler, fraktionale PLL). Fall b (7,1 MHz fraktional) ist die Zeile `Fractional` in M-301.
3. Größte Nebenlinien innerhalb ±100 kHz in dBc und Abstand.

| Zustand | Nebenlinien (dBc @ Abstand) | Eigenrauschen-Reserve (dB) |
|---|---|---|
| a) 10 MHz ganzzahlig | | |
| c) 7,1 MHz + CH3 | | |

**Messwert:** in der Tabelle oben

#### M-303 · Abstimmen in `Low noise`

`Bezug` A-206, E-206, FW-01

1. `Tuning` = `Low noise`; OSZI mit Timeout-Trigger auf CLK0.
2. CH1 von 9,990 bis 10,010 MHz in 1-kHz-Schritten (je `+` einmal); außerdem 7,0 bis 7,2 MHz.
3. Aussetzer zählen, Frequenz notieren.

**Messwert (Aussetzer, Dauer):**

### 6.4 IQ-Abstimmen und Phase

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-304 | IQ: Master abstimmen (Realignment) | OSZI; Aufbau 3 | Aussetzer beider Ausgänge bei **jedem** Schritt (erwartet, auch bei einer Korrekturänderung); Dauer notieren. | 15 min | i |
| M-305 | PLL-Änderung ohne Reset (nur Testbuild) | OSZI, ZÄHLER, Soundkarte oder SDR | zu dokumentieren; die IQ-Phase ist nach der Änderung unverändert (±1°); die Einschwingzeit ist so kurz, dass sie bei Versuchen nicht stört. | 30 min | i |
| M-306 | Wiederholgenauigkeit nach Realignment | OSZI; Aufbau 3 | nach jedem Realignment wieder 90° (Mittelwert ± 1° + u_φ), Streuung < 0,5°. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-307 | Ausgangs-Skew bei Phase 0° (Auswertung) | OSZI | konstanter Versatz (± u_t); Phasenfehler ≤ 1° bis 99 MHz, sonst Skew-Kompensation (FW-04). | 10 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-308 | 180° bei ungeradem Teiler (Auswertung) | – | Phasenfehler bei ungeradem Teiler höchstens 1° größer als bei geradem; sonst FW-03. | 5 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-309 | Tastverhältnis | OSZI | zu dokumentieren; die Doku verspricht 50 %. Abweichung > 2 % → Handbuch und A-201 präzisieren. | 15 min | i |

#### M-304 · IQ: Master abstimmen (Realignment)

`Bezug` A-218, FW-02

1. CH1 = 10 MHz an, CH3 an (Master CH1, 90°). OSZI: beide Kanäle, Timeout-Trigger auf CLK0, Single.
2. Schrittweite 1 kHz, `+` einmal tippen; Aussetzer beider Ausgänge messen (Dauer). 10 Wiederholungen.

**Messwert (Dauer min / max in µs):**

#### M-305 · PLL-Änderung ohne Reset (nur Testbuild)

`Bezug` FW-02, E-206 · Build: PLL ohne Reset (zu erstellen)

1. Testbuild flashen; Tuning `Low noise`.
2. CH1 springt von 10,000 000 auf 10,001 000 MHz (NUMPAD und Schritttaste); SDR-Wasserfall oder ZÄHLER mit 1 ms Torzeit.
3. Einschwingzeit und Überschwingen (Hz, ms) notieren; im IQ-Betrieb (CH3 90°) danach die Phase messen (Aufbau 3).
4. Ab S35: auch ein Schritt über 112,5 MHz (dort setzt heute jeder Schritt die PLL zurück).

**Messwert:**

#### M-306 · Wiederholgenauigkeit nach Realignment

`Bezug` A-218, FW-02, FW-04

1. CH1 = 10 MHz, CH3 90°. Master 10× um 1 kHz verstellen (`+`, `−` abwechselnd).
2. Nach jedem Schritt (2 s Wartezeit) φ messen; Mittelwert und Standardabweichung.

**Messwert (Mittel, σ):**

#### M-307 · Ausgangs-Skew bei Phase 0° (Auswertung)

`Bezug` A-216, FW-04

1. Δt bei 0° aus M-115 (10 MHz) und M-116 (4,73 / 14,2 / 99 MHz) übernehmen; optional 30 und 50 MHz bei 0° nachmessen.
2. Prüfen, ob Δt unabhängig von f ist (konstanter Skew) und φ = Δt · f · 360° die Abweichungen in M-115 und M-116 erklärt.

**Messwert (Δt je Frequenz):**

#### M-308 · 180° bei ungeradem Teiler (Auswertung)

`Bezug` A-217, FW-03

1. Phasenfehler bei 180° und 270° aus M-116 (14,2 MHz, Teiler 63; 99 MHz, Teiler 9) gegen M-115 (10 MHz, Teiler 90) vergleichen.

**Messwert:**

#### M-309 · Tastverhältnis

`Bezug` A-201, DOK-01

1. Tastverhältnis = positive Impulsbreite / Periode, Schwelle 50 %, ≥ 100 Perioden gemittelt.
2. CLK0 bei 7,1 MHz (fraktional) und 10 MHz (ganzzahlig), CLK2 bei 14,2 MHz (IQ, Teiler 63) und 99 MHz (Teiler 9).

**Messwert (% je Punkt):**

### 6.5 Störungen und Pegel

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-310 | Störungen durch die Plattform | SPEKTRUM | zu dokumentieren; Entscheidung über Abhilfen (6.1). | 45 min | i |
| M-311 | Übersprechen CH1 ↔ CH2 | SPEKTRUM | zu dokumentieren (Übersprechen mit CH2 aus bzw. an). | 20 min | i |
| M-312 | Ausgangspegel an 50 Ω (gekürzt) | SPEKTRUM | zu dokumentieren (Pegeltabelle fürs Handbuch; LO-Pegel für Mischer). | 20 min | i |
| M-313 | Aufwärmdrift | ZÄHLER, REF, THERMO | zu dokumentieren; Aufwärmzeit fürs Handbuch (Zeit bis Drift < 0,1 ppm/10 min); TCXO-Entscheidung (6.1). M-411 lässt sich aus derselben Aufzeichnung auswerten. | 60 min (15 min aktiv; Start nach ≥ 2 h Ruhe) | i |

#### M-310 · Störungen durch die Plattform

`Board` Messboard (zweites Board optional, Stufe 3) · `Bezug` E-310, FW-06

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

**Messwert:** in der Tabelle oben

#### M-311 · Übersprechen CH1 ↔ CH2

`Bezug` FW-05

1. CH1 = 10 MHz an; CH2 = 10,1 MHz zuerst aus (MultiSynth läuft weiter), dann an.
2. Spektrum an CLK0: Linie bei 10,1 MHz in dBc.

**Messwert:**

#### M-312 · Ausgangspegel an 50 Ω (gekürzt)

`Bezug` DOK-01

1. CH1 an; Pegel der Grundwelle an 50 Ω (mit Dämpfungskorrektur): bei 10 MHz für Drive 2 / 4 / 6 / 8 mA, bei 1 / 30 / 100 MHz für 8 mA. Die volle Tabelle (alle Frequenzen × alle Drives) ist optional.

| f (MHz) | 2 mA (dBm) | 4 mA | 6 mA | 8 mA |
|---|---|---|---|---|
| 1 | – | – | – | |
| 10 | | | | |
| 30 | – | – | – | |
| 100 | – | – | – | |

**Messwert:** in der Tabelle oben

#### M-313 · Aufwärmdrift

`Bezug` DOK-02, DOK-01

1. Board und Modul mindestens 2 h stromlos, Raumtemperatur notieren.
2. Einschalten, CH1 = 10 MHz (Korrektur nach M-108), Torzeit 10 s, Aufzeichnung als `.csv` (`counter_log.py`, Anhang D).
3. 60 min laufen lassen; Raumtemperatur alle 15 min.
4. Drift in ppm über der Zeit auftragen.

**Messwert (Drift 0–15 min / 15–60 min in ppm):**

### 6.6 Bus und Touch

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-314 | Leitung dauerhaft auf GND | PC (Log), LOGIK optional; Aufbau 5 | Code 5; Dialog „I2C line stays low …“; die GUI stockt < 100 ms; nach Entfernen verbunden. Liefert die IDF nicht 5: `Blocked` und `NotFound` zusammenlegen (E-212). Nach der Migration (S41) wiederholen; der Fake-Test aus S39 bildet die Annahme ab. | 15 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-315 | Registerdump und Klonwarnung | PC (Log) | Werte notieren; das Modul wird angenommen; Log ohne „Unexpected reg183“ (sonst Klonwarnung und Info-Karte `(clone?)`). | 10 min (+ 10 min je Klon) | ☐ ✔ ☐ ✘ ☐ ○ |
| M-316 | CONFIG wischen, Druckaussetzer (U-20, E-311) | – | 0 ungewollte Auslösungen; Verteilung der Aussetzer dokumentiert (Grundlage für die Überbrückungszeit in S33); keine Aussetzer > 70 ms bei normalem Druck (Überbrückung seit S33 70 ms). | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-314 · Leitung dauerhaft auf GND

`Board` Messboard · `Bezug` A-223, E-212, S39 · Build: Debug

1. SDA über ≤ 100 Ω nach GND legen, dann SCL.
2. Rückgabecode im Debug-Log lesen; GUI-Bedienung prüfen (Tipps, Frame-Zeit).
3. Fehler entfernen.

**Messwert (Code vor / nach Migration):**

#### M-315 · Registerdump und Klonwarnung

`Board` Messboard · `Bezug` A-222, E-212 · Build: Debug

1. Register 0, 3, 183 nach dem Einschalten aus dem Log lesen (eigenes Modul; wenn vorhanden auch MS5351M).

**Messwert:**

#### M-316 · CONFIG wischen, Druckaussetzer (U-20, E-311)

`Board` beide · `Bezug` E-311, S33 · Build: Touch-Log

1. CONFIG: auf freier Fläche, auf Tasten (`Corr. +` mit Wiederholung) und auf Dropdowns wischen, je leicht und fest, je 10×.
2. Während der Einrast-Animation einmal tippen.
3. `+` auf der CH1-Seite 5 s halten (leicht und fest); Druckaussetzer (`gap`, `drop`) im Touch-Log zählen und ihre Dauer notieren.

**Messwert (Auslösungen, Aussetzer min / max / Anzahl):**

### 6.7 Ab WEB-HMI

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-317 | WLAN-Störungen im Spektrum | SPEKTRUM; Aufbau 2 | zu dokumentieren; bei Linien > −60 dBc im nutzbaren Bereich Hinweis im Handbuch. | 45 min | i |
| M-318 | WLAN aus entspricht 1.3.x | SPEKTRUM | Spektrum wie R0 (Linienpegel ± 3 dB, keine neuen Linien). | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |
| M-319 | Flash, RAM und Heap gegen die Basis | PC | Abweichung > 10 % begründen; minimaler Heap ≥ 40 KB, größter Block ≥ 16 KB (Vorgabe des früheren WEB-HMI-Konzepts; die Grenzen des Servers stehen in A-710). Die R0-Vorwerte stammen vom Original-CYD ohne Modul mit dem Stand von S05/S06; beim ersten Stufe-1-Lauf durch Werte von 1.3.x ersetzen, wenn sie abweichen. | 20 min | ☐ ✔ ☐ ✘ ☐ ○ |

#### M-317 · WLAN-Störungen im Spektrum

`Bezug` WLAN-Vorabmessung (MES-03), M-302

1. CH1 = 10 MHz an; Span 200 kHz, RBW 100 Hz, zusätzlich Span 2 kHz, RBW 10 Hz (Beacons im Abstand ≈ 102,4 ms können Seitenlinien im Abstand ≈ 10 Hz erzeugen).
2. WLAN `Off`; `Client` im Leerlauf; `Client` mit Datenverkehr (Ping-Flut, drei Browser mit Live-Updates, SCPI-Skript 10 Befehle/s); `Access point` mit einer Station.
3. Neue Nebenlinien (Abstand, dBc) gegenüber `Off` notieren.

**Messwert:**

#### M-318 · WLAN aus entspricht 1.3.x

1. WLAN `Off`; M-302 a und M-310 a wiederholen; mit den R0-Werten vergleichen.

**Messwert:**

#### M-319 · Flash, RAM und Heap gegen die Basis

`Bezug` Build-Gate, Heap-Grenzen (A-710) · Build: Release und Debug

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

**Messwert:**

---

## 7. Stufe 3 – Optional

Diese Messungen sind **kein Freigabekriterium**. Sie laufen bei Anlass (Befund, Ausbildungsversuch, Rückfrage) oder wenn Zeit ist. Die Einträge sind knapp gehalten; Aufbauten, Methoden und Bewertung wie in Abschnitt 3. Ergebnis je Zeile: ✔ ✘ ○ i –.

### 7.1 Robustheit und Fehlerfälle

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-401 | IQ-Fehlerzustand: CH1 10 MHz an, CH3 an (Master CH1), 270°; Status auf MAIN, Header, CH3-Seite; Dialoge zählen; dann 90° | OSZI, Build IQ-Fehler | CH3 `ERR` (Infozeile `IQ setup failed, retrying`), **ein** Dialog mit Kanal und Abhilfe, CH1 läuft ungestört; bei 90° nach ≤ 2 s `ON`, Log `[APP] Output error cleared` (host-getestet: `test_fault_iq`) | 10 min | ☐ |
| M-402 | CLK2 im IQ-Fehlerzustand (C-1), im selben Flash wie M-401: `Off state` `High`, `Low`, `High-Z` je 1 min bei `ERR` beobachten | OSZI (DC, 1 MΩ), Build IQ-Fehler | CLK2 schwingt **nicht**, nur der Ruhepegel (High ≈ 3,3 V, Low 0 V, High-Z undefiniert); Register host-getestet | 15 min | ☐ |
| M-403 | Wackelkontakt: CH1 an, 30 s SDA-Kabel wackeln lassen; Log und Zähler | ZÄHLER | Frequenz nach Beruhigung richtig; eine Logzeile je Zustandswechsel | 10 min | ☐ |
| M-404 | Modul ohne (bzw. mit abgetrennten) Pull-ups; booten, 10 min CH1 an | LOGIK | Erkennung bei 100 kHz; Betrieb mit 400 kHz stabil **oder** Log „I2C fails at 400 kHz; staying at 100 kHz“ und Betrieb mit 100 kHz stabil (host-getestet) | 20 min | ☐ |
| M-405 | Dauer ohne Modul: Board ohne Modul, Debug-Build, 10 min; Heap alle 30 s; kann nebenbei während Stufe 1 laufen | Build Debug | Heap konstant (ein `Wire.end()`/`begin()` pro 2 s), keine wiederholten Logzeilen | 15 min | ☐ |
| M-406 | Vertauschte Adern, Aktivität am Chip: LOGIK direkt am Modul, Kabel vertauscht, booten | LOGIK, Aufbau 5 | nur START/STOP und Fragmente, keine Schreibzugriffe auf Register 16 bis 183 (host-getestet) | 15 min | ☐ |
| M-407 | 3,3 V offen: Modul nur mit GND und SDA/SCL booten; danach 3,3 V anschließen | LOGIK, DMM | „not found“, „line stays low“ oder „not an SI5351“, **kein Teilschreiben**; nach Anschluss von 3,3 V verbunden | 10 min | ☐ |
| M-408 | GND offen, sonst wie M-407 | LOGIK, DMM | wie M-407 | 10 min | ☐ |
| M-409 | Reset während I2C-Zugriff: Debug-Build mit Dauerlesen, 20× EN | LOGIK, Build Debug | nach jedem Boot verbunden (`[I2C] Line held low; releasing bus` erlaubt; host-getestet) | 20 min | ☐ |
| M-410 | Fremdgerät auf 0x60 an CN1, booten | LOGIK, I2C-Modul 0x60 | Dialog „The device at I2C address 0x60 is not an SI5351“; höchstens ein Schreibzugriff (Register 3); keine weiteren Dialoge (host-getestet) | 15 min | ☐ |

### 7.2 HF-Charakterisierung und Ausbildung

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-411 | Kurzzeitstabilität: nach der Aufwärmzeit CH1 10 MHz, Torzeit 1 s, 100 Werte (oder aus M-313); Standardabweichung und Allan-Abweichung für τ = 1 s | ZÄHLER, REF | zu dokumentieren | 15 min | ☐ |
| M-412 | Phasendrift: CH1 10 MHz, CH3 90°, 30 min alle 5 min φ, Raumtemperatur | OSZI, THERMO | Drift < 0,5° in 30 min | 35 min | ☐ |
| M-413 | Ausbildungsaufbau: IQ an QSD (FST3253) oder zwei Mischer; Testsignal ±1 kHz um den LO; Seitenbandunterdrückung | SPEKTRUM, QSD | passt zu M-115, M-116, M-307 (1° ≈ 41 dB, 2° ≈ 35 dB) | 45 min | ☐ |
| M-414 | Oberwellen: CH1 10 MHz, Drive 2 / 4 / 6 / 8 mA; Grundwelle (dBm) und 2. bis 5. Harmonische (dBc), Span 0 … 60 MHz, RBW 1 kHz | SPEKTRUM | zu dokumentieren; Rechteck mit 50 %: 3. ≈ −9,5 dBc, 5. ≈ −14 dBc, 2. und 4. sehr klein (sonst M-309 prüfen) | 30 min | ☐ |
| M-415 | Wechsel ganzzahlig → fraktional: wie M-302 a mit Korrektur 0 und +0,01 ppm | SPEKTRUM | Nebenlinien erscheinen; dokumentieren | 15 min | ☐ |
| M-416 | Phasenrauschen: CH1 10 und 14,2 MHz, Rauschmarker bei 100 Hz, 1 kHz, 10 kHz, 100 kHz; Eigenrauschen aus M-004 b daneben | SPEKTRUM, REF | zu dokumentieren; < 6 dB über Eigenrauschen = obere Schranke (○) | 45 min | ☐ |
| M-417 | Spektrum CLK2 gegen CLK0: CH3 IQ bei 4,73 und 10 MHz | SPEKTRUM | an CLK2 keine zusätzlichen Nebenlinien | 20 min | ☐ |
| M-418 | Nahfeld: Ausgänge aus, WLAN aus; Sonde ≈ 1 cm über Display, ESP32-Modul, Ausgangsleitungen, Span 1–500 MHz; danach mit CH1 10 MHz (Mischprodukte?) | SPEKTRUM, SONDE | zu dokumentieren (40-MHz-Quarz, SPI 40 MHz, 25-MHz-Quarz des SI5351); ergänzt FW-06 und DOK-03 | 45 min | ☐ |
| M-310 (Board 2) | M-310 am zweiten Board (Modul umstecken) | SPEKTRUM | wie M-310 am Messboard | 45 min | ☐ |
| M-419 | Diodenringmischer: `Off state` `Low`; LO = CH1 10 MHz auf +7 dBm (nach M-312), RF = CH2 10,455 MHz über 30 dB; ZF bei 455 kHz und 20,455 MHz; Konversionsverlust, LO-Durchschlag | SPEKTRUM, MISCHER, Aufbau 4 | ZF bei 455 kHz und 20,455 MHz, Konversionsverlust ≈ 6–7 dB; LO-Durchschlag dokumentieren (DOK-01) | 45 min | ☐ |
| M-420 | Schwebung an der Soundkarte: CH1 10,000 MHz und CH2 10,001 MHz an den Mischer, ZF an Line-In, FFT ≥ 96 kHz | MISCHER, Soundkarte | sauberer 1-kHz-Ton; Seitenlinien und Rauschsockel dokumentieren (DOK-01) | 30 min | ☐ |
| M-421 | Oberwellen bei 455 kHz (ab S35): Spektrum 0 … 5 MHz | SPEKTRUM | zu dokumentieren; Tiefpass-Hinweis im Handbuch | 10 min | ☐ |
| M-422 | Spread −0,5 % bei 10 MHz: Span 100 kHz um 9,975 MHz, RBW 1 kHz, Max-Hold; CH2 7,1 MHz am Zähler; Modulationsrate (Zero-Span an der Bandkante oder SDR-FM); `Out`-Zeile | SPEKTRUM, ZÄHLER | Band 9,95 … 10,00 MHz; Modulation ≈ 31,5 kHz; CH2 unverändert (±0,5 Hz + u); `Out 9.975 000 MHz mean, SSC -0.5 %` (Register host-getestet) | 30 min | ☐ |
| M-423 | Spread mit IQ: `+/-0.5 %`, CH3 90°; Δt mit Spread aus und ein; Spektren beider Ausgänge | OSZI, SPEKTRUM | Phase bleibt (≤ 1° + u_φ); beide Ausgänge gleich breit | 30 min | ☐ |

### 7.3 Bedienung

Für die Tests mit Personen gelten die Bedingungen aus 2.4; Auswertung des Touch-Logs mit `touchstat.py` (Anhang D).

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-424 | Numpad schließen mit Finger-Nachhalten: „5“ `kHz` bestätigen und Finger 0,5 s liegen lassen; dann „100“ `Hz` | Bediener | Kanalseite erscheint; das Numpad öffnet **nicht** erneut; Schrittweite unverändert | 5 min | ☐ |
| M-425 | Trefferquote echter Ziele (U-4, U-6): P1 bis P5 je 20 Tipps auf Dialog-`OK`, `MAIN`, Phase `90`/`180`, Chips `1`/`1M`, `Corr. −`/`+`, CONFIG-Listeneintrag, Numpad `◀`/`C` | 5 Personen, Touch-Log | ≤ 2 % Fehltipps; **0** Nachbartreffer bei Phase und Schaltern | 45 min | ☐ |
| M-426 | Karte öffnen ohne Schalten (U-3): CH1 OFF, 20× Karte über Frequenz bzw. `MHz` öffnen | P1, P2 | 0 ungewollte Schaltvorgänge | 15 min | ☐ |
| M-427 | Halten über 5 s (U-20, enthält M-504): Step 1 Hz, `+` 5 s halten (leicht und fest); Schritte zählen, erster Schritt und Takt aus der Frequenzänderung bzw. Video | Bediener, Touch-Log | heute erster Schritt nach ≈ 0,4 s, dann alle ≈ 0,1 s, ≈ 47 Schritte, kein Flackern, keine Unterbrechung; nach S33 die dort festgelegten Werte | 10 min | ☐ |
| M-428 | Lesbarkeit aus der Entfernung (U-8): ≈ 500 lx; MAIN mit 10 Zustandskombinationen; 0,4 / 0,7 / 1,0 / 1,5 m, 0° / 30° / 45°, Backlight 30 / 80 / 100 % | P1 bis P5, LUX, VIDEO | ≥ 95 % bei 1,0 m für ON/OFF und Frequenz; Grenzentfernung je Element dokumentieren | 60 min | ☐ |
| M-429 | Farbsehschwäche (U-7, U-19): Testperson mit Rot-Grün-Schwäche oder Fotos aus M-123 mit Simulationsfilter | 1 Person oder Fotos | alle Zustände auch ohne Farbe unterscheidbar (Kontraste host-getestet: `test_contrast`) | 10 min | ☐ |
| M-430 | Aufgabentest (Anhang C): T1 bis T7 erst ohne, dann mit Handbuch; laut denken; SUS und Verständnisfragen | 5 Lernende + Lehrende, VIDEO | Erfolg ohne Handbuch ≥ 80 % (T1 bis T3), SUS ≥ 70; Verständnisfragen ≥ 80 % richtig | 5 × 30 min | ☐ |
| M-431 | Berührung beim Einschalten (U-10, enthält M-516): 20× einschalten mit Daumen auf dem Display (0,5 bis 3 s, einmal > 3 s), einmal 1 s berühren und halten, bis die Hauptseite erscheint | Bediener, VIDEO | 20/20 richtiges Bild; kein Tipp landet in der GUI (seit 1.3.2 keine Boot-Geste, E-302) | 15 min | ☐ |
| M-432 | Fehlerdialoge mit Testperson: M-103 und M-105 ausführen; Person liest vor und erklärt die Abhilfe | 1–2 Personen | Meldung verstanden; `OK` im ersten Versuch (U-21) | 20 min | ☐ |
| M-433 | Eingabestift (U-5): M-126 und M-425 mit dem Stift (P1 bis P3) | 3 Personen, Trefferkarte | zu dokumentieren; Entscheidung „Stift am Laborplatz“ (6.1) | 30 min | ☐ |
| M-434 | Spannungsausfall bemerken (U-22): CH1 an, Stecker unangekündigt ziehen; Zeit bis zum Bemerken; nach dem Anstecken: weiß die Person, dass sie einschalten muss? Änderung < 5 s vor dem Ziehen | 1–2 Personen | Ausfall innerhalb 30 s bemerkt; Verlust < 5 s alter Änderungen steht im Handbuch (Autosave host-getestet) | 20 min | ☐ |
| M-435 | CONFIG scrollen (U-24): Einrasten, Scrollposition nach MAIN → CONFIG; eine Person, die CONFIG nicht kennt, sucht `Off state` ohne Hinweis | 1 Person, VIDEO | rastet an Zeilenkanten ein; Wischen löst nichts aus (M-316); Scrollposition erhalten; Person erkennt ohne Hinweis, dass die Seite scrollt | 20 min | ☐ |
| M-436 | Backlight (ersetzt die frühere Messung B1): `−` bis 10 %, `+` bis 100 %; Halten wiederholt nicht; Lesbarkeit bei 10 % (≈ 500 lx); Wert nach Neustart | Bediener, LUX optional | Stufen 10 … 100 % (host-getestet); bei 10 % noch lesbar; Halten wiederholt nicht; Wert bleibt | 15 min | ☐ |

### 7.4 Qualität

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-437 | Stromaufnahme, Mittel über 10 s: (a) Boot, Ausgänge aus, Backlight 100 %; (b) Backlight 10 %; (c) CH1 bis CH3 an (10 MHz, 8 mA, 50 Ω); (d) WLAN `Client` im Leerlauf; (e) `Access point` ohne Station; (f) 3 Web-Clients; (g) Spitzenwerte | STROM | zu dokumentieren; Hinweis für Netzteil oder Powerbank | 30 min | ☐ |
| M-438 | Bootzeit: Zeit bis zum ersten Bild, zur Hauptseite und zum Verbinden des Moduls, aus den Videos von M-122 | VIDEO | zu dokumentieren; R1 höchstens 1 s länger als R0 | 10 min | ☐ |

### 7.5 WEB-HMI (optional, erst wenn die WEB-HMI existiert)

Was hier steht, ist großteils durch die Host-Suites der WEB-HMI abgedeckt oder Komfort. Aufbau 7; nur im eigenen Labornetz.

| ID | Was | Aufbau / Geräte | Bestanden, wenn | Dauer | Ergebnis |
|---|---|---|---|---|---|
| M-439 | Zwei WLANs speichern (A stärker); A ab, dann A ein und B ab; RSSI und gewähltes Netz · Bezug: WiFiMulti | Aufbau 7 | das stärkere Netz gewinnt; bei Misserfolg Wechsel | 30 min | ☐ |
| M-440 | Statische IP: gültige Werte; ungültige (IP = Gateway, Broadcast, Netzadresse, außerhalb des Subnetzes) · Bezug: A-712, `test_net_settings` | Aufbau 7 | gültige übernommen, ungültige mit Klartextmeldung abgelehnt | 20 min | ☐ |
| M-441 | mDNS und Captive-DNS: `ping <name>.local` und `http://<name>.local` auf Windows, Android, iOS; im AP-Modus öffnet sich die Einrichtungsseite; im Client-Modus `nslookup example.com <Geräte-IP>` ohne Antwort · Bezug: E-506, A-704 | Aufbau 7 | Name löst auf; Captive-DNS nur im AP-Modus | 45 min | ☐ |
| M-442 | Zweite SCPI-Verbindung (`nc <IP> 5025`) wird abgelehnt (Info-Karte `connected` / `rejected <IP>`); Leerlauf-Trennung nach 10 min; `Reconnect all` · Bezug: A-720 | Aufbau 7 | wie angegeben | 30 min | ☐ |
| M-443 | Web auf drei Clients: Änderungen live; Feld mit Fokus wird nicht überschrieben; vierter Client „Too many clients (max. 3)“; Netz trennen → Seite grau | Aufbau 7 | wie angegeben | 30 min | ☐ |
| M-444 | Tastaturbedienung im Browser: `↑`/`↓`, `←`/`→`, `Bild↑`/`Bild↓`, Mausrad, `1`/`2`/`3`, `F`/`Enter`/`Esc`, `O`, `?`; `Space` · Bezug: `data/www/README.md` | Aufbau 7 | Belegung wie in `data/www/README.md`; `Space` schaltet nichts | 20 min | ☐ |
| M-445 | Zweistufige Netzänderung: ohne Bestätigung Rückfall nach 120 s; mit Bestätigung übernommen; ohne Web-Passwort zusätzlich Touch-Bestätigung · Bezug: E-618, A-712 | Aufbau 7 | Rückfall nach 120 s (± 5 s) | 20 min | ☐ |
| M-446 | Einrichtung durch Novizen: W1 bis W3 (Anhang C), mit und ohne QR-Hilfe; war „mobile Daten aus“ nötig? | Aufbau 7; 5 Personen | ≥ 80 % Erfolg ohne Hilfe, ≤ 5 min je Aufgabe | 5 × 15 min | ☐ |
| M-447 | 16-stelliges Passwort mit der Bildschirmtastatur (Finger und Stift) | Aufbau 7; 5 Personen | Abbruchquote < 10 %, mittlere Zeit < 3 min (Vorschlag) | 5 × 10 min | ☐ |
| M-448 | Fremde Änderungen bemerken (Hervorhebung, `REMOTE <Quelle>`) | Aufbau 7; 1 Person | ≥ 90 % bemerkt | 20 min | ☐ |
| M-449 | T1 bis T3 (Anhang C) im Browser nur mit der Tastatur | Aufbau 7; 1 Person | ≤ 1 Fehlbedienung je Aufgabe | 30 min | ☐ |
| M-450 | `Identify` (bzw. `SYSTem:IDENtify`) mit ≥ 3 Geräten im Raum · Bezug: E-613 | Aufbau 7 | richtiges Gerät in ≥ 95 % | 20 min | ☐ |
| M-451 | Großansicht am Beamer aus 5 m | Aufbau 7 | Frequenzen und Zustände lesbar | 15 min | ☐ |
| M-452 | SCPI-Befehlsbaum vollständig: `tools/device_check.py --profile M-452` mit gültigen und ungültigen Parametern, `SYSTem:ERRor?` nach jedem Befehl, Stichproben am ZÄHLER · Bezug: A-717, A-719, `test_scpi` | Aufbau 7 | alle Erwartungen erfüllt; keine Abstürze; Frequenzen nur als Ganzzahlen | 60 min | ☐ |
| M-453 | REST: `GET /api/state` gegen die Anzeige; alle Schlüssel in `POST /api/config` und `/api/channel/<n>`; unbekannte Schlüssel und falsche Typen (`{"drive":"4"}`, `{"drive":4.5}`) · Bezug: `test_rest_json` | Aufbau 7 | Zustand stimmt; Fehler 400 mit `{"error":"-2xx,…"}` | 30 min | ☐ |
| M-454 | `*RST` und Speichern: `*RST` → Ausgänge aus, Standardwerte; stromlos → alte Werte zurück; `*RST` + `SYSTem:SAVE` + Neustart → Standardwerte · Bezug: E-608, C-17, `test_command` | Aufbau 7 | `*RST` speichert nicht; nach `SYSTem:SAVE` gelten die Standardwerte | 15 min | ☐ |
| M-455 | Gerätename, mDNS, IPv6 (A-704, A-716): ungültige Namen und SSIDs abgelehnt; TXT-Einträge ohne Version und Commit; `Host: <name>.local` und `<IP>` akzeptiert, anderer Host 421; `curl -6` ohne Verbindung | Aufbau 7; `dns-sd` / `avahi-browse` | wie A-716 und A-704 | 20 min | ☐ |
| M-456 | WPA2 und PMF (A-715): Router-Log bzw. Wireshark; RSN-Element der AP-Beacons; Router nur mit WPA/TKIP | Aufbau 7; Wireshark optional | WPA2 mindestens; PMF capable; WPA/TKIP abgelehnt | 30 min | ☐ |

---

## 8. Entfallen und erledigt

### 8.1 Entfallen

Die Nummern bleiben reserviert, damit Verweise auflösbar bleiben.

| ID | Titel | Grund |
|---|---|---|
| M-501 | Start ohne Rauschen | in M-122 aufgegangen (Kaltstarts mit Blick auf Beleuchtung und erstes Bild) |
| M-502 | Flashen beider Boards | jeder Smoke-Test flasht beide Boards mit derselben Firmware; CI baut die Umgebungen; die Erkennung prüft M-122 |
| M-503 | Pegel im Zustand OFF, Werkseinstellung | in M-117 aufgegangen (M-117 misst `Low` jetzt an CLK0 bis CLK2) |
| M-504 | Wiederholung beim Halten | in M-427 aufgegangen (M-427 misst ersten Schritt und Takt mit) |
| M-505 | Numpad: ungültige und gültige Eingaben | Logik host-getestet (`test_input_logic`: Grenzen, Fehlertexte); Anzeige in M-123 Schritt 2 |
| M-506 | Ziffer antippen | host-getestet (`charIndexAt`, Tap-Ziel, `stepMarkup`); Treffsicherheit zeigt M-126 |
| M-507 | Trefferquote Ziffer-Antippen | entschieden in S26 (Ziffer-Antippen ist Komfortfunktion); Zuordnung host-getestet, Touch-Genauigkeit in M-126 |
| M-508 | Korrektur +50 ppm bei kleinen Frequenzen | Korrekturrechnung und VCO-Grenzen host-getestet (S14, C-7); am Gerät nichts Neues gegenüber M-108, M-109 |
| M-509 | Korrektur um 1 ppm ändern | in M-114 Schritt 3 aufgegangen; kein PLL-Reset host-getestet (`test_correction_change_does_not_reset_pll`) |
| M-510 | Hz-Schritte bei hoher Frequenz | 1-Hz-Raster bis 100 MHz host-getestet (`test_standalone_accuracy_*`); M-109 misst 99,999 999 MHz |
| M-511 | IQ: Korrektur ändern | derselbe Realignment-Mechanismus wie M-304 (host-getestet); M-304 reicht für FW-02 |
| M-512 | CH3 wartet unter 4,730 MHz | host-getestet (`test_ch3_below_iq_range_waits_and_resumes`); WAIT-Anzeige in M-123 |
| M-513 | Korrektur −1 ppm bei 100 MHz mit IQ | host-getestet (`test_iq_vco_stays_in_range_at_iq_limits`, VCO-Grenzen mit Korrektur seit S14) |
| M-514 | CH3 über 100 MHz | Testfall `FreqTooHigh` mit Master > 100 MHz kommt mit S35 als Host-Test |
| M-515 | Gespeicherte Wahl der alten Boot-Geste wird gelöscht | host-getestet (`test_stored_display_choice_is_removed_once`); beide Boards zeigen seit 1.3.2 `detected` (Smoke-Test S30); 1.3.1 dafür nachzuflashen lohnt nicht |
| M-516 | Lange Berührung beim Einschalten | in M-431 aufgegangen |
| M-517 | Kurze Berührung beim Einschalten | Boot-Geste entfernt (S27, E-302); es gibt nichts mehr umzuschalten |
| M-518 | Messungen der früheren Gruppen A und B und M-310 mit der Einheitsfirmware | Doppelung: die board-abhängigen Prüfungen (M-122, M-123, M-125, M-126, optional M-310) laufen ohnehin auf beiden Boards |
| M-519 | Anstecken nach dem Boot (gerade und vertauscht) | in M-104 aufgegangen; host-getestet (`test_hot_plug_straight_then_swapped`) |
| M-520 | Im Betrieb abziehen und vertauscht wieder anstecken | host-getestet (`test_rewiring_while_running_restores_iq`); am Gerät decken M-105 (Abziehen), M-106 (IQ nach Chip-Neustart) und M-102 (vertauscht) das Verhalten ab |
| M-521 | CN1-Belegung auf dem CYD2USB | durch Betrieb belegt: das CYD2USB verbindet sich über CN1 (Smoke-Tests 1.3.1 und 1.3.2); M-101 und M-102 laufen auf ihm |
| M-522 | `Integer` sperrt Spread | host-getestet (`test_integer_mode_turns_spread_off`); gesperrte Zeile in M-123 sichtbar |
| M-523 | „~“ nach dem Neustart (C-2) | host-getestet (Bootreihenfolge S08, `test_actual_frequency_known_only_after_stored_settings`) |
| M-524 | Unnötige Flash-Schreibvorgänge (C-5) | host-getestet (`test_unchanged_settings_do_not_write_flash`, S11) |
| M-525 | Speicherfristen je Quelle | wird mit S45 host-getestet (`test_command`: Fristen 5 s / 30 s, ON/OFF speichert nicht), analog M-524 |

### 8.2 Erledigt

M-526 bis M-528 sind am 2026-10-28 gelaufen (Messwerte in Spezifikation E-301). Nach dem Flashen der Einheitsfirmware: 5/5 EN-Resets je Board richtig erkannt (Original-CYD: ILI9341, ID 000000; CYD2USB: ST7789, ID 8181B3). Die Ports waren damals vertauscht (COM13 Original-CYD, COM7 CYD2USB); seit 2026-11-04 gilt die Zuordnung aus 1.4.

| ID | Test | Ergebnis |
|---|---|---|
| M-526 | Diagnose-Sketch: MADCTL-Rücklesen, RDDID, Pull an GPIO12, SPI-Modus 0/3, Schreiben mit 40 MHz | ✔ erledigt 2026-10-28, siehe Spezifikation E-301 |
| M-527 | Original-CYD einschalten: Log „ILI9341 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe M-126 |
| M-528 | CYD2USB einschalten: Log „ST7789 (detected)“, Bild, Farben, Ausrichtung, Touch | ✔ 2026-10-28: Bild OK, Touch (Dialog „OK“) OK; Ecken siehe M-126 |

M-128 und M-129 sind ebenfalls am Gerät geprüft (2026-11-04, S09); sie stehen als bedingte Messungen in Stufe 1 (5.9), weil sie nach der Migration einmal wiederholt werden.

---

## 9. Auswertung und Freigabe

### 9.1 Zusammenfassung

| Stufe | Messungen | ✔ | ✘ | ○ | i | – | Befunde |
|---|---|---|---|---|---|---|---|
| 1 | M-101 bis M-110, M-113 bis M-127; bedingt M-111, M-112, M-128, M-129 | | | | | | |
| 1W | M-201 bis M-216 | | | | | | |
| 2 | M-301 bis M-316; ab WEB-HMI M-317 bis M-319 | | | | | | |
| 3 | nach Lauf | | | | | | |

### 9.2 Vergleich R0 gegen R1 (vor und nach der Migration)

Gleiches Messboard, gleiche Geräte; die Drift des Quarzes über die Klammermessung herausrechnen. Abweichungen über den Grenzen sind Befunde (3.6).

| Größe | Messung | Zulässige Abweichung R1 gegen R0 |
|---|---|---|
| Frequenzgenauigkeit | M-107, M-109 | ≤ 0,5 Hz (Klammermessung) |
| IQ-Phasen | M-115, M-116 | ≤ 1° (+ u_φ) |
| Aussetzer beim Abstimmen | M-113, M-114 (M-303, M-304 falls gemessen) | keine zusätzlichen |
| Pegel im Zustand OFF | M-117, M-118 | ± 0,1 V |
| Erkennung und Verbindung | M-122, M-101, M-102, M-104 | 100 % wie R0 |
| Touch | M-125, M-126 | gleich oder besser |
| Nebenlinien, Störungen (falls Stufe 2 auf R0 lief) | M-302, M-301, M-310 | ± 3 dB; keine neuen Linien über −70 dBc |
| Boot und Heap | M-319, M-438 | ± 10 % (M-319), ≤ +1 s (M-438) |

### 9.3 Entscheidungsblatt

Die Tabelle aus 6.1 wird mit den Messwerten ausgefüllt; jede Entscheidung kommt als E-nnn in die Spezifikation und als bestätigter oder gestrichener Folgeschritt in den [ARBEITSPLAN.md](ARBEITSPLAN.md):

| Entscheidung | Messwerte | Ergebnis | Folgeschritt / E-nnn |
|---|---|---|---|
| Standard-Tuning | | | FW-01 |
| IQ ohne Reset | | | FW-02 |
| ungerade IQ-Teiler | | | FW-03 |
| Phasen-Feinabgleich | | | FW-04 |
| `High-Z` über PDN, MultiSynth aus | | | FW-05 |
| Störquellen-Abhilfen | | | FW-06 |
| TCXO | | | DOK-02 |
| Tastverhältnis, Pegeltabelle, Aufwärmzeit, Neustart-Hinweis | | | DOK-01 |
| IDF-Rückgabecode, Klon-Module | | | S39, E-212 |
| Druckaussetzer | | | S33 |
| Stift am Laborplatz | | | Handbuch |
| WLAN bei Messungen | | | S58 |
| Heap-Grenzen | | | MES-04 |

### 9.4 Freigabekriterien

**Release 1.3.x und jede Weitergabe:**

1. Smoke-Test (4.3) auf beiden Boards bestanden,
2. die nach 5.10 betroffenen Blöcke von Stufe 1 mit ✔ (beim ersten Release nach dem Eintreffen der Messgeräte: Stufe 1 vollständig),
3. keine offenen P0/P1-Befunde.

**Release 1.4.0:**

1. Stufe 1 auf R1 vollständig mit ✔ (M-109, M-115, M-116 mit ✔ oder begründetem ○), dazu M-128 und M-129 nach der Migration,
2. Stufe 1W mit ✔; ein ✘ bei M-206 bis M-216 stoppt die Freigabe; M-205 ohne Neustart und mit stabilem Heap,
3. Vergleich R0/R1 (9.2) innerhalb der Grenzen,
4. Stufe-2-Entscheidungen getroffen oder bewusst vertagt (Eintrag im Arbeitsplan),
5. alle Befunde priorisiert im [ARBEITSPLAN.md](ARBEITSPLAN.md), keine offenen P0/P1,
6. Handbuch DE und EN passen zu den Messwerten (Hinweis zu M-118, Pegeltabelle, Tastverhältnis, Aufwärmzeit, Neustart-Hinweis, WLAN-Hinweis, soweit gemessen).

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
| Korrektur nach M-108 (ppm) | |
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

### B.1 Anforderungen (A-nnn) und Entscheidungen (E-nnn)

„Host“ heißt: durch Host-Tests abgedeckt, am Gerät nicht gemessen (Nummern in Klammern = entfallene Messungen).

| Anforderung / Entscheidung | Gerät: Stufe 1 | Stufe 2 / 3 | Host |
|---|---|---|---|
| A-201 | M-109, M-111 | M-309 (2), M-414 (3) | Genauigkeit (M-510) |
| A-202 | M-123 | – | `test_input_logic` (M-505) |
| A-203 | M-117 | – | – |
| A-204 | M-120 | M-434 (3) | Autosave |
| A-205 | – | M-312 (2), M-414 (3) | – |
| A-206 | M-114, M-110 | M-305, M-301, M-303 (2) | Teilerplanung |
| A-207 | – | M-422, M-423 (3) | Spread (M-522) |
| A-208, A-209 | M-117, M-118 | M-402 (3) | Off state |
| A-210 | M-113, M-114 | – | kein PLL-Reset |
| A-212 | M-107, M-108, M-114 | – | Korrektur (M-508, M-509) |
| A-213, A-214 | M-109, M-110 | – | Genauigkeit (M-508, M-510) |
| A-215 | M-110 | – | (M-522, M-523) |
| A-216 bis A-218 | M-115, M-116 | M-304, M-307, M-308, M-306 (2), M-412 (3) | Phasenregister, Realignment (M-511, M-513) |
| A-219 | M-106 | – | (M-512, M-514) |
| A-220 | – | M-401, M-402 (3) | `test_fault_iq` |
| A-221 bis A-223 | M-101, M-102, M-104 | M-314, M-315 (2), M-403, M-404, M-406 bis M-410 (3) | Adern, Fremdgerät, Leitung low (M-519, M-520) |
| A-224 | – | M-403, M-404, M-406 bis M-410 (3) | Host |
| A-225 bis A-228, A-315 | M-103 bis M-106, M-119 | M-401, M-432 (3) | Wiederverbinden |
| A-301 | M-122 | M-431 (3) | `test_display_detect` (M-515 bis M-517) |
| A-302 | M-122 | – | (M-501) |
| A-304, A-305 | M-123, M-124 | M-435 (3) | – |
| A-306 | M-126 | M-425, M-428 (3) | – |
| A-307 | M-123 | – | – |
| A-308, A-309 | M-123 | M-426 (3) | – |
| A-314 | M-124, M-121 | M-436 (3) | – |
| A-316 | – | M-316 (2), M-424, M-427 (3) | `test_input_logic` (M-504, M-506, M-507) |
| A-319 | M-125, M-126 | M-435 (3) | `test_touch_cal`, `test_touch_filter` |
| A-401 bis A-403 | M-120, M-121 | – | Speichern (M-524) |
| A-405 | – | – | Host |
| A-406 | M-120 (Logzeilen) | – | (M-524) |
| A-801 bis A-804, A-806 | M-124 | – | Build, CI (M-502) |
| E-206, E-207 | M-110 | M-301, M-303, M-302 (2), M-422, M-423, M-415, M-416 (3) | Teilerplanung, Spread |
| E-208 | M-117, M-118 | M-402 (3) | Off state |
| E-212 | M-101, M-102 | M-314, M-315 (2) | Identitätsprüfung |
| E-301 | M-122, M-127 (M-526 bis M-528 erledigt) | – | `test_display_detect` (M-518) |
| E-310 | – | M-310 (2), M-436 (3) | Backlight-Stufen |
| E-311 | – | M-316 (2), M-435 (3) | – |
| E-805 | M-124 | – | – |

### B.2 Sicherheitsanforderungen der WEB-HMI

| Anforderung | Messung (Stufe) |
|---|---|
| A-701, A-702 | M-202, M-206 (1W) |
| A-703 | M-211 (1W) |
| A-704 | M-206, M-207 (1W), M-455 (3) |
| A-705, A-706 | M-206, M-207, M-215 (1W) |
| A-707 | M-214 (1W) |
| A-708, A-709 | M-128 (1), M-209 (1W) |
| A-710, A-711 | M-215 (1W), M-210 (1W) |
| A-712 | M-440, M-445 (3), `test_net_settings` |
| A-713 | M-216 (1W) |
| A-714 | M-212 (1W) |
| A-715 | M-456 (3) |
| A-716 | M-455 (3) |
| A-717, A-718 | M-208 (1W), M-452 (3) |
| A-719 | M-452, M-453 (3) |
| A-720 | M-442 (3) |
| A-721 | M-213 (1W) |
| A-722 | M-203 (1W) |
| A-723 | M-205 (1W), M-129 (1) |
| A-724, A-725 | Eintrittskriterien EK-4, EK-5 |

---

## Anhang C · Aufgaben und SUS-Fragebogen (Stufe 3)

### C.1 Aufgaben für M-430 (Reihenfolge fest, erst ohne, dann mit Handbuch)

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

WEB-HMI (M-446): **W1** „Buche das Gerät in das Test-WLAN ein.“ · **W2** „Verbinde dein Smartphone mit dem Gerät (Access point) und öffne die Weboberfläche.“ · **W3** „Ändere im Browser die Frequenz von CH1 auf 10 MHz und schalte ihn ein.“

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

Die beiden Skizzen unten sind Beispiele für eigene Skripte (Ordner `messungen/skripte/`); die Helfer der Liste liegen in `tools/` (AF39, `tools/README.md`) und sind gegen `tools/web_mock.py` und `tools/fake_scpi_device.py` geprüft, nicht am Gerät.

```python
# scpi_tcp.py – 1000x *IDN? über TCP (M-203)
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
# scpi_usb.py – serielle Verbindung ohne Reset-Impuls (M-203); vor Gebrauch am Board prüfen
import serial
s = serial.Serial(); s.port = "COM7"; s.baudrate = 115200; s.timeout = 1
s.dtr = False; s.rts = False; s.open()
s.write(b"*IDN?\n"); print(s.readline())
```

- `tools/counter_log.py` (M-313, M-411): liest den ZÄHLER per SCPI über TCP oder USB im Takt der Torzeit und schreibt `time;frequency;temperature` in eine CSV (Abfrage mit `--query` einstellbar, Temperatur mit `--temp-query`).
- `tools/touchstat.py` (M-126, M-425, M-316, M-427): wertet `[HITMAP]`- und `[TOUCH]`-Zeilen aus: Abweichung je Zielkreuz, Fehltippquote (`--tolerance`), Druckdauer, kurze Pausen (Doppeltipp), Aussetzer, Berührungen je Widget.
- M-206, M-214, M-215, M-216: `tools/web_check.py <IP>` (Header, Guard-Antworten, Grenzen, Rate-Limit, Sitzung; mit Passwort `--password`) und `tools/contract_check.py <IP>` (gemeinsame Vertragsvektoren, ohne Web-Passwort); die curl-Zeilen der Tests bleiben für die Fälle, die beide nicht abdecken (CORS-Header, Kanalzustand nach jedem Fall).
- M-452 und M-203: `tools/device_check.py --profile M-452` (jede Abfrage des Befehlsbaums mit `SYST:ERR?` danach, gültige und ungültige Parameter mit Soll-Fehlercode, `*RST`) und `--profile M-203` (1000× `*IDN?`, Median < 50 ms).
- `tools/slowloris.py` (M-210): 30 offene Anfragen, ein Header-Byte alle 2 s, Abschluss nach 10 s; je Verbindung angenommen, abgewiesen, vom Gerät geschlossen (wann) oder beantwortet (nur im eigenen Labor).

---

## Anhang E · Befundvorlage

| Feld | Inhalt |
|---|---|
| Befund-Nr. | MB-n |
| Mess-ID, Lauf, Board | z. B. M-116, R1, Board B |
| Beobachtung / Messwert | |
| Soll | |
| Aufbau, Einstellungen, Foto/Screenshot | |
| Reproduzierbar? (n von m) | |
| Vermutete Ursache | |
| Priorität (P0 Sicherheit/Blocker, P1 Fehler, P2 Verbesserung, P3 Hinweis) | |
| Verweis (Arbeitsplan: Befund und Aufgabe FW-/DOC-/REL-/MES-/USR-nn; Spezifikation A-nnn/E-nnn) | |
| Status, Datum Wiederholungstest | |
