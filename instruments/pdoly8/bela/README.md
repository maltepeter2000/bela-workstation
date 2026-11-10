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

Für einen Controller mit 4 Reihen × 16 Pots: 8 Gruppen zu je 8, jede Reihe ein durchgehender CC-Block ohne reservierte Nummern, kein Parameter doppelt. NRPN (14 Bit) mit denselben Nummern: CC 99 = 0, CC 98 = Nummer, CC 6 / 38 = MSB / LSB.

| Reihe | Pots 1–8 | Pots 9–16 |
|---|---|---|
| **1** (CC 12–27) | PLAY | FILTER |
| **2** (CC 40–55) | OSC | WALSH-RING |
| **3** (CC 70–85) | SYNC-FM | MG-FMSOURCE-DEG3 |
| **4** (CC 102–117) | DELAY-EQ | PERFORM |

| Gruppe | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| **PLAY** 12–19 | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| **FILTER** 20–27 | `cut` | `res` | `egi` | `env3` | `kbd` | `vcut` | `drv` | `nois` |
| **OSC** 40–47 | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **WALSH-RING** 48–55 | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `ring` | `rmf` |
| **SYNC-FM** 70–77 | `sync` | `swp2` | `swe` | `swm` | `fm1` | `fm2` | `fmr` | `fmhz` |
| **MG-FMSOURCE-DEG3** 78–85 | `mgr` | `mgi` | `mgd` | `mgv` | `fms` | `fmig` | `pol` | `trg` |
| **DELAY-EQ** 102–109 | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| **PERFORM** 110–117 | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |

LFO2, LFO3, DEG4 und `drvp`, `tone`, `leak`, `mgf` liegen auf freien Nummern außerhalb der Blöcke (ohne Pot). Nicht per CC: `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4` (über `env1`–`env4` erreichbar).

