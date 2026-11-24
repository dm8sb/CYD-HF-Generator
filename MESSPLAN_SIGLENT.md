# Messplan Siglent – automatische Messungen im Netzwerk

Stand: 2026-11-24 · Plan-Version 1.2 · Prüfling: Messboard (CYD2USB mit SI5351), Firmware ab 1.4.0 (SCPI über USB, TCP und VXI-11)

Dieser Plan beschreibt einen **festen Gesamtaufbau** aus Oszilloskop, Spektrumanalysator und einer GPS-geregelten 10-MHz-Referenz. Ein PC-Skript steuert über das LAN die Messgeräte und über SCPI den Prüfling. Die Messungen sind zu **Läufen** zusammengefasst, die ohne Umstecken nacheinander laufen; umgesteckt wird nur für wenige, benannte Varianten (Abschnitt 4). Am Ende erzeugt das Skript ein **vollständiges Messprotokoll mit Bildern** (Abschnitt 10).

Er ergänzt [MESSPLAN.md](MESSPLAN.md) und ersetzt ihn nicht. Jede Messung hier nennt die Mess-IDs (M-nnn), für die sie Werte liefert. Sollwerte, Grenzen, Bewertung (✔ ✘ ○ i –) und Befundvorlage (MB-n) stehen dort und gelten unverändert. Bei einem Widerspruch gilt MESSPLAN.md.

**IDs in diesem Plan:** V-nn = Vorbereitung und Eigenmessung des Aufbaus, L1 … L6 = automatische Läufe, S1 … S3 = Steckvarianten, H = Handmessungen ohne diese Geräte.

## Inhalt

1. Geräte, Spezifikationen und Bewertung
2. Messverfahren
3. Gesamtaufbau G
4. Steckvarianten S1 bis S3
5. Vorbereitung V-01 bis V-07
6. Läufe L1 bis L6
7. Abdeckung von MESSPLAN.md
8. Software
9. Ablauf und Zeitbedarf
10. Messprotokoll mit Bildern
11. Werte und Bilder für Dokumentation und Handbuch
- Anhang A: Quellen der Gerätedaten

---

## 1. Geräte, Spezifikationen und Bewertung

### 1.1 Siglent SDS1204X-E (Oszilloskop, 4 Kanäle) – kurz **SDS**

| Merkmal | Wert (Datenblatt SDS1000X-E; 4-Kanal-Angaben von Hersteller und Händlern) | Folge |
|---|---|---|
| Bandbreite | 200 MHz (Anstiegszeit ≈ 1,75 ns) | Rechteck wird ab ≈ 30 MHz zum Sinus; Flanken für Δt bis ≈ 100 MHz nutzbar |
| Abtastrate | 1 GSa/s je ADC; zwei ADCs für vier Kanäle; **500 MSa/s je Kanal, wenn alle vier an sind** | für Phasen nur **einen Kanal je ADC** einschalten → 1 GSa/s (V-02 prüft die Paare) |
| Speicher | 14 Mpts (ein Kanal je ADC), 7 Mpts (zwei Kanäle je ADC) | ≈ 14 ms Aufzeichnung in beiden Fällen |
| Eingang | **nur 1 MΩ ∥ 18 pF**, max. 400 Vpk | 50 Ω nur über Durchgangsabschluss |
| Vertikal | 8 bit, 500 µV/div … 10 V/div, Bandbreitenbegrenzung 20 MHz | die 20-MHz-Grenze macht aus Rechtecken bis ≈ 15 MHz saubere Sinus |
| Kanalversatz | < 100 ps (Datenblatt 2-Kanal-Modell) | wird in V-03 gemessen und abgezogen |
| Zeitbasis | **±25 ppm**, **kein Referenzeingang** | Frequenz nur als **Verhältnis zur GPS-Referenz auf Kanal 4** (2.1) |
| Trigger | Edge, Slope, Pulse, Window, Runt, Interval, **Dropout (Timeout)**, Pattern, Video; I2C/SPI/UART-Trigger und -Dekoder | Aussetzer (M-113, M-114, M-303, M-304); I2C optional |
| Messungen | 38 Parameter mit Statistik, Skew, Phase, Gate; Sequence bis 80 000 Segmente; FFT 1 Mpts | Kontrolle; ausgewertet wird aus den Rohdaten |
| Hardware-Zähler | 6 Stellen | nicht genau genug |
| Schnittstelle | LAN (VXI-11), USBTMC | `TCPIP::<ip>::INSTR` |

**Bewertung:** Ohne Referenzeingang ist das SDS allein kein Frequenzmesser. Mit der GPS-Referenz auf Kanal 4 und der Auswertung der **Rohdaten am PC** wird es aber zum genauesten Frequenzvergleicher im Aufbau, weil der Zeitbasisfehler im Verhältnis zweier Kanäle herausfällt (2.1). Es ist zugleich das Gerät für Phasen, Aussetzer, Tastverhältnis, Kurvenformen und Gleichpegel.

### 1.2 Siglent SSA3032X Plus (Spektrumanalysator) – kurz **SSA**

| Merkmal | Wert (Datenblatt SSA3000X; Plus-Werte von Hersteller und Händlern, in V-06 bestätigen) | Folge |
|---|---|---|
| Bereich | 9 kHz … 3,2 GHz, Auflösung 1 Hz | alle Oberwellen bis 160 MHz × 20 |
| RBW / VBW | 1 Hz … 1 MHz (1-3-10); unter 30 Hz FFT-Modus | Nebenlinien bis ≈ 10 Hz Abstand |
| Zähler (Marker) | **1 Hz Auflösung**, Genauigkeit = f × Referenzfehler + 1 Hz | mit GPS ±1 Hz absolut: genug für `Integer` (kHz) und als Gegenprobe, **nicht** für die 0,5-Hz-Grenze von M-109 |
| Referenzeingang | 10 MHz, −5 … +10 dBm, 50 Ω, BNC (Rückseite) | schließt die GPS-Kette mit 50 Ω ab |
| Phasenrauschen | < −95 dBc/Hz (typ. −98) bei 10 kHz Ablage, gemessen bei fc = 1 GHz | **vermutlich schlechter als der SI5351**: M-416 liefert nur obere Schranken (○) |
| DANL | ≈ −141 dBm/Hz ohne Vorverstärker, ≈ −161 dBm/Hz mit (10 … 200 MHz, typ.) | Nebenlinien bis ≈ −100 dBc sichtbar |
| Verzerrung | 2. Harmonische −65 dBc bei −30 dBm am Mischer; TOI +10 dBm; 1-dB-Kompression > −5 dBm | für Oberwellen Mischerpegel ≤ −30 dBm einstellen |
| Pegel | Gesamtgenauigkeit ±0,7 dB, Frequenzgang ±0,8 dB | M-312 mit ±1 dB Unsicherheit |
| Grenzwerte Eingang | +30 dBm Dauer, **±50 V DC** | 3,3 V DC (`Off state` = `High`) schaden nicht; das 20-dB-Glied bleibt trotzdem immer davor |
| Eingang | 50 Ω, **N-Buchse**, VSWR < 1,5 | Adapter N auf BNC |
| Schnittstelle | LAN (VXI-11), USBTMC | `TCPIP::<ip>::INSTR` |

**Bewertung:** Das SSA übernimmt Spektren, Pegel, Oberwellen, Nebenlinien, Spread und Plattformstörungen und ist der zweite Frequenzmesser (Gegenprobe zu 2.1). Für Phasenrauschen reicht sein Eigenrauschen voraussichtlich nicht; das zeigt V-06.

### 1.3 Siglent SDG2122X (Funktionsgenerator mit Zähler) – **entbehrlich**

