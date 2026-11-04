# Benutzeranleitung – CYD HF Generator

Firmware-Version: 1.3.1

Diese Anleitung ist die Referenz für das Verhalten der Firmware; [USER_MANUAL_EN.md](USER_MANUAL_EN.md) ist ihre Übersetzung.

## 1. Überblick

Der **CYD HF Generator** ist ein 3-Kanal-HF-Rechteckgenerator auf Basis eines ESP32-2432S028R Displays und eines SI5351-Taktgenerators.

Das Gerät erzeugt Rechtecksignale mit 50 % Tastverhältnis im Bereich:

```text
1 MHz bis 100 MHz
```

Die drei Ausgänge sind:

| Ausgang | Funktion |
|---|---|
| CH1 / CLK0 | Frei einstellbare Frequenz |
| CH2 / CLK1 | Frei einstellbare Frequenz |
| CH3 / CLK2 | IQ-Follower von CH1 oder CH2 |

CH3 folgt immer der Frequenz seines Masters und kann mit einer Phasenlage von 0°, 90°, 180° oder 270° betrieben werden. Der IQ-Betrieb ist im Bereich **4,730 MHz bis 100 MHz** möglich; dort sind alle vier Phasen verfügbar.

Auf der CONFIG-Seite lassen sich außerdem das Teilerverfahren des SI5351 (`Tuning`), Spread Spectrum für CH1 und der Zustand abgeschalteter Ausgänge wählen (Abschnitt 8).

---

## 2. Grundbedienung

Nach dem Einschalten bleibt das Display dunkel, bis das erste Bild gezeichnet ist; dann erscheint die Hauptseite. **Alle Ausgänge sind nach dem Einschalten aus**, auch wenn sie beim Ausschalten an waren. Frequenzen, CH3-Master und -Phase sowie alle CONFIG-Einstellungen bleiben gespeichert.

Die Hauptseite zeigt für jeden Kanal eine Karte mit:

- Kanalname und Status
- eingestellter Frequenz (Sollfrequenz)
- eigenem ON/OFF-Schalter
- Zusatzzeile: bei CH1/CH2 die Moduszeile (siehe unten) und `IQ master of CH3`, wenn CH3 eingeschaltet ist und diesem Kanal folgt (mit Moduszeile kurz `| IQ master`); bei CH3 Master und Phase, z. B. `CH1 / 90°`, bei `WAIT` und `! ERR` mit dem Grund (z. B. `CH1 / 90° / master off`)

Im Modus `Tuning` = `Integer` steht vor der Frequenz ein `~`, wenn die tatsächliche Frequenz um 1 Hz oder mehr von der Sollfrequenz abweicht (Abschnitt 8).

### Moduszeile

CONFIG-Einstellungen, die das Signal eines Kanals gegenüber den Werkseinstellungen verändern, stehen als Moduszeile auf der Hauptseite (Karten CH1 und CH2) und auf den Kanalseiten (CH1 bis CH3, in der Karte unter der `Out`-Zeile). Mit den Werkseinstellungen ist sie leer. Die Einträge stehen in dieser Reihenfolge, getrennt durch ` | `:

| Eintrag | Erscheint bei |
|---|---|
| `OFF = High` / `OFF = High-Z` | `Off state` ≠ `Low` |
| `SSC -0.5 %`, `SSC -1.5 %`, `SSC +/-0.5 %` | `Spread CH1` ≠ `Off`, nur bei CH1 und bei CH3 mit Master CH1 |
| `Low noise` / `Integer` | `Tuning` ≠ `Fractional` |
| `2 mA`, `6 mA`, `8 mA` | `Drive all` ≠ `4 mA` |

Beispiel: `SSC -0.5 % | Low noise | 8 mA` auf CH1, `Low noise | 8 mA` auf CH2. Mit `OFF = High` ist die Zeile orange, weil ein ausgeschalteter Ausgang dann Gleichspannung führt; sonst grau. Ist die Zeile auf der Hauptseite zu lang, endet sie mit `...`; die Kanalseite zeigt sie vollständig.

Bedienung auf der Hauptseite:

- **Karte antippen** öffnet die Seite des Kanals.
- **Schalter auf der Karte** (rechts, durch eine Linie abgetrennt) schaltet den Ausgang direkt ein oder aus, ohne die Seite zu wechseln. Ein Tipp auf die Frequenz oder `MHz` öffnet nur die Kanalseite.
- Die Frequenzen stehen rechtsbündig, Dezimalpunkt und `MHz` aller Karten untereinander.
- Die Statusleiste unten zeigt links den Speicherzustand (Abschnitt 8, `Saved`, `Saving...`, rot `Save failed` / `Not saved`) und hat rechts den Button `CONFIG` (gelb). Die Kanäle öffnen sich über ihre Karten.
- Auf farbigen Tasten und gewählten Chips ist die Schrift dunkel, ausgeschaltete Schalter haben eine hellgraue Spur, nicht gewählte Chips einen grauen Rand.
- Tasten am Bildschirmrand (z. B. `MAIN` oben rechts, `CONFIG` unten rechts, die äußeren NUMPAD-Tasten) reagieren bis an den Rand, auch auf einen Tipp knapp daneben in Richtung Rand. Der Abstand zwischen zwei Tasten bleibt frei.

Statusanzeige unter dem Kanalnamen (in der Titelzeile der Kanalseiten ebenso). Jeder Zustand außer `OFF` steht als farbiges Feld mit dunkler Schrift; Fehler beginnen mit `!`. So sind aktive und gestörte Ausgänge auch ohne Farbunterscheidung und aus der Entfernung erkennbar.

| Status | Bedeutung |
|---|---|
| `ON` (grünes Feld) | Ausgang aktiv |
| `OFF` (graue Schrift, kein Feld) | Ausgang aus |
| `! ERR` (rotes Feld) | Einstellen des Ausgangs ist fehlgeschlagen, obwohl der SI5351 antwortet; die Firmware versucht es alle 2 s erneut. Der Schalter zeigt weiter die gewünschte Einstellung |
| `WAIT` (oranges Feld) | CH3 ist eingeschaltet, kann aber gerade nicht laufen (Master aus oder Masterfrequenz außerhalb des IQ-Bereichs: unter 4,730 MHz, mit `Spread CH1` auf Master CH1 etwas enger, Abschnitt 8) |
| `! NO HW` (rotes Feld) | SI5351 nicht gefunden oder Verbindung verloren; die Firmware verbindet automatisch neu |

