# pdoly8~ auf dem Bela Pepper

Minimaler Test-Patch: MIDI über USB, die 8 Pots (und CV) steuern die 8 Controls, Factory-Presets zum schnellen Ausprobieren.

## Bedienung

| | |
|---|---|
| MIDI (USB) | Noten (4 Stimmen), Pitch Bend (`bnd`, Standard ±2 Halbtöne), CC 20–27 = Controls 0–7, Program Change 1–64 = Factory-Preset 0–63 |
| Pots 1–8 | `har1` `har2` `env1` `env2` `intv` `detn` `lev1` `lev2` |
| CV-Eingänge | beim Pepper addiert sich die CV zu ihrem Pot, steuert also dasselbe Control |
| Button 1 / 2 | Factory-Preset runter / hoch (0–63, läuft rund) |
| Start | Factory-Preset 0 (Programm 11) |

Pot 5 (`intv`) und CC 24 werden auf 0–12 Halbtöne umgerechnet. Ein Pot übernimmt erst, wenn er bewegt wird (Totzone 2 Schritte, `p8pot.pd`), damit ein geladenes Preset stehen bleibt, bis man dreht. Die Bela-Konsole zeigt `preset: N` beim Umschalten.

## Installation

1. **External:** `bela/pdoly8~.pd_linux` (fertig gebaut: ARMv7 Cortex-A8, NEON, Hard-Float) **flach** nach `/root/Bela/projects/pd-externals/` kopieren.
   Alternativ auf dem Bela selbst bauen: Repo hinkopieren, `make bela PDINCLUDE=/usr/local/include/libpd` und die entstandene `pdoly8~.pd_linux` dorthin kopieren.
2. **Patch:** den Inhalt von `bela/project/` (mit dem Ordner `presets/`) in ein Bela-Projekt kopieren, z. B. `/root/Bela/projects/pdoly8/`.
3. **Projekteinstellungen:** Blocksize 32 (bei zu hoher CPU-Last 64), USB-MIDI-Port prüfen (`amidi -l`).

```
scp 'bela/pdoly8~.pd_linux' root@bela.local:/root/Bela/projects/pd-externals/
scp -r bela/project root@bela.local:/root/Bela/projects/pdoly8
```

## Dateien

- `pdoly8~.pd_linux` — das External für den Bela (`make bela-zig` baut es von Linux aus neu, mit zig).
- `project/_main.pd` — der Patch; alles über send/receive (`p8-in` an das Objekt, `p8-out` vom Objekt, `p8-preset N` lädt ein Factory-Preset).
- `project/p8pot.pd` — Totzone für Pots und CV.
- `project/p8factory.pd`, `project/presets/poly800mk2-factory.txt` — die 64 Factory-Programme (Kopie aus dem Hauptordner).
- `../tools/gen_bela.py` — erzeugt `project/` neu (Pot-Bereiche, CC-Nummern, Buttons stehen oben im Skript).

## Stand

- Getestet in Pd (libpd 0.56) auf dem Desktop mit simulierten Pots: Preset beim Start, Noten, Preset-Wechsel, Pot 1 → `har1`, Pot 5 → `intv` in Halbtönen.
- Auf dem Bela noch nicht gelaufen: CPU-Last, MIDI-Port und die Pot-Bereiche (aus beladrumrack übernommen, `POT_LO`/`POT_HI`) sind noch zu prüfen.