| Merkmal | Wert (Datenblatt SDG2000X EN02H) |
|---|---|
| Zähler | 100 mHz … 200 MHz; Eingang **1 MΩ**, DC ±2,5 V, AC 5 Vpp; Frequenz, Periode, Pulsbreite, Tastverhältnis; **Auflösung nicht angegeben** |
| Referenz | Eingang 10 MHz ≥ 1,4 Vpp an 5 kΩ; Ausgang 10 MHz 2 … 3,3 Vpp an HiZ |
| Signalquelle | 2 Kanäle bis 120 MHz, 1 µHz Auflösung, an die externe Referenz koppelbar |

**Bewertung: Das SDG wird im Gesamtaufbau nicht gebraucht.**

- **Als Zähler bringt es nichts Neues.** Die Frequenz misst das SDS gegen GPS genauer (2.1), die Gegenprobe macht der Markerzähler des SSA. Das Tastverhältnis misst das SDS aus den Rohdaten. Ein Abgriff mit T-Stück an CLK0 würde dagegen die Pegel- und Oberwellenmessung am SSA stören.
- **Als Referenzverteiler wird es nicht gebraucht.** Die GPS-Kette führt direkt zu SDS und SSA (3.2).
- **Einziger echter Nutzen: eine bekannte Prüffrequenz.** An GPS gekoppelt liefert das SDG beliebige Frequenzen bis 120 MHz mit bekanntem Sollwert. Damit lässt sich die Frequenzmessung 2.1 auch außerhalb von 10 MHz gegen eine unabhängige Quelle prüfen. Das ist V-04 Teil b: optional, einmalig und etwa 30 min. Ohne SDG prüft V-04 Teil a die Methode mit GPS gegen GPS und mit dem Prüfling gegen den SSA-Zähler; das reicht für die Bewertung.

Wer das SDG ohnehin auf dem Tisch hat, macht V-04 b beim ersten Messtag einmal. Danach bleibt es weg.

### 1.4 GPS-TCXO 10 MHz – kurz **GPS**

Mit GPS-Regelung langfristig ≈ 1e-10 oder besser. Kurzzeitstabilität und Ausgangspegel sind unbekannt; V-01 misst den Pegel, V-04 die Streuung. Vor jedem Lauf muss die Lock-Anzeige leuchten.

### 1.5 Material

| Teil | Anzahl | vorhanden | Zweck |
|---|---|---|---|
| Messleitungen BNC, gleiche Länge | 3 (+ 2 beliebig) | ja | CLK0, CLK1, CLK2; Referenzkette |
| 50-Ω-Durchgangsabschluss | 2 (+1) | ja | SDS Kanal 1 und 3 (Kanal 2 in V-04) |
| 50-Ω-Abschluss | 1 | ja | V-06 (Eigenrauschen SSA) |
| Dämpfungsglieder 3 / 10 / 20 dB | je 1 | ja | 20 dB vor dem SSA; 3 dB in der Referenzkette bei Bedarf (3.2) |
| **BNC-T-Stück** | 2 | prüfen | Referenzkette (1), V-03 (1) |
| **Adapter N-Stecker auf BNC-Buchse** | 1 | prüfen | SSA-Eingang |
| Adapter SMA auf BNC | 3 | prüfen | Ausgänge des SI5351-Moduls |
| **USB-Isolator** (USB 2.0 Full Speed, z. B. ADuM4160) | 1 | beschaffen | trennt die Masseschleife PC – Board – Messgeräte (3.3) |
| Tastköpfe 10:1 (beim SDS) | 2 | ja | EN, SDA, SCL (S3) |
| DMM | 1 | ja | M-117, M-118 (S2) |
| Thermometer | 1 | ja | Raumtemperatur im Protokoll |
| Smartphone-Kamera | 1 | ja | Fotos von Aufbau und Gerät für das Protokoll (10.3) |

---

## 2. Messverfahren

### 2.1 Frequenz als Verhältnis zur GPS-Referenz (SDS)

Das SDS zeichnet den Prüflingsausgang (Kanal 1) und GPS (Kanal 4) **in derselben Aufnahme** auf. Der PC lädt die Rohdaten (8 bit) und bestimmt die Frequenz beider Kanäle in Einheiten der SDS-Zeitbasis:

1. Nulldurchgänge (50 % zwischen Min und Max) mit linearer Interpolation zwischen den Abtastwerten finden, nur steigende Flanken.
2. Ausgleichsgerade durch Flankenzeit über Flankennummer legen; die Steigung ist die Periode. Bei Sinus (≤ 15 MHz mit 20-MHz-Grenze) geht alternativ eine Sinus-Anpassung nach IEEE 1057.
3. **f_mess = 10 MHz × f_K1 / f_K4.** Der Zeitbasisfehler des SDS (±25 ppm) fällt im Verhältnis heraus.

Abschätzung: 14 ms Aufnahme, bei 10 MHz ≈ 140 000 Flanken, Flankenrauschen ≈ 50 ps → relative Unsicherheit ≈ 1e-10 je Aufnahme. Tatsächlich begrenzen die Kurzzeitstabilität von GPS-TCXO und Prüflingsquarz; erwartet ≈ 1e-9 (≙ 0,01 Hz bei 10 MHz, 0,1 Hz bei 100 MHz). **V-04 bestimmt den tatsächlichen Wert u_f**, bevor M-108 und M-109 bewertet werden.

Grenzen:

- Nutzbar bis 160 MHz: Nyquist liegt bei 500 MSa/s bei 250 MHz, die Bandbreite von 200 MHz verhindert Aliasing durch Oberwellen weitgehend. Über 100 MHz sinkt der Pegel; V-04 prüft das an der höchsten Frequenz.
- Bei 8 kHz liegen in 14 ms nur ≈ 110 Perioden. Die Zeitbasis wird dann auf 10 ms/div gestellt (die Abtastrate sinkt); die Unsicherheit ist größer, liegt aber weit unter 0,5 Hz.

Die **Klammermessung** aus MESSPLAN.md 3.3 bleibt gültig: 10 MHz vor und nach dem Messpunkt, δ = f_mess − f_soll · (f10a + f10b) / (2 · 10 MHz). Im Lauf wird 10 MHz nach jeweils 10 Messpunkten gemessen und linear interpoliert, weil die Drift langsam ist (M-313).

### 2.2 Frequenz am SSA (Gegenprobe)

Marker auf den Träger, Markerzähler an, Span 10 kHz, RBW 1 kHz; Auflösung 1 Hz. Gegenprobe zu 2.1 an CLK0, während das SDS CLK1 misst; beide Kanäle laufen dabei mit derselben Frequenz. Weichen beide um mehr als 1 Hz + u voneinander ab, hält der Lauf an (Fehler im Aufbau).

### 2.3 Phase und Zeitversatz (SDS)

- Master an Kanal 1, CLK2 an Kanal 3, **nur Kanal 1 und 3 eingeschaltet** (je ein ADC → 1 GSa/s, V-02). Kanal 4 (GPS) ist dabei aus.
- Aus einer Aufnahme werden die steigenden Flanken beider Kanäle wie in 2.1 bestimmt. **Δt = Mittelwert(t_K3 − t_K1)** über alle Perioden, modulo Periode; das ersetzt die 256fache Mittelung am Gerät durch ≈ 10⁵ Flankenpaare.
- **φ = (Δt − Δt₀) · f · 360°**, Δt₀ aus V-03.
- **Kabeltausch** bei 99 MHz (dort ist 1° ≈ 28 ps): Messung mit am SDS vertauschten Kabeln wiederholen. φ = (φ_a − φ_b) / 2 eliminiert Kanal- und Kabelversatz vollständig. Das Skript hält dafür an.

### 2.4 Aussetzer erkennen

- **SDS:** Dropout-Trigger auf Kanal 1 mit Zeitschwelle ≈ 3 Perioden, Single-Modus. Das Skript liest nach jedem Schritt den Triggerstatus. Jede Auslösung ist ein Aussetzer; die Dauer kommt aus den Rohdaten.
- **SSA:** Zero-Span auf dem Träger von CLK0, RBW 1 MHz, Sweepzeit 1 s, Video-Trigger auf einen Pegelabfall um 10 dB. Damit werden Aussetzer an CLK0 gleichzeitig mit denen an CLK1 erkannt (Zeitauflösung ≈ 1 µs). Aussetzer kürzer als ≈ 2 µs erkennt nur das SDS.

