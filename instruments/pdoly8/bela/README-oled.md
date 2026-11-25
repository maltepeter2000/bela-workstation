# pdoly8~ auf dem Bela mit OLED (ohne Pepper)

Nur MIDI-Steuerung: Noten, Pitch Bend, Program Change für die Factory-Presets, MIDI-CC (Controller-Schicht, Kanal 1) für 84 Einstellungen. Das OLED zeigt die zuletzt geänderte Einstellung mit Wert und beim Presetwechsel das Programm.

## MIDI

| | |
|---|---|
| Ports | Keyboard am USB-Host-Port des Bela (`hw:1`) und der Bela als USB-MIDI-Gerät des Computers (`hw:0`, z. B. Ableton), wie im tx81z-Projekt |
| Noten | 4 Stimmen, Velocity (`vamp`, `vcut`) |
| Pitch Bend | ±`bnd` Halbtöne (Standard 2) |
| Program Change 1–64 | Factory-Preset 0–63 (Poly-800-Programm 11 … 88) |
| CC | Controller-Schicht `[p8cc 1]`: 11 Seiten à 8 auf Kanal 1, Rückmeldung an den Controller, Pickup; Tabelle unten |

## Keine Klicks mehr bei CC

- `pdoly8~` glättet jetzt jede kontinuierliche Einstellung intern über `mrph` ms (Standard 25, überbrückt die 10–20 ms zwischen den Stufen eines gedrehten 7-Bit-Reglers): Cutoff, Resonanz, FM, Ring, Drive, Rauschen, DCO2-Tonhöhe, Detune, Vibrato, EG-Tiefe, FM-Verhältnis und -Versatz, Klangfarbe, Pegel, Walsh, Pulsbreite, Sync-Sweep-Tiefen, PWM, Key-Tracking, Velocity→Cutoff, EQ, Stereo-Breite und die Modulations-Slots (Stärke, Ziel, Kanten von Rechteck- und S&H-LFOs). Aus den 7-Bit-Stufen eines Controllers werden Rampen.
- Das Delay schaltet bei jeder Änderung nicht mehr 40 ms stumm (das war beim Original so und die größte Quelle der Knackser): `dgld 1` (Standard) lässt die Zeit gleiten wie bei einem Bandecho, Feedback und Pegel werden geglättet. `dgld 0` = Verhalten des Originals.
- Gemessen (eine CC-Stufe, Sprung im Ausgang): Delay vorher 0,009–0,024, jetzt 0; Drive 0,0009 → 0,00014; FM 0,00037 → 0,00002; Cutoff/Resonanz 0,00005–0,00008 → 0.

## OLED

Über O2O wie im tx81z-Projekt: OSC `/display-strings-and-numbers <Zeile 1> \n <Zeile 2>` an 127.0.0.1:7562, höchstens alle 40 ms. Zeile 1 ist der Name der Einstellung, Zeile 2 der Wert mit Einheit, Schalterstellung oder Ziel, z. B. `Cutoff` / `50.1`, `FM source` / `other DCO`. Beim Presetwechsel: `Program` / `25`.

Gesendet wird von `pdoly8~` selbst aus einem eigenen Thread (`oled 127.0.0.1 7562`, `oledtext <zeile1> <zeile2>`). Der Audio-Thread kopiert nur den Text; Netzwerk-Aufrufe und Konsolen-Ausgaben im Audio-Thread erzwingen auf dem Bela einen Xenomai-Moduswechsel und können Audio-Blöcke kosten, also Klicks beim Drehen. Der Patch selbst enthält deshalb kein `[netsend]` und kein `[print]` mehr.

## CC-Tabelle

Controller-Schicht `[p8cc 1]`: MIDI-Kanal 1 (Argument, 0 = alle Kanäle), dieselben CC-Seiten wie bei allen Instrumenten: Seite k am Launchkey = Reihe k im Editor. CC 20–119 ohne 32, 38, 64–69, 84, 88, 96–101 (Bank-/Data-LSB, Pedale, Portamento Control, High-Res Velocity, NRPN/RPN): 10 volle Seiten und eine mit 4 Plätzen.

