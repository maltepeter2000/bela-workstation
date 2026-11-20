# Controller-Schicht für alle Instrumente

(Übernommen aus dem Repo PDoly800, mit den Ergänzungen für d50~ am Ende.)

Ein System für alle Pd-Instrumente: Der Launchkey bleibt immer gleich eingestellt, jedes Instrument
bekommt eine eigene Controller-Schicht und einen eigenen Editor. Vorlage ist pdoly8~.

## Regeln

1. **Ein MIDI-Kanal pro Instrument.** Der Kanal wird als Argument gesetzt: `[p8cc 1]` = Kanal 1,
   0 = alle Kanäle. Am Launchkey wechselt man den Keys-Kanal; Tasten und Encoder gehen dann
   gemeinsam an das gewählte Instrument.
2. **Dieselben CC-Seiten für alle Instrumente.** Seite k am Launchkey = Reihe k im Editor.
   CC 20–119 ohne die Nummern mit fester Bedeutung:

   | Seite | CC |
   |---|---|
   | 1 | 20–27 |
   | 2 | 28 29 30 31 33 34 35 36 |
   | 3 | 37 39 40 41 42 43 44 45 |
   | 4 | 46–53 |
   | 5 | 54–61 |
   | 6 | 62 63 70 71 72 73 74 75 |
   | 7 | 76–83 |
   | 8 | 85 86 87 89 90 91 92 93 |
   | 9 | 94 95 102 103 104 105 106 107 |
   | 10 | 108–115 |
   | 11 | 116–119 (4 Plätze) |

   Ausgelassen: 32, 38 (Bank-/Data-LSB), 64–69 (Pedale), 84 (Portamento Control),
   88 (High-Res Velocity), 96–101 (Data Inc/Dec, NRPN, RPN), 120–127 (Channel Mode).
   84 Regler pro Instrument, kein NRPN.
3. **Jedes Instrument legt seine Parameter nach eigener Logik in die Reihen.** Lücken sind
   erlaubt. Was nicht auf eine Seite passt, ist nur im Editor (Maus) erreichbar.
4. **Rückmeldung an den Controller.** Jede Änderung, die nicht vom Encoder selbst kommt (Preset,
   Program Change, Editor, Message), geht als CC auf demselben Kanal und derselben Nummer zurück.
   Die eigene Firmware des Launchkey übernimmt sie als Encoder-Stand: der nächste Dreh geht vom
   richtigen Wert weiter.
5. **Pickup als Rückfallebene.** Nimmt ein Controller keine Rückmeldung an, wirkt ein Encoder nach
   einer Änderung von außen erst, wenn er den Wert erreicht (±3) oder überquert. `pickup 0`
   schaltet das ab.
6. **Sync-Anfrage.** CC 3 auf dem Kanal des Instruments (oder die Message `sync`) schickt alle
   Werte an den Controller. Die Launchkey-Firmware sendet CC 3 = 127 vor ihrem ersten CC auf einem
   neuen Kanal (nach dem Einschalten und nach einem Kanalwechsel).

## Was ein Instrument dafür braucht

- `n <name> <0-127>`: setzt einen Parameter aus einer Reglerstellung (Bereich, Zonen, bipolar
  liegen im Instrument).
- `ninfo <name>`: antwortet mit `info <name> <0-127> <Text>`.
- **Jede Änderung meldet das Instrument selbst** als `info <name> <0-127> <Text>` auf einem Outlet,
  in derselben logischen Zeit (pdoly8~: Clock mit Verzögerung 0). Daran erkennt die Schicht, ob die
  Änderung vom eigenen Encoder kam (keine Rückmeldung) oder von außen.
- Der Text ist ein Symbol (Leerzeichen als geschütztes Leerzeichen), z. B. `2 Hz`, `square`.

## Bausteine

| Baustein | Aufgabe | Erzeugt von |
|---|---|---|
| `tools/ctlayer.py` | gemeinsam für alle Instrumente: Seiten, Abstraktion mit Kanalfilter, Rückmeldung, Pickup, Sync | – |
| `<inst>cc.pd` | die Controller-Schicht eines Instruments, Tabelle eingebettet | `ctlayer.write(pfad, titel, rows)` |
| `<inst>edit.pd` | Editor: Reihen = Seiten, dann Reihen ohne CC; Regler, Zahl 0–127, Wert mit Einheit | Generator des Instruments |
| Launchkey-Firmware | Encoder-Modus Custom 1 = diese Seiten, Kanal = Keys-Kanal, Rückmeldung, CC 3 | `maltepeter2000/launchkey_firmware`, `cfw/cc_pages.c` |

