#!/usr/bin/env python3
"""Help patches without GUI, the same for every instrument (copy unchanged into each repository).

A help is: title, subtitle, one example ([<inst>.i~ 1] -> [dac~]), then text sections, the
controls row by row as in the editor, credits. Grid and colours as in the skill pd-help-layout:
yellow writing surface, pink strip, blue panel, blue heading squares; font 12.

    import helpdraw as H
    H.write(paths, title=..., subtitle=..., obj="juno.i~", sections=[...], credits=...)

sections: a list of
    ("text", title, paragraphs)        paragraph mode: Pd wraps, one paragraph per line start
    ("lines", title, lines)            line mode: every line at most 62 characters
    ("controls", intro, rows, line)    intro paragraphs, then one heading per editor row:
                                       rows = [(row title, [name, ...]), ...],
                                       line(name) -> "name (label) lo..hi, CC n: text"
"""
from style import PINK, YELLOW, BLUE, INK

BS = chr(92)
SEMI, COMMA, DOLLAR, SPACE = BS + ";", BS + ",", BS + "$", BS + " "
M = 20
YEL_X, YEL_Y, YEL_W = 2 * M, M, 700
PINK_X, PINK_W, PINK_TOP = M, M, YEL_Y + 3 * M
BLUE_W = 200
BLUE_X = YEL_X + YEL_W + M - BLUE_W
BLUE_TOP = PINK_TOP + 3 * M
CX = YEL_X + M
LBL = 24
TX = CX + LBL - 2
LINE = 16
MAXCH = 62
GAP = 4


def esc(s):
    return s.replace(",", " " + COMMA).replace(";", SEMI).replace("$", DOLLAR)


class Patch:
    def __init__(self):
        self.lines, self.conns = [], []

    def add(self, t):
        self.lines.append(t)
        return len(self.lines) - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def cnv(self, x, y, w, h, lbl="empty", xo=20, yo=12, fs=30, bg=YELLOW, sel=19):
        return self.add(f"#X obj {x} {y} cnv {sel} {w} {h} empty empty {lbl} "
                        f"{xo} {yo} 0 {fs} {bg} {INK} 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, text.replace(" ", SPACE), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, para=False, semi=False):
        if not para:
            assert all(len(l) <= MAXCH for l in lines), [l for l in lines if len(l) > MAXCH]
        elif not semi:
            assert all(";" not in l for l in lines), [l for l in lines if ";" in l]
        return self.add(f"#X text {x} {y} " + (" " + SEMI + " ").join(esc(l) for l in lines) +
                        f", f {MAXCH + 2};")

    def obj(self, x, y, text):
        return self.add(f"#X obj {x} {y} {text};")


def wrap(t, w=MAXCH):
    out, cur = [], ""
    for word in t.split():
        if cur and len(cur) + 1 + len(word) > w:
            out.append(cur)
            cur = word
        else:
            cur = (cur + " " + word).strip()
    return out + [cur]


def nlines(paras):
    return sum(len(wrap(t)) for t in paras)


def section(p, y, title, lines, para=False):
    """heading plus one comment; returns y after it"""
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    if p:
        p.comment(TX, y, lines, para=para)
    y += LINE * (nlines(lines) if para else len(lines)) + GAP
    return y + 10


def entry(text):
    """one reference line, continued with an indent of two"""
    first, *rest = wrap(text)
    return [first] + (["  " + r for r in wrap(" ".join(rest), MAXCH - 2)] if rest else [])


def build(p, title, subtitle, obj, sections, credits):
    y = YEL_Y + M
    if p:
        p.heading(CX, y, title, 22)
        p.heading(CX, y + 36, subtitle, 16)
    y += 80
    # the one example, no GUI
    if p:
        ob = p.obj(CX, y, f"{obj} 1")
        p.comment(CX + 130, y, ["<- click it: the editor"])
        da = p.obj(CX, y + 40, "dac~")
        p.conn(ob, 0, da, 0)
        p.conn(ob, 1, da, 1)
    y += 86
    for sec in sections:
        kind = sec[0]
        if kind == "text":
            y = section(p, y, sec[1], sec[2], para=True)
        elif kind == "lines":
            y = section(p, y, sec[1], sec[2])
        elif kind == "controls":
            _, intro, rows, line = sec
            y = section(p, y, "Controls", intro, para=True)
            for rtitle, names in rows:
                y = section(p, y, rtitle, [l for nm in names if nm for l in entry(line(nm))])
        else:
            raise ValueError(kind)
    y += 6
    if p:
        p.comment(TX, y, [credits], para=True, semi=True)
    return y + LINE * nlines([credits]) + M


def write(paths, **kw):
    bottom = build(None, **kw)
    p = Patch()
    p.cnv(YEL_X, YEL_Y, YEL_W, bottom - YEL_Y)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bottom - BLUE_TOP, bg=BLUE)
    assert build(p, **kw) == bottom
    for path in paths:
        with open(path, "w") as f:
            f.write(f"#N canvas 348 20 820 {bottom + M} 12;\n")
            f.write("\n".join(p.lines) + "\n")
            f.write("\n".join(p.conns) + "\n")
        print(f"wrote {path} ({len(p.lines)} objects, height {bottom + M})")
