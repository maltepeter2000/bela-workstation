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

Über O2O wie im tx81z-Projekt: OSC `/display-strings-and-numbers <Zeile 1> \n <Zeile 2>` an 127.0.0.1:7562, höchstens alle 40 ms. Zeile 1 ist der Name der Einstellung, Zeile 2 der Wert mit Einheit, Schalterstellung oder Ziel, z. B. `Cutoff` / `50.1`, `FM source` / `other DCO`, `LFO2 target 1` / `cut`. Beim Presetwechsel: `Program` / `25`.

Gesendet wird von `pdoly8~` selbst aus einem eigenen Thread (`oled 127.0.0.1 7562`, `oledtext <zeile1> <zeile2>`). Der Audio-Thread kopiert nur den Text; Netzwerk-Aufrufe und Konsolen-Ausgaben im Audio-Thread erzwingen auf dem Bela einen Xenomai-Moduswechsel und können Audio-Blöcke kosten, also Klicks beim Drehen. Der Patch selbst enthält deshalb kein `[netsend]` und kein `[print]` mehr.

## CC-Tabelle

Für einen Controller mit 4 Reihen × 16 Pots: 8 Gruppen zu je 8, CC 20–83 durchgehend, kein Parameter doppelt. NRPN (14 Bit) mit denselben Nummern: CC 99 = 0, CC 98 = Nummer, CC 6 / 38 = MSB / LSB.

Achtung, in den Blöcken liegen Nummern, die manche Keyboards selbst senden: **32** (Bank-Select LSB, `kbd`), **38** (`pw1`; direkt nach einem CC 6 gilt er als NRPN-LSB), **64–67** (Pedale: ein Sustain-Pedal stellt `fms` um).

| Reihe | Pots 1–8 | Pots 9–16 |
|---|---|---|
| **1** (CC 20–35) | PLAY | FILTER |
| **2** (CC 36–51) | OSC | WALSH-RING |
| **3** (CC 52–67) | SYNC-FM | MG-FMSOURCE-DEG3 |
| **4** (CC 68–83) | DELAY-EQ | PERFORM |

| Gruppe | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| **PLAY** 20–27 | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| **FILTER** 28–35 | `cut` | `res` | `egi` | `env3` | `kbd` | `vcut` | `drv` | `nois` |
| **OSC** 36–43 | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| **WALSH-RING** 44–51 | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `ring` | `rmf` |
| **SYNC-FM** 52–59 | `sync` | `swp2` | `swe` | `swm` | `fm1` | `fm2` | `fmr` | `fmhz` |
| **MG-FMSOURCE-DEG3** 60–67 | `mgr` | `mgi` | `mgd` | `mgv` | `fms` | `fmig` | `pol` | `trg` |
| **DELAY-EQ** 68–75 | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| **PERFORM** 76–83 | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |

LFO2, LFO3, DEG4 und `drvp`, `tone`, `leak`, `mgf` liegen auf 84–93 und 102–117 (ohne Pot). Nicht per CC: `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4` (über `env1`–`env4` erreichbar).

