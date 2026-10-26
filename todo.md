# TODO – Code- & Design-Review CYD HF Generator

Stand: 2026-10-26 · Review-Basis: Commit `6bac503` · Umsetzung: Firmware 1.1.0 (noch nicht committet)

Prioritäten: **P1** = Fehler / sichtbares Problem · **P2** = deutliche Verbesserung · **P3** = Aufräumen / nice-to-have
Status: `[x]` umgesetzt · `[~]` teilweise · `[ ]` offen · `[-]` verworfen

> Die Layout-Befunde wurden aus den Koordinaten im Code und den LVGL-8.3-Theme-Defaults berechnet
> (320 px Breite → `DISP_SMALL` → Card-Padding 13 px, Switch-Default 52×30 px).
> Die Umsetzung ist kompiliert, aber **noch nicht am Gerät getestet**. Den Messplan enthält Abschnitt 8.

Genehmigte Abweichungen von der Spezifikation: Numpad mit Zurück-Taste, Korrektur in ppm,
IQ-Modus ab 4,73 MHz mit 180°/270° per Ausgangsinvertierung.

---

## 1. Layout & Überlappungen

- [x] **P1 Header-Leiste**: Padding 0. Elemente werden per `lv_obj_align` platziert, der Switch hat eine feste Größe von 50×26 px. Der MAIN-Button liegt vollständig im Display.
- [x] **P1 MAIN, CH3-Karte**: Die Karte ist zweizeilig. Der Status (ON/OFF/WAIT/NO HW) steht als eigenes farbiges Label unter dem Kanalnamen.
- [x] **P1 CH3, Frequenzkarte**: Die Karte ist 66 px hoch, beide Zeilen passen hinein.
- [x] **P1 CH3, Fußtext**: Der Text wird umbrochen und bleibt innerhalb der Displaybreite.
- [x] **P1 CONFIG, Karten**: Die Zeilen sind 44 px hoch, die Buttons füllen die 34 px Inhaltshöhe exakt aus.
- [x] **P1 NUMPAD**: 4×4-Raster mit ≈ 44 px Tastenhöhe (siehe 2).
- [x] **P1** `LV_OBJ_FLAG_SCROLLABLE` ist auf Screen, Header und Karten entfernt.
- [x] **P2** Zentrales Layoutraster und Farbschema in [gui_layout.h](src/gui/gui_layout.h).
- [x] **P2** Karten-, Button- und Chip-Stil zentral in `addCard_` / `addButton_` / `addChip_`.

## 2. Bedienung (UX)

- [x] **P1 Helligkeit**: Minimum 5 %. Gespeicherte Werte unter 5 % (aus 1.0.0) werden beim Start auf 80 % gesetzt.
- [x] **P1 „OK“ schließt den Dialog.** Doppeltes Antippen ist abgesichert.
- [x] **P1 Mehrfache Fehlerdialoge**: Es ist immer nur ein Dialog offen, gleiche Texte werden nicht gestapelt. Ohne SI5351 erscheint ein einziger Boot-Hinweis, die Kanäle zeigen „NO HW“ bzw. „SI5351 not found“.
- [x] **P1 Veralteter Zustand**: Nach jeder Aktion aktualisiert `refresh_()` alle Widgets aus dem `AppController`-Zustand.
- [x] **P2 Schrittweite auf der Kanalseite**: Chips 1 · 10 · 100 · 1k · 10k · 100k · 1M. CONFIG x10 / /10 nutzt dieselben Stufen (bisher bis 100 MHz).
- [x] **P2 Gedrückt halten = Auto-Repeat** für `−`/`+` (Frequenz und Korrektur).
- [x] **P2 `−` links, `+` rechts.**
- [x] **P2 MAIN interaktiv**: Ein Tipp auf die Karte öffnet den Kanal, jede Karte hat einen eigenen ON/OFF-Schalter.
- [x] **P2 NUMPAD neu**: 4×4-Raster, ◀ zurück zur Kanalseite, aktuelle Frequenz als Platzhalter, nur ein Dezimalpunkt.
- [x] **P2 Eingabeprüfung**: Werte außerhalb 1–100 MHz werden mit rotem Rahmen und Hinweis abgelehnt, nicht mehr stillschweigend geklemmt.
- [x] **P2 Phasenauswahl**: Hat sich erledigt, denn alle vier Phasen sind im ganzen IQ-Bereich machbar (siehe 5).
- [x] **P2 CH3-Wunschzustand**: CH3 bleibt angefordert und startet automatisch, sobald der Master an ist und ≥ 4,73 MHz läuft. Der Status lautet „Waiting: …“.
- [x] **P2 Korrektur in ppm**: Schrittweite 0,01 / 0,1 / 1 ppm, Bereich ±50 ppm, Auto-Repeat.
- [x] **P2 Helligkeit**: Der Prozentwert aktualisiert sich live beim Ziehen.
- [x] **P3 Touch-Ziele**: Alle Bedienelemente sind ≥ 34 px hoch, Kanal-Buttons 72 px, Switches mit erweiterter Klickfläche.
- [-] **P3 RGB-LED als Ausgangsanzeige**: Nicht genehmigt, die LED zeigt weiter die aktive Seite (Spezifikation 8.1).