| CC / NRPN | Parameter | Anzeige (OLED) | Bereich |
|---|---|---|---|
| **PLAY** | | | |
| 12 | `har1` | Timbre 1 | 0 … 127 |
| 13 | `har2` | Timbre 2 | 0 … 127 |
| 14 | `env1` | Env 1 morph | 0 … 127 |
| 15 | `env2` | Env 2 morph | 0 … 127 |
| 16 | `intv` | Interval | 0 … 12 st |
| 17 | `detn` | Detune | 0 … 127 |
| 18 | `lev1` | Level 1 | 0 … 127 |
| 19 | `lev2` | Level 2 | 0 … 127 |
| **FILTER** | | | |
| 20 | `cut` | Cutoff | 0 … 99 |
| 21 | `res` | Resonance | 0 … 15 |
| 22 | `egi` | EG depth | 0 … 15 |
| 23 | `env3` | Env 3 morph | 0 … 127 |
| 24 | `kbd` | Key track | off / half / full |
| 25 | `vcut` | Vel>cutoff | 0 … 127 |
| 26 | `drv` | Drive | 0 … 127 |
| 27 | `nois` | Noise | 0 … 15 |
| **OSC** | | | |
| 40 | `oct1` | Octave 1 | -1 / 0 / +1 / +2 / +3 / +4 |
| 41 | `oct2` | Octave 2 | -1 / 0 / +1 / +2 / +3 / +4 |
| 42 | `pw1` | Pulse width 1 | 0 … 127 |
| 43 | `pw2` | Pulse width 2 | 0 … 127 |
| 44 | `pwm1` | PWM 1 | 0 … 127 |
| 45 | `pwm2` | PWM 2 | 0 … 127 |
| 46 | `hmg` | MG>timbre | 0 … 127 |
| 47 | `drft` | Drift | 0 … 127 |
| **WALSH-RING** | | | |
| 48 | `wsh1` | Walsh pos 1 | 0 … 127 |
| 49 | `wsh2` | Walsh pos 2 | 0 … 127 |
| 50 | `wmx1` | Walsh mix 1 | 0 … 127 |
| 51 | `wmx2` | Walsh mix 2 | 0 … 127 |
| 52 | `wsm` | MG>Walsh | 0 … 127 |
| 53 | `wse` | EG3>Walsh | -127 … 127 |
| 54 | `ring` | Ring mod | 0 … 127 |
| 55 | `rmf` | Ring source | 0 … 14 |
| **SYNC-FM** | | | |
| 70 | `sync` | Sync | off / on |
| 71 | `swp2` | DCO2 pitch | 0 … 72 st |
| 72 | `swe` | EG3>DCO2 | 0 … 72 st |
| 73 | `swm` | MG>DCO2 | 0 … 24 st |
| 74 | `fm1` | FM depth 1 | 0 … 127 |
| 75 | `fm2` | FM depth 2 | 0 … 127 |
| 76 | `fmr` | FM ratio | 0.125f … 16 x |
| 77 | `fmhz` | FM offset | -1000 … 1000 Hz |
| **MG-FMSOURCE-DEG3** | | | |
| 78 | `mgr` | MG rate | 0 … 15 |
| 79 | `mgi` | Vibrato | 0 … 200 ct |
| 80 | `mgd` | MG delay | 0 … 15 |
| 81 | `mgv` | MG>cutoff | 0 … 15 |
| 82 | `fms` | FM source | sine / other DCO / input |
| 83 | `fmig` | FM input gain | 0 … 127 |
| 84 | `pol` | EG polarity | down / up |
| 85 | `trg` | Env 3 trigger | single / multi |
| **DELAY-EQ** | | | |
| 102 | `dtim` | Delay time | 0 … 99 |
| 103 | `dfb` | Delay feedback | 0 … 15 |
| 104 | `dmf` | Delay mod rate | 0 … 31 |
| 105 | `dmi` | Delay mod depth | 0 … 31 |
| 106 | `dlev` | Delay level | 0 … 15 |
| 107 | `dgld` | Delay changes | mute / glide |
| 108 | `treb` | Treble | -6 … 6 |
| 109 | `bass` | Bass | -6 … 6 |
| **PERFORM** | | | |
| 110 | `port` | Portamento | 0 … 127 |
| 111 | `pmod` | Glide mode | last / own / legato |
| 112 | `mono` | Voice mode | poly / mono |
| 113 | `retr` | Mono retrig | legato / retrig |
| 114 | `vamp` | Vel>level | 0 … 127 |
| 115 | `bnd` | Bend range | 0 … 12 st |
| 116 | `sprd` | Stereo spread | 0 … 127 |
| 117 | `mrph` | Smoothing | 0 … 100 ms |
| **ohne Pot** | | | |
| 2 | `drvp` | Drive position | post / pre |
| 3 | `tone` | Output stage | off / on |
| 4 | `leak` | Leak | off / on |
| 5 | `mgf` | MG rate | 0.1f … 30 Hz |
| 28 | `env4` | Env 4 morph | 0 … 127 |
| 29 | `trg4` | Env 4 trigger | single / multi |
| 30 | `d4t1` | Env4 target 1 | Ziel 0–32 |
| 31 | `d4a1` | Env4 amount 1 | -127 … 127 |
| 33 | `d4t2` | Env4 target 2 | Ziel 0–32 |
| 34 | `d4a2` | Env4 amount 2 | -127 … 127 |
| 56 | `l3r` | LFO3 rate | 0.01f … 50 Hz |
| 57 | `l3w` | LFO3 wave | tri / sine / saw up / saw down / square / S&H |
| 58 | `l3k` | LFO3 retrig | free / key / phrase |
| 59 | `l3f` | LFO3 fade | 0 … 10 s |
| 60 | `l3t1` | LFO3 target 1 | Ziel 0–32 |
| 61 | `l3a1` | LFO3 amount 1 | -127 … 127 |
| 62 | `l3t2` | LFO3 target 2 | Ziel 0–32 |
| 63 | `l3a2` | LFO3 amount 2 | -127 … 127 |
| 86 | `l2r` | LFO2 rate | 0.01f … 50 Hz |
| 87 | `l2w` | LFO2 wave | tri / sine / saw up / saw down / square / S&H |
| 88 | `l2k` | LFO2 retrig | free / key / phrase |
| 89 | `l2f` | LFO2 fade | 0 … 10 s |
| 90 | `l2t1` | LFO2 target 1 | Ziel 0–32 |
| 91 | `l2a1` | LFO2 amount 1 | -127 … 127 |
| 92 | `l2t2` | LFO2 target 2 | Ziel 0–32 |
| 93 | `l2a2` | LFO2 amount 2 | -127 … 127 |

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
