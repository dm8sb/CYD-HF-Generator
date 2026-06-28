# Benutzeranleitung – CYD HF Generator

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

CH3 folgt immer der Frequenz seines Masters und kann mit einer Phasenlage von 0°, 90°, 180° oder 270° betrieben werden, sofern diese Kombination mit dem SI5351 gültig ist.

---

## 2. Grundbedienung

Nach dem Einschalten erscheint die Hauptseite.

Die Hauptseite zeigt:

- CH1 Frequenz und ON/OFF-Zustand
- CH2 Frequenz und ON/OFF-Zustand
- CH3 Frequenz, Master, Phase und ON/OFF-Zustand
- Navigation zu CH1, CH2, CH3 und CONFIG

Die Navigationsbuttons sind farbig:

| Button | Farbe |
|---|---|
| CH1 | Rot |
| CH2 | Grün |
| CH3 | Blau |
| CONFIG | Gelb |

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

---

## 4. CH1 und CH2 verwenden

CH1 und CH2 sind frei einstellbare Frequenzausgänge.

### CH1 oder CH2 einschalten

1. Auf der Hauptseite `CH1` oder `CH2` drücken.
2. Den ON/OFF-Schalter in der Titelzeile einschalten.
3. Die Anzeige wechselt auf `ON`.
4. Am entsprechenden SI5351-Ausgang liegt ein Signal an:

| Kanal | SI5351-Ausgang |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |

### CH1 oder CH2 ausschalten

1. Kanal-Seite öffnen.
2. ON/OFF-Schalter ausschalten.
3. Die Anzeige wechselt auf `OFF`.
4. Der Ausgang wird deaktiviert.

---

## 5. Frequenz mit + und - ändern

Auf CH1 und CH2 gibt es:

```text
+
-
NUMPAD
```

Die Tasten `+` und `-` ändern die Frequenz um den aktuell eingestellten Delta-Schritt.

Der Delta-Schritt wird in CONFIG eingestellt.

Beispiel:

```text
Aktuelle Frequenz: 10.000 000 MHz
Delta: 1 000 Hz
+ drücken
Neue Frequenz: 10.001 000 MHz
```

Noch ein Beispiel:

```text
Aktuelle Frequenz: 7.100 000 MHz
Delta: 10 000 Hz
- drücken
Neue Frequenz: 7.090 000 MHz
```

---

## 6. Frequenz über NUMPAD eingeben

Auf CH1 oder CH2:

1. `NUMPAD` drücken.
2. Zahlen eingeben.
3. Optional Dezimalpunkt verwenden.
4. Eine Einheit drücken:
   - `Hz`
   - `kHz`
   - `MHz`

Die Einheit bestätigt die Eingabe direkt.

Es gibt keinen separaten OK-Button.

### Numpad-Tasten

| Taste | Funktion |
|---|---|
| 0–9 | Zifferneingabe |
| . | Dezimalpunkt |
| `<--` | Letztes Zeichen löschen |
| Clear | Eingabe löschen |
| Hz | Wert als Hz übernehmen |
| kHz | Wert als kHz übernehmen |
| MHz | Wert als MHz übernehmen |

Maximal 9 Zeichen sind erlaubt.

### Beispiel 1: 10 MHz eingeben

1. CH1 öffnen.
2. `NUMPAD` drücken.
3. Eingeben:

```text
10
```

4. `MHz` drücken.
5. CH1 wird auf ca. 10 MHz gesetzt.

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

- Master: CH1 oder CH2
- Phase: 0°, 90°, 180° oder 270°
- ON/OFF

Die Frequenz von CH3 ist immer identisch mit der Frequenz des Masters.

### Beispiel: CH3 folgt CH1 mit 90° Phase

1. CH1 auf 10 MHz einstellen.
2. CH1 einschalten.
3. Zur CH3-Seite wechseln.
4. Master auf `CH1` stellen.
5. Phase auf `90` stellen.
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
4. Master auf `CH2` stellen.
5. Phase auf `180` stellen.
6. CH3 einschalten.

Ergebnis:

```text
CH2 / CLK1 = 5 MHz
CH3 / CLK2 = 5 MHz, 180° verschoben
```

Hinweis: Nicht jede Phase ist bei jeder Frequenz technisch möglich. Wenn die SI5351-Phasenregistergrenze überschritten wird, zeigt das Gerät eine Fehlermeldung und deaktiviert CH3 sicher.

---

## 8. CONFIG-Seite

Auf der CONFIG-Seite können globale Einstellungen geändert werden.

### Backlight

Mit dem Slider wird die Display-Helligkeit eingestellt:

```text
0 % bis 100 %
```

### Delta-Schritt

Der Delta-Schritt bestimmt, wie stark `+` und `-` die Frequenz ändern.

Buttons:

| Button | Funktion |
|---|---|
| x10 | Delta-Schritt verzehnfachen |
| /10 | Delta-Schritt durch zehn teilen |

Beispiel:

```text
Delta: 1 000 Hz
x10 drücken
Delta: 10 000 Hz
```

### Frequency Correction

Mit `+10` und `-10` kann die SI5351-Frequenzkorrektur verändert werden.

Diese Einstellung dient zur Kalibrierung des SI5351-Quarzes.

Beispiel:

Wenn ein Frequenzzähler bei 10 MHz eine Abweichung zeigt, kann die Korrektur angepasst werden, bis der Messwert möglichst genau stimmt.

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

---

## 9. Speicherung der Einstellungen

Änderungen werden automatisch gespeichert.

Ablauf:

1. Eine Einstellung ändern.
2. 5 Sekunden warten.
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
- Delta-Schritt
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
4. Master = CH1 einstellen.
5. Phase = 90 einstellen.
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
3. 5 Sekunden warten, bis gespeichert wurde.

Diese Einstellung gilt danach für alle Kanäle.

---

## 11. Fehler und Hinweise

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

### CH3 lässt sich nicht mit bestimmter Phase aktivieren

Mögliche Ursache:

```text
Die gewünschte Phase ist bei dieser Frequenz nicht im SI5351-Phasenregister darstellbar.
```

Lösung:

- andere Phase wählen
- andere Frequenz wählen
- CH3 erneut einschalten

---

## 12. Serielle Diagnose

Serial Monitor:

```bash
pio device monitor -b 115200
```

Wichtige Meldungen:

```text
[I2C] Found device at 0x60
[SI5351] Init OK
[SI5351] Set Freq CLK0: ...
[SI5351] Output mask: ...
[SETTINGS] Saved to NVS
```

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