Die Farben der Kanäle: CH1 rot, CH2 grün, CH3 blau, CONFIG gelb.

Die Kanalseiten und CONFIG haben eine Titelzeile mit dem Button `MAIN` rechts zur Rückkehr auf die Hauptseite. Auf den Kanalseiten enthält sie zusätzlich den ON/OFF-Schalter und daneben den tatsächlichen Zustand wie auf der Hauptseite: `ON`, `OFF`, `WAIT` (nur CH3), `! ERR` oder `! NO HW`. Der Schalter zeigt, was eingestellt ist, der Text, was der Ausgang gerade tut; bei CH3 kann der Schalter also auf ON stehen, während der Text `WAIT` zeigt. Das NUMPAD hat stattdessen die Taste `◀` (Abschnitt 6).

Die RGB-LED zeigt auf den Kanal-Seiten ebenfalls die Kanal-Farbe:

| Seite | RGB-LED |
|---|---|
| CH1 | Rot |
| CH2 | Grün |
| CH3 | Blau |
| MAIN, CONFIG, NUMPAD | aus |

---

## 3. Frequenzanzeige

Die Frequenz wird in MHz mit gruppierten Dezimalstellen angezeigt.

Beispiele:

```text
1.000 000 MHz
5.000 000 MHz
10.700 000 MHz
100.000 000 MHz
```

Das entspricht:

| Anzeige | Frequenz |
|---|---:|
| 1.000 000 MHz | 1 MHz |
| 5.000 000 MHz | 5 MHz |
| 10.700 000 MHz | 10,7 MHz |
| 100.000 000 MHz | 100 MHz |

Angezeigt und gespeichert wird die **eingestellte Sollfrequenz** mit 1 Hz Auflösung. Der SI5351 kann nicht jede Frequenz exakt erzeugen; die tatsächliche Frequenz steht in der Zeile `Out` der Kanalseite (Abschnitt 5).

---

## 4. CH1 und CH2 verwenden

CH1 und CH2 sind frei einstellbare Frequenzausgänge.

### CH1 oder CH2 einschalten

1. Auf der Hauptseite die Karte `CH1` bzw. `CH2` antippen.
2. Den ON/OFF-Schalter in der Titelzeile einschalten.
3. Die Anzeige wechselt auf `ON`.
4. Am entsprechenden SI5351-Ausgang liegt ein Signal an:

| Kanal | SI5351-Ausgang |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |

Alternativ kann der Kanal direkt mit dem Schalter auf seiner Karte der Hauptseite eingeschaltet werden.

### CH1 oder CH2 ausschalten

1. Kanal-Seite öffnen (oder Schalter auf der Hauptseite verwenden).
2. ON/OFF-Schalter ausschalten.
3. Die Anzeige wechselt auf `OFF`.
4. Der Ausgang wird abgeschaltet und nimmt den unter `Off state` gewählten Zustand an (Standard `Low`, Abschnitt 8).

---

## 5. Frequenz mit + und − ändern

Die Kanalseite von CH1 und CH2 enthält:

- die Frequenzkarte mit der Zeile `Out`
- eine Reihe Schritt-Tasten: `1M`, `100k`, `10k`, `1k`, `100`, `10`, `1` (in Hz), wie die Stellen der Frequenz darüber: links die großen, rechts die kleinen Schritte
- große Tasten `−` (links) und `+` (rechts)
- die Taste `NUMPAD`

Die Tasten `−` und `+` ändern die Frequenz um den aktuell gewählten Schritt. Der gewählte Schritt ist farbig hervorgehoben und wird direkt auf der Kanalseite (oder in CONFIG) gewählt.

**Stelle direkt antippen:** Ein Tipp auf eine Ziffer der Frequenzanzeige wählt diese Stelle als Schritt (z. B. die dritte Nachkommastelle = 1 kHz). Die Stelle, die `−`/`+` ändert, ist in der Kanalfarbe hervorgehoben. Ziffern über 1 MHz (10 MHz, 100 MHz) wählen den größten Schritt 1 MHz. Ein Tipp auf die Einheit `MHz` öffnet das NUMPAD. Das Antippen ist eine Abkürzung zu den Schritt-Tasten: Die Ziffern sind nur 2–3 mm breit, ein Tipp auf den Punkt oder eine Lücke wählt die nähere Nachbarziffer.

**Gedrückt halten:** Nach ca. 0,4 s wiederholt sich der Schritt automatisch etwa alle 0,1 s, bis die Taste losgelassen wird.

Ob eine Frequenzänderung den Ausgang kurz unterbricht, hängt von `Tuning` ab (Abschnitt 8):

- `Fractional` (Standard): keine Unterbrechung. Nur in seltenen Fällen muss die PLL umgeschaltet werden; dann gibt es einen kurzen Aussetzer.
- `Low noise`: kurzer Aussetzer, wenn der Teiler wechselt.
- `Integer`: kurzer Aussetzer, wenn sich die PLL ändert.
- Im IQ-Betrieb setzen Master und CH3 bei jeder Frequenzänderung kurz aus (Abschnitt 7).

### Tatsächliche Frequenz (`Out`)

Unter der Frequenz zeigt die Zeile `Out` die Frequenz, die der SI5351 mit den programmierten Teilern tatsächlich erzeugt, und in Klammern die Abweichung von der Sollfrequenz:

```text
Out 10.000 000 MHz (+0.3 Hz)
```

- Die Werte setzen voraus, dass die Frequenzkorrektur (Abschnitt 8) stimmt.
- Ab 1 Hz Abweichung erscheint die Zeile orange.
- Abweichung je Modus: `Fractional` ≤ 0,5 Hz, `Low noise` bis 1,5 Hz. Im Modus `Integer` liegt die Frequenz auf einem Raster und kann um einige kHz abweichen, z. B. bei 7,1 MHz:

```text
Out 7.102 273 MHz (+2272.7 Hz)
```

- Mit Spread Spectrum auf CH1 zeigt die Zeile die mittlere Frequenz und die Einstellung:

```text
Out 9.975 000 MHz mean, SSC -0.5 %
```

- Bei ausgeschaltetem Kanal beginnt die Zeile mit `Off | `, z. B. `Off | Out 10.000 000 MHz (+0.3 Hz)`: So würde der Ausgang nach dem Einschalten laufen.
- Folgt CH3 diesem Kanal, steht in der Moduszeile darunter `IQ master of CH3 (90°)` mit der Phase von CH3; Frequenzänderungen verschieben dann beide Ausgänge.