Verkabelung im Patch:

```
[r <bus>-out]          [p8edit <bus>]
|
[p8cc 1]               [r <bus>]
|                      |
[s <bus>]              [pdoly8~]
                       |  (Outlet 3)
                       [s <bus>-out]
```

In Pd unter MIDI-Einstellungen den Launchkey als Ein- und Ausgang wählen, sonst kommt die
Rückmeldung nicht an.

## Neues Instrument anschließen

1. Im Instrument `n`, `ninfo` und die Meldung jeder Änderung als `info` einbauen.
2. Reihen festlegen: Liste von `(Titel, [Name, …])`, höchstens 8 pro Reihe, Reihe 11 höchstens 4.
3. `ctlayer.write("<inst>cc.pd", "<inst>cc", rows)` im Generator aufrufen.
4. Editor-Generator nach dem Muster von `tools/gen_editor.py`: erst die CC-Reihen, dann die übrigen.
5. Kanal festlegen und in der Help-Datei als Argument setzen.

## d50~: was anders ist

- **Auswahl statt fester Belegung.** Der D-50 hat 311 Werte (4 Partials × 54, 2 Tones × 38, Patch 16),
  die Schicht 84 Plätze. Der Regler `part` (Seite 2, erster Platz) wählt U1, U2, L1 oder L2. Die
  Seiten 2–6 zeigen dieses Partial, die Seiten 7–11 den Tone, zu dem es gehört. Beim Umschalten
  meldet d50~ alle Werte, die sich dadurch ändern, als `info`: Controller und Editor folgen.
- **Seite 1 ist die Performance-Reihe:** `cut res att rel chorus reverb balance volume`. `cut`, `res`,
  `att`, `rel` sind Verschiebungen über dem Patch (64 = unverändert), werden nicht gespeichert und
  gehen bei jedem Patch-Wechsel auf 0. `cut` wirkt auch auf klingende Noten.
- **Getrennter Sendekanal.** d50~ meldet `channel <0-16>` (Empfang) und `txchannel <1-16>` (Senden).
  `tools/ctlayer.py` in diesem Repo hat dafür einen Zusatz: mit `txchannel` geht die Rückmeldung
  auf diesem Kanal hinaus, sonst wie bisher auf dem Empfangskanal. Der Zusatz ist rückwärtskompatibel
  und kann in PDoly800 und tx81z-pd übernommen werden.
- **Nur im Editor (ohne CC):** Modi der WG, Bias, Keyfollow der Hüllkurven, die Patch-Parameter
  (Key Mode, Split, Key Shift, Portamento, Reverb-Typ …) und Master Tune / Stimmenzahl.
- Bausteine: `tools/d50_params.py` (Tabelle, Reihen), `tools/gen_d50cc.py`, `tools/gen_editor.py`,
  `tools/gen_help.py`; alles neu erzeugen mit `make gen` in `d50~/`.

## juno~

Keine Ebenen, keine Makros: 33 Regler auf fünf Seiten (`tools/juno_params.py`).
Seite 1 ist Filter und Hüllkurve, Seiten 2 und 3 der Rest des Patches, Seite 4
die Einstellungen des Instruments, die kein Patch speichert (volume, tune,
bend, octave, vellevel, velcutoff, model), Seite 5 die Stimmen (calibrate,
chorusnoise). Schieberegler laufen 0–127 wie im Juno-106-Patch, Pot und Wert
sind also dieselbe Zahl. `junocc.pd`, `junopads.pd` und `junoedit.pd`
entstehen mit `make gen` in `pd/`.

### Pads

`[junopads 10]` (`tools/ctpads.py`, Belegung in `PADS` von `tools/juno_params.py`):
Launchkey im Drum-Modus, eigene Firmware, Drums Channel 10, Noten 36–43 oben,
44–51 unten. Jede Spalte eine Aufgabe: oben hoch, darunter runter bzw. das Gegenstück.

| Pad | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| oben (36–39) | factory + | user + | init (2×) | chorus I → II → I+II → aus |
| unten (44–47) | factory − | user − | save | Juno-106 / 60 |

`save` speichert in den zuletzt gewählten User-Platz und schreibt die Bankdatei.
`juno~` muss auf einem anderen Kanal hören (Hilfe-Patch: `channel 1`).

## Stand

- d50~: 11 Seiten belegt (Seite 1 Performance, 2–6 gewähltes Partial, 7–11 sein Tone), Kanal einstellbar.
- juno~: 5 Seiten belegt (Seite 1 Filter und Hüllkurve, 2–3 Patch, 4 Setup, 5 Stimmen), Kanal einstellbar; Pads auf Kanal 10.