### 2.5 Pegel und Spektren (SSA)

Wie in MESSPLAN.md 3.5, dazu:

- Die Dämpfung von Kabel A und 20-dB-Glied kommt aus V-05 und wird automatisch abgezogen.
- Für **Oberwellen** muss der Mischerpegel ≤ −30 dBm sein: zusätzlich 10 dB Eingangsdämpfung am SSA, elektronisch eingestellt. V-06 prüft die Eigenverzerrung.
- **Phasenrauschen** (M-416) nur mit dem Eigenrauschen aus V-06 daneben. Liegt der Messwert < 6 dB darüber, ist er eine obere Schranke (○).

---

## 3. Gesamtaufbau G

### 3.1 Verkabelung

```text
REFERENZKETTE (einmal aufbauen, bleibt für alle Läufe)

  GPS-TCXO 10 MHz ── Kabel R1 ──► [T] SDS Kanal 4 (1 MΩ)
                                   │
                                   └─ Kabel R2 ──► SSA Rückseite „Ref In“ (50 Ω = Abschluss)

PRÜFLING (Messboard, SI5351-Modul)

  CLK0 (CH1) ── Kabel A ──► 20 dB ──► Adapter BNC/N ──► SSA RF In (50 Ω)

  CLK1 (CH2) ── Kabel B ──► 50-Ω-Durchgang ──► SDS Kanal 1        ┐ B und C
  CLK2 (CH3) ── Kabel C ──► 50-Ω-Durchgang ──► SDS Kanal 3        ┘ gleich lang

  SDS Kanal 2: Tastkopf 10:1, frei (EN, SDA nach Bedarf, S3)

STEUERUNG

  PC ── LAN ── Switch ── SDS, SSA (VXI-11)
  PC ── USB-Isolator ── USB ── Messboard (SCPI über USB)
```

Begründung der Belegung:

- **CLK0 an das SSA:** CH1 trägt PLL A und damit Spread Spectrum (M-422, M-423), Oberwellen und Plattformstörungen. Die Leitung ist ohne Abgriff am SSA mit 50 Ω abgeschlossen.
- **CLK1 und CLK2 an das SDS:** Für die IQ-Messungen ist **CH2 der Master** (`SOUR3:IQ:MAST 2`). Beide Leitungen sind gleich aufgebaut: gleiche Kabellänge, Abschluss direkt am Eingang. Kanal 1 und 3 liegen an verschiedenen ADCs (1 GSa/s).
- **GPS an Kanal 4:** Frequenzreferenz für 2.1. Kanal 4 teilt den ADC mit Kanal 3 und wird für Phasenmessungen abgeschaltet.
- Der Master CH1 für IQ ist mit Steckvariante S1 messbar (4.1).

### 3.2 Pegel der Referenzkette

Die Kette ist am SSA mit 50 Ω abgeschlossen, das SDS greift hochohmig ab. Das SSA verlangt −5 … +10 dBm an 50 Ω, also **0,36 … 2,0 Vpp** (Sinus) an Kanal 4. V-01 misst die Spannung.

- **Über 2,0 Vpp:** 3-dB-Glied direkt vor `Ref In` des SSA (bei Bedarf 10 dB).
- **Unter 0,36 Vpp:** Der GPS-TCXO ist für einen 50-Ω-Eingang zu schwach. Dann lockt das SSA nicht; der Plan läuft trotzdem, aber die SSA-Frequenzwerte gelten nur relativ (2.2 entfällt als absolute Gegenprobe).

### 3.3 Masse und Störungen

- Messgeräte und PC sind über den Schutzleiter geerdet. Ohne Isolator schließt die USB-Masse des Boards über die Koaxschirme eine Masseschleife; Brumm und Störlinien erscheinen dann im Spektrum (M-310). LAN ist über Übertrager getrennt und unkritisch.
- **Standard ist der USB-Isolator.** Ohne Isolator: Board aus einer Powerbank versorgen und über WLAN steuern (SCPI über TCP oder VXI-11). WLAN verfälscht aber alle Spektren (Standardbedingung „WLAN aus“ in MESSPLAN.md 3.2). Diese Variante gilt deshalb nur für L1 bis L3 und kommt ins Protokoll.
- M-310 d (Versorgung) vergleicht ausdrücklich Isolator, PC-USB direkt und Powerbank.

### 3.4 Sicherheit

- Das 20-dB-Glied bleibt immer vor dem SSA. Bei +10 dBm Grundwelle kommen −10 dBm am Eingang an; erlaubt sind +30 dBm und ±50 V DC.
- Umstecken nur mit ausgeschalteten Ausgängen (`OUTP1 OFF; OUTP2 OFF; OUTP3 OFF`). Das Skript schaltet vor jeder Steckaufforderung ab und wartet auf Bestätigung.
- Vor jedem Lauf prüft das Skript `OUTP:REM?` = `ALLOWED`, sonst lehnt der Prüfling das Einschalten ab.

---

## 4. Steckvarianten S1 bis S3

Jede Variante ändert nur die genannten Stecker. Das Skript fordert sie an, wartet auf Bestätigung und prüft danach, ob an den erwarteten Kanälen Signal anliegt.

### 4.1 S1 – CLK0 und CLK1 tauschen (am Prüfling)

Kabel A und B **am Modul** tauschen: CLK1 geht dann ans SSA, CLK0 an SDS Kanal 1.

Für: IQ mit **Master CH1** (M-115, M-116 in der Originalform), Aussetzer an CLK0 auf dem SDS (M-114 in der Originalform), Tastverhältnis von CLK0, Übersprechen in Gegenrichtung (M-311), Spektrum von CLK1.

### 4.2 S2 – Gleichpegel (1 MΩ)

Die Durchgangsabschlüsse an SDS Kanal 1 und 3 abziehen, die Kabel direkt aufstecken (1 MΩ) und danach wieder zurückstecken (50 Ω). Das DMM wird parallel nach M-117 angeschlossen.

Für: M-117, M-118, M-402 (Pegel im Zustand OFF an 1 MΩ und 50 Ω). Für CLK0 zusammen mit S1.

### 4.3 S3 – Tastköpfe

- **S3a – EN:** Tastkopf an Kanal 2 auf EN des ESP32, Masse an GND. Für M-119. Den Neustart löst das Skript über RTS/DTR der USB-Seriellen aus; das geht auch durch den Isolator.
- **S3b – I2C:** Tastkopf an Kanal 2 auf SDA; GPS an Kanal 4 abziehen und dort einen zweiten Tastkopf an SCL. Dann laufen I2C-Trigger und -Dekoder des SDS. Für M-314, M-315 und, soweit ohne Logikanalysator möglich, M-403 bis M-410.

---

## 5. Vorbereitung V-01 bis V-07

Einmal pro Messtag, vor den Läufen. V-03 bis V-06 brauchen kurz einen anderen Aufbau; danach wird G hergestellt und bleibt.