Bei Fehlern steht statt der Frequenz in Rot:

| Anzeige | Bedeutung |
|---|---|
| `SI5351 not found` | SI5351 fehlt oder Verbindung verloren (Status `! NO HW`) |
| `Output error, retrying` | Einstellen fehlgeschlagen, die Firmware versucht es alle 2 s erneut (Status `! ERR`) |

Beispiel:

```text
Aktuelle Frequenz: 10.000 000 MHz
Schritt: 1k
+ drücken
Neue Frequenz: 10.001 000 MHz
```

Noch ein Beispiel:

```text
Aktuelle Frequenz: 7.100 000 MHz
Schritt: 10k
− drücken
Neue Frequenz: 7.090 000 MHz
```

Die Frequenz bleibt immer im Bereich 1 MHz bis 100 MHz.

---

## 6. Frequenz über NUMPAD eingeben

Auf CH1 oder CH2:

1. `NUMPAD` drücken.
2. Zahlen eingeben. Unter dem Eingabefeld steht links die aktuelle Frequenz, z. B. `Now 10.000 000 MHz`.
3. Optional einen Dezimalpunkt verwenden (nur einer ist möglich).
4. Eine Einheit drücken:
   - `Hz`
   - `kHz`
   - `MHz`

Die Einheit bestätigt die Eingabe direkt und kehrt zur Kanalseite zurück.

Mit der Taste `◀` oben links geht es **ohne Änderung** zurück zur Kanalseite.

### Tastenfeld

```text
 7   8   9   MHz
 4   5   6   kHz
 1   2   3   Hz
 .   0   ⌫   C
```

| Taste | Funktion |
|---|---|
| 0–9 | Zifferneingabe |
| . | Dezimalpunkt |
| `⌫` | Letztes Zeichen löschen |
| C | Eingabe löschen |
| Hz | Wert als Hz übernehmen |
| kHz | Wert als kHz übernehmen |
| MHz | Wert als MHz übernehmen |
| `◀` | Zurück zur Kanalseite ohne Änderung |

Maximal 9 Zeichen sind erlaubt; ein zehntes zeigt `Max. 9 characters`.

### Ungültige Eingabe

Liegt der Wert außerhalb von 1–100 MHz oder ist die Eingabe leer bzw. ungültig, wird sie **nicht übernommen**. Die Eingabe bleibt stehen, der Rahmen wird rot und rechts unter dem Feld erscheint der Grund in Rot:

| Anzeige | Bedeutung |
|---|---|
| `Allowed 1 - 100 MHz` | Wert außerhalb des Bereichs, z. B. `500` `MHz` |
| `Enter a number` | keine Ziffer eingegeben (leer oder nur `.`) |
| `Max. 9 characters` | Feld voll |

Die Eingabe lässt sich mit `⌫` korrigieren oder mit `C` löschen; die nächste Taste löscht den Hinweis.

### Beispiel 1: 10 MHz eingeben

1. CH1 öffnen.
2. `NUMPAD` drücken.
3. Eingeben:

```text
10
```

4. `MHz` drücken.
5. CH1 wird auf 10 MHz gesetzt.

Anzeige:

```text
10.000 000 MHz
```

### Beispiel 2: 10,7 MHz eingeben

1. CH2 öffnen.
2. `NUMPAD` drücken.
3. Eingeben:

```text
10.7
```

4. `MHz` drücken.

Anzeige:

```text
10.700 000 MHz
```

### Beispiel 3: 1 000 kHz eingeben

1. CH1 öffnen.
2. `NUMPAD` drücken.
3. Eingeben:

```text
1000
```

4. `kHz` drücken.

Anzeige:

```text
1.000 000 MHz
```

### Beispiel 4: 5 000 000 Hz eingeben

1. CH1 öffnen.
2. `NUMPAD` drücken.
3. Eingeben:

```text
5000000
```

4. `Hz` drücken.

Anzeige:

```text
5.000 000 MHz
```

---

## 7. CH3 als IQ-Follower verwenden

CH3 ist kein frei einstellbarer Kanal. CH3 folgt CH1 oder CH2.

Auf der CH3-Seite können eingestellt werden:

- Master: Tasten `CH1` oder `CH2`
- Phase: Tasten `0`, `90`, `180` oder `270` (Grad)
- ON/OFF über den Schalter in der Titelzeile

Die gewählte Taste ist farbig hervorgehoben. Die Frequenz von CH3 ist immer identisch mit der Frequenz des Masters; die Drive-Strength stellt CONFIG für alle Kanäle ein.

### Statuszeile

Unter der Frequenz zeigt die CH3-Seite den aktuellen Zustand:

| Anzeige | Bedeutung |
|---|---|
| `Off  \|  follows CHx` | CH3 ausgeschaltet |
| `Out 10.000 000 MHz (+0.3 Hz)` | IQ-Ausgang läuft; tatsächliche Frequenz von CLK2 (Abschnitt 5) |
| `Waiting: master CHx is off` | CH3 eingeschaltet, Master aus |
| `Waiting: CHx below 4.730 MHz` | CH3 eingeschaltet, Masterfrequenz zu niedrig (mit Spread Spectrum auf CH1 höher, z. B. `4.749`) |
| `Waiting: CHx above 99.502 MHz` | CH3 eingeschaltet, Masterfrequenz zu hoch; nur mit Spread `+/-0.5 %` auf Master CH1 |
| `IQ setup failed, retrying` | Einstellen des IQ-Paars fehlgeschlagen (Status `! ERR`); CLK2 ist aus, der Master läuft weiter, die Firmware versucht es alle 2 s erneut |
| `SI5351 not found` | SI5351 fehlt |

**Wichtig:** Ist CH3 eingeschaltet, aber gerade nicht möglich (Master aus oder unter 4,730 MHz), bleibt der Schalter auf ON. Der Ausgang CLK2 bleibt aus, bis die Bedingungen erfüllt sind – dann startet CH3 **automatisch**. Es erscheint keine Fehlermeldung.

### Frequenzbereich und Phasen

| Phase | Verfügbar |
|---|---|
| 0° | 4,730 – 100 MHz |
| 90° | 4,730 – 100 MHz |
| 180° | 4,730 – 100 MHz |
| 270° | 4,730 – 100 MHz |

