# pdoly8~ auf dem Bela Pepper

10 Seiten zu je 8 Pots, Pickup, CV-Lock, Factory-Presets, MIDI über USB mit CC für alle Seiten-Parameter.

## Bedienung

| | |
|---|---|
| Taste 1 / 2 | Seite zurück / vor (LED 1–10 zeigt die Seite, die Konsole nennt sie) |
| Taste 3 / 4 | Factory-Preset runter / hoch (Taste 4: beim Loslassen) |
| Pots 1–8 | die Parameter der Seite. **Pickup:** nach Seiten- oder Presetwechsel übernimmt ein Pot erst, wenn er den aktuellen Wert überstreicht |
| CV-Eingänge | ohne Kabel setzt der Pot den Wert; mit Kabel schwächt der Pot die CV ab |
| **CV-Lock** | Taste 4 halten und einen Pot drehen: dieser Eingang bleibt auf seinem Parameter, auf allen Seiten, ohne Pickup (für CV). Dasselbe noch einmal löst ihn. Die Konsole meldet `locked pot N: n <name>` / `unlocked pot N`; das Preset springt dabei nicht weiter |
| Audio In 1 | externe FM-Quelle: Seite FM, `fms` auf „Eingang“, Pot 6 = Pegel (`fmig`) |
| MIDI (USB) | Noten (4 Stimmen), Pitch Bend, Program Change 1–64 = Preset 0–63, CC siehe unten (dieselbe Tabelle wie `bela-oled/`, jeder Kanal) |

## Seiten

| Seite | Pot 1 | Pot 2 | Pot 3 | Pot 4 | Pot 5 | Pot 6 | Pot 7 | Pot 8 |
|---|---|---|---|---|---|---|---|---|
| **1 PLAY** | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| **2 FILTER** | `cut` | `res` | `egi` | `kbd` | `env3` | `vcut` | `drv` | `nois` |
| **3 OSC** | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **4 WALSH** | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `lev1` | `lev2` |
| **5 SYNC-RING** | `sync` | `swp2` | `swe` | `swm` | `ring` | `rmf` | `lev1` | `lev2` |
| **6 FM** | `fm1` | `fm2` | `fmr` | `fmhz` | `fms` | `fmig` | `lev1` | `lev2` |
| **7 MG-DEG4** | `mgr` | `mgi` | `mgd` | `mgv` | `env4` | `trg4` | `d4t1` | `d4a1` |
| **8 LFO2** | `l2r` | `l2w` | `l2k` | `l2f` | `l2t1` | `l2a1` | `l2t2` | `l2a2` |
| **9 LFO3** | `l3r` | `l3w` | `l3k` | `l3f` | `l3t1` | `l3a1` | `l3t2` | `l3a2` |
| **10 FX-PERFORM** | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `port` | `mono` | `vamp` |

Schalter (`kbd`, `sync`, `fms`, `mono`, LFO-Welle …) und Ziele (`d4t1`, `l2t1` …) werden über den Drehweg in gleich große Zonen geteilt; ±127-Parameter haben 0 in der Pot-Mitte. Bei Ziel-Pots nennt die Konsole das gewählte Ziel.

## MIDI-CC

Controller-Schicht `[p8cc 1]`: MIDI-Kanal 1 (Argument, 0 = alle Kanäle), dieselben CC-Seiten wie bei allen Instrumenten: Seite k am Launchkey = Reihe k im Editor. CC 20–119 ohne 32, 38, 64–69, 84, 88, 96–101 (Bank-/Data-LSB, Pedale, Portamento Control, High-Res Velocity, NRPN/RPN): 10 volle Seiten und eine mit 4 Plätzen.

Rückmeldung: Änderungen von woanders (Preset, Program Change, Editor) gehen als CC auf demselben Kanal und derselben Nummer an den Controller zurück. Pickup: danach wirkt ein Encoder erst, wenn er den Wert erreicht oder überquert (`pickup 0` schaltet das ab).

