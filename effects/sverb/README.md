# sverb~ – Signalsmith Reverb als Send-Effekt

Der Reverb aus [Signalsmith-Audio/basics](https://github.com/Signalsmith-Audio/basics) (MIT)
als Pd-Objekt für Mac und Bela Gem. Ein FDN-Hall mit 8 Kanälen, Early Reflections, Dämpfung
und Detune im Feedback. Standard ist der Send-Betrieb: **dry 0, wet 1 (100 % wet)**.

- Inlet 0: Audio links (oder mono) und Messages
- Inlet 1: Audio rechts. **Nicht verbunden: mono.** Das linke Signal geht auf beide Seiten
  des Halls, der Ausgang ist trotzdem stereo (`mono auto|0|1` schaltet um)
- Inlet 2: Messages
- Outlet 0/1: Audio L/R, Outlet 2: Events (`value …`, `dump sverb …`, `display …`)

`sverb~-help.pd` öffnen: Toggle oben spielt alle 2 s den Referenz-Clap (`vrs23-clap.wav`,
dieselbe Datei wie bei vrs23~, liegt in `pd/`), der Bang einmal. Der Clap läuft wie bei
einem Send-Weg trocken auf den Ausgang und in `sverb~` (nur linker Eingang: mono).

`[sverb~ insert]` startet als Insert (dry 1, wet 0,5), `[sverb~ ambient]` mit dem Preset
„ambient“. Alle Messages und Controls stehen in `sverb~-help.pd` und am Anfang von
`pd/sverb~.cpp`. Protokoll wie vrs23~ und skandrums: `<control> <0-127>`, `val <control>
<wert>` in Einheiten, `ctl`, `params`, `get`, `dump`, `defaults`, dazu `preset ambient`,
`clear`, `send`, `insert`, `mono`, `print`.

## Bauen

```
make            # für diesen Rechner nach pd/
make mac-zig    # Universal-Binary und Help nach bin/macos (zig, llvm-lipo)
make bela-gem   # aarch64, glibc 2.31 nach ../../build/bela-gem
make gen        # pd/sverb~-help.pd aus tools/gen_help.py
```

Oben im Repo laufen dieselben Targets für alle Instrumente und Effekte, `make dist` nimmt
sverb~ in den Branch `pd` auf.

## Änderungen an Signalsmith Basics

`third_party/signalsmith` enthält reverb.h und stfx-library.h aus basics (369e906) und die
nötigen Header aus dsp (ca390d2). In `reverb.h` zwei markierte Änderungen (`bela-workstation:`):

- Der Detune-LFO läuft als rotierender Phasor statt `cos`/`sin` pro Sample. Die Ausgabe ist
  im Test (10 s, float) bitgenau gleich, auf x86 rund 17 % schneller, auf dem A53 mehr.
- `reset()` leert auch `delay4` (fehlte upstream, `clear` ließ sonst einen Rest stehen).

`third_party/pd/g_canvas.h` (Pd 0.54) braucht das Objekt, um zu sehen, ob Inlet 1 verbunden
ist.