180° und 270° werden durch Invertieren des CH3-Ausgangs erzeugt (0° bzw. 90° + Invertierung). Unterhalb von 4,730 MHz würde der SI5351-Oszillator (VCO) unter 600 MHz und damit außerhalb seiner Spezifikation laufen.

Genauigkeit: In den Modi `Fractional` und `Low noise` weicht die IQ-Frequenz höchstens ≈ 1,4 Hz von der Sollfrequenz ab. Im Modus `Integer` nutzt auch der IQ-Betrieb eine ganzzahlige PLL; die Frequenz liegt dann auf dem Raster (Abschnitt 8), die Phasen funktionieren unverändert.

### Abstimmen im IQ-Betrieb

Master und CH3 teilen sich eine PLL. Jede Änderung der Masterfrequenz oder der Frequenzkorrektur und jeder Phasenwechsel von 0°/180° auf 90°/270° (oder umgekehrt) schaltet beide Ausgänge kurz ab, stellt sie neu ein und richtet die Phasenlage über einen Neustart der PLL neu aus. Ein Wechsel zwischen 0° und 180° bzw. 90° und 270° und eine Änderung der Drive-Strength kommen ohne Aussetzer aus.

Spread Spectrum (`Spread CH1`) wirkt auch auf CH3, wenn CH1 sein Master ist; mit Master CH2 ist CH3 nicht betroffen.

### Beispiel: CH3 folgt CH1 mit 90° Phase

1. CH1 auf 10 MHz einstellen.
2. CH1 einschalten.
3. Zur CH3-Seite wechseln.
4. Master `CH1` antippen.
5. Phase `90` antippen.
6. CH3 einschalten.

Ergebnis:

```text
CH1 / CLK0 = 10 MHz, 0° Referenz
CH3 / CLK2 = 10 MHz, 90° verschoben
```

### Beispiel: CH3 folgt CH2 mit 180° Phase

1. CH2 auf 5 MHz einstellen.
2. CH2 einschalten.
3. CH3 öffnen.
4. Master `CH2` antippen.
5. Phase `180` antippen.
6. CH3 einschalten.

Ergebnis:

```text
CH2 / CLK1 = 5 MHz
CH3 / CLK2 = 5 MHz, 180° verschoben
```

---

## 8. CONFIG-Seite

Auf der CONFIG-Seite werden globale Einstellungen geändert. Jede Änderung wirkt sofort und wird automatisch gespeichert (Abschnitt 9).

### Aufbau und Scrollen

Die Titelzeile mit Speicheranzeige und `MAIN` bleibt stehen. Der Bereich darunter ist höher als der Bildschirm und lässt sich mit dem Finger senkrecht verschieben. CONFIG ist die einzige Seite, die scrollt.

| Zeile | Bedienelemente |
|---|---|
| `Drive all` | Auswahlliste `2 mA` … `8 mA` |
| `Off state` | Auswahlliste `Low`, `High`, `High-Z` |
| `Tuning` | Auswahlliste `Fractional`, `Low noise`, `Integer` |
| `Spread CH1` | Auswahlliste `Off`, `-0.5 %`, `-1.5 %`, `+/-0.5 %` |
| `Corr.` | Wert in ppm, Schritt-Taste, `−`, `+` |
| `Backlight` | `−`, Wert, `+` |
| Info-Karte | Firmware, Build, Display, SI5351 |

- Oben stehen die Einstellungen, die das Signal verändern. Den Frequenzschritt stellen die Schritt-Tasten der Kanalseiten ein (Abschnitt 5).
- Beim Öffnen sind drei Zeilen ganz und die obere Hälfte der vierten zu sehen. Die breite Scrollleiste rechts ist immer sichtbar und zeigt, dass die Liste weitergeht.
- Die Seite rastet an den Zeilen ein.
- Gescrollt wird erst ab etwa 10 px Fingerbewegung; ein kurzer Tipp bleibt ein Tipp. Beginnt das Wischen auf einer Taste, löst die Taste nicht aus, auch nicht die Wiederholung bei `Corr.`.
- Auswahllisten öffnen sich nach oben, wenn unten kein Platz ist.
- Die Scrollposition bleibt erhalten, wenn man über `MAIN` zur Hauptseite und wieder zu CONFIG wechselt (bis zum Ausschalten).

### Speicheranzeige

In der Titelzeile zwischen `CONFIG` und `MAIN` steht der Speicherzustand, in jeder Scrollposition; die Statusleiste der Hauptseite zeigt ihn ebenso:

| Anzeige | Bedeutung |
|---|---|
| `Saving...` | Änderung noch nicht gespeichert (Autosave nach 5 s) |
| `Saved` | Alles gespeichert |
| `Save failed` (rot) | Speichern im Flash ist fehlgeschlagen, zusätzlich erscheint ein Dialog; die Firmware versucht es alle 30 s erneut |
| `Not saved` (rot) | Flash-Speicher (NVS) beim Start nicht verfügbar, Änderungen gehen beim Ausschalten verloren |

### Drive all

Die Drive-Strength gilt global für alle Kanäle (CH1, CH2 und CH3); CH3 hat keine eigene Einstellung.

Auswahl:

```text
2 mA
4 mA
6 mA
8 mA
```

### Off state (Zustand abgeschalteter Ausgänge)

Legt fest, welchen Pegel ein ausgeschalteter Ausgang führt (SI5351-Register 24). Die Einstellung gilt für CLK0, CLK1 und CLK2.

| Option | Ausgang im Zustand OFF |
|---|---|
| `Low` (Standard) | LOW (0 V), wie bis Firmware 1.2.0 |
| `High` | HIGH (Versorgungsspannung der Ausgänge, meist 3,3 V) |
| `High-Z` | hochohmig |

- `High` muss bestätigt werden: Der Dialog `Off state High` erklärt die Gleichspannung am ausgeschalteten Ausgang. `High` (orange) übernimmt die Einstellung, `Cancel` lässt die bisherige stehen. `Low` und `High-Z` wirken ohne Rückfrage.
- Die Änderung wirkt sofort; laufende Ausgänge bleiben unberührt.
- Weicht die Einstellung von `Low` ab, zeigen Hauptseite und Kanalseiten `OFF = High` bzw. `OFF = High-Z` in der Moduszeile (Abschnitt 2).
- Die Firmware schreibt den Zustand beim Verbinden mit dem SI5351, bevor ein Ausgang eingeschaltet werden kann.
- **`High`:** In eine DC-gekoppelte Last (z. B. 50 Ω) fließt ein Dauerstrom, begrenzt durch die Drive-Strength; an einem DC-gekoppelten Mischer liegt eine Gleichspannung an.
- Für DC-gekoppelte Diodenringmischer `Low` oder `High-Z` verwenden.
- Bis zur Hardwareprüfung K2 (`MESSPLAN.md`) bleibt bei `High` und `High-Z` der Ausgangstreiber abgeschalteter Ausgänge versorgt, auch der von CLK2 bei ausgeschaltetem CH3. Bei `Low` wird CLK2 wie bisher ganz abgeschaltet, solange CH3 nicht im IQ-Betrieb läuft.

