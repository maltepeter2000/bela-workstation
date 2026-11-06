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
| MIDI (USB) | Noten (4 Stimmen), Pitch Bend, Program Change 1–64 = Preset 0–63, CC siehe unten (jeder Kanal) |

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

Ein CC setzt den Parameter wie der Pot (gleiche Umrechnung, über `n <name> <0-127>` im External). Danach lesen die Pots ihre Pickup-Ziele neu.

| Seite | CC → Parameter |
|---|---|
| 1 PLAY | 20 `har1`, 21 `har2`, 22 `env1`, 23 `env2`, 24 `intv`, 25 `detn`, 26 `lev1`, 27 `lev2` |
| 2 FILTER | 40 `cut`, 41 `res`, 42 `egi`, 43 `kbd`, 44 `env3`, 45 `vcut`, 46 `drv`, 47 `nois` |
| 3 OSC | 48 `oct1`, 49 `oct2`, 50 `pw1`, 51 `pw2`, 52 `pwm1`, 53 `pwm2`, 54 `hmg`, 55 `drft` |
| 4 WALSH | 56 `wsh1`, 57 `wsh2`, 58 `wmx1`, 59 `wmx2`, 60 `wsm`, 61 `wse`, 26 `lev1`, 27 `lev2` |
| 5 SYNC-RING | 70 `sync`, 71 `swp2`, 72 `swe`, 73 `swm`, 74 `ring`, 75 `rmf`, 26 `lev1`, 27 `lev2` |
| 6 FM | 76 `fm1`, 77 `fm2`, 78 `fmr`, 79 `fmhz`, 80 `fms`, 81 `fmig`, 26 `lev1`, 27 `lev2` |
| 7 MG-DEG4 | 82 `mgr`, 83 `mgi`, 84 `mgd`, 85 `mgv`, 86 `env4`, 87 `trg4`, 88 `d4t1`, 89 `d4a1` |
| 8 LFO2 | 102 `l2r`, 103 `l2w`, 104 `l2k`, 105 `l2f`, 106 `l2t1`, 107 `l2a1`, 108 `l2t2`, 109 `l2a2` |
| 9 LFO3 | 110 `l3r`, 111 `l3w`, 112 `l3k`, 113 `l3f`, 114 `l3t1`, 115 `l3a1`, 116 `l3t2`, 117 `l3a2` |
| 10 FX-PERFORM | 12 `dtim`, 13 `dfb`, 14 `dmf`, 15 `dmi`, 16 `dlev`, 17 `port`, 18 `mono`, 19 `vamp` |
| nur MIDI | 2 `pmod`, 3 `retr`, 4 `pol`, 5 `trg`, 8 `drvp`, 9 `mgf`, 28 `bnd`, 29 `mrph`, 30 `tone`, 31 `leak`, 33 `treb`, 34 `bass`, 35 `sprd`, 36 `d4t2`, 37 `d4a2` |

Ausführlich:

| CC | Parameter | Funktion | Seite / Pot |
|---|---|---|---|
| 2 | `pmod` | Portamento-Modus | nur MIDI |
| 3 | `retr` | Mono-Retrigger / Legato | nur MIDI |
| 4 | `pol` | DEG3-Polarität | nur MIDI |
| 5 | `trg` | DEG3-Trigger | nur MIDI |
| 8 | `drvp` | Overdrive post / pre | nur MIDI |
| 9 | `mgf` | MG-Rate in Hz (log) | nur MIDI |
| 12 | `dtim` | Delay-Zeit | 10 FX-PERFORM / 1 |
| 13 | `dfb` | Delay-Feedback | 10 FX-PERFORM / 2 |
| 14 | `dmf` | Delay-Mod-Rate | 10 FX-PERFORM / 3 |
| 15 | `dmi` | Delay-Mod-Tiefe | 10 FX-PERFORM / 4 |
| 16 | `dlev` | Delay-Pegel | 10 FX-PERFORM / 5 |
| 17 | `port` | Portamento | 10 FX-PERFORM / 6 |
| 18 | `mono` | Poly / Mono | 10 FX-PERFORM / 7 |
| 19 | `vamp` | Velocity → Lautstärke | 10 FX-PERFORM / 8 |
| 20 | `har1` | Klangfarbe DCO1 | 1 PLAY / 1 |
| 21 | `har2` | Klangfarbe DCO2 | 1 PLAY / 2 |
| 22 | `env1` | Hüllkurven-Morph DCO1 | 1 PLAY / 3 |
| 23 | `env2` | Hüllkurven-Morph DCO2 | 1 PLAY / 4 |
| 24 | `intv` | Intervall DCO2 (0–12 Halbtöne) | 1 PLAY / 5 |
| 25 | `detn` | Verstimmung DCO2 | 1 PLAY / 6 |
| 26 | `lev1` | Pegel DCO1 | 1 PLAY / 7 |
| 27 | `lev2` | Pegel DCO2 | 1 PLAY / 8 |
| 28 | `bnd` | Pitch-Bend-Bereich | nur MIDI |
| 29 | `mrph` | Glättung | nur MIDI |
| 30 | `tone` | Ausgangsstufe an / aus | nur MIDI |
| 31 | `leak` | Leak an / aus | nur MIDI |
| 33 | `treb` | EQ Höhen | nur MIDI |
| 34 | `bass` | EQ Bässe | nur MIDI |
| 35 | `sprd` | Stereo-Breite | nur MIDI |
| 36 | `d4t2` | DEG4-Ziel 2 | nur MIDI |
| 37 | `d4a2` | DEG4-Stärke 2 | nur MIDI |
| 40 | `cut` | Cutoff | 2 FILTER / 1 |
| 41 | `res` | Resonanz | 2 FILTER / 2 |
| 42 | `egi` | DEG3 → Cutoff | 2 FILTER / 3 |
| 43 | `kbd` | Keyboard-Tracking (aus / halb / voll) | 2 FILTER / 4 |
| 44 | `env3` | Hüllkurven-Morph DEG3 | 2 FILTER / 5 |
| 45 | `vcut` | Velocity → Cutoff | 2 FILTER / 6 |
| 46 | `drv` | Overdrive | 2 FILTER / 7 |
| 47 | `nois` | Rauschen | 2 FILTER / 8 |
| 48 | `oct1` | Oktave DCO1 (−1…+4) | 3 OSC / 1 |
| 49 | `oct2` | Oktave DCO2 (−1…+4) | 3 OSC / 2 |
| 50 | `pw1` | Pulsbreite DCO1 | 3 OSC / 3 |
| 51 | `pw2` | Pulsbreite DCO2 | 3 OSC / 4 |
| 52 | `pwm1` | MG → Pulsbreite DCO1 | 3 OSC / 5 |
| 53 | `pwm2` | MG → Pulsbreite DCO2 | 3 OSC / 6 |
| 54 | `hmg` | MG → Klangfarbe | 3 OSC / 7 |
| 55 | `drft` | Analog-Drift | 3 OSC / 8 |
| 56 | `wsh1` | Walsh-Position DCO1 | 4 WALSH / 1 |
| 57 | `wsh2` | Walsh-Position DCO2 | 4 WALSH / 2 |
| 58 | `wmx1` | Mix Füße ↔ Walsh DCO1 | 4 WALSH / 3 |
| 59 | `wmx2` | Mix Füße ↔ Walsh DCO2 | 4 WALSH / 4 |
| 60 | `wsm` | MG → Walsh-Position | 4 WALSH / 5 |
| 61 | `wse` | DEG3 → Walsh-Position (Mitte = 0) | 4 WALSH / 6 |
| 70 | `sync` | Hard Sync (aus / an) | 5 SYNC-RING / 1 |
| 71 | `swp2` | Tonhöhe DCO2 (0–72 Halbtöne) | 5 SYNC-RING / 2 |
| 72 | `swe` | DEG3 → DCO2 (Sync-Sweep) | 5 SYNC-RING / 3 |
| 73 | `swm` | MG → DCO2 | 5 SYNC-RING / 4 |
| 74 | `ring` | Ringmodulation | 5 SYNC-RING / 5 |
| 75 | `rmf` | Ringmodulator (Walsh-Stufe 0–14) | 5 SYNC-RING / 6 |
| 76 | `fm1` | FM-Tiefe DCO1 | 6 FM / 1 |
| 77 | `fm2` | FM-Tiefe DCO2 | 6 FM / 2 |
| 78 | `fmr` | FM-Verhältnis (0,125–16, log) | 6 FM / 3 |
| 79 | `fmhz` | FM-Versatz in Hz (Mitte = 0) | 6 FM / 4 |
| 80 | `fms` | FM-Quelle (Sinus / anderer DCO / Eingang) | 6 FM / 5 |
| 81 | `fmig` | Pegel FM-Eingang (Audio In 1) | 6 FM / 6 |
| 82 | `mgr` | MG-Rate (Stufen 0–15) | 7 MG-DEG4 / 1 |
| 83 | `mgi` | MG → Pitch (Vibrato) | 7 MG-DEG4 / 2 |
| 84 | `mgd` | MG-Verzögerung | 7 MG-DEG4 / 3 |
| 85 | `mgv` | MG → Cutoff | 7 MG-DEG4 / 4 |
| 86 | `env4` | Hüllkurven-Morph DEG4 | 7 MG-DEG4 / 5 |
| 87 | `trg4` | DEG4-Trigger (single / multi) | 7 MG-DEG4 / 6 |
| 88 | `d4t1` | DEG4-Ziel 1 | 7 MG-DEG4 / 7 |
| 89 | `d4a1` | DEG4-Stärke 1 (Mitte = 0) | 7 MG-DEG4 / 8 |
| 102 | `l2r` | LFO2-Rate (0,01–50 Hz, log) | 8 LFO2 / 1 |
| 103 | `l2w` | LFO2-Welle (6 Zonen) | 8 LFO2 / 2 |
| 104 | `l2k` | LFO2-Retrigger (frei / Taste / Phrase) | 8 LFO2 / 3 |
| 105 | `l2f` | LFO2-Einblendzeit | 8 LFO2 / 4 |
| 106 | `l2t1` | LFO2-Ziel 1 | 8 LFO2 / 5 |
| 107 | `l2a1` | LFO2-Stärke 1 | 8 LFO2 / 6 |
| 108 | `l2t2` | LFO2-Ziel 2 | 8 LFO2 / 7 |
| 109 | `l2a2` | LFO2-Stärke 2 | 8 LFO2 / 8 |
| 110 | `l3r` | LFO3-Rate | 9 LFO3 / 1 |
| 111 | `l3w` | LFO3-Welle | 9 LFO3 / 2 |
| 112 | `l3k` | LFO3-Retrigger | 9 LFO3 / 3 |
| 113 | `l3f` | LFO3-Einblendzeit | 9 LFO3 / 4 |
| 114 | `l3t1` | LFO3-Ziel 1 | 9 LFO3 / 5 |
| 115 | `l3a1` | LFO3-Stärke 1 | 9 LFO3 / 6 |
| 116 | `l3t2` | LFO3-Ziel 2 | 9 LFO3 / 7 |
| 117 | `l3a2` | LFO3-Stärke 2 | 9 LFO3 / 8 |

Nicht per Pot oder CC: `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4` (über die Morph-Regler `env1`–`env4` erreichbar), per Nachricht natürlich schon.

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