| ID | Was | Aufbau | Ergebnis | ersetzt / liefert |
|---|---|---|---|---|
| V-01 | Referenzkette: Pegel und Kurvenform an SDS Kanal 4; Referenzstatus des SSA per SCPI (extern, gelockt) | G | 0,36 … 2,0 Vpp, SSA gelockt (3.2) | M-003 |
| V-02 | ADC-Paare des SDS: Kanalkombinationen einschalten, Abtastrate abfragen (`SARA?`) | G | Paare (erwartet K1/K2 und K3/K4); bei K1 + K3 jeweils 1 GSa/s | – |
| V-03 | Kanalversatz Δt₀: CLK1 über ein T-Stück auf zwei gleich lange Kabel → SDS K1 und K3 (je Durchgangsabschluss); Δt messen, Kabel tauschen, erneut messen; bei 10, 30 und 99 MHz | T am Prüfling | Δt₀ je Frequenz, u_t, u_φ | M-004 a |
| V-04 a | Frequenzmessung prüfen ohne SDG: (1) GPS über T auf K1 und K4 → Verhältnis muss 1 sein, Streuung = Grenze der Methode; (2) CH1 = CH2 = 10 MHz: 2.1 an CLK1 gegen den SSA-Zähler an CLK0; (3) CLK1 bei 99,999 999 und 160 MHz: Pegel und Flanken auswertbar? | G, für (1) GPS-T an K1 | u_f (Ziel ≤ 1e-9); Übereinstimmung mit dem SSA ≤ 1 Hz | M-003 (u_ZÄHLER) |
| V-04 b | *optional, nur mit SDG:* SDG an GPS gekoppelt, Rechteck 8 kHz / 1 / 7,1 / 28,4 / 99,999 999 / 120 MHz an SDS K1; Abweichung von 2.1 vom Sollwert | SDG an K1, GPS an SDG `10MHz In` | u_f an beliebigen Frequenzen gegen eine unabhängige Quelle | M-003 |
| V-05 | Dämpfung Kabel A + 20 dB: CLK0 = 10 MHz Rechteck, 8 mA; Pegel der Grundwelle und der ungeraden Oberwellen (30, 50 … 150 MHz) einmal **ohne** 20-dB-Glied (SSA-Eingangsdämpfung 30 dB) und einmal **mit** | G | Dämpfung je Frequenz 10 … 150 MHz; wird in L4 abgezogen. Die Oberwellen des Prüflings ersetzen eine durchstimmbare Quelle | M-004 b (5) |
| V-06 | SSA-Eigenwerte: (1) Eingang mit 50 Ω abgeschlossen, Rauschmarker bei 100 Hz … 100 kHz um 10 MHz; (2) Eigenverzerrung: Oberwellen von CLK0 bei SSA-Eingangsdämpfung 10, 20 und 30 dB; ändert sich ein dBc-Wert mit der Dämpfung, entsteht er im SSA | 50 Ω bzw. G | Eigenrauschen (dBc/Hz), Dämpfung ohne Eigenverzerrung für L4.8 | M-004 b (4, 6) |
| V-07 | Prüfling: `*IDN?`, `SYST:SI5351?` = `CONN`, `SYST:CORR?`, Version und Commit (Info-Karte, Foto); 15 min Aufwärmen mit CH1 = CH2 = 10 MHz; `*RST`, Drive 8 mA; Fotos des Aufbaus (10.3) | G | Kopf des Protokolls | M-001, M-002, M-005 |

---

## 6. Läufe L1 bis L6 (Gesamtaufbau G)

Jeder Lauf startet mit `*RST` und setzt die Standardbedingungen aus MESSPLAN.md 3.2 per SCPI (`OUTP:DRIV 8`, `SYST:TUN FRAC`, `SSC OFF`, `OUTP:OFFS LOW`, Korrektur aus L1). Jede Messung wird als Zeile ins Protokoll geschrieben (10). Zwischen Einstellen und Messen wartet das Skript 2 s (IQ) bzw. 10 s (Frequenz).

### 6.1 L1 – Frequenz und Kalibrierung

Messgeräte: SDS (2.1, CLK1), SSA-Zähler (CLK0). Beide Kanäle laufen mit derselben Frequenz, so dass jede Messung doppelt vorliegt.

| Schritt | Ablauf | liefert |
|---|---|---|
| L1.1 | `SYST:CORR 0`; CH1 = CH2 = 10 MHz; 10 Aufnahmen 2.1 → Quarzfehler ε₀ in ppm | M-107 |
| L1.2 | `SYST:CORR` = ε₀ (Vorzeichen nach Handbuch §8: positive Korrektur senkt die Frequenz); erneut messen und nachstellen, bis \|Abweichung\| < 0,2 Hz bei 10 MHz | M-108 (Messteil; der Bedienteil „Tipps und Zeit“ bleibt Handmessung H) |
| L1.3 | Klammermessung für die Punkte aus M-109 (1 / 7,1 / 28,4 / 99,999 999 MHz) und M-111/M-112 (8 kHz, 455 kHz, 112,499 996 / 112,500 004 / 160 MHz) | M-109, M-111, M-112 |
| L1.4 | **Breiter Sweep:** 200 zufällige Frequenzen 8 kHz … 160 MHz (fester Startwert des Zufallsgenerators) je `Tuning` (`FRAC`, `LNO`, `INT`); je Punkt f_mess gegen `FREQ:ACT?` des Prüflings | Gerät = Modell (Host-Test), über M-109 hinaus |
| L1.5 | `Integer`, 7,1 MHz: f_mess gegen `FREQ:ACT?` (7,102 273 MHz) | M-110 |
| L1.6 | Spread `DOWN05` auf CH1: CH2 (SDS) bleibt auf 7,1 MHz ± u | M-422 (CH2-Teil) |

Bewertung L1.4: |f_mess − f_act| ≤ u_f (V-04) + 0,05 Hz (Auflösung von `FREQ:ACT?`). Jede Ausnahme ist ein Befund.

### 6.2 L2 – Abstimmen ohne Aussetzer

Messgeräte: SDS Dropout-Trigger auf K1 (CLK1), SSA Zero-Span auf CLK0 (2.4).

| Schritt | Ablauf | liefert |
|---|---|---|
| L2.1 | CH1 = 10 MHz fest (SSA beobachtet); CH2 abstimmen: je 20 Schritte 1 MHz, 1 kHz, 1 Hz (`SYST:STEP` und Frequenz setzen), 20 Sprünge zufällig zwischen 1 und 99 MHz | M-113 (CH1 ungestört), Aussetzer an CH2 selbst |
| L2.2 | CH2 = 10 MHz fest (SDS beobachtet); CH1 abstimmen wie L2.1 | M-113 in Gegenrichtung |
| L2.3 | CH2 = 10 MHz, Schritt 1 Hz, 200 Schritte mit 10 Schritten/s (unter der Firmware-Grenze von 20 Schreibbefehlen/s); CH2 = 99 … 100 MHz in 1-MHz-Schritten | M-114 (an CH2; Original an CH1 mit S1) |
| L2.4 | `SYST:CORR` +1,00 ppm und zurück; Frequenzsprung an CH2 mit 2.1 (≈ 10 Hz bei 10 MHz) | M-114 (M-509) |
| L2.5 | `SYST:TUN LNO`; CH2 9,990 → 10,010 MHz in 1-kHz-Schritten, 7,0 → 7,2 MHz | M-303 |
| L2.6 | IQ: CH3 an (Master CH2, 90°); CH2 10× um 1 kHz verstellen; Aussetzer und Dauer an K1 und K3 | M-304 |
| L2.7 | Spread auf CH1 ein- und ausschalten; Aussetzer an CLK0 (SSA Zero-Span), Dauer | Handbuch 8 Spread (11.1) |

Hinweis: SCPI geht über dieselben Funktionen des `AppController` wie die Tasten. Die Wiederholfunktion beim Halten von `+` am Touch prüft L2 nicht; sie bleibt eine kurze Handprüfung (H).

### 6.3 L3 – IQ-Phasen und Kurvenformen

Messgeräte: SDS K1 (CLK1, Master CH2) und K3 (CLK2), nur diese beiden Kanäle an (1 GSa/s); Methode 2.3.