| Seite | CC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| **1** PLAY | 20-27 | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| **2** FILTER | 28-31 33-36 | `cut` | `res` | `egi` | `env3` | `kbd` | `vcut` | `drv` | `nois` |
| **3** OSC | 37 39-45 | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **4** WALSH-RING | 46-53 | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `ring` | `rmf` |
| **5** SYNC-FM | 54-61 | `sync` | `swp2` | `swe` | `swm` | `fm1` | `fm2` | `fmr` | `fmhz` |
| **6** MG-FMSOURCE-DEG3 | 62-63 70-75 | `mgr` | `mgi` | `mgd` | `mgv` | `fms` | `fmig` | `pol` | `trg` |
| **7** DELAY-EQ | 76-83 | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| **8** PERFORM | 85-87 89-93 | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |
| **9** DEG4-DRIVE | 94-95 102-107 | `env4` | `trg4` | `d4t1` | `d4a1` | `d4t2` | `d4a2` | `drvp` | `tone` |
| **10** LFO2 | 108-115 | `l2r` | `l2w` | `l2k` | `l2f` | `l2t1` | `l2a1` | `l2t2` | `l2a2` |
| **11** LFO3 | 116-119 | `l3r` | `l3w` | `l3t1` | `l3a1` |  |  |  |  |

Nicht per CC (nur Editor oder Message): `l3k`, `l3f`, `l3t2`, `l3a2`, `leak`, `mgf`, `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4`.

| CC | Parameter | Anzeige (OLED) | Bereich |
|---|---|---|---|
| **Seite 1: PLAY** | | | |
| 20 | `har1` | Timbre 1 | 0 … 127 |
| 21 | `har2` | Timbre 2 | 0 … 127 |
| 22 | `env1` | Env 1 morph | 0 … 127 |
| 23 | `env2` | Env 2 morph | 0 … 127 |
| 24 | `intv` | Interval | 0 … 12 st |
| 25 | `detn` | Detune | 0 … 127 |
| 26 | `lev1` | Level 1 | 0 … 127 |
| 27 | `lev2` | Level 2 | 0 … 127 |
| **Seite 2: FILTER** | | | |
| 28 | `cut` | Cutoff | 0 … 99 |
| 29 | `res` | Resonance | 0 … 15 |
| 30 | `egi` | EG depth | 0 … 15 |
| 31 | `env3` | Env 3 morph | 0 … 127 |
| 33 | `kbd` | Key track | off / half / full |
| 34 | `vcut` | Vel>cutoff | 0 … 127 |
| 35 | `drv` | Drive | 0 … 127 |
| 36 | `nois` | Noise | 0 … 15 |
| **Seite 3: OSC** | | | |
| 37 | `oct1` | Octave 1 | -1 / 0 / +1 / +2 / +3 / +4 |
| 39 | `oct2` | Octave 2 | -1 / 0 / +1 / +2 / +3 / +4 |
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
| **Seite 6: MG-FMSOURCE-DEG3** | | | |
| 62 | `mgr` | MG rate | 0 … 15 |
| 63 | `mgi` | Vibrato | 0 … 200 ct |
| 70 | `mgd` | MG delay | 0 … 15 |
| 71 | `mgv` | MG>cutoff | 0 … 15 |
| 72 | `fms` | FM source | sine / other DCO / input |
| 73 | `fmig` | FM input gain | 0 … 127 |
| 74 | `pol` | EG polarity | down / up |
| 75 | `trg` | Env 3 trigger | single / multi |
| **Seite 7: DELAY-EQ** | | | |
| 76 | `dtim` | Delay time | 0 … 99 |
| 77 | `dfb` | Delay feedback | 0 … 15 |
| 78 | `dmf` | Delay mod rate | 0 … 31 |
| 79 | `dmi` | Delay mod depth | 0 … 31 |
| 80 | `dlev` | Delay level | 0 … 15 |
| 81 | `dgld` | Delay changes | mute / glide |
| 82 | `treb` | Treble | -6 … 6 |
| 83 | `bass` | Bass | -6 … 6 |
| **Seite 8: PERFORM** | | | |
| 85 | `port` | Portamento | 0 … 127 |
| 86 | `pmod` | Glide mode | last / own / legato |
| 87 | `mono` | Voice mode | poly / mono |
| 89 | `retr` | Mono retrig | legato / retrig |
| 90 | `vamp` | Vel>level | 0 … 127 |
| 91 | `bnd` | Bend range | 0 … 12 st |
| 92 | `sprd` | Stereo spread | 0 … 127 |
| 93 | `mrph` | Smoothing | 0 … 100 ms |
| **Seite 9: DEG4-DRIVE** | | | |
| 94 | `env4` | Env 4 morph | 0 … 127 |
| 95 | `trg4` | Env 4 trigger | single / multi |
| 102 | `d4t1` | Env4 target 1 | Ziel 0–32 |
| 103 | `d4a1` | Env4 amount 1 | -127 … 127 |
| 104 | `d4t2` | Env4 target 2 | Ziel 0–32 |
| 105 | `d4a2` | Env4 amount 2 | -127 … 127 |
| 106 | `drvp` | Drive position | post / pre |
| 107 | `tone` | Output stage | off / on |
| **Seite 10: LFO2** | | | |
| 108 | `l2r` | LFO2 rate | 0.01f … 50 Hz |
| 109 | `l2w` | LFO2 wave | tri / sine / saw up / saw down / square / S&H |
| 110 | `l2k` | LFO2 retrig | free / key / phrase |
| 111 | `l2f` | LFO2 fade | 0 … 10 s |
| 112 | `l2t1` | LFO2 target 1 | Ziel 0–32 |
| 113 | `l2a1` | LFO2 amount 1 | -127 … 127 |
| 114 | `l2t2` | LFO2 target 2 | Ziel 0–32 |
| 115 | `l2a2` | LFO2 amount 2 | -127 … 127 |
| **Seite 11: LFO3** | | | |
| 116 | `l3r` | LFO3 rate | 0.01f … 50 Hz |
| 117 | `l3w` | LFO3 wave | tri / sine / saw up / saw down / square / S&H |
| 118 | `l3t1` | LFO3 target 1 | Ziel 0–32 |
| 119 | `l3a1` | LFO3 amount 1 | -127 … 127 |