### Tuning (Teilerverfahren)

Der SI5351 erzeugt jede Frequenz in zwei Stufen: Eine PLL vervielfacht den 25-MHz-Quarz auf 600–900 MHz (VCO), ein Teiler (MultiSynth) teilt das auf die Ausgangsfrequenz herunter. Beide Stufen können ganzzahlig oder fraktional (mit Bruchteil) arbeiten. Fraktionale Teiler erlauben feine Schritte, erzeugen aber Nebenlinien nahe am Träger. `Tuning` legt fest, welche Stufe fraktional arbeitet; die Einstellung gilt für alle Kanäle.

| Option | PLL | Teiler (MultiSynth) | Eigenschaften |
|---|---|---|---|
| `Fractional` (Standard) | fest 900 bzw. 887,5 MHz | fraktional | feinste Schritte, Abstimmen ohne Aussetzer, Abweichung ≤ 0,5 Hz; Nebenlinien durch den fraktionalen Teiler möglich |
| `Low noise` | fraktional | gerade, ganzzahlig | weniger Nebenlinien; kurzer Aussetzer, wenn der Teiler wechselt; Abweichung bis 1,5 Hz |
| `Integer` | ganzzahlig | gerade, ganzzahlig | geringste Nebenlinien; Frequenz auf einem Raster, Abweichung bis in den kHz-Bereich |

Ein Wechsel des Modus stellt alle Ausgänge neu ein; laufende Ausgänge setzen dabei einmal kurz aus.

**Fractional:** Verhalten wie Firmware 1.2.0. Die PLL bleibt fest, nur der Teiler ändert sich. Trifft 900 MHz eine Frequenz nicht genau genug, wechselt die PLL auf 887,5 MHz; nur dann setzt der Ausgang kurz aus.

**Low noise:** Der Teiler ist eine gerade ganze Zahl, die PLL übernimmt Feinabstimmung und Frequenzkorrektur (Verfahren der VFOs von QRP Labs und Etherkit). Beim Abstimmen bleibt der Teiler, solange die PLL im Bereich 600–900 MHz bleibt; dann ändert sich nur die PLL, ohne Aussetzer. Erst wenn der Teiler wechseln muss, setzt der Ausgang kurz aus. Beispiel: Teiler 90 deckt 6,67–10 MHz ab.

**Integer:** PLL und Teiler sind ganzzahlig. Die Frequenz kann deshalb nur Rasterwerte annehmen:

```text
Ausgang = Quarz × a / Teiler    (a = 24 … 36, Teiler gerade)
```

Die Firmware wählt den Rasterpunkt, der der Sollfrequenz am nächsten liegt. Beispiel: Soll 7,1 MHz ergibt 25 MHz × 25 / 88 = 7,102 273 MHz.

- Die Frequenzkorrektur kann im Raster nicht ausgeglichen werden; sie verschiebt nur die angezeigte tatsächliche Frequenz.
- `+`/`−` ändern die Sollfrequenz. Die tatsächliche Frequenz ändert sich erst, wenn ein anderer Rasterpunkt näher liegt; die Zeile `Out` zeigt das.
- Die Hauptseite zeigt die Sollfrequenz mit vorangestelltem `~`, wenn die Abweichung 1 Hz oder mehr beträgt.
- Spread Spectrum ist gesperrt.

Der Modus ist für Ausbildungsversuche zum Vergleich ganzzahliger und fraktionaler Teiler gedacht, nicht für genaue Frequenzen.

### Spread CH1 (Spread Spectrum)

Spread Spectrum (SSC) moduliert die Frequenz von PLL A mit etwa 31,5 kHz und verteilt die Leistung des Trägers auf ein Frequenzband. Der SI5351 kann das nur auf PLL A. Deshalb wirkt es auf CH1 und auf CH3, wenn CH1 sein Master ist; CH2 (PLL B) bleibt unberührt.

| Option | Wirkung |
|---|---|
| `Off` | aus (Standard) |
| `-0.5 %` | Down-Spread: Frequenz schwankt zwischen der Sollfrequenz und 0,5 % darunter |
| `-1.5 %` | Down-Spread: zwischen der Sollfrequenz und 1,5 % darunter |
| `+/-0.5 %` | Center-Spread: ±0,5 % um die Sollfrequenz |

Beispiel: CH1 = 10 MHz mit `-0.5 %` belegt das Band 9,95–10,00 MHz; die Kanalseite zeigt die mittlere Frequenz `Out 9.975 000 MHz mean, SSC -0.5 %`.

- Beim Umschalten startet PLL A neu: CH1 setzt kurz aus. Läuft CH3 im IQ-Betrieb mit Master CH1, wird die Phasenlage danach neu ausgerichtet.
- Im Modus `Integer` ist die Option gesperrt und zeigt ausgegraut `Off (Integer)`. Ein Wechsel auf `Integer` schaltet Spread Spectrum aus; nach dem Zurückwechseln muss es neu gewählt werden.
- Auch mit Spread muss der SI5351-Oszillator (VCO) im Bereich 600–900 MHz bleiben. Mit Master CH1 wird der IQ-Bereich von CH3 deshalb etwas schmaler: `-0.5 %` ab 4,749 MHz, `-1.5 %` ab 4,797 MHz, `+/-0.5 %` von 4,749 bis 99,502 MHz. Außerhalb wartet CH3 (`WAIT`) wie unter 4,730 MHz. Die CH3-Seite zeigt den gültigen Bereich an.
- Für Mischer- und Empfängerversuche mit sauberem Träger `Off` lassen.

### Frequenzkorrektur (`Corr.`)

Die Korrektur gleicht die Toleranz des SI5351-Quarzes aus. Sie wird in **ppm** angezeigt, z. B. `+9.70 ppm`.

