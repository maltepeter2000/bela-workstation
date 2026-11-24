# pdoly8~ auf dem Bela Pepper

8 Seiten zu je 8 Pots, Pickup, CV-Lock, Factory-Presets, MIDI über USB mit CC für alle Seiten-Parameter.

## Bedienung

| | |
|---|---|
| Taste 1 / 2 | Seite zurück / vor (LED 1–8 zeigt die Seite, die Konsole nennt sie) |
| Taste 3 / 4 | Factory-Preset runter / hoch (Taste 4: beim Loslassen) |
| Pots 1–8 | die Parameter der Seite. **Pickup:** nach Seiten- oder Presetwechsel übernimmt ein Pot erst, wenn er den aktuellen Wert überstreicht |
| CV-Eingänge | ohne Kabel setzt der Pot den Wert; mit Kabel schwächt der Pot die CV ab |
| **CV-Lock** | Taste 4 halten und einen Pot drehen: dieser Eingang bleibt auf seinem Parameter, auf allen Seiten, ohne Pickup (für CV). Dasselbe noch einmal löst ihn. Die Konsole meldet `locked pot N: n <name>` / `unlocked pot N`; das Preset springt dabei nicht weiter |
| Audio In 1 | externe FM-Quelle: Seite FM, `fms` auf „Eingang“, Pot 6 = Pegel (`fmig`) |
| MIDI (USB) | Noten (4 Stimmen), Pitch Bend, Program Change 1–64 = Preset 0–63, CC siehe unten (dieselbe Tabelle wie `bela/oled/`, jeder Kanal) |

## Seiten

| Seite | Pot 1 | Pot 2 | Pot 3 | Pot 4 | Pot 5 | Pot 6 | Pot 7 | Pot 8 |
|---|---|---|---|---|---|---|---|---|
| **1 PLAY** | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| **2 FILTER** | `cut` | `res` | `egi` | `kbd` | `env3` | `vcut` | `drv` | `nois` |
| **3 OSC** | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **4 WALSH** | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `lev1` | `lev2` |
| **5 SYNC-RING** | `sync` | `swp2` | `swe` | `swm` | `ring` | `rmf` | `lev1` | `lev2` |
| **6 FM** | `fm1` | `fm2` | `fmr` | `fmhz` | `fms` | `fmig` | `lev1` | `lev2` |
| **7 MG-MORE** | `mgr` | `mgi` | `mgd` | `mgv` | `drvp` | `tone` | `leak` | `dgld` |
| **8 FX-PERFORM** | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `port` | `mono` | `vamp` |

Schalter (`kbd`, `sync`, `fms`, `mono`, `tone`, `leak`, `dgld` …) werden über den Drehweg in gleich große Zonen geteilt; ±127-Parameter haben 0 in der Pot-Mitte.

## MIDI-CC

Controller-Schicht `[p8cc 1]`: MIDI-Kanal 1 (Argument, 0 = alle Kanäle), dieselben CC-Seiten wie bei allen Instrumenten: Seite k am Launchkey = Reihe k im Editor. CC 20–119 ohne 32, 38, 64–69, 84, 88, 96–101 (Bank-/Data-LSB, Pedale, Portamento Control, High-Res Velocity, NRPN/RPN): 10 volle Seiten und eine mit 4 Plätzen.

Rückmeldung: Änderungen von woanders (Preset, Program Change, Editor) gehen als CC auf demselben Kanal und derselben Nummer an den Controller zurück. Pickup: danach wirkt ein Encoder erst, wenn er den Wert erreicht oder überquert (`pickup 0` schaltet das ab). CC 3 vom Controller auf demselben Kanal fordert alle Werte an (die Launchkey-Firmware schickt ihn vor ihrem ersten CC auf einem neuen Kanal).

| Seite | CC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| **1** PLAY | 20-27 | `har1` | `oct1` | `har2` | `oct2` | `env1` | `env2` | `lev1` | `lev2` |
| **2** FILTER | 28-31 33-36 | `cut` | `res` | `egi` | `kbd` | `env3` | `vcut` | `drv` | `drvp` |
| **3** OSC | 37 39-45 | `intv` | `detn` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **4** WALSH-RING | 46-53 | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `ring` | `rmf` |
| **5** SYNC-FM | 54-61 | `sync` | `swp2` | `swe` | `swm` | `fm1` | `fm2` | `fmr` | `fmhz` |
| **6** FMSOURCE-MG | 62-63 70-75 | `fms` | `fmig` | `mgr` | `mgf` | `mgi` | `mgd` | `mgv` | `nois` |
| **7** DELAY-EQ | 76-83 | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| **8** PLAY MODE | 85-87 89-93 | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |
| **9** MORE | 94-95 102-107 | `atk` | `rel` | `vol` | `tone` | `leak` | `pol` | `trg` |  |