## Installation

1. **External:** `bela/pdoly8~.pd_linux` (fertig gebaut: ARMv7 Cortex-A8, NEON, Hard-Float) **flach** nach `/root/Bela/projects/pd-externals/` kopieren.
   Alternativ auf dem Bela selbst bauen: `make bela PDINCLUDE=/usr/local/include/libpd` und die `pdoly8~.pd_linux` dorthin kopieren.
2. **Patch:** den Inhalt von `bela/project/` in ein Bela-Projekt kopieren, z. B. `/root/Bela/projects/pdoly8/`.
3. **Projekteinstellungen:** Blocksize 32 (bei zu hoher CPU-Last 64), USB-MIDI-Port prüfen (`amidi -l`).

```
scp 'bela/pdoly8~.pd_linux' root@bela.local:/root/Bela/projects/pd-externals/
scp bela/project/* root@bela.local:/root/Bela/projects/pdoly8/
```

## Dateien

- `pdoly8~.pd_linux` — das External für den Bela (`make bela-zig` baut es von Linux aus neu, mit zig, und erzeugt `project/` neu).
- `project/_main.pd` — der Patch: Seiten- und CC-Tabelle (`pd tables`), Tasten, LEDs, MIDI, Pots; alles über send/receive.
- `project/p8knob.pd` — ein Pot: Pickup, CV-Lock, Senden an `pdoly8~`.
- `project/p8factory.pd` — die 64 Factory-Programme, direkt im Patch gespeichert.
- `../tools/gen_bela.py` — erzeugt `project/` neu: Seiten (`PAGES`), CCs (`CCS`), Tasten, LEDs und Pot-Bereiche stehen oben im Skript.

## Stand

- Getestet in Pd (libpd 0.56) auf dem Desktop mit simulierten Pots, Tasten und MIDI-CC: Preset beim Start, Pickup, Seitenwechsel, CV-Lock und Entsperren, Preset per Taste 4, CC-Umrechnung, FM über Audio In 1.
- Pot-Bereiche: pro Pot aus beladrumrack (Maxima 0.907 / 0.916 / 0.923 / 0.924 / 0.877 / 0.890 / 0.917 / 0.997, im Patch mit 1 % Reserve).
- LED- und Tasten-Pins aus beladrumrack (LED 1–10 = Pins 17, 18, 21, 13, 14, 11, 12, 15, 16, 19; Tasten 1–4 = Pins 26, 25, 24, 23).
- Auf dem Bela zu prüfen: CPU-Last, Reihenfolge der LEDs, CV-Bereich mit gestecktem Kabel.
