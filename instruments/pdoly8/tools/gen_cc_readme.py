#!/usr/bin/env python3
"""Writes the MIDI CC tables (CC_ROWS in gen_bela.py, pages of tools/ctlayer.py) into
bela/README.md (section "## MIDI-CC") and bela-oled/README.md ("## CC-Tabelle")."""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gen_bela as g                             # noqa: E402

src = open(os.path.join(ROOT, "pdoly8~.c")).read()
lab = {m[0]: (m[1].strip(), m[2].strip().replace("P8_NT - 1", "32"), m[3], m[4])
       for m in re.findall(r'\{"(\w+)",\'\w\',([^,]+),([^,]+),"([^"]*)","([^"]*)"\}', src)}


def rng(n):
    lo, hi, _, u = lab[n]
    if "|" in u:
        return u.replace("|", " / ")
    if u == "@":
        return "Ziel 0–32"
    return f"{lo} … {hi}" + (f" {u}" if u else "")


import ctlayer as CL                             # noqa: E402

grid = "| Seite | CC | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |\n|---|---|---|---|---|---|---|---|---|---|\n"
for k, (t, ns) in enumerate(g.CC_ROWS):
    cells = [f"`{n}`" for n in ns] + [""] * (8 - len(ns))
    grid += f"| **{k + 1}** {t} | {CL.page_text(k)} | " + " | ".join(cells) + " |\n"
full = "| CC | Parameter | Anzeige (OLED) | Bereich |\n|---|---|---|---|\n"
for k, (t, ns) in enumerate(g.CC_ROWS):
    full += f"| **Seite {k + 1}: {t}** | | | |\n"
    for n in ns:
        full += f"| {g.CC_OF[n]} | `{n}` | {lab[n][2]} | {rng(n)} |\n"
body = f"""Controller-Schicht `[p8cc 1]`: MIDI-Kanal 1 (Argument, 0 = alle Kanäle), dieselben CC-Seiten wie bei allen Instrumenten: Seite k am Launchkey = Reihe k im Editor. CC 20–119 ohne 32, 38, 64–69, 84, 88, 96–101 (Bank-/Data-LSB, Pedale, Portamento Control, High-Res Velocity, NRPN/RPN): 10 volle Seiten und eine mit 4 Plätzen.

Rückmeldung: Änderungen von woanders (Preset, Program Change, Editor) gehen als CC auf demselben Kanal und derselben Nummer an den Controller zurück. Pickup: danach wirkt ein Encoder erst, wenn er den Wert erreicht oder überquert (`pickup 0` schaltet das ab). CC 3 vom Controller auf demselben Kanal fordert alle Werte an (die Launchkey-Firmware schickt ihn vor ihrem ersten CC auf einem neuen Kanal).

{grid}
Nicht per CC (nur Editor oder Message): `l3k`, `l3f`, `l3t2`, `l3a2`, `leak`, `mgf`, `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4`.

{full}
"""
for p, head in [("bela-oled/README.md", "## CC-Tabelle"), ("bela/README.md", "## MIDI-CC")]:
    path = os.path.join(ROOT, p)
    s = open(path).read()
    i = s.index(head)
    j = s.index("\n## ", i + 3)
    s = s[:i] + head + "\n\n" + body + s[j + 1:]
    open(path, "w").write(s)
print("CC tables written")