| CC / NRPN | Parameter | Anzeige (OLED) | Bereich |
|---|---|---|---|
| **PLAY** | | | |
| 20 | `har1` | Timbre 1 | 0 … 127 |
| 21 | `har2` | Timbre 2 | 0 … 127 |
| 22 | `env1` | Env 1 morph | 0 … 127 |
| 23 | `env2` | Env 2 morph | 0 … 127 |
| 24 | `intv` | Interval | 0 … 12 st |
| 25 | `detn` | Detune | 0 … 127 |
| 26 | `lev1` | Level 1 | 0 … 127 |
| 27 | `lev2` | Level 2 | 0 … 127 |
| **FILTER** | | | |
| 28 | `cut` | Cutoff | 0 … 99 |
| 29 | `res` | Resonance | 0 … 15 |
| 30 | `egi` | EG depth | 0 … 15 |
| 31 | `env3` | Env 3 morph | 0 … 127 |
| 32 | `kbd` | Key track | off / half / full |
| 33 | `vcut` | Vel>cutoff | 0 … 127 |
| 34 | `drv` | Drive | 0 … 127 |
| 35 | `nois` | Noise | 0 … 15 |
| **OSC** | | | |
| 36 | `oct1` | Octave 1 | -1 / 0 / +1 / +2 / +3 / +4 |
| 37 | `oct2` | Octave 2 | -1 / 0 / +1 / +2 / +3 / +4 |
| 38 | `pw1` | Pulse width 1 | 0 … 127 |
| 39 | `pw2` | Pulse width 2 | 0 … 127 |
| 40 | `pwm1` | PWM 1 | 0 … 127 |
| 41 | `pwm2` | PWM 2 | 0 … 127 |
| 42 | `hmg` | MG>timbre | 0 … 127 |
| 43 | `drft` | Drift | 0 … 127 |
| **WALSH-RING** | | | |
| 44 | `wsh1` | Walsh pos 1 | 0 … 127 |
| 45 | `wsh2` | Walsh pos 2 | 0 … 127 |
| 46 | `wmx1` | Walsh mix 1 | 0 … 127 |
| 47 | `wmx2` | Walsh mix 2 | 0 … 127 |
| 48 | `wsm` | MG>Walsh | 0 … 127 |
| 49 | `wse` | EG3>Walsh | -127 … 127 |
| 50 | `ring` | Ring mod | 0 … 127 |
| 51 | `rmf` | Ring source | 0 … 14 |
| **SYNC-FM** | | | |
| 52 | `sync` | Sync | off / on |
| 53 | `swp2` | DCO2 pitch | 0 … 72 st |
| 54 | `swe` | EG3>DCO2 | 0 … 72 st |
| 55 | `swm` | MG>DCO2 | 0 … 24 st |
| 56 | `fm1` | FM depth 1 | 0 … 127 |
| 57 | `fm2` | FM depth 2 | 0 … 127 |
| 58 | `fmr` | FM ratio | 0.125f … 16 x |
| 59 | `fmhz` | FM offset | -1000 … 1000 Hz |
| **MG-FMSOURCE-DEG3** | | | |
| 60 | `mgr` | MG rate | 0 … 15 |
| 61 | `mgi` | Vibrato | 0 … 200 ct |
| 62 | `mgd` | MG delay | 0 … 15 |
| 63 | `mgv` | MG>cutoff | 0 … 15 |
| 64 | `fms` | FM source | sine / other DCO / input |
| 65 | `fmig` | FM input gain | 0 … 127 |
| 66 | `pol` | EG polarity | down / up |
| 67 | `trg` | Env 3 trigger | single / multi |
| **DELAY-EQ** | | | |
| 68 | `dtim` | Delay time | 0 … 99 |
| 69 | `dfb` | Delay feedback | 0 … 15 |
| 70 | `dmf` | Delay mod rate | 0 … 31 |
| 71 | `dmi` | Delay mod depth | 0 … 31 |
| 72 | `dlev` | Delay level | 0 … 15 |
| 73 | `dgld` | Delay changes | mute / glide |
| 74 | `treb` | Treble | -6 … 6 |
| 75 | `bass` | Bass | -6 … 6 |
| **PERFORM** | | | |
| 76 | `port` | Portamento | 0 … 127 |
| 77 | `pmod` | Glide mode | last / own / legato |
| 78 | `mono` | Voice mode | poly / mono |
| 79 | `retr` | Mono retrig | legato / retrig |
| 80 | `vamp` | Vel>level | 0 … 127 |
| 81 | `bnd` | Bend range | 0 … 12 st |
| 82 | `sprd` | Stereo spread | 0 … 127 |
| 83 | `mrph` | Smoothing | 0 … 100 ms |
| **ohne Pot** | | | |
| 84 | `env4` | Env 4 morph | 0 … 127 |
| 85 | `trg4` | Env 4 trigger | single / multi |
| 86 | `d4t1` | Env4 target 1 | Ziel 0–32 |
| 87 | `d4a1` | Env4 amount 1 | -127 … 127 |
| 88 | `d4t2` | Env4 target 2 | Ziel 0–32 |
| 89 | `d4a2` | Env4 amount 2 | -127 … 127 |
| 90 | `drvp` | Drive position | post / pre |
| 91 | `tone` | Output stage | off / on |
| 92 | `leak` | Leak | off / on |
| 93 | `mgf` | MG rate | 0.1f … 30 Hz |
| 102 | `l2r` | LFO2 rate | 0.01f … 50 Hz |
| 103 | `l2w` | LFO2 wave | tri / sine / saw up / saw down / square / S&H |
| 104 | `l2k` | LFO2 retrig | free / key / phrase |
| 105 | `l2f` | LFO2 fade | 0 … 10 s |
| 106 | `l2t1` | LFO2 target 1 | Ziel 0–32 |
| 107 | `l2a1` | LFO2 amount 1 | -127 … 127 |
| 108 | `l2t2` | LFO2 target 2 | Ziel 0–32 |
| 109 | `l2a2` | LFO2 amount 2 | -127 … 127 |
| 110 | `l3r` | LFO3 rate | 0.01f … 50 Hz |
| 111 | `l3w` | LFO3 wave | tri / sine / saw up / saw down / square / S&H |
| 112 | `l3k` | LFO3 retrig | free / key / phrase |
| 113 | `l3f` | LFO3 fade | 0 … 10 s |
| 114 | `l3t1` | LFO3 target 1 | Ziel 0–32 |
| 115 | `l3a1` | LFO3 amount 1 | -127 … 127 |
| 116 | `l3t2` | LFO3 target 2 | Ziel 0–32 |
| 117 | `l3a2` | LFO3 amount 2 | -127 … 127 |

## Audio

Ausgänge 1/2. Audio-Eingang 1 ist die externe FM-Quelle (`fms` = input, CC 64; Pegel `fmig`, CC 65).

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