| Element | Funktion |
|---|---|
| Schritt-Taste (`0.01` / `0.1` / `1`) | Schrittweite in ppm, jedes Antippen wechselt zur nächsten |
| `−` / `+` | Korrektur um die Schrittweite ändern, gedrückt halten wiederholt |

Bereich: ±50 ppm. Ein positiver Wert bedeutet: Der Ausgang war ohne Korrektur zu hoch.

Die Ausgänge werden nach jeder Änderung neu konfiguriert; der SI5351 wird dabei nicht neu initialisiert. Im Modus `Integer` kann die Korrektur nicht ausgeglichen werden, sie verschiebt nur die angezeigte tatsächliche Frequenz (siehe `Tuning`).

**Kalibrieren:**

1. CH1 auf 10 MHz stellen und einschalten (`Tuning` = `Fractional` oder `Low noise`).
2. Mit Frequenzzähler messen.
3. Abweichung in Hz durch 10 teilen = Korrektur in ppm.
4. Diesen Wert zur aktuellen Korrektur addieren.

Beispiel:

```text
Gemessen: 10 000 097 Hz  ->  +97 Hz bei 10 MHz  ->  +9.70 ppm einstellen
```

### Backlight

`−` und `+` ändern die Display-Helligkeit in 10-%-Schritten:

```text
10 % bis 100 %
```

Der aktuelle Wert steht zwischen den Tasten. Gedrückt halten wiederholt nicht. Das Minimum von 10 % verhindert, dass das Display versehentlich ganz dunkel und damit unbedienbar wird. Gespeicherte Werte älterer Firmware werden auf die nächste 10-%-Stufe gerundet; Werte unter 5 % (z. B. 0 % aus Firmware 1.0.0) werden auf 80 % gesetzt.

### Info-Karte

Am Ende der Seite zeigt eine Karte Angaben für die Fehlersuche ohne seriellen Monitor, zum Beispiel:

```text
Firmware 1.3.1 | dev 641ebe6+
Build 2026-10-28 16:42
Display ST7789 (detected)
SI5351 SDA 22 SCL 27
```

| Zeile | Inhalt |
|---|---|
| `Firmware` | Version, Branch und Commit, aus dem die Firmware gebaut wurde. `+` hinter dem Commit: mit nicht eingecheckten Änderungen gebaut. Ohne git steht `unknown` |
| `Build` | Datum und Uhrzeit des Builds |
| `Display` | Display-Controller (`ILI9341` oder `ST7789`) und Herkunft: `detected` (erkannt), `stored` (gespeicherte Wahl einer früheren Boot-Geste), `boot gesture` (bei diesem Start per Boot-Geste gewählt), `default` (nichts erkannt, ILI9341 angenommen), `build flag` (beim Bauen festgelegt) |
| `SI5351` | I2C-Pins der Verbindung; Zusatz ` (swapped)` bei vertauschten Adern, ` (clone?)` bei ungewöhnlichem Register 183 (z. B. Nachbau). Ohne Verbindung: `SI5351 not found` |

---

## 9. Speicherung der Einstellungen

Änderungen werden automatisch gespeichert.

Ablauf:

1. Eine Einstellung ändern.
2. 5 Sekunden warten (auf CONFIG wechselt die Anzeige in der Titelzeile von `Saving...` auf `Saved`).
3. Die Firmware speichert automatisch in NVS.
4. Im Serial Monitor erscheint:

```text
[SETTINGS] Saved to NVS
```

Gespeichert werden:

- CH1 Frequenz
- CH2 Frequenz
- CH3 Master
- CH3 Phase
- Backlight
- Frequenzschritt
- Frequenzkorrektur
- globale Drive-Strength
- `Tuning`, `Spread CH1` und `Off state`

Fehlen die drei neuen Optionen im Speicher (Einstellungen aus Firmware 1.2.0), gelten die Standardwerte `Fractional`, `Off` und `Low`, also das Verhalten von 1.2.0. Ungültige gespeicherte Werte werden beim Start durch den Standardwert ersetzt.

Die Wahl des Display-Controllers per Boot-Geste wird sofort gespeichert (Abschnitt 13).

Touch-Kalibrierung wird nicht gespeichert. Es werden immer Default-Werte verwendet.

Der ON/OFF-Zustand der Ausgänge wird nicht gespeichert; nach dem Einschalten sind alle Ausgänge aus. Ein- und Ausschalten löst deshalb kein Speichern aus, ebenso wenig eine Einstellung, die schon den gewählten Wert hat. Das schont den Flash-Speicher.

---

## 10. Beispielanwendungen

### Beispiel A: Einfacher 10-MHz-Ausgang auf CH1

1. Hauptseite öffnen.
2. `CH1` drücken.
3. `NUMPAD` drücken.
4. `10` eingeben.
5. `MHz` drücken.
6. CH1 auf `ON` stellen.

Ausgang:

```text
CLK0 = 10 MHz Rechtecksignal
```

---

### Beispiel B: Zwei unabhängige Frequenzen

Ziel:

```text
CH1 = 10 MHz
CH2 = 14 MHz
```

Vorgehen:

1. CH1 öffnen.
2. 10 MHz eingeben.
3. CH1 einschalten.
4. Zur Hauptseite zurück.
5. CH2 öffnen.
6. 14 MHz eingeben.
7. CH2 einschalten.

Ausgänge:

```text
CLK0 = 10 MHz
CLK1 = 14 MHz
```

---

### Beispiel C: IQ-Ausgang 10 MHz mit 90°

Ziel:

```text
CH1 = 10 MHz
CH3 = 10 MHz mit 90° Phasenverschiebung
```

Vorgehen:

1. CH1 auf 10 MHz stellen.
2. CH1 einschalten.
3. CH3 öffnen.
4. Master `CH1` antippen.
5. Phase `90` antippen.
6. CH3 einschalten.

Ausgänge:

```text
CLK0 = 10 MHz Referenz
CLK2 = 10 MHz, 90° verschoben
```

---

### Beispiel D: Ausgangsleistung ändern

Ziel:

```text
Alle Kanäle mit 8 mA Drive-Strength
```

Vorgehen:

1. CONFIG öffnen.
2. Bei `Drive all` den Wert `8 mA` auswählen.
3. Warten, bis in der Titelzeile `Saved` angezeigt wird.

Diese Einstellung gilt danach für alle Kanäle.

---

