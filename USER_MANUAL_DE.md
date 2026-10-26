# Benutzeranleitung – CYD HF Generator

Firmware-Version: 1.1.0

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

---

## 2. Grundbedienung

Nach dem Einschalten erscheint die Hauptseite.

Die Hauptseite zeigt für jeden Kanal eine Karte mit:

- Kanalname und Status
- Frequenz
- eigenem ON/OFF-Schalter
- Zusatzinfo (CH3: Master, Phase und ggf. Grund für `WAIT`)

Bedienung auf der Hauptseite:

- **Karte antippen** öffnet die Seite des Kanals.
- **Schalter auf der Karte** schaltet den Ausgang direkt ein oder aus, ohne die Seite zu wechseln.
- Die Buttons unten führen zu CH1, CH2, CH3 und CONFIG.

Statusanzeige unter dem Kanalnamen:

| Status | Bedeutung |
|---|---|
| `ON` (grün) | Ausgang aktiv |
| `OFF` (grau) | Ausgang aus |
| `WAIT` (orange) | CH3 ist eingeschaltet, kann aber gerade nicht laufen (Master aus oder Masterfrequenz unter 4,730 MHz) |
| `NO HW` (rot) | SI5351 wurde beim Start nicht gefunden |

Die Navigationsbuttons sind farbig:

| Button | Farbe |
|---|---|
| CH1 | Rot |
| CH2 | Grün |
| CH3 | Blau |
| CONFIG | Gelb |

Auf allen Unterseiten befinden sich in der Titelzeile der ON/OFF-Schalter (auf Kanalseiten) mit Zustandsanzeige `ON`/`OFF` und rechts der Button `MAIN` zur Rückkehr auf die Hauptseite.

Die RGB-LED zeigt auf den Kanal-Seiten ebenfalls die Kanal-Farbe:

| Seite | RGB-LED |
|---|---|
| CH1 | Rot |
| CH2 | Grün |
| CH3 | Blau |

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

Angezeigt und gespeichert wird die **eingestellte Sollfrequenz** mit 1 Hz Auflösung. Der SI5351 kann nicht jede Frequenz exakt erzeugen; die berechnete Abweichung steht in der Infozeile der Kanalseite (siehe Abschnitt 5).

---

## 4. CH1 und CH2 verwenden

CH1 und CH2 sind frei einstellbare Frequenzausgänge.

### CH1 oder CH2 einschalten

1. Auf der Hauptseite die Karte oder den Button `CH1` bzw. `CH2` antippen.
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
4. Der Ausgang wird deaktiviert.

---

## 5. Frequenz mit + und - ändern

Die Kanalseite von CH1 und CH2 enthält:

- die Frequenzkarte mit Infozeile
- eine Reihe Schritt-Tasten: `1`, `10`, `100`, `1k`, `10k`, `100k`, `1M` (in Hz)
- große Tasten `-` (links) und `+` (rechts)
- die Taste `NUMPAD`

Die Tasten `-` und `+` ändern die Frequenz um den aktuell gewählten Schritt. Der gewählte Schritt ist farbig hervorgehoben und wird direkt auf der Kanalseite (oder in CONFIG) gewählt.

**Gedrückt halten:** Nach ca. 0,4 s wiederholt sich der Schritt automatisch etwa alle 0,1 s, bis die Taste losgelassen wird.

Die Infozeile zeigt:

```text
Step 1 kHz  |  Deviation +0.3 Hz
```