Rückmeldung: Änderungen von woanders (Preset, Program Change, Editor) gehen als CC auf demselben Kanal und derselben Nummer an den Controller zurück. Pickup: danach wirkt ein Encoder erst, wenn er den Wert erreicht oder überquert (`pickup 0` schaltet das ab). CC 3 vom Controller auf demselben Kanal fordert alle Werte an (die Launchkey-Firmware schickt ihn vor ihrem ersten CC auf einem neuen Kanal).

| Seite | CC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| **1** PLAY | 20-27 | `har1` | `oct1` | `har2` | `oct2` | `env1` | `env2` | `lev1` | `lev2` |
| **2** FILTER | 28-31 33-36 | `cut` | `res` | `egi` | `kbd` | `env3` | `vcut` | `drv` | `drvp` |
| **3** OSC | 37 39-45 | `intv` | `detn` | `pw1` | `pw2` | `pwm1` | `pwm2` | `drft` | `nois` |
| **4** WALSH-RING | 46-53 | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wse` | `wsm` | `ring` | `rmf` |
| **5** SYNC-XMOD | 54-61 | `sync` | `swp2` | `swe` | `swm` | `xmod` | `extfm` | `xme` | `xmm` |
| **6** MG | 62-63 70-75 | `mgr` | `mgd` | `mgi` | `mgv` | `hmg` | – | – | – |
| **7** DELAY-EQ | 76-83 | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| **8** PLAY MODE | 85-87 89-93 | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |
| **9** MORE | 94-95 102-107 | `atk` | `rel` | `vol` | `tone` | `leak` | `pol` | `trg` |  |

Nicht per CC (nur Editor oder Message): `ch1` (oben im Editor) und die Einzelwerte der Hüllkurven `a1`…`r3`; `mgf` (MG-Rate in Hz) nur als Message.

| CC | Parameter | Anzeige (OLED) | Bereich |
|---|---|---|---|
| **Seite 1: PLAY** | | | |
| 20 | `har1` | Timbre 1 | 0 … 127 |
| 21 | `oct1` | Octave 1 | -1 / 0 / +1 / +2 / +3 / +4 |
| 22 | `har2` | Timbre 2 | 0 … 127 |
| 23 | `oct2` | Octave 2 | -1 / 0 / +1 / +2 / +3 / +4 |
| 24 | `env1` | Env 1 morph | 0 … 127 |
| 25 | `env2` | Env 2 morph | 0 … 127 |
| 26 | `lev1` | Level 1 | 0 … 127 |
| 27 | `lev2` | Level 2 | 0 … 127 |
| **Seite 2: FILTER** | | | |
| 28 | `cut` | Cutoff | 0 … 99 |
| 29 | `res` | Resonance | 0 … 15 |
| 30 | `egi` | EG depth | 0 … 15 |
| 31 | `kbd` | Key track | off / half / full |
| 33 | `env3` | Env 3 morph | 0 … 127 |
| 34 | `vcut` | Vel>cutoff | 0 … 127 |
| 35 | `drv` | Drive | 0 … 127 |
| 36 | `drvp` | Drive position | post / pre |
| **Seite 3: OSC** | | | |
| 37 | `intv` | Interval | 0 … 12 st |
| 39 | `detn` | Detune | 0 … 127 |
| 40 | `pw1` | Pulse width 1 | 0 … 127 |
| 41 | `pw2` | Pulse width 2 | 0 … 127 |
| 42 | `pwm1` | PWM 1 | -127 … 127 |
| 43 | `pwm2` | PWM 2 | -127 … 127 |
| 44 | `drft` | Drift | 0 … 127 |
| 45 | `nois` | Noise | 0 … 15 |
| **Seite 4: WALSH-RING** | | | |
| 46 | `wsh1` | Walsh pos 1 | 0 … 127 |
| 47 | `wsh2` | Walsh pos 2 | 0 … 127 |
| 48 | `wmx1` | Walsh mix 1 | 0 … 127 |
| 49 | `wmx2` | Walsh mix 2 | 0 … 127 |
| 50 | `wse` | EG3>Walsh | -127 … 127 |
| 51 | `wsm` | MG>Walsh | 0 … 127 |
| 52 | `ring` | Ring mod | 0 … 127 |
| 53 | `rmf` | Ring source | 0 … 14 |
| **Seite 5: SYNC-XMOD** | | | |
| 54 | `sync` | Sync | off / on |
| 55 | `swp2` | DCO2 pitch | 0 … 72 st |
| 56 | `swe` | EG3>DCO2 | 0 … 72 st |
| 57 | `swm` | MG>DCO2 | 0 … 24 st |
| 58 | `xmod` | Cross mod | 0 … 127 |
| 59 | `extfm` | Ext FM | 0 … 127 |
| 60 | `xme` | EG3>Xmod | -127 … 127 |
| 61 | `xmm` | MG>Xmod | 0 … 127 |
| **Seite 6: MG** | | | |
| 62 | `mgr` | MG rate | 0 … 15 |
| 63 | `mgd` | MG delay | 0 … 15 |
| 70 | `mgi` | Vibrato | 0 … 200 ct |
| 71 | `mgv` | MG>cutoff | 0 … 15 |
| 72 | `hmg` | MG>timbre | 0 … 127 |
| **Seite 7: DELAY-EQ** | | | |
| 76 | `dtim` | Delay time | 0 … 99 |
| 77 | `dfb` | Delay feedback | 0 … 15 |
| 78 | `dmf` | Delay mod rate | 0 … 31 |
| 79 | `dmi` | Delay mod depth | 0 … 31 |
| 80 | `dlev` | Delay level | 0 … 15 |
| 81 | `dgld` | Delay changes | mute / glide |
| 82 | `treb` | Treble | -6 … 6 |
| 83 | `bass` | Bass | -6 … 6 |
| **Seite 8: PLAY MODE** | | | |
| 85 | `port` | Portamento | 0 … 127 |
| 86 | `pmod` | Glide mode | last / own / legato |
| 87 | `mono` | Voice mode | poly / mono |
| 89 | `retr` | Mono retrig | legato / retrig |
| 90 | `vamp` | Vel>level | 0 … 127 |
| 91 | `bnd` | Bend range | 0 … 12 st |
| 92 | `sprd` | Stereo spread | 0 … 127 |
| 93 | `mrph` | Smoothing | 0 … 100 ms |
| **Seite 9: MORE** | | | |
| 94 | `atk` | Attack all | -63 … 63 |
| 95 | `rel` | Release all | -63 … 63 |
| 102 | `vol` | Volume | 0 … 127 |
| 103 | `tone` | Output stage | off / on |
| 104 | `leak` | Leak | off / on |
| 105 | `pol` | EG polarity | down / up |
| 106 | `trg` | Env 3 trigger | single / multi |

## Audio

Ausgänge 1/2. Audio-Eingang 1 ist die externe FM-Quelle für beide DCOs (Tiefe `extfm`, CC 59).

## Installation

1. **External:** `bela/oled/pdoly8~.pd_linux` (fertig gebaut: ARMv7 Cortex-A8, NEON, Hard-Float) **flach** nach `/root/Bela/projects/pd-externals/`.
2. **Patch:** den Inhalt von `bela/oled/` in ein Bela-Projekt kopieren, z. B. `/root/Bela/projects/pdoly8-oled/`.
3. **O2O** muss wie beim tx81z-Projekt auf dem Bela laufen (Port 7562).
4. Projekteinstellungen: Blocksize 32 (bei zu hoher CPU-Last 64).

```
scp 'bela/oled/pdoly8~.pd_linux' root@bela.local:/root/Bela/projects/pd-externals/
ssh root@bela.local mkdir -p /root/Bela/projects/pdoly8-oled
scp bela/oled/* root@bela.local:/root/Bela/projects/pdoly8-oled/
```

## Dateien

- `pdoly8~.pd_linux` — das External für den Bela (`make bela-zig` baut es neu und erzeugt beide Bela-Ordner neu).
- `oled/_main.pd` — der Patch: MIDI, CC über `p8cc` → `n <name> <wert>`, OLED über O2O.
- `oled/p8factory.pd` — die 64 Factory-Programme, im Patch gespeichert.
- `../tools/gen_bela_oled.py` — erzeugt `oled/` neu; die CC-Tabelle kommt aus `../tools/gen_bela.py` (`CCS`).

## Stand

- Getestet in Pd (libpd 0.56) auf dem Desktop: Preset beim Start, CC mit Rückmeldung und Pickup, Program Change, Noten; die OSC-Nachrichten an O2O mit einem UDP-Empfänger mitgelesen.
- Auf dem Bela noch nicht gelaufen: CPU-Last, MIDI-Ports, Darstellung auf dem OLED.