## 3. Optik

- [x] **P2 Kontrast**: Auf gelben bzw. hellen Flächen steht automatisch dunkle Schrift (`lv_color_brightness`).
- [x] **P2 Einheitliche Kartenhöhen** auf MAIN (je 58 px).
- [x] **P2 Statusfarben**: grün = aktiv, grau = aus, orange = angefordert, aber wartend, rot = keine Hardware.
- [x] **P3 Button-Schatten entfernt.**
- [-] **P3 Doppelte Statusanzeige im Header**: Bleibt, denn Spezifikation 9.3 verlangt die Textanzeige ON/OFF.
- [~] **P3 Schriften**: 18, 24 und 32 sind entfernt. Genutzt werden 12/14/16/20/28.

## 4. Code-Qualität & Architektur

- [x] **P1 Kein Seiten-Neuaufbau in Event-Callbacks**: Seiten werden einmal pro Navigation gebaut, die Navigation läuft verzögert per `lv_async_call`.
- [x] **P2 `enum class Action`** statt String-Aktionen mit `strcmp`.
- [x] **P2 Lesbarkeit**: Keine Mehrfachanweisungen pro Zeile mehr in den Pages.
- [x] **P2 Null-Checks**: Ein zentraler Event-Handler.
- [~] **P3 Globale Instanzen**: `addBtn`-Duplikat entfernt. `GUI` und `gGui` (statischer Callback-Zugriff) bestehen weiter.
- [x] **P3 RGB-LED** liegt jetzt in [status_led.cpp](src/drivers/status_led.cpp).
- [x] **P2 Gewünschte und erzeugte Frequenz getrennt**: Gespeichert wird die exakte Wunschfrequenz. Die Abweichung des SI5351 wird auf der Kanalseite angezeigt.
- [~] **P2 Fehlerzustand**: Bei einem Fehlschlag kommt ein Dialog, der Schalter zeigt aber weiter ON, während der Ausgang aus ist.
- [x] **P3 `isValidDrive()` / `isValidPhase()`** in [constants.h](include/constants.h).
- [x] **P3 Toter Code entfernt** (alle genannten Funktionen und Dateien). Touch-Kalibrierung als `TOUCH_CAL_DEFAULT`.
- [x] **P2 Logging**: Ausführliche SI5351- und Backlight-Logs nur noch mit `LOG_DEBUG=1` ([config.h](include/config.h)).
- [ ] **P3 Display-Flush mit DMA und Doppelpuffer.**

## 5. SI5351 / HF-Funktion

- [x] **P1 IQ unter 1,4 MHz falsch / VCO außerhalb der Spezifikation**: IQ-Modus erst ab 4,73 MHz (`IQ_MIN_HZ`).
- [x] **P2 PLL-Reset**: Die PLL-Programmierung erfolgt jetzt in der Firmware, es wird nur die betroffene PLL zurückgesetzt.
- [x] **P2 Korrektur ohne Neu-Initialisierung**: Die Bibliotheksvariable `si5351Correction` wird direkt gesetzt.
- [x] **P2 `nearestSi5351Hz` entfernt**: Die tatsächliche Frequenz wird aus den programmierten PLL- und MultiSynth-Werten berechnet.
- [x] **P2 180°/270° per CLK_INV** im gesamten IQ-Bereich.
- [x] **P3 IQ-Bereich und Phasenrealisierung** in Handbuch und Spezifikation dokumentiert.