| Schritt | Ablauf | liefert |
|---|---|---|
| L3.1 | 10 MHz, Phasen 0 / 90 / 180 / 270°, je 5 Aufnahmen | M-115 |
| L3.2 | 4,73 / 14,2 / 99 MHz (Teiler 127 / 63 / 9), je vier Phasen; bei 99 MHz mit Kabeltausch (Skript hält an) | M-116 |
| L3.3 | Phase 0° zusätzlich bei 30 und 50 MHz; Δt über f | M-307 |
| L3.4 | Auswertung 180°/270° bei ungeradem Teiler (63, 9) gegen geraden (90) | M-308 |
| L3.5 | 10 MHz, 90°; 10× Master ±1 kHz, je 2 s warten, φ messen; Mittelwert und σ | M-306 |
| L3.6 | Tastverhältnis aus den Rohdaten: CLK1 bei 7,1 / 10 MHz, CLK2 bei 14,2 / 99 MHz | M-309 |
| L3.7 | Spread `CENT05` auf CH1, IQ an CH2: Phase CH2/CH3 bleibt unbeeinflusst (Gegenprobe); Master CH1 mit Spread in L3.9 | M-423 (Phasenteil) |
| L3.8 | 10 MHz, 90°, 30 min: jede Minute φ, Raumtemperatur | M-412 |
| L3.9 | **S1** (Master CH1): L3.1 und L3.2 bei 10 und 99 MHz; Spread `CENT05` mit IQ; Tastverhältnis CLK0 bei 7,1 und 10 MHz | M-115, M-116 Originalform, M-423, M-309 |
| L3.10 | CH3 aus, K3 aus; CLK1 bei 10 MHz mit Drive 2 / 4 / 6 / 8 mA: Kurvenform, Uss, Anstiegszeit an 50 Ω | M-312 (Uss) |

Bei 99 MHz ist 1° ≈ 28 ps. Ist u_φ nach dem Kabeltausch > 0,5°, gilt das Ergebnis als ○ (MESSPLAN.md M-116).

### 6.4 L4 – Spektrum, Pegel, Störungen (CLK0 am SSA)

Einstellungen nach MESSPLAN.md 3.5; je Messung ein Bildschirmfoto per SCPI und die Kurve als Daten (751 Punkte).

| Schritt | Ablauf | liefert |
|---|---|---|
| L4.1 | 7,1 MHz, Span 200 kHz, RBW 100 Hz, Mittelung 10; `FRAC`, `LNO`, `INT`: größte Nebenlinien und Abstand; Rauschmarker bei 1 kHz | M-301 |
| L4.2 | 10 MHz ganzzahlig; 7,1 MHz mit CH3 an (Master CH1) | M-302 |
| L4.3 | 10 MHz mit Korrektur 0 und +0,01 ppm | M-415 |
| L4.4 | Backlight 100 / 50 / 10 % (`SYST:BACK`), Span 20 kHz, RBW 10 Hz, Max-Hold 60 s; Zero-Span 10 s auf den 2-s-Takt | M-310 a, c |
| L4.5 | Neuzeichnen: das Skript ändert CH2 zehnmal pro Sekunde (statt `+` halten); Spektrum von CLK0 vorher und währenddessen | M-310 b (sinngemäß) |
| L4.6 | CH2 = 10,1 MHz aus / an; Linie bei 10,1 MHz an CLK0 in dBc | M-311 |
| L4.7 | Grundwelle: 10 MHz bei Drive 2 / 4 / 6 / 8 mA; 1 / 30 / 100 MHz bei 8 mA; dazu 20 Punkte 1 … 160 MHz bei 8 mA; Dämpfung aus V-05 abziehen | M-312 |
| L4.8 | Oberwellen 2 … 9 bei 10 MHz je Drive; Eingangsdämpfung nach V-06; Span 0 … 100 MHz, RBW 1 kHz | M-414 |
| L4.9 | Phasenrauschen 10 und 14,2 MHz bei 100 Hz … 100 kHz Ablage; Eigenrauschen aus V-06 daneben | M-416 (voraussichtlich ○) |
| L4.10 | Spread `DOWN05`: Span 100 kHz um 9,975 MHz, RBW 1 kHz, Max-Hold; Zero-Span an der Bandkante für die Modulationsrate | M-422 |
| L4.11 | 455 kHz: Spektrum 0 … 5 MHz | M-421 |
| L4.12 | WLAN als Client an (einmalig über REST oder GUI), dann wie L4.4 | M-317, M-318 |

### 6.5 L5 – Aufwärmdrift und Kurzzeitstabilität (unbeaufsichtigt)

Start nach ≥ 2 h stromlos, am besten als erster Lauf am Morgen, wenn die Messgeräte schon warm sind.

| Schritt | Ablauf | liefert |
|---|---|---|
| L5.1 | Einschalten, CH1 = CH2 = 10 MHz (Korrektur vom Vortag); 60 min alle 10 s: 2.1 an CLK1, SSA-Zähler an CLK0; Raumtemperatur alle 15 min (von Hand, das Skript fragt) | M-313 |
| L5.2 | aus derselben Aufzeichnung nach 30 min: Allan-Abweichung für τ = 1 … 100 s (2.1 liefert ≈ 1 Wert/s, begrenzt durch die Übertragung) | M-411 |

### 6.6 L6 – Varianten mit Umstecken (halbautomatisch)

| Schritt | Variante | Ablauf | liefert |
|---|---|---|---|
| L6.1 | S1 | Aussetzer an CLK0 auf dem SDS: L2.3 mit CH1 | M-114 Originalform |
| L6.2 | S1 | Spektrum CLK1; CH1 = 10,1 MHz aus / an | M-311 Gegenrichtung |
| L6.3 | Kabel C an das SSA (statt A) | Spektrum CLK2 bei IQ 4,73 und 10 MHz gegen CLK0 aus L4 | M-417 |
| L6.4 | S2 (+ S1 für CLK0) | `OUTP:OFFS LOW/HIGH/HIZ`, Ausgänge ein und aus; Mittelwert an K1/K3 bei 1 MΩ und 50 Ω; DMM und 10 kΩ von Hand | M-117, M-118 |
| L6.5 | S3a | CH2 = 10 MHz an; 20× Neustart über RTS; Zeit vom EN-Abfall bis zum Ende des Signals an K1; 1× `pio run -t upload` | M-119 |
| L6.6 | S3b | I2C-Trigger auf Adresse 0x60, Dekoder; Leitung auf GND legen (von Hand) | M-314 (statt LOGIK), M-315 |
| L6.7 | Versorgung | L4.4 mit PC-USB direkt, USB-Netzteil, Powerbank | M-310 d |
| L6.8 | Build IQ-Fehler | Fehlerzustand mit dem Build `esp32-2432S028R-faultiq`, CLK2 an K3 (DC, 1 MΩ mit S2) | M-401, M-402 |

---

## 7. Abdeckung von MESSPLAN.md

Zeichen: **●** vollständig mit diesem Aufbau (automatisch) · **◐** teilweise oder eingeschränkt · **○** nur als obere Schranke · **–** nicht mit diesen Geräten (Handmessung H oder anderes Gerät).