Nicht per CC (nur Editor oder Message): `l3k`, `l3f`, `l3t2`, `l3a2`, `leak`, `mgf`, `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4`.

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
| 42 | `pwm1` | PWM 1 | 0 … 127 |
| 43 | `pwm2` | PWM 2 | 0 … 127 |
| 44 | `hmg` | MG>timbre | 0 … 127 |
| 45 | `drft` | Drift | 0 … 127 |
| **Seite 4: WALSH-RING** | | | |
| 46 | `wsh1` | Walsh pos 1 | 0 … 127 |
| 47 | `wsh2` | Walsh pos 2 | 0 … 127 |
| 48 | `wmx1` | Walsh mix 1 | 0 … 127 |
| 49 | `wmx2` | Walsh mix 2 | 0 … 127 |
| 50 | `wsm` | MG>Walsh | 0 … 127 |
| 51 | `wse` | EG3>Walsh | -127 … 127 |
| 52 | `ring` | Ring mod | 0 … 127 |
| 53 | `rmf` | Ring source | 0 … 14 |
| **Seite 5: SYNC-FM** | | | |
| 54 | `sync` | Sync | off / on |
| 55 | `swp2` | DCO2 pitch | 0 … 72 st |
| 56 | `swe` | EG3>DCO2 | 0 … 72 st |
| 57 | `swm` | MG>DCO2 | 0 … 24 st |
| 58 | `fm1` | FM depth 1 | 0 … 127 |
| 59 | `fm2` | FM depth 2 | 0 … 127 |
| 60 | `fmr` | FM ratio | 0.125f … 16 x |
| 61 | `fmhz` | FM offset | -1000 … 1000 Hz |
| **Seite 6: FMSOURCE-MG** | | | |
| 62 | `fms` | FM source | sine / other DCO / input |
| 63 | `fmig` | FM input gain | 0 … 127 |
| 70 | `mgr` | MG rate | 0 … 15 |
| 71 | `mgf` | MG rate | 0.1f … 30 Hz |
| 72 | `mgi` | Vibrato | 0 … 200 ct |
| 73 | `mgd` | MG delay | 0 … 15 |
| 74 | `mgv` | MG>cutoff | 0 … 15 |
| 75 | `nois` | Noise | 0 … 15 |
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

## Installation

1. **External:** `bela/pdoly8~.pd_linux` (fertig gebaut: ARMv7 Cortex-A8, NEON, Hard-Float) **flach** nach `/root/Bela/projects/pd-externals/` kopieren.
   Alternativ auf dem Bela selbst bauen: `make bela PDINCLUDE=/usr/local/include/libpd` und die `pdoly8~.pd_linux` dorthin kopieren.
2. **Patch:** den Inhalt von `bela/pepper/` in ein Bela-Projekt kopieren, z. B. `/root/Bela/projects/pdoly8/`.
3. **Projekteinstellungen:** Blocksize 32 (bei zu hoher CPU-Last 64), USB-MIDI-Port prüfen (`amidi -l`).

```
scp 'bela/pdoly8~.pd_linux' root@bela.local:/root/Bela/projects/pd-externals/
scp bela/pepper/* root@bela.local:/root/Bela/projects/pdoly8/
```

## Dateien

- `pdoly8~.pd_linux` — das External für den Bela (`make bela-zig` baut es von Linux aus neu, mit zig, und erzeugt `pepper/` neu).
- `pepper/_main.pd` — der Patch: Seiten- und CC-Tabelle (`pd tables`), Tasten, LEDs, MIDI, Pots; alles über send/receive.
- `pepper/p8knob.pd` — ein Pot: Pickup, CV-Lock, Senden an `pdoly8~`.
- `pepper/p8factory.pd` — die 64 Factory-Programme, direkt im Patch gespeichert.
- `../tools/gen_bela.py` — erzeugt `pepper/` neu: Seiten (`PAGES`), CCs (`CCS`), Tasten, LEDs und Pot-Bereiche stehen oben im Skript.

## Stand

- Getestet in Pd (libpd 0.56) auf dem Desktop mit simulierten Pots, Tasten und MIDI-CC: Preset beim Start, Pickup, Seitenwechsel, CV-Lock und Entsperren, Preset per Taste 4, CC-Umrechnung, FM über Audio In 1.
- Pot-Bereiche: pro Pot aus beladrumrack (Maxima 0.907 / 0.916 / 0.923 / 0.924 / 0.877 / 0.890 / 0.917 / 0.997, im Patch mit 1 % Reserve).
- LED- und Tasten-Pins aus beladrumrack (LED 1–10 = Pins 17, 18, 21, 13, 14, 11, 12, 15, 16, 19; Tasten 1–4 = Pins 26, 25, 24, 23).
- Auf dem Bela zu prüfen: CPU-Last, Reihenfolge der LEDs, CV-Bereich mit gestecktem Kabel.
