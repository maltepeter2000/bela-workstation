#!/usr/bin/env python3
"""Drawing helpers for the generated help patches (the layout of the pd-help-layout
skill, taken from gen_help.py of d50~ in pico-vintage-synths): the patch writer, text
wrapping, sections and settings blocks. The help generator sets BUS before drawing.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

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
PINK, YELLOW, BLUE = "#ffcfd6", "#fafaf2", "#1e3791"

BUS = DOLLAR + "0-bus"


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
                        f"{xo} {yo} 0 {fs} {bg} #000000 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, text.replace(" ", SPACE), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, para=False, semi=False):
        if not para:
            assert all(len(l) <= MAXCH for l in lines), [l for l in lines if len(l) > MAXCH]
        elif not semi:
            assert all(";" not in l for l in lines)
        return self.add(f"#X text {x} {y} " +
                        (" " + SEMI + " ").join(esc(l) for l in lines) +
                        f", f {MAXCH + 2};")

    def obj(self, x, y, text):
        return self.add(f"#X obj {x} {y} {text};")

    def msg(self, x, y, text):
        return self.add(f"#X msg {x} {y} {text};")

    def atom(self, x, y, w, lo, hi):
        return self.add(f"#X floatatom {x} {y} {w} {lo} {hi} 0 - - - 0;")


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


def rows_of(groups):
    return [g[k:k + 4] for g in groups for k in range(0, len(g), 4)]


def split_text(groups, paras):
    """Paragraphs go to the group whose parameter name starts them; those
    before the first such paragraph are the block's intro."""
    intro, per = [], [[] for _ in groups]
    cur = None
    for t in paras:
        head = t.split(" (")[0].split(",")[0].strip()
        hit = next((gi for gi, g in enumerate(groups)
                    if any(head == nm for nm, _, _ in g)), None)
        if hit is not None:
            cur = hit
        if cur is None:
            intro.append(t)
        else:
            per[cur].append(t)
    return intro, per


def draw_block(p, title, groups, examples, paras, y):
    """p None = layout only; returns y after the block."""
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    intro, per = split_text(groups, paras)
    if not groups:
        # Messages only: they come first, their text directly below.
        if examples:
            if p:
                sp = p.obj(CX + 400, y, f"s {BUS}")
                for i, txt in enumerate(examples):
                    r, c = divmod(i, 3)
                    m = p.msg(CX + c * 130, y + 34 * r, txt)
                    p.conn(m, 0, sp, 0)
            y += 34 * ((len(examples) + 2) // 3)
        if p:
            p.comment(TX, y, intro, para=True)
        return y + LINE * nlines(intro) + GAP + 10
    if intro:
        if p:
            p.comment(TX, y, intro, para=True)
        y += LINE * nlines(intro) + GAP
    for gi, g in enumerate(groups):
        for row in rows_of([g]):
            if p:
                snd = p.obj(CX + 400, y + 24, f"s {BUS}")
                for c, (nm, lo, hi) in enumerate(row):
                    a = p.atom(CX + c * 100, y, 6, lo, hi)
                    m = p.msg(CX + c * 100, y + 24, f"{nm} {DOLLAR}1")
                    p.conn(a, 0, m, 0)
                    p.conn(m, 0, snd, 0)
            y += 54
        if examples and gi == len(groups) - 1:
            if p:
                sp = p.obj(CX + 400, y, f"s {BUS}")
                for i, txt in enumerate(examples):
                    r, c = divmod(i, 3)
                    m = p.msg(CX + c * 130, y + 34 * r, txt)
                    p.conn(m, 0, sp, 0)
            y += 34 * ((len(examples) + 2) // 3)
        if per[gi]:
            if p:
                p.comment(TX, y, per[gi], para=True)
            y += LINE * nlines(per[gi]) + GAP
    return y + 10


def section(p, y, title, lines, para=False):
    """Heading plus one comment; returns y after it."""
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    if p:
        p.comment(TX, y, lines, para=para)
    y += LINE * (nlines(lines) if para else len(lines)) + GAP
    return y + 10