| M-ID | Inhalt | Abdeckung | Lauf | Anmerkung |
|---|---|---|---|---|
| M-003, M-004 | Geräte, Eigenmessung | ● | V-01 … V-06 | |
| M-101 … M-106 | Verbindung, Modul an/ab, Versorgungsausfall | ◐ | – | Umstecken von Hand; `SYST:SI5351?` und SDS-Aufzeichnung laufen automatisch mit |
| M-107, M-108 | Quarzfehler, Kalibrieren | ● / ◐ | L1.1, L1.2 | Bedienaufwand (Tipps, Zeit; U-11) bleibt H |
| M-109 … M-112 | Genauigkeit, `Integer`, Grenzen | ● | L1.3 … L1.5 | Auflösung über 2.1 statt über einen Zähler |
| M-113, M-114 | Aussetzer beim Abstimmen | ● | L2.1 … L2.4, L6.1 | Halten von `+` am Touch bleibt H |
| M-115, M-116 | IQ-Phasen | ● | L3.1, L3.2, L3.9 | 99 MHz mit Kabeltausch, sonst ggf. ○ |
| M-117, M-118 | Pegel im Zustand OFF | ◐ | L6.4 | 10-kΩ-Versuche und DMM von Hand |
| M-119 | Ausgang beim Neustart | ● | L6.5 | Neustart per RTS |
| M-120 … M-129 | Speichern, Display, Touch, Watchdog | – | H | keine HF-Messung |
| M-201 … M-216 | WLAN, Web, Sicherheit | – | H | PC und Netz |
| M-301, M-302 | Tuning-Modi, Nebenlinien | ● | L4.1, L4.2 | |
| M-303, M-304 | Aussetzer `Low noise`, Realignment | ● | L2.5, L2.6 | |
| M-305 | PLL ohne Reset (Testbuild) | ◐ | L1 | Einschwingen über 2.1 mit 1-ms-Fenstern |
| M-306 … M-309 | Wiederholgenauigkeit, Skew, ungerade Teiler, Tastverhältnis | ● | L3.3 … L3.6, L3.9 | |
| M-310 | Plattformstörungen | ● | L4.4, L4.5, L6.7 | b) sinngemäß per SCPI statt Halten |
| M-311, M-312 | Übersprechen, Pegel | ● | L4.6, L4.7, L3.10, L6.2 | Pegel ±1 dB |
| M-313 | Aufwärmdrift | ● | L5.1 | |
| M-314, M-315 | I2C auf GND, Registerdump | ◐ | L6.6 | SDS-Dekoder statt LOGIK |
| M-316 | Touch | – | H | |
| M-317, M-318 | WLAN im Spektrum | ● | L4.12 | |
| M-401, M-402 | IQ-Fehlerzustand | ● | L6.8 | |
| M-403 … M-410 | I2C-Fehlerfälle | ◐ | L6.6 | zwei Leitungen, keine lange Aufzeichnung; LOGIK bleibt besser |
| M-411, M-412 | Kurzzeitstabilität, Phasendrift | ● | L5.2, L3.8 | |
| M-413, M-419, M-420 | QSD, Mischer, Soundkarte | – | – | Mischer nötig; das SSA misst die ZF, sobald einer da ist |
| M-414, M-415 | Oberwellen, ganzzahlig → fraktional | ● | L4.8, L4.3 | |
| M-416 | Phasenrauschen | ○ | L4.9 | Eigenrauschen des SSA voraussichtlich höher als das des SI5351 |
| M-417, M-418 | CLK2-Spektrum, Nahfeld | ● / – | L6.3 / – | Nahfeld mit Sonde am SSA von Hand |
| M-421 … M-423 | 455 kHz, Spread, Spread mit IQ | ● | L4.10, L4.11, L1.6, L3.7, L3.9 | |

**Nicht mit diesen Geräten:** Phasenrauschen unterhalb des Eigenrauschens des SSA (dafür bräuchte es einen Phasenrauschmesser oder Kreuzkorrelation). Δt deutlich unter 20 ps ist nicht sicher messbar (200 MHz Bandbreite, 8 bit).

---

## 8. Software

Die Skripte liegen in [tools/messung/](tools/messung/) (Python 3.8+, Kommentare und Ausgaben englisch):

| Datei | Inhalt |
|---|---|
| `measure.py` | Kommandozeile: `run`, `check`, `report`, `docplots`, `list` |
| `steps.py` | V-01 … V-07 und L1.1 … L6.8 als Funktionen mit ID, Titel und Mess-IDs; Gruppen `V`, `L1` … `L6`, `all` (= V, L1 … L4, L6) |
| `bench.py` | Messplatz G: Steckaufforderungen (Abschnitt 4), Messfunktionen 2.1 bis 2.5, Kalibrierwerte aus V-03 … V-06 |
| `analysis.py` | Auswertung ohne Geräte: Flanken mit Hysterese und sinc-Verfeinerung, Frequenz als Verhältnis, Phase, Tastverhältnis, Aussetzer, Klammermessung, Allan-Abweichung, Spektrum |
| `siglent.py` | SDS, SSA, SDG über SCPI; alle Siglent-Befehle an dieser einen Stelle |
| `dut.py` | Prüfling nur mit den Befehlen aus [docs/REMOTE_API.md](docs/REMOTE_API.md) 3.3; nach jedem Schreibbefehl `SYST:ERR?`, höchstens ≈ 17 Schreibbefehle/s (Grenze der Firmware: 20/s) |
| `links.py` | Verbindungen `visa:`, `tcp:`, `usb:`, `sim:` |
| `store.py` | Ablage 10.1, Bewertung nach MESSPLAN.md 3.6 |
| `report.py` | Diagramme und Protokoll (Abschnitt 10), Handbuchbilder (11) |
| `sim.py` | simulierter Messplatz mit denselben SCPI-Texten (Trockenlauf, Tests) |
| `test_messung.py` | Tests der Auswertung gegen synthetische Signale und ein Trockenlauf bis zum Protokoll |

```text
pip install numpy matplotlib pyvisa pyvisa-py pyserial

python tools/messung/test_messung.py                         # Tests (≈ 2 min)
python tools/messung/measure.py run all L5 --sim --quick     # Trockenlauf ohne Geräte
python tools/messung/measure.py check --dut usb:COM13 --sds visa:TCPIP::<ip>::INSTR --ssa visa:TCPIP::<ip>::INSTR
python tools/messung/measure.py run V L1 L2 L3 L4 L6 --dut usb:COM13 --sds ... --ssa ... --board B
python tools/messung/measure.py run L5 ...                   # am nächsten Morgen
python tools/messung/measure.py report messungen/<Kampagne>  # nach Handeinträgen erneut erzeugen
```

Festlegungen:

- **Erst `check`, dann V-01:** `check` sendet jede Abfrage der Siglent-Geräte einmal und listet, was nicht antwortet. Die Befehle folgen den Programmierhandbüchern der Serien SDS1000X-E, SSA3000X Plus und SDG2000X; die mit `VERIFY` markierten (Dropout-Trigger, `SAST?`, Detektornamen, Markerzähler, Rauschmarker, Bildschirmfoto des SSA) sind die wahrscheinlichsten Abweichungen und werden beim ersten Lauf am Gerät bestätigt oder in `siglent.py` korrigiert.
- **Aufnahmelänge:** 14M Punkte (14 ms bei 1 GSa/s) für L1.1 … L1.3 und L5; der Sweep L1.4 nimmt 1,4M, weil die Übertragung von 14M über das LAN mehrere Sekunden dauert. Unter 50 kHz läuft die Aufnahme mit 100 MSa/s, damit die 10-MHz-Referenz nicht unterabgetastet wird.
- **Flankenzeit mit sinc-Verfeinerung:** Lineare Interpolation zwischen zwei Abtastwerten hat einen Fehler, der davon abhängt, wo die Flanke zwischen den Abtastwerten liegt. Bei 10 MHz und 1 GSa/s liegt jede Flanke an derselben Stelle, der Fehler mittelt sich nicht heraus: 30 ps Versatz wurden in den Tests als 50 ps gemessen. Die Auswertung verfeinert deshalb jede Flanke mit Lanczos-Interpolation (das Signal ist mit 200 MHz Bandbreite bei 1 GSa/s nahezu bandbegrenzt); der Restfehler liegt bei ≤ 3 ps. Verfeinert werden höchstens 20 000 gleichmäßig verteilte Flanken je Aufnahme, nummeriert aus allen Flanken.
- **Abbruch:** Ein fehlgeschlagener Schritt wird mit ○ und der Fehlermeldung protokolliert, die Ausgänge gehen aus, der Lauf geht mit dem nächsten Schritt weiter. Strg+C schaltet die Ausgänge ab und beendet den Lauf; das Protokoll entsteht trotzdem.
- **Simulation:** `--sim` ersetzt Prüfling und Messgeräte durch ein Modell (Quarzfehler +9,7 ppm, Zeitbasis des SDS +18 ppm, 30 ps Versatz CLK2, 49,6 % Tastverhältnis). Seine Werte belegen nur, dass jeder Codepfad läuft; das Protokoll eines Trockenlaufs trägt im Kopf „Simulation: ja“.

---

## 9. Ablauf und Zeitbedarf