### Beispiel E: Fraktionale und ganzzahlige Teiler vergleichen

1. CH1 auf 7,1 MHz stellen und einschalten, Spektrumanalysator an CLK0.
2. In CONFIG `Tuning` nacheinander auf `Fractional`, `Low noise` und `Integer` stellen.
3. Nebenlinien vergleichen. Die Kanalseite zeigt jeweils die tatsächliche Frequenz, bei `Integer` `Out 7.102 273 MHz (+2272.7 Hz)`.

---

## 11. Fehler und Hinweise

### Fehlermeldungen

Fehler werden als Dialog angezeigt. `OK` (große hellgraue Taste unten) schließt den Dialog. Es ist immer höchstens ein Dialog offen; weitere Meldungen erscheinen nacheinander, gleiche Meldungen nur einmal. Verbindungsmeldungen schließen sich nach dem Wiederverbinden selbst, Meldungen zu Ausgangsfehlern bleiben offen, bis sie mit `OK` geschlossen werden.

### SI5351 nicht gefunden

Wird der SI5351 beim Start nicht gefunden, erscheint **eine** Meldung. Danach zeigen die Kanäle auf der Hauptseite `! NO HW`, die Kanalseiten `SI5351 not found`. Einstellungen lassen sich trotzdem ändern und werden gespeichert.

Die Firmware prüft alle 2 Sekunden, ob der SI5351 antwortet. Wird er nachträglich angeschlossen oder ist ein Wackelkontakt behoben, verbindet sie automatisch neu, stellt die eingestellten Frequenzen und ON/OFF-Zustände wieder her und schließt die Verbindungsmeldung. Geht die Verbindung im Betrieb verloren (oder verliert der SI5351 kurz die Versorgung), erscheint die Meldung `Connection lost` und die Kanäle zeigen `! NO HW`, bis die Verbindung wieder steht. Die Firmware versucht sofort nach dem Erkennen neu zu verbinden; nach einem kurzen Versorgungsausfall laufen die Ausgänge daher spätestens nach etwa 2 s wieder.

SDA und SCL dürfen an CN1 beliebig angeschlossen sein (GPIO22/GPIO27). Die Firmware probiert beide Zuordnungen, auch nach dem Umstecken im Betrieb. Die Info-Karte auf CONFIG und der Serial Monitor zeigen die gefundene Zuordnung, bei vertauschten Adern mit dem Zusatz `(swapped)`.

Weitere Meldungen beim Verbinden:

| Meldung | Bedeutung |
|---|---|
| `SI5351 not found ...` | Kein Gerät auf Adresse 0x60: Kabel, 3,3 V und GND prüfen. Die Suche läuft weiter. |
| `I2C line stays low ...` | Eine I2C-Leitung liegt dauerhaft auf GND: Kurzschluss, fehlende 3,3 V am Modul oder defektes Modul. Die Suche läuft weiter. |
| `The device at I2C address 0x60 is not an SI5351 ...` | Auf 0x60 antwortet ein anderes Gerät. Es wird nicht programmiert (höchstens ein Testbyte in Register 3), bis es abgezogen wird. Ein SI5351, der beim Anstecken kurz falsche Werte liefert, wird automatisch erneut geprüft. |

Hinweis: Bleibt ein SI5351-Modul beim Abziehen des Kabels versorgt, laufen seine Ausgänge weiter. Beim Wiederverbinden schaltet die Firmware sie für einige Millisekunden ab und stellt sie dann wieder her.

### Ausgangsfehler

Schlägt das Einstellen eines Ausgangs fehl, obwohl der SI5351 antwortet, erscheint ein Dialog. Der Kanal zeigt `! ERR` (Hauptseite, Titelzeile), die Kanalseite `Output error, retrying` bzw. die CH3-Seite `IQ setup failed, retrying`. Der Dialog nennt den Kanal und was weiterläuft. Der Schalter behält die gewünschte Einstellung; die Firmware versucht es alle 2 s erneut, ohne weiteren Dialog. Bei einem IQ-Fehler läuft der Master weiter.

| Meldung | Bedeutung |
|---|---|
| `CH1: frequency setup failed. Retrying every 2 s.` | CH1 (bzw. CH2) ließ sich nicht einstellen |
| `CH3: IQ setup failed, CLK2 stays off. CH1 keeps running. Retrying every 2 s.` | Das IQ-Paar ließ sich nicht einstellen; CLK2 bleibt aus, der Master (hier CH1) läuft weiter |

### Einstellungen werden nicht gespeichert

Ist der Flash-Speicher (NVS) beim Start nicht verfügbar, erscheint ein Dialog und CONFIG zeigt `Not saved`. Schlägt ein Speichervorgang fehl, erscheint ein Dialog und CONFIG zeigt `Save failed`. Die Änderung bleibt vorgemerkt: Die Firmware versucht alle 30 s erneut zu speichern, ohne weiteren Dialog, und zeigt `Save failed`, bis es gelingt. Eine neue Änderung wird wie gewohnt nach 5 s gespeichert.

### Neustart nach internem Fehler

Geht der Speicher der Bedienoberfläche aus oder hängt die Firmware länger als 5 s, startet das Gerät selbst neu, statt mit laufenden Ausgängen einzufrieren. Danach sind wie nach jedem Einschalten alle Ausgänge aus; die gespeicherten Einstellungen bleiben erhalten. Bis die Firmware den SI5351 neu initialisiert, laufen seine Ausgänge kurz weiter. Im Serial Monitor steht der Grund (Abschnitt 12).

### Kein Ausgangssignal

Prüfen:

1. Zeigen Hauptseite bzw. Titelzeile `ON` (nicht nur der Schalter)?
2. Richtiger Ausgang gemessen?

```text
CH1 = CLK0
CH2 = CLK1
CH3 = CLK2
```

3. SI5351 wird per I2C gefunden? Die Info-Karte auf CONFIG zeigt die Pins (`SI5351 SDA 22 SCL 27`) oder `SI5351 not found`. Im Serial Monitor sollte stehen:

```text
[APP] SI5351 at 0x60, SDA=22 SCL=27
```

4. Gemeinsame Masse vorhanden?
5. SI5351 mit 3,3 V versorgt?
6. OE-Pin des SI5351-Moduls, falls vorhanden, auf aktivem Pegel?

Viele SI5351-Module haben OE active-low:

```text
OE = GND -> Ausgänge aktiv
OE = HIGH -> Ausgänge deaktiviert
```