- `Step` = aktueller Schritt
- `Deviation` = berechnete Abweichung der vom SI5351 erzeugten Frequenz von der Sollfrequenz (Rundung der PLL-/Teilerwerte, unter der Annahme, dass die Frequenzkorrektur stimmt)

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
- drücken
Neue Frequenz: 7.090 000 MHz
```

Die Frequenz bleibt immer im Bereich 1 MHz bis 100 MHz.

---

## 6. Frequenz über NUMPAD eingeben

Auf CH1 oder CH2:

1. `NUMPAD` drücken.
2. Zahlen eingeben. Die aktuelle Frequenz steht grau als Platzhalter im Eingabefeld.
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

Maximal 9 Zeichen sind erlaubt.

### Ungültige Eingabe

Liegt der Wert außerhalb von 1–100 MHz oder ist die Eingabe leer bzw. ungültig, wird sie **nicht übernommen**. Das Eingabefeld wird geleert, der Rahmen rot und der Hinweis `1 - 100 MHz` erscheint. Mit der nächsten Taste kann neu eingegeben werden.

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

Die gewählte Taste ist farbig hervorgehoben. Die Frequenz von CH3 ist immer identisch mit der Frequenz des Masters.

### Statuszeile

Unter der Frequenz zeigt die CH3-Seite den aktuellen Zustand:

| Anzeige | Bedeutung |
|---|---|
| `Off  \|  follows CHx` | CH3 ausgeschaltet |
| `IQ active, follows CHx` | IQ-Ausgang läuft |
| `Waiting: master CHx is off` | CH3 eingeschaltet, Master aus |
| `Waiting: CHx below 4.730 MHz` | CH3 eingeschaltet, Masterfrequenz zu niedrig |
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

Auf der CONFIG-Seite können globale Einstellungen geändert werden.

### Backlight

Mit dem Slider wird die Display-Helligkeit eingestellt:

```text
5 % bis 100 %
```

Der aktuelle Wert steht rechts neben dem Slider. Das Minimum von 5 % verhindert, dass das Display versehentlich ganz dunkel und damit unbedienbar wird. Ein gespeicherter Wert unter 5 % (z. B. aus Firmware 1.0.0) wird beim Start auf 80 % zurückgesetzt.

### Step (Frequenzschritt)

Der Schritt bestimmt, wie stark `+` und `-` die Frequenz ändern. Es sind dieselben Stufen wie auf den Kanalseiten:

```text
1 Hz, 10 Hz, 100 Hz, 1 kHz, 10 kHz, 100 kHz, 1 MHz
```

| Button | Funktion |
|---|---|
| x10 | Nächstgrößere Stufe |
| /10 | Nächstkleinere Stufe |

Beispiel:

```text
Step: 1 kHz
x10 drücken
Step: 10 kHz
```

### Frequenzkorrektur (Corr.)

Die Korrektur gleicht die Toleranz des SI5351-Quarzes aus. Sie wird in **ppm** angezeigt, z. B. `+9.70 ppm`.

| Element | Funktion |
|---|---|
| Schritt-Taste (`0.01` / `0.1` / `1`) | Schrittweite in ppm, jedes Antippen wechselt zur nächsten |
| `-` / `+` | Korrektur um die Schrittweite ändern, gedrückt halten wiederholt |

Bereich: ±50 ppm. Ein positiver Wert bedeutet: Der Ausgang war ohne Korrektur zu hoch.

Die Ausgänge werden nach jeder Änderung neu konfiguriert; der SI5351 wird dabei nicht neu initialisiert.

**Kalibrieren:**

1. CH1 auf 10 MHz stellen und einschalten.
2. Mit Frequenzzähler messen.
3. Abweichung in Hz durch 10 teilen = Korrektur in ppm.
4. Diesen Wert zur aktuellen Korrektur addieren.

Beispiel:

```text
Gemessen: 10 000 097 Hz  ->  +97 Hz bei 10 MHz  ->  +9.70 ppm einstellen
```

### Drive all

Die Drive-Strength gilt global für alle Kanäle.

Auswahl:

```text
2 mA
4 mA
6 mA
8 mA
```

Diese Einstellung gilt für:

```text
CH1
CH2
CH3
```

CH3 hat keine eigene Drive-Strength-Einstellung.

### Speicheranzeige

Rechts in der Zeile `Drive all` steht der Speicherzustand:

| Anzeige | Bedeutung |
|---|---|
| `Saving...` | Änderung noch nicht gespeichert (Autosave nach 5 s) |
| `Saved` | Alles gespeichert |

---

## 9. Speicherung der Einstellungen

Änderungen werden automatisch gespeichert.

Ablauf:

1. Eine Einstellung ändern.
2. 5 Sekunden warten (auf CONFIG wechselt die Anzeige von `Saving...` auf `Saved`).
3. Die Firmware speichert automatisch in NVS.
4. Im Serial Monitor erscheint:

```text
[SETTINGS] Saved to NVS
```

Gespeichert werden unter anderem:

- CH1 Frequenz
- CH2 Frequenz
- CH1 ON/OFF
- CH2 ON/OFF
- CH3 ON/OFF
- CH3 Master
- CH3 Phase
- Backlight
- Frequenzschritt
- Frequenzkorrektur
- globale Drive-Strength

Touch-Kalibrierung wird nicht gespeichert. Es werden immer Default-Werte verwendet.

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
3. Warten, bis rechts `Saved` angezeigt wird.

Diese Einstellung gilt danach für alle Kanäle.

---

## 11. Fehler und Hinweise

### Fehlermeldungen

Fehler werden als Dialog angezeigt. `OK` schließt den Dialog. Es ist immer höchstens ein Dialog offen; gleiche Meldungen werden nicht mehrfach gestapelt.

### SI5351 nicht gefunden

Wird der SI5351 beim Start nicht gefunden, erscheint **eine** Meldung. Danach zeigen die Kanäle auf der Hauptseite `NO HW`, die Kanalseiten `SI5351 not found`. Einstellungen lassen sich trotzdem ändern und werden gespeichert.

### Kein Ausgangssignal

Prüfen:

1. Kanal steht auf `ON`?
2. Richtiger Ausgang gemessen?

```text
CH1 = CLK0
CH2 = CLK1
CH3 = CLK2
```

3. SI5351 wird per I2C gefunden?

Im Serial Monitor sollte stehen:

```text
[I2C] Found device at 0x60
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
```

Lösung:

- Master einschalten
- Masterfrequenz auf mindestens 4,730 MHz stellen

CH3 startet dann automatisch; der Schalter muss nicht erneut betätigt werden.

---

## 12. Serielle Diagnose

Serial Monitor:

```bash
pio device monitor -b 115200
```

Wichtige Meldungen:

```text
[I2C] Found device at 0x60
[SI5351] Init OK; all outputs High-Z
[APP] CH1 frequency: ...
[SI5351] Correction (0.01 ppm): ...
[SETTINGS] Saved to NVS
```

Ausführliche SI5351-Meldungen (Ausgangsmaske, Phasenregister usw.) erscheinen nur, wenn die Firmware mit `LOG_DEBUG 1` (in `include/config.h`) gebaut wurde. Im Normalbetrieb sind sie abgeschaltet, weil serielle Ausgaben die Bedienung verlangsamen.

---

## 13. Aktuelle Hardware-Pinbelegung

Für dieses Projekt wurde die SI5351-I2C-Belegung per I2C-Scan bestätigt:

```text
SDA = GPIO22
SCL = GPIO27
SI5351 Adresse = 0x60
```

Display und Touch verwenden separate SPI-Busse.

---

## 14. Sicherheitshinweis

Die SI5351-Ausgänge liefern digitale Rechtecksignale. Vor Anschluss an empfindliche HF-Schaltungen sollte geprüft werden:

- Signalpegel
- Lastimpedanz
- eventuelle Dämpfung
- galvanische Verbindung
- Masseführung