| Block | Inhalt | aktiv | unbeaufsichtigt |
|---|---|---|---|
| Aufbau | G verkabeln, IP-Adressen, Geräte einschalten (≥ 40 min Aufwärmzeit SSA) | 40 min | 40 min |
| V | V-01 … V-07 (V-04 b + 30 min, wenn das SDG genutzt wird) | 60 min | – |
| L1 | Frequenz (mit Sweep 3 × 200 Punkte) | 5 min | ≈ 2 h |
| L2 | Aussetzer | 5 min | 30 min |
| L3 | IQ-Phasen und Kurvenformen (mit 30 min Drift) | 15 min (Kabeltausch, S1) | 1 h 15 min |
| L4 | Spektrum | 10 min | 1 h 30 min |
| L6 | Varianten | 1 h 30 min | – |
| L5 | Aufwärmdrift (nächster Morgen) | 10 min | 60 min |
| Protokoll | erzeugen, Handeinträge, Durchsicht (10.4) | 45 min | – |
| | **Summe** | **≈ 4 h 30 min** | **≈ 7 h** |

**Reihenfolge am ersten Tag:** Aufbau → V-01, V-02, V-07 → V-03 … V-06 → G herstellen → L1 → L2 → L3 → L4 → L6. Am zweiten Morgen L5, danach das Protokoll.

---

## 10. Messprotokoll mit Bildern

Am Ende einer Messkampagne erzeugt das Skript ein **vollständiges Messprotokoll**: ein Markdown-Dokument mit allen Messwerten, Bewertungen, Diagrammen und Bildschirmfotos, das auf GitHub direkt lesbar ist. Es ersetzt das von Hand ausgefüllte Protokoll aus MESSPLAN.md Anhang A für alle Messungen dieses Plans; Handmessungen (H) werden im selben Dokument nachgetragen.

### 10.1 Ablage

```text
messungen/
  <Datum>_<Board>_<Firmware>.md          Protokoll (committet)
  <Datum>_<Board>_<Firmware>/
    img/                                 Bilder des Protokolls (committet)
      setup-*.jpg                        Fotos von Aufbau und Gerät
      <M-ID>_<Schritt>.png               Diagramme aus Messdaten
      <M-ID>_<Schritt>_screen.png        Bildschirmfotos der Messgeräte
    data/                                verdichtete Daten je Messung als CSV (committet)
    raw/                                 Rohdaten der SDS-Aufnahmen, Logs (nicht committet)
  firmware/                              Prüflinge (nicht committet)
```

`.gitignore` hält `raw/`, `firmware/` und alles andere unter `messungen/*/` heraus und lässt `img/` und `data/` durch. Größe: Diagramme als PNG mit ≤ 100 KB, Bildschirmfotos auf 8 Farben bzw. JPEG reduziert, Fotos auf 1600 px Breite; Ziel ≤ 10 MB je Protokoll.

### 10.2 Aufbau des Protokolls

| Abschnitt | Inhalt | Quelle |
|---|---|---|
| Kopf | Datum, Bediener, Board, Firmware (Version, Commit, SHA-256), Korrektur, Raumtemperatur Anfang/Ende, Versorgung (Isolator / Powerbank), Variante der Referenzkette | V-07, `meta.json` |
| Messgeräte | `*IDN?` und Firmware von SDS, SSA (und SDG), GPS-Lock; Datum der letzten Kalibrierung (von Hand) | V-01 |
| Aufbau | Fotos des Gesamtaufbaus, der Referenzkette, des Prüflings mit Kabeln; Schaltbild aus 3.1 | V-07, Fotos |
| Unsicherheiten | Δt₀, u_t, u_φ je Frequenz; u_f; Kabeldämpfung über f (Diagramm); Eigenrauschen und Eigenverzerrung des SSA | V-03 … V-06 |
| Zusammenfassung | eine Zeile je M-ID: Bewertung ✔ ✘ ○ i –, Kernwert, Verweis auf den Abschnitt; Zählung je Zeichen; Befunde MB-n | aus allen Läufen |
| L1 … L6 | je Schritt: Einstellungen (SCPI-Befehle wörtlich), Messwerttabelle mit Sollwert, Messwert, u und Bewertung, Diagramm, bei Spektren das Bildschirmfoto | Läufe |
| Handmessungen | Tabellen für H-Punkte (Touch, Speichern, 10-kΩ-Versuche, Tipps bei M-108), vom Skript leer vorbereitet | von Hand |
| Befunde | je ✘ und je unerwartetem Verhalten ein Eintrag nach MESSPLAN.md Anhang E, mit Bild | Läufe, von Hand |
| Anhang | Liste aller Dateien in `img/` und `data/`, Skriptversion (Commit) | Generator |

### 10.3 Bilder im Protokoll

**Diagramme aus Messdaten** (gezeichnet vom Skript, einheitlich gestaltet):

| Diagramm | Lauf |
|---|---|
| Kabel- und Dämpfungsglied-Verlust über f; Kanalversatz Δt₀ über f | V-03, V-05 |
| Abweichung f_mess − f_act über f, je `Tuning` eine Farbe, Grenze als Linie | L1.4 |
| Klammermessung: δ je Messpunkt mit Fehlerbalken | L1.3 |
| Aussetzer: Zeitverlauf um jeden ausgelösten Dropout (K1, K3) | L2 |
| IQ-Kurvenformen 0 / 90 / 180 / 270° bei 10 MHz (4 Teilbilder) | L3.1 |
| Phasenabweichung über f, je Phase eine Linie, Grenze ±1° | L3.2, L3.3 |
| Phasendrift über 30 min mit Raumtemperatur | L3.8 |
| Kurvenform je Drive an 50 Ω | L3.10 |
| Spektren (Tuning-Vergleich, Oberwellen, Spread, 455 kHz, Störungen, WLAN) aus den Kurvendaten | L4 |
| Pegel über f bei 8 mA; Pegel über Drive bei 10 MHz | L4.7 |
| Aufwärmdrift in ppm über 60 min; Allan-Abweichung über τ | L5 |
| Neustart: EN und CLK1 über der Zeit | L6.5 |
| Pegel im Zustand OFF je `Off state` (Balken) | L6.4 |

**Bildschirmfotos** der Messgeräte bei jedem Spektrum (L4), bei jedem ausgelösten Aussetzer (L2) und je einmal für Phase und Tastverhältnis (L3), als Beleg der Geräteeinstellungen.

**Fotos** (von Hand, das Skript fordert sie in V-07 an): Gesamtaufbau, Referenzkette an den Rückseiten, Prüfling mit Kabeln, Info-Karte des Prüflings, Steckvarianten S1 bis S3.

### 10.4 Erzeugen und abschließen

1. Jeder Lauf schreibt seine Messwerte (`results.csv`), Kurvendaten (`data/`) und Bildschirmfotos (`img/`) sofort; ein abgebrochener Lauf verliert nichts.
2. Der Generator erzeugt aus diesen Dateien Diagramme und Protokoll; er lässt sich jederzeit erneut aufrufen (z. B. nach Handeinträgen) und überschreibt nur die erzeugten Teile. Handeinträge stehen in Blöcken, die er nicht anfasst.
3. Durchsicht von Hand: Plausibilität, Befunde, Handmessungen. Danach ein Commit je Messtag (ARBEITSPLAN Regel 10); Befunde sofort in ARBEITSPLAN Abschnitt 2.

---

## 11. Werte und Bilder für Dokumentation und Handbuch

Die Läufe liefern auch die Zahlen und Bilder, die im Handbuch bisher als „typisch, nicht gemessen“ oder „folgt nach den Messungen“ stehen. Dafür gelten drei Regeln:

1. **Ins Handbuch kommen nur bewertete Werte.** Ein Wert geht erst hinein, wenn der Lauf ohne ✘ durchgelaufen ist und V-03 bis V-06 die Unsicherheit belegen. Jeder Wert nennt Bedingungen (Drive, Last, Frequenz, Board) und Unsicherheit.
2. **Diagramme statt Bildschirmfotos.** Die Bildschirmfotos bleiben im Protokoll. Für das Handbuch werden ausgewählte Diagramme aus dem Protokoll übernommen, sprachneutral beschriftet (Formelzeichen und Einheiten wie `f / MHz`, `dBc`, `t / ms`), damit DE und EN dieselbe Datei verwenden. Herstellernamen der Messgeräte stehen nur im Protokoll.
3. **Reproduzierbar:** Die Daten eines Handbuchbilds kommen nach `docs/data/meas-<thema>.csv` (eine Datei aus `messungen/<Kampagne>/data/`, mit Kopfzeilen: Datum, Firmware, Board, Bedingungen, Verweis auf das Protokoll). `python tools/messung/measure.py docplots` zeichnet daraus `docs/img/meas-<thema>.png`, ohne Messgeräte, mit derselben Gestaltung wie das Protokoll.

### 11.1 Werte

| Stelle im Handbuch (DE, Abschnitt) | heute | Messung | Lauf | Ergebnis im Handbuch |
|---|---|---|---|---|
| 18 „Ausgangspegel an 50 Ω“, Tabelle Drive → dBm | typisch, nicht gemessen | M-312 | L4.7, L3.10 | Grundwelle in dBm je Drive bei 10 MHz; Spalte Uss gemessen statt gerechnet; Zeilen 1 / 30 / 100 MHz bei 8 mA |
| 8 `Drive all`: Hinweis auf die Pegeltabelle | „noch nicht gemessen“ | M-312 | L4.7 | Verweis bleibt, Vermerk entfällt |
| 18 „LO-Pegel“: 8 mA + 3 dB bzw. 6 mA + 1 dB | typisch | M-312 | L4.7 | Empfehlung mit gemessenem Pegel |
| 18 „Oberwellen und Tiefpass“ | gerechnet | M-414 | L4.8 | gemessene Spalte neben der gerechneten, je Drive |
| 18 Tastverhältnis | „noch nicht gemessen (M-309)“ | M-309 | L3.6, L3.9 | Tastverhältnis in % bei 7,1 / 10 / 14,2 / 99 MHz; 2. Oberwelle gemessen |
| 1 „Über 100 MHz sinkt der Ausgangspegel“ | qualitativ | M-312 | L4.7 | Pegelabfall in dB bei 112,5 / 150 / 160 MHz |
| 1, 5 „unter 1 MHz starke Oberwellen“ | qualitativ | M-421 | L4.11 | Oberwellen bei 455 kHz in dBc |
| 5 `Out`, Abweichung je Modus | Host-Test | M-109 | L1.3, L1.4 | „am Gerät bestätigt: größte gemessene Abweichung … Hz“ |
| 8 `Tuning`: Nebenlinien je Modus | „Vergleichsmessungen in MESSPLAN“ | M-301, M-302, M-415 | L4.1 … L4.3 | Tabelle Modus → größte Nebenlinie (dBc @ Abstand) |
| 8 Spread Spectrum: Band, Rate, Aussetzer beim Umschalten | Sollwerte, qualitativ | M-422 | L4.10, L2.7 | gemessene Bandgrenzen, Rate, Dauer des Aussetzers |
| 7 IQ: Phasengenauigkeit, Aussetzer beim Abstimmen des Masters | Anforderung, qualitativ | M-115, M-116, M-304, M-307 | L3, L2.6 | Abweichung je Frequenz, Kanalversatz, Dauer in ms |
| 8 `Off state`: „bis zur Hardwareprüfung M-118“ | offen | M-117, M-118 | L6.4 | Pegeltabelle; Hinweis entfällt oder wird präzisiert (entscheidet FW-05) |
| 13 „TCXO (optional)“ | „folgt nach M-313, M-411“ | M-313, M-411 | L5 | Aufwärmzeit bis < 0,1 ppm/10 min, Allan-Abweichung bei 1 s, Empfehlung |
| 13 „Flashen und Reset“: Dauer | „folgt nach den Messungen“ | M-119 | L6.5 | Zeit vom Reset bis zum Ende des alten Signals, beim Upload in s |
| 13 „Aufbau und Versorgung“ | „wird noch gemessen“ | M-310 | L4.4, L4.5, L6.7 | Nebenlinien durch Backlight, Neuzeichnen, Versorgung in dBc |
| 15/16 WLAN-Einfluss | – | M-317, M-318 | L4.12 | Störlinien mit WLAN an (dBc) |
| docs/hardware.md „Outputs“, „Hints for a clean signal“ | – | M-312, M-310 | L4 | dieselben Werte auf Englisch |

### 11.2 Bilder

| Bild | Inhalt | aus | Abschnitt |
|---|---|---|---|
| `meas-iq-phases.png` | CLK1 und CLK2 bei 10 MHz, vier Teilbilder 0 / 90 / 180 / 270° | L3.1 | 7 |
| `meas-iq-accuracy.png` | Phasenabweichung über f, Grenze ±1° | L3.2, L3.3 | 7 |
| `meas-waveform-drive.png` | Kurvenform an 50 Ω bei 10 MHz für 2 / 4 / 6 / 8 mA | L3.10 | 8 `Drive all` |
| `meas-harmonics.png` | Spektrum 0 … 100 MHz bei 10 MHz, 8 mA; gerechnete Pegel als Marken | L4.8 | 18 |
| `meas-level-vs-frequency.png` | Grundwellenpegel über f bei 8 mA | L4.7 | 1, 18 |
| `meas-tuning-spurs.png` | ±100 kHz um 7,1 MHz: `Fractional`, `Low noise`, `Integer` | L4.1 | 8 `Tuning` |
| `meas-spread.png` | 10 MHz ohne Spread und mit `-0.5 %` | L4.10 | 8 Spread |
| `meas-warmup.png` | Frequenzabweichung über 60 min nach dem Einschalten | L5.1 | 13 TCXO |
| `meas-reset.png` | EN und CLK1 beim Neustart | L6.5 | 13 Flashen und Reset |
| `meas-455k-harmonics.png` | Spektrum 0 … 5 MHz bei 455 kHz | L4.11 | 1 / 5 |

Werte und Bilder kommen in DE und EN im selben Commit ins Handbuch (Aufgabe DOK-01). Der Vermerk „typisch, nicht gemessen“ entfällt nur für die tatsächlich gemessenen Zeilen. Die Bilder fallen unter `REUSE.toml` (`docs/**`).

---

## Anhang A: Quellen der Gerätedaten

| Gerät | Quelle | Hinweis |
|---|---|---|
| SDS1204X-E | Siglent, *SDS1000X-E Series Data Sheet* (Ausgabe für die 2-Kanal-Modelle); 4-Kanal-Werte (500 MSa/s bei vier Kanälen, 200 MHz) aus Siglent-Produktseiten und Händlerdaten (Batronix, TEquipment) | ADC-Paare und Kanalversatz des 4-Kanal-Modells in V-02 und V-03 messen |
| SSA3032X Plus | Siglent, *SSA3000X Data Sheet* (Vorgängerserie ohne „Plus“) für Referenz, Zähler, Eingangsgrenzen, Verzerrung; Plus-Werte (RBW ab 1 Hz, Phasenrauschen −98 dBc/Hz typ., DANL −161 dBm/Hz typ.) aus Siglent-Datenblatt E04G laut Händlerangaben | das aktuelle Plus-Datenblatt war nicht abrufbar; Werte in V-06 bestätigen |
| SDG2122X | Siglent, *SDG2000X Series Data Sheet* EN02H (siglentna.com, Download 2427) | Zählerauflösung nicht angegeben |

Die Datenblätter beschreiben Grenzen und typische Werte. Als Unsicherheiten verwendet dieser Plan nur die Eigenmessungen V-03 bis V-06 (Δt₀, u_t, u_φ, u_f, Kabeldämpfung, Eigenrauschen), nicht die Datenblattwerte.
