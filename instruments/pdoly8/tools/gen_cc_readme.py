#!/usr/bin/env python3
"""Writes the MIDI CC tables (from CC_GROUPS / CC_EXTRA in gen_bela.py) into
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


G = g.CC_GROUPS
over = "| Reihe | Pots 1–8 | Pots 9–16 |\n|---|---|---|\n"
for r in range(4):
    a, b = G[2 * r], G[2 * r + 1]
    over += f"| **{r + 1}** (CC {a[1]}–{b[1] + 7}) | {a[0]} | {b[0]} |\n"
grid = "| Gruppe | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |\n|---|---|---|---|---|---|---|---|---|\n"
for t, c0, ns in G:
    grid += f"| **{t}** {c0}–{c0 + 7} | " + " | ".join(f"`{n}`" for n in ns) + " |\n"
full = "| CC / NRPN | Parameter | Anzeige (OLED) | Bereich |\n|---|---|---|---|\n"
for t, c0, ns in G:
    full += f"| **{t}** | | | |\n"
    for k, n in enumerate(ns):
        full += f"| {c0 + k} | `{n}` | {lab[n][2]} | {rng(n)} |\n"
full += "| **ohne Pot** | | | |\n"
for cc, n in g.CC_EXTRA:
    full += f"| {cc} | `{n}` | {lab[n][2]} | {rng(n)} |\n"
body = f"""Für einen Controller mit 4 Reihen × 16 Pots: 8 Gruppen zu je 8, CC 20–83 durchgehend, kein Parameter doppelt. NRPN (14 Bit) mit denselben Nummern: CC 99 = 0, CC 98 = Nummer, CC 6 / 38 = MSB / LSB.

Achtung, in den Blöcken liegen Nummern, die manche Keyboards selbst senden: **32** (Bank-Select LSB, `kbd`), **38** (`pw1`; direkt nach einem CC 6 gilt er als NRPN-LSB), **64–67** (Pedale: ein Sustain-Pedal stellt `fms` um).

{over}
{grid}
LFO2, LFO3, DEG4 und `drvp`, `tone`, `leak`, `mgf` liegen auf 84–93 und 102–117 (ohne Pot). Nicht per CC: `ch1` und die Einzelwerte der Hüllkurven `a1`…`r4` (über `env1`–`env4` erreichbar).

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
