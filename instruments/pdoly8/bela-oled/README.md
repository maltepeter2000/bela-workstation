# pdoly8~ auf dem Bela mit OLED (ohne Pepper)

Nur MIDI-Steuerung: Noten, Pitch Bend, Program Change für die Factory-Presets, MIDI-CC und 14-Bit-NRPN für alle Einstellungen. Das OLED zeigt die zuletzt geänderte Einstellung mit Wert und beim Presetwechsel das Programm.

## MIDI

| | |
|---|---|
| Ports | Keyboard am USB-Host-Port des Bela (`hw:1`) und der Bela als USB-MIDI-Gerät des Computers (`hw:0`, z. B. Ableton), wie im tx81z-Projekt |
| Noten | 4 Stimmen, Velocity (`vamp`, `vcut`) |
| Pitch Bend | ±`bnd` Halbtöne (Standard 2) |
| Program Change 1–64 | Factory-Preset 0–63 (Poly-800-Programm 11 … 88) |
| CC | 8 Gruppen à 8 für 4 × 16 Pots plus LFO2/LFO3/DEG4 ohne Pot, Tabelle unten, jeder Kanal |
| NRPN (14 Bit) | CC 99 = 0, CC 98 = die CC-Nummer der Einstellung, dann CC 6 (MSB) und CC 38 (LSB). Nur MSB geht auch (wie ein normaler CC) |

## Keine Klicks mehr bei CC

- `pdoly8~` glättet jetzt jede kontinuierliche Einstellung intern über `mrph` ms (Standard 25, überbrückt die 10–20 ms zwischen den Stufen eines gedrehten 7-Bit-Reglers): Cutoff, Resonanz, FM, Ring, Drive, Rauschen, DCO2-Tonhöhe, Detune, Vibrato, EG-Tiefe, FM-Verhältnis und -Versatz, Klangfarbe, Pegel, Walsh, Pulsbreite, Sync-Sweep-Tiefen, PWM, Key-Tracking, Velocity→Cutoff, EQ, Stereo-Breite und die Modulations-Slots (Stärke, Ziel, Kanten von Rechteck- und S&H-LFOs). Aus den 7-Bit-Stufen eines Controllers werden Rampen.
- Das Delay schaltet bei jeder Änderung nicht mehr 40 ms stumm (das war beim Original so und die größte Quelle der Knackser): `dgld 1` (Standard) lässt die Zeit gleiten wie bei einem Bandecho, Feedback und Pegel werden geglättet. `dgld 0` = Verhalten des Originals.
- Höhere Auflösung: über NRPN 14 Bit für jede Einstellung; Einstellungen mit mehr als 15 Werten werden stufenlos gesetzt. Schalter, Oktaven und Ziele bleiben in Stufen.
- Gemessen (eine CC-Stufe, Sprung im Ausgang): Delay vorher 0,009–0,024, jetzt 0; Drive 0,0009 → 0,00014; FM 0,00037 → 0,00002; Cutoff/Resonanz 0,00005–0,00008 → 0.

## OLED

Über O2O wie im tx81z-Projekt: OSC `/display-strings-and-numbers <Zeile 1> \n <Zeile 2>` an 127.0.0.1:7562, höchstens alle 40 ms. Zeile 1 ist der Name der Einstellung, Zeile 2 der Wert mit Einheit, Schalterstellung oder Ziel, z. B. `Cutoff` / `50.1`, `FM source` / `other DCO`, `LFO2 target 1` / `cut`. Beim Presetwechsel: `Program` / `25`. Die Texte erzeugt `pdoly8~` selbst (`disp <name>`).

## CC-Tabelle

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

## Audio

Ausgänge 1/2. Audio-Eingang 1 ist die externe FM-Quelle (`fms` = input, CC 82; Pegel `fmig`, CC 83).

## Installation

1. **External:** `bela-oled/pdoly8~.pd_linux` (fertig gebaut: ARMv7 Cortex-A8, NEON, Hard-Float) **flach** nach `/root/Bela/projects/pd-externals/`.
2. **Patch:** den Inhalt von `bela-oled/project/` in ein Bela-Projekt kopieren, z. B. `/root/Bela/projects/pdoly8-oled/`.
3. **O2O** muss wie beim tx81z-Projekt auf dem Bela laufen (Port 7562).
4. Projekteinstellungen: Blocksize 32 (bei zu hoher CPU-Last 64).

```
scp 'bela-oled/pdoly8~.pd_linux' root@bela.local:/root/Bela/projects/pd-externals/
ssh root@bela.local mkdir -p /root/Bela/projects/pdoly8-oled
scp bela-oled/project/* root@bela.local:/root/Bela/projects/pdoly8-oled/
```

## Dateien

- `pdoly8~.pd_linux` — das External für den Bela (`make bela-zig` baut es neu und erzeugt beide Bela-Ordner neu).
- `project/_main.pd` — der Patch: MIDI, CC/NRPN → `n <name> <wert>`, OLED über O2O.
- `project/p8factory.pd` — die 64 Factory-Programme, im Patch gespeichert.
- `../tools/gen_bela_oled.py` — erzeugt `project/` neu; die CC-Tabelle kommt aus `../tools/gen_bela.py` (`CCS`).

## Stand

- Getestet in Pd (libpd 0.56) auf dem Desktop: Preset beim Start, CC, NRPN (14 Bit exakt), Program Change, Noten; die OSC-Nachrichten an O2O mit einem UDP-Empfänger mitgelesen.
- Auf dem Bela noch nicht gelaufen: CPU-Last, MIDI-Ports, Darstellung auf dem OLED.
