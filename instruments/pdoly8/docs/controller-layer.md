# Controller-Schicht für alle Instrumente

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

## Stand

- pdoly8~: Kanal 1, 11 Seiten belegt; Desktop, Bela Pepper und Bela OLED nutzen dieselbe Schicht.
- Launchkey-Firmware: gebaut und mit Host-Tests geprüft, auf der Hardware noch nicht getestet.