## 6. Build & Projekt

- [x] **P2 Bibliothek** auf Commit `301b87c` gepinnt.
- [ ] **P3 Arduino-Core 3.x**: Bei einem Update `ledcSetup` / `ledcAttachPin` umstellen.
- [x] **P3 `lv_conf.h`**: Ungenutzte Fonts und Widgets sind aus, das spart ca. 46 KB Flash. LVGL-Warnungen gehen auf Serial.
- [x] **P3 `settings.begin()`** wird ausgewertet.
- [ ] **P3 Unit-Tests** (`pio test -e native`) für `FrequencyUtils`.

## 7. Dokumentation

- [x] README-Pins korrigiert. Handbücher DE/EN und Spezifikation sind auf 1.1.0 aktualisiert, inkl. Abschnitt 40 „Changes in 1.1.0 and Approved Deviations“.

---

## 8. Messplan für die Hardware-Verifikation

Geräte: Frequenzzähler, Oszilloskop (2 Kanäle), Spektrumanalysator.

| # | Test | Erwartung |
|---|---|---|
| 1 | Booten ohne SI5351 (oder mit getrenntem SDA) | genau **ein** Dialog, „OK“ schließt ihn, Karten zeigen „NO HW“ |
| 2 | Helligkeit am Slider ganz nach links | Display bleibt bei 5 % sichtbar |
| 3 | CH1 = 10 MHz, Zähler an CLK0 | Abweichung zum Zähler ≈ angezeigte „Deviation“ + Quarzfehler |
| 4 | Kalibrierung: Abweichung bei 10 MHz in Hz ÷ 10 = ppm eintragen | danach < 1 Hz Abweichung bei 10 MHz |
| 5 | CH1 = 80 MHz, Schrittweite 1 Hz, 10× `+` | Zähler steigt um ≈ 10 Hz und bleibt **nicht hängen** |
| 6 | Oszilloskop auf CH1 (Trigger auf CH1, Nachleuchten), CH2 mehrfach verstellen | **kein** Aussetzer an CH1 (in 1.0.0 setzte jede Änderung beide PLLs zurück) |
| 7 | CH3 an, Master CH1 = 10 MHz, Phase 0/90/180/270, Scope CLK0 gegen CLK2 | 0°, 90°, 180°, 270° (± Kabellaufzeit) |
| 8 | wie 7 bei 5 MHz, 30 MHz und 99 MHz | alle vier Phasen korrekt |
| 9 | Master auf 4,72 MHz, dann 4,73 MHz | CH3 erst „WAIT … < 4.73 MHz“, danach automatisch aktiv |
| 10 | Spektrum CLK2 bei 4,73 MHz und 10 MHz | keine auffälligen Nebenlinien gegenüber CLK0 |
| 11 | Korrektur um +1 ppm ändern, Zähler an CH1 = 10 MHz | Frequenz sinkt um ≈ 10 Hz. Kurzer Umschaltaussetzer ist erlaubt, kein Komplettausfall |
| 12 | `+` gedrückt halten | erster Schritt nach 0,4 s, dann alle 0,1 s, ohne Flackern |
| 13 | Numpad: „500“ + MHz, „0.5“ + MHz, „10.7“ + MHz | Fehler, Fehler, 10,700 000 MHz |
| 13a | Numpad: Eingabe mit „Hz“ bzw. „kHz“ bestätigen, Finger kurz liegen lassen | Kanalseite erscheint, Numpad öffnet **nicht** erneut, Schrittweite bleibt unverändert |
| 13b | Korrektur −1 ppm, CH3 an, Master = 100 MHz | Master und CH3 laufen (keine Meldung „CH3 IQ setup failed“) |
| 14 | Einstellung ändern, > 5 s warten („Saved“), aus- und einschalten | Einstellung bleibt erhalten |
| 15 | Sichtprüfung aller Seiten | nichts abgeschnitten, keine Scrollbalken, kein Überlappen |