### CH3 zeigt WAIT bzw. „Waiting“

Mögliche Ursachen:

```text
Master ist ausgeschaltet.
Masterfrequenz liegt unter 4,730 MHz.
Mit Spread Spectrum auf Master CH1: Masterfrequenz außerhalb des schmaleren IQ-Bereichs
(-0.5 %: ab 4,749 MHz, -1.5 %: ab 4,797 MHz, +/-0.5 %: 4,749 bis 99,502 MHz).
```

Lösung:

- Master einschalten
- Masterfrequenz auf mindestens 4,730 MHz stellen (mit Spread auf den Bereich, den die CH3-Seite anzeigt)
- oder `Spread CH1` auf `Off` stellen

CH3 startet dann automatisch; der Schalter muss nicht erneut betätigt werden.

### Grenzen

- Touch-Kalibrierung ist fest eingestellt und lässt sich nicht ändern.
- Die angezeigte tatsächliche Frequenz setzt voraus, dass die Frequenzkorrektur stimmt.
- IQ-Betrieb erst ab 4,730 MHz Masterfrequenz.

---

## 12. Serielle Diagnose

Serial Monitor:

```bash
pio device monitor -b 115200
```

Wichtige Meldungen:

| Meldung | Bedeutung |
|---|---|
| `[BOOT] CYD HF Generator 1.3.1 (dev 641ebe6+, 2026-10-28 16:42)` | Start: Version, Branch, Commit und Build-Zeit wie auf der Info-Karte |
| `[I2C] Line held low; releasing bus` | Eine I2C-Leitung lag beim Start auf LOW und wurde freigegeben |
| `[SETTINGS] Loaded from NVS` | Einstellungen geladen |
| `[SI5351] Init OK; all outputs off` | SI5351 initialisiert, alle Ausgänge aus |
| `[APP] SI5351 at 0x60, SDA=22 SCL=27` | Verbunden; bei vertauschten Adern `SDA=27 SCL=22 (swapped)` |
| `[APP] SI5351 not found` | Kein SI5351 gefunden, die Suche läuft weiter |
| `[TOUCH] XPT2046 init OK on VSPI` | Touch bereit |
| `[DISPLAY] ST7789 (detected); ...` | Display-Controller, Herkunft und Messwerte der Erkennung |
| `[BOOT] Setup complete` | Start abgeschlossen |
| `[SETTINGS] Saved to NVS` | Autosave |
| `[APP] Restoring outputs` | Nach dem Wiederverbinden werden die Ausgänge wiederhergestellt |
| `[APP] Output error cleared` | Ein Ausgangsfehler (`! ERR`) ist behoben |

Fehlermeldungen beginnen mit `[ERROR]`, z. B.:

| Meldung | Bedeutung |
|---|---|
| `[ERROR][SI5351] Device lost or reset` | Überwachung: SI5351 antwortet nicht oder hat seine Einstellung verloren |
| `[ERROR][SI5351] Unexpected register 183 (clone?); continuing` | Ungewöhnliches Register 183, z. B. Nachbau; das Modul wird trotzdem verwendet |
| `[ERROR][APP] I2C fails at 400 kHz; staying at 100 kHz` | Bus läuft nur mit 100 kHz (z. B. schwache Pull-ups) |
| `[ERROR][SETTINGS] Saving to NVS failed` | Speichern fehlgeschlagen |
| `[ERROR][FATAL] LVGL assert; restarting` | Speicher der Bedienoberfläche erschöpft o. Ä.; das Gerät startet neu |
| `task_wdt: Task watchdog got triggered … loopTask` | Firmware hing 5 s; das Gerät startet neu |

Dialogtexte erscheinen zusätzlich im Log, z. B. `[ERROR][SI5351] CH1: frequency setup failed. Retrying every 2 s.`.

Frequenzänderungen und ausführliche SI5351-Meldungen (Ausgangsmaske, Phasenregister, I2C-Scan usw.) erscheinen nur, wenn die Firmware mit `LOG_DEBUG 1` (in `include/config.h`) gebaut wurde. Im Normalbetrieb sind sie abgeschaltet, weil serielle Ausgaben die Bedienung verlangsamen.

---

## 13. Aktuelle Hardware-Pinbelegung

Der SI5351 wird an CN1 angeschlossen. SDA und SCL dürfen vertauscht sein; die Firmware erkennt die Zuordnung:

```text
SDA / SCL = GPIO22 / GPIO27 (beliebig; zuerst probiert: SDA = GPIO22)
SI5351 Adresse = 0x60
```

Display und Touch verwenden separate SPI-Busse. Die vollständige Pinbelegung steht in [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md), Abschnitt 2.

### Board-Varianten

Unter der Bezeichnung ESP32-2432S028R werden zwei Boards verkauft, die sich nur im Display-Controller unterscheiden. Dieselbe Firmware läuft auf beiden; sie erkennt den Controller beim Einschalten:

| Board | Erkennungsmerkmal | Display-Controller |
|---|---|---|
| Original-CYD | nur Micro-USB-Buchse | ILI9341 |
| CYD2USB | zusätzliche USB-C-Buchse | ST7789 |

Die Info-Karte auf CONFIG und der Serial Monitor zeigen das Ergebnis, zum Beispiel `Display ST7789 (detected)` bzw. `[DISPLAY] ST7789 (detected); ...`. Pinbelegung, Touch und Bedienung sind bei beiden Varianten gleich.

**Display zeigt nur Rauschen (Boot-Geste):** Display berühren, Gerät einschalten und gedrückt halten, bis die Beleuchtung zweimal blinkt, dann loslassen. Die Firmware verwendet dann den anderen Controller und merkt sich die Wahl. Eine zweite Boot-Geste schaltet zurück auf die automatische Erkennung. Die Geste wirkt nur, wenn die Berührung schon beim Einschalten besteht und 1,5 s gehalten wird. Eine Berührung beim Einschalten wird nicht als Tipp in die Bedienung übernommen.

---

## 14. Sicherheitshinweis

Die SI5351-Ausgänge liefern digitale Rechtecksignale. Vor Anschluss an empfindliche HF-Schaltungen sollte geprüft werden:

- Signalpegel
- Lastimpedanz
- eventuelle Dämpfung
- galvanische Verbindung
- Masseführung
- Zustand abgeschalteter Ausgänge (`Off state`): Bei `High` liegt an einem ausgeschalteten Ausgang Gleichspannung an
