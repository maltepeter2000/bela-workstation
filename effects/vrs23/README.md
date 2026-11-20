# vrs23~ – Dynacord VRS 23 in Pd vanilla

Simulation des Dynacord VRS 23 (Echo/Hall, 1981), aufgebaut aus den Baugruppen der
Schaltpläne 80 207 und 80 205. Reines Pd vanilla (getestet mit Pd 0.54), keine Externals.
Steuerung nur über Messages, mit demselben Protokoll wie skandrums.

## Benutzung

Den Ordner in den Pd-Suchpfad legen (oder den Patch daneben speichern) und
`[vrs23~]` anlegen:

- Inlet 0: Audio (mono)
- Inlet 1: Messages, z. B. `mode 1`, `key 3`, `regn 90`
- Outlet 0/1: Audio L/R, Outlet 2: Events (`dump vrs23 ...`, `value ...`)

`vrs23~-help.pd` öffnen: Toggle oben startet den Referenz-Clap (`vrs23-clap.wav`),
die Zahlenboxen ändern die Controls direkt, unten speichert `[skpreset]` Einstellungen.

## Messages

Die vollständige Liste mit Standardwerten steht in der Hilfedatei.

**Frontplatte:** `inp` `regn` `key` `mode` `fine` `swp` `dpth` `tone` `rtrn` `fx` `mix` `vol`

**Klang-Feineinstellungen:** `cmpd` `ctau` `trck` `nois` `drv` `aafc` `aaq` `rkfc` `rkq`
`emph` `clka` `rclk` `rnge` `lfom` `gmax` `rvgm` `fxlv` `bias` `iclp` `cclp`

**Protokoll wie skandrums:** `<control> <0-127>`, `ctl <index> <wert>`, `params v0 v1 ...`,
`defaults`/`reset`, `get <control>` (Antwort `value <control> <v>`), `print`, `dump`
(Antwort `dump vrs23 v0 ...`), `dump vrs23 v0 ...` stellt einen Dump wieder her.

**Presets:** `skpreset.pd` (aus skandrums-pd, CC0) liegt bei. Outlet 2 → `[skpreset name]`
links, `[skpreset]` links → `vrs23~` Inlet 1, Slotnummer 0–15 → `[skpreset]` rechts.
Die Werte liegen in `[text define -k name]` und werden mit dem Patch gespeichert.

## Dateien

| Datei | Baugruppe |
| --- | --- |
| `vrs23~.pd` | Hauptobjekt, Verdrahtung, Regenerations- und Abgriff-Matrix |
| `vrs-clock.pd` | Delay-Kennlinie, zwei Takte (80 207 / 80 205), Sweep-LFO |
| `vrs-bbdw.pd` / `vrs-bbdr.pd` | BBD-Reihe (5 × NE 503), Schreib- und Leseseite |
| `vrs-tap.pd` | ein Abgriff: Verzögerung in Takten statt Millisekunden |
| `vrs-sh.pd` | Taktartefakte (Sample-and-Hold mit f_clk) |
| `vrs-komp.pd` / `vrs-exp.pd` | NE570-Kompressor (rückgekoppelt) und -Expander |
| `vrs-sk3.pd` / `vrs-lp2.pd` | 3 × Sallen-Key-Tiefpass (Anti-Aliasing, Rekonstruktion) |
| `vrs-preemph.pd` / `vrs-deemph.pd` | Pre-Emphasis (I001) und Ausgangs-Shelf (I009) |
| `vrs-tone.pd` | Tone-Netzwerk |
| `skpreset.pd` | Preset-Slots (aus skandrums-pd) |
| `vrs23-clap.wav` | Referenzton für die Hilfedatei |

## Was noch Annahme ist

Diese Punkte sind Parameter, bis die Messung am Gerät sie klärt:

- `rclk`: Takt der oberen Reihe im Reverb (19 oder 30,75 kHz)
- `ctau`: Kompander-Zeitkonstante
- `aaq`, `rkq`: Filtergüten (die Schaltplan-Lesung ergibt +13 dB Überhöhung, vermutlich falsch gelesen)
- `gmax`, `rvgm`: wie weit die Regeneration reicht
- `bias`, `iclp`, `cclp`: Färbung bei hohem Pegel. Standard so abgestimmt, dass bei Nennpegel
  (Sinus 0,3 am Eingang, `inp 64`) etwa 2,4 % Klirr mit H2 und H3 zu gleichen Teilen entstehen;
  das Datenblatt nennt 2 % im Effektweg. Ab etwa +11 dB begrenzt die Eingangsstufe hart.

## Hinweise

- Die Filterkoeffizienten werden beim Laden aus der Samplerate berechnet. Nach einem
  Samplerate-Wechsel den Patch neu laden.
- Delays, Kompander-Detektoren und Regeneration sind über `delwrite~`/`delread~` gekoppelt.
  Kompressor und Expander messen beide mit 64 Samples Verzögerung. Diese Symmetrie
  ist nötig, sonst schaukelt sich die Regeneration auf.
- Kurze Klicks verliert der Kompander stark (wie ein echter NE570 bei τ = 5 ms).
  Für Vergleiche mit dem Gerät deshalb den Sweep auswerten, nicht die Impulse.
- Die Eimerkette rechnet in Takten wie das Original: Jeder Abgriff liegt n × 768 Takte hinter
  dem Eingang. Ändert sich der Takt (`fine`, Sweep-LFO), läuft der ganze Pufferinhalt
  schneller oder langsamer heraus und wird dabei höher oder tiefer. Test: Ein 1-kHz-Ton im
  Puffer kommt nach `fine 0` → `fine 127` mit 2,51 kHz und auf 1/2,51 gestaucht heraus.
- Rechenlast: 30 s Audio rechnen offline in ca. 2,6 s auf einem Desktop-Kern (etwa 12-fache
  Echtzeit). Auf Bela vor dem Einsatz die CPU-Last prüfen.

## Werkzeuge (Ordner `tools`)

- `vrs23_testsignal.wav`: Impulse + Sweep (24 bit) für Messungen am Gerät und an der Simulation.
- `analyse_vrs23.py` + `vrs23_sweep_meta.npz`: wertet eine Aufnahme des Testsignals aus,
  Aufruf `python3 analyse_vrs23.py aufnahme.wav name`.
- `make_testsignal.py`: erzeugt das Testsignal neu.
- `build.py`: erzeugt alle .pd-Dateien in `effects/vrs23` (`python3 tools/build.py`). Achtung: überschreibt Änderungen, die direkt in
  den .pd-Dateien gemacht wurden.
