#!/usr/bin/env python3
"""Generator fuer das Pd-vanilla-Projekt vrs23~ (Dynacord VRS 23 Simulation).

Erzeugt alle .pd-Dateien im Ordner ./vrs23. Patches werden hier erzeugt,
damit Objektnummern und Verbindungen garantiert stimmen.
"""
import os

OUT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # effects/vrs23
os.makedirs(OUT, exist_ok=True)


def esc(s):
    """Text fuer Kommentare/Messages: Komma, Semikolon, Dollar escapen."""
    s = s.replace("\\,", ",").replace("\\;", ";")
    return s.replace("$", "\\$").replace(",", " \\,").replace(";", " \\;")


class Patch:
    def __init__(self, w=900, h=700, font=12):
        self.head = f"#N canvas 40 40 {w} {h} {font};"
        self.items, self.conns, self.n = [], [], 0

    def _add(self, line):
        self.items.append(line)
        i = self.n
        self.n += 1
        return i

    def obj(self, x, y, text):
        return self._add(f"#X obj {x} {y} {text.replace('$', chr(92) + '$')};")

    def msg(self, x, y, text):
        return self._add(f"#X msg {x} {y} {text};")

    def text(self, x, y, text, width=None):
        w = f", f {width}" if width else ""
        return self._add(f"#X text {x} {y} {esc(text)}{w};")

    def sub(self, x, y, name, q, w=900, h=700):
        """q (Patch) als Subpatch [pd name] einbetten."""
        body = [f"#N canvas 60 60 {w} {h} {name} 0;"] + q.items + q.conns + [f"#X restore {x} {y} pd {name};"]
        return self._add("\n".join(body))

    def shift_y(self, above, dy):
        """Alle Objekte ab y >= above um dy verschieben (nur oberste Ebene)."""
        import re
        out = []
        for it in self.items:
            m = re.match(r"#X (obj|msg|text) (\d+) (\d+) ", it)
            if m and "\n" not in it and int(m.group(3)) >= above:
                it = it.replace(f"#X {m.group(1)} {m.group(2)} {m.group(3)} ",
                                f"#X {m.group(1)} {m.group(2)} {int(m.group(3)) + dy} ", 1)
            out.append(it)
        self.items = out

    def c(self, a, ao, b, bi=0):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def save(self, name):
        with open(os.path.join(OUT, name), "w") as f:
            f.write("\n".join([self.head] + self.items + self.conns) + "\n")


E = " \\; "   # Trenner zwischen expr-Ausdruecken
K = " \\, "   # Komma in expr-Funktionen


# ---------------------------------------------------------------- vrs-lp2
def build_lp2():
    p = Patch(520, 300)
    p.text(200, 10, "Tiefpass 2. Ordnung (RBJ) - rechter Eingang: Liste fc Q")
    i_sig = p.obj(10, 10, "inlet~")
    i_ctl = p.obj(200, 40, "inlet")
    t = p.obj(200, 70, "t l b")
    sr = p.obj(300, 100, "samplerate~")
    la = p.obj(200, 130, "list append")
    W = f"(6.2831853*min($f1{K}0.45*$f3)/$f3)"
    A0 = f"(1+sin({W})/(2*$f2))"
    exprs = [f"2*cos({W})/{A0}", f"-(1-sin({W})/(2*$f2))/{A0}",
             f"(1-cos({W}))/2/{A0}", f"(1-cos({W}))/{A0}", f"(1-cos({W}))/2/{A0}"]
    ex = p.obj(200, 160, "expr " + E.join(exprs))
    pk = p.obj(200, 190, "pack f f f f f")
    bq = p.obj(10, 220, "biquad~")
    o = p.obj(10, 250, "outlet~")
    p.c(i_sig, 0, bq, 0); p.c(i_ctl, 0, t)
    p.c(t, 1, sr); p.c(sr, 0, la, 1); p.c(t, 0, la, 0); p.c(la, 0, ex)
    for k in range(5):
        p.c(ex, k, pk, k)
    p.c(pk, 0, bq, 0); p.c(bq, 0, o)
    p.save("vrs-lp2.pd")


# ---------------------------------------------------------------- vrs-sk3
def build_sk3():
    p = Patch(700, 360)
    p.text(10, 10, "3 x Sallen-Key-Tiefpass (Anti-Aliasing oder Rekonstruktion). $2 = aa oder rk")
    p.text(10, 30, "fc-Faktor und Guete-Mischung: 0 = Butterworth 7.5 kHz \\; 1 = Schaltplan-Lesung")
    i = p.obj(10, 60, "inlet~")
    rf = p.obj(200, 60, "r $1-$2-fc")
    rq = p.obj(330, 60, "r $1-$2-q")
    tf = p.obj(200, 90, "t b f")
    tq = p.obj(330, 90, "t b f")
    pk = p.obj(200, 120, "pack 0 f f")
    fs, qs = (8000, 7500, 7000), (0.83, 1.31, 3.43)
    qb = (0.5176, 0.7071, 1.9319)
    ex_list = []
    for k in range(3):
        ex_list.append(f"$f2*(7500+$f3*({fs[k]}-7500))")
        ex_list.append(f"{qb[k]}+$f3*({qs[k]}-{qb[k]})")
    ex = p.obj(200, 150, "expr " + E.join(ex_list))
    packs = [p.obj(200 + 110 * k, 180, "pack f f") for k in range(3)]
    lps = [p.obj(10, 220 + 30 * k, "vrs-lp2") for k in range(3)]
    o = p.obj(10, 320, "outlet~")
    p.c(rf, 0, tf); p.c(rq, 0, tq)
    p.c(tf, 1, pk, 1); p.c(tf, 0, pk, 0); p.c(tq, 1, pk, 2); p.c(tq, 0, pk, 0)
    p.c(pk, 0, ex)
    for k in range(3):
        p.c(ex, 2 * k, packs[k], 0); p.c(ex, 2 * k + 1, packs[k], 1)
        p.c(packs[k], 0, lps[k], 1)
    p.c(i, 0, lps[0]); p.c(lps[0], 0, lps[1]); p.c(lps[1], 0, lps[2]); p.c(lps[2], 0, o)
    p.save("vrs-sk3.pd")


# ---------------------------------------------------------------- vrs-sh
def build_sh():
    p = Patch(520, 300)
    p.text(10, 10, "BBD-Taktartefakte: Sample-and-Hold mit f_clk \\, ueberblendet (clka)")
    i = p.obj(10, 40, "inlet~")
    f = p.obj(150, 40, "inlet~")
    ph = p.obj(150, 70, "phasor~")
    sh = p.obj(10, 100, "samphold~")
    d = p.obj(10, 130, "-~")
    m = p.obj(10, 160, "*~ 0")
    r = p.obj(100, 130, "r $1-clka")
    a = p.obj(10, 190, "+~")
    o = p.obj(10, 220, "outlet~")
    p.c(f, 0, ph); p.c(i, 0, sh, 0); p.c(ph, 0, sh, 1)
    p.c(sh, 0, d, 0); p.c(i, 0, d, 1); p.c(d, 0, m, 0); p.c(r, 0, m, 1)
    p.c(i, 0, a, 0); p.c(m, 0, a, 1); p.c(a, 0, o)
    p.save("vrs-sh.pd")


# ---------------------------------------------------------------- vrs-bbdw / vrs-bbdr
# Schreib- und Leseseite getrennt: Pd sortiert jede Abstraktion als einen Knoten,
# nur so entsteht keine DSP-Schleife ueber die Regeneration.
def build_reihe():
    # Schreibseite: Audio und Taktzaehler (als cos/sin, damit interpoliertes Lesen ueber den
    # Ueberlauf hinweg stimmt) in Delay-Lines schreiben.
    p = Patch(700, 300)
    p.text(10, 10, "BBD-Reihe \\, Schreibseite: Taktartefakte \\, Saettigung \\, Speicher. $2 = o oder u")
    p.text(10, 30, "Eingaenge: Audio \\, f_clk \\, Taktzaehler cos \\, Taktzaehler sin (von vrs-bbdr)")
    i = p.obj(10, 60, "inlet~")
    f = p.obj(200, 60, "inlet~")
    ic = p.obj(330, 60, "inlet~")
    is_ = p.obj(450, 60, "inlet~")
    sh = p.obj(10, 90, "vrs-sh $1")
    sat = p.obj(10, 120, "expr~ (tanh(($v1+$f3)*$f2)-tanh($f3*$f2))/$f2")
    rb = p.obj(300, 90, "r $1-bias")
    rd = p.obj(200, 90, "r $1-drv")
    dw = p.obj(10, 150, "delwrite~ $1-bbd-$2 1000")
    dc = p.obj(330, 150, "delwrite~ $1-tc-$2 1000")
    ds = p.obj(450, 180, "delwrite~ $1-ts-$2 1000")
    dfw = p.obj(200, 210, "delwrite~ $1-tf-$2 1000")
    p.c(f, 0, dfw)
    p.c(i, 0, sh, 0); p.c(f, 0, sh, 1); p.c(sh, 0, sat, 0); p.c(rd, 0, sat, 1); p.c(rb, 0, sat, 2); p.c(sat, 0, dw)
    p.c(ic, 0, dc); p.c(is_, 0, ds)
    p.save("vrs-bbdw.pd")

    # Ein Abgriff: Verzoegerung in TAKTEN (n Chips x 768), nicht in ms.
    # Gesucht ist die Zeit d, in der der Taktzaehler um D = n*768 Takte weitergelaufen ist.
    # Drei Newton-Schritte ueber den gespeicherten Taktzaehler \, Ableitung = Takt zum Schreibzeitpunkt.
    p = Patch(900, 420)
    p.text(10, 10, "Abgriff nach $3 Chips: Verzoegerung = Zeit fuer $3 x 768 Takte (wie in der echten Eimerkette)")
    p.text(10, 30, "Aendert sich der Takt \\, laeuft der ganze Pufferinhalt schneller oder langsamer heraus (Tonhoehe aendert sich)")
    f = p.obj(10, 60, "inlet~")
    c = p.obj(250, 60, "inlet~")
    s_ = p.obj(400, 60, "inlet~")
    D = "( $3 * 768 )"
    d0 = p.obj(10, 90, f"expr~ min({D}*1000/max($v1{K}1000){K}990)")
    p.c(f, 0, d0)
    it = []
    prev = d0
    for k in range(3):
        y = 130 + 90 * k
        rc = p.obj(250, y, "delread4~ $1-tc-$2")
        rs = p.obj(400, y, "delread4~ $1-ts-$2")
        rf = p.obj(550, y, "delread4~ $1-tf-$2")
        ang = f"atan2($v2*$v3-$v1*$v4{K}$v1*$v3+$v2*$v4)"
        nx = p.obj(10, y + 40, f"expr~ max(min($v6+({D}-{ang}*65536/6.2831853)*1000/max($v5{K}1000){K}990){K}0)")
        p.c(prev, 0, rc); p.c(prev, 0, rs)
        p.c(c, 0, nx, 0); p.c(s_, 0, nx, 1); p.c(rc, 0, nx, 2); p.c(rs, 0, nx, 3)
        p.c(rf, 0, nx, 4); p.c(prev, 0, rf); p.c(prev, 0, nx, 5)
        prev = nx
    ra = p.obj(10, 420, "delread4~ $1-bbd-$2")
    o = p.obj(10, 450, "outlet~")
    p.c(prev, 0, ra); p.c(ra, 0, o)
    p.save("vrs-tap.pd")

    # Leseseite: Taktzaehler (phasor~ mit f/65536 -> 0..1 = 0..65536 Takte) und vier Abgriffe.
    p = Patch(820, 360)
    p.text(10, 10, "BBD-Reihe \\, Leseseite: 5 x NE503 (1536 Stufen = 768 Takte je Chip) \\, Eingang f_clk")
    p.text(10, 30, "Ausgaenge: Abgriff nach 1 \\, 2 \\, 4 \\, 5 Chips (13-kHz-RC) \\, Ende roh \\, Taktzaehler cos \\, sin")
    f = p.obj(10, 60, "inlet~")
    fd = p.obj(500, 60, "/~ 65536")
    ph = p.obj(500, 90, "phasor~")
    cc = p.obj(500, 120, "cos~")
    sm = p.obj(600, 120, "-~ 0.25")
    ss = p.obj(600, 150, "cos~")
    p.c(f, 0, fd); p.c(fd, 0, ph); p.c(ph, 0, cc); p.c(ph, 0, sm); p.c(sm, 0, ss)
    taps = []
    for k, n in enumerate((1, 2, 4, 5)):
        t = p.obj(10 + 110 * k, 190, f"vrs-tap $1 $2 {n}")
        p.c(f, 0, t, 0); p.c(cc, 0, t, 1); p.c(ss, 0, t, 2)
        taps.append(t)
    lps = [p.obj(10 + 110 * k, 230, "lop~ 13000") for k in range(4)]
    xs = [10, 120, 230, 340, 450, 560, 670]
    outs = [p.obj(x, 280, "outlet~") for x in xs]
    for k in range(4):
        p.c(taps[k], 0, lps[k]); p.c(lps[k], 0, outs[k])
    p.c(taps[3], 0, outs[4]); p.c(cc, 0, outs[5]); p.c(ss, 0, outs[6])
    p.save("vrs-bbdr.pd")


# ---------------------------------------------------------------- vrs-komp / vrs-exp
def build_komp():
    p = Patch(600, 260)
    p.text(10, 10, "Eingangsstufe I001 (clip~) -> NE570 Kompressor 2:1 rueckgekoppelt -> Output-Clip P11 (tanh)")
    p.text(10, 30, "(Detektor 64 Samples verzoegert \\, wie im Expander). Nennpegel: Mittelwert 0.2 \\, Regelbereich +-30 dB")
    i0 = p.obj(10, 60, "inlet~")
    ric = p.obj(600, 60, "r $1-iclp")
    neg = p.obj(600, 85, "* -1")
    i = p.obj(10, 85, "clip~ -1 1")
    p.c(i0, 0, i); p.c(ric, 0, i, 2); p.c(ric, 0, neg); p.c(neg, 0, i, 1)
    rv = p.obj(200, 60, "delread~ $0-env")
    lb = p.obj(330, 230, "loadbang")
    sr = p.obj(330, 255, "samplerate~")
    dl = p.obj(330, 280, "expr 64000/$f1")
    ex = p.obj(10, 100, f"expr~ $v1*if($f3>0.5{K}min(0.2/max($v2{K}0.0002){K}31.6){K}1)")
    ro = p.obj(400, 60, "r $1-cmpd")
    oc = p.obj(10, 125, "expr~ $f2*tanh($v1/$f2)")
    rcc = p.obj(600, 110, "r $1-cclp")
    o = p.obj(10, 160, "outlet~")
    p.c(rcc, 0, oc, 1)
    ab = p.obj(200, 140, "abs~")
    lp = p.obj(200, 170, "lop~ 30")
    rc = p.obj(300, 140, "r $1-cdet")
    sn = p.obj(200, 200, "delwrite~ $0-env 20")
    p.c(lb, 0, sr); p.c(sr, 0, dl); p.c(dl, 0, rv)
    p.c(i, 0, ex, 0); p.c(rv, 0, ex, 1); p.c(ro, 0, ex, 2); p.c(ex, 0, oc); p.c(oc, 0, o)
    p.c(ex, 0, ab); p.c(ab, 0, lp); p.c(rc, 0, lp, 1); p.c(lp, 0, sn)
    p.save("vrs-komp.pd")


def build_exp():
    p = Patch(640, 260)
    p.text(10, 10, "NE570 Expander 1:2 \\, Detektor sieht das verzoegerte Signal mit Rauschen.")
    p.text(10, 30, "Detektor ueber delwrite~/delread~ um 64 Samples verzoegert \\, genau wie im Kompressor.")
    p.text(10, 50, "Ohne diese Symmetrie gewinnt die Regenerationsschleife Energie.")
    i = p.obj(10, 80, "inlet~")
    ab = p.obj(200, 80, "abs~")
    lp = p.obj(200, 110, "lop~ 30")
    rc = p.obj(300, 80, "r $1-cdet")
    sn = p.obj(200, 140, "delwrite~ $0-env 20")
    rv = p.obj(200, 170, "delread~ $0-env")
    lb = p.obj(350, 110, "loadbang")
    sr = p.obj(350, 135, "samplerate~")
    dl = p.obj(350, 160, "expr 64000/$f1")
    ex = p.obj(10, 210, f"expr~ $v1*if($f3>0.5{K}min(max($v2/0.2{K}0.0316){K}31.6)*$f4{K}1)")
    ro = p.obj(420, 170, "r $1-cmpd")
    rt = p.obj(520, 170, "r $1-trck")
    o = p.obj(10, 250, "outlet~")
    p.c(i, 0, ex, 0); p.c(i, 0, ab); p.c(ab, 0, lp); p.c(rc, 0, lp, 1); p.c(lp, 0, sn)
    p.c(lb, 0, sr); p.c(sr, 0, dl); p.c(dl, 0, rv)
    p.c(rv, 0, ex, 1); p.c(ro, 0, ex, 2); p.c(rt, 0, ex, 3); p.c(ex, 0, o)
    p.save("vrs-exp.pd")


# ---------------------------------------------------------------- vrs-tone / emphasis
def build_tone():
    p = Patch(600, 300)
    p.text(10, 10, "Tone-Netzwerk (22k/10n/47k): Shelf-Tiefpass \\, 0 = dunkel (1.45 kHz)")
    i = p.obj(10, 40, "inlet~")
    lp = p.obj(10, 80, "lop~ 1000")
    mlp = p.obj(10, 110, "*~ 0")
    mdi = p.obj(120, 110, "*~ 0")
    ad = p.obj(10, 140, "+~")
    o = p.obj(10, 170, "outlet~")
    r = p.obj(220, 40, "r $1-tone")
    ex = p.obj(220, 70, f"expr 1/(6.2831853*($f1*47000+11000)*1e-08){E}$f1*47000/($f1*47000+11000)")
    t = p.obj(220, 100, "t f f")
    inv = p.obj(300, 130, "expr 1-$f1")
    p.c(i, 0, lp); p.c(lp, 0, mlp); p.c(i, 0, mdi); p.c(mlp, 0, ad, 0); p.c(mdi, 0, ad, 1)
    p.c(ad, 0, o); p.c(r, 0, ex); p.c(ex, 0, lp, 1); p.c(ex, 1, t)
    p.c(t, 0, mdi, 1); p.c(t, 1, inv); p.c(inv, 0, mlp, 1)
    p.save("vrs-tone.pd")


def build_emph(name, fc, title, e_lp, e_dir):
    p = Patch(600, 260)
    p.text(10, 10, title)
    i = p.obj(10, 40, "inlet~")
    lp = p.obj(10, 70, f"lop~ {fc}")
    mlp = p.obj(10, 100, "*~ 0")
    mdi = p.obj(120, 100, "*~ 0")
    ad = p.obj(10, 130, "+~")
    o = p.obj(10, 160, "outlet~")
    r = p.obj(250, 40, "r $1-emph")
    ex = p.obj(250, 70, f"expr {e_lp}{E}{e_dir}")
    p.c(i, 0, lp); p.c(lp, 0, mlp); p.c(i, 0, mdi); p.c(mlp, 0, ad, 0); p.c(mdi, 0, ad, 1)
    p.c(ad, 0, o); p.c(r, 0, ex); p.c(ex, 0, mlp, 1); p.c(ex, 1, mdi, 1)
    p.save(name)


# ---------------------------------------------------------------- vrs-clock
def build_clock():
    p = Patch(900, 420)
    p.text(10, 10, "Takte: obere Reihe (VCO 80 207) und untere Reihe (Echo synchron \\, Reverb VCO 80 205)")
    names = ["mode", "key", "fine", "rclk", "rnge"]
    rs = [p.obj(10 + 90 * k, 40, f"r $1-{n}") for k, n in enumerate(names)]
    ts = [p.obj(10 + 90 * k, 70, "t b f") for k in range(5)]
    pk = p.obj(10, 100, "pack 0 f f f f f")
    KK = f"pow($f6{K}$f4)"
    top = f"19000*{KK}*if($f2<0.5{K}if($f3<1.5{K}1.6184{K}1){K}if($f5>0.5{K}1.6184{K}1))"
    bot = f"if($f2<0.5{K}{top}{K}30750*{KK})"
    ex = p.obj(10, 130, f"expr {top}{E}{bot}")
    m1 = p.msg(10, 160, "\\$1 50")
    m2 = p.msg(200, 160, "\\$1 50")
    l1 = p.obj(10, 190, "line~")
    l2 = p.obj(200, 190, "line~")
    for k in range(5):
        p.c(rs[k], 0, ts[k]); p.c(ts[k], 1, pk, k + 1); p.c(ts[k], 0, pk, 0)
    p.c(pk, 0, ex); p.c(ex, 0, m1); p.c(ex, 1, m2); p.c(m1, 0, l1); p.c(m2, 0, l2)
    # LFO
    p.text(500, 40, "Sweep-LFO (Dreieck)")
    rsw = p.obj(500, 70, "r $1-swp")
    ph = p.obj(500, 100, "phasor~")
    tri = p.obj(500, 130, "expr~ abs($v1*2-1)*2-1")
    rd = p.obj(650, 70, "r $1-dpth")
    rl = p.obj(750, 70, "r $1-lfom")
    td = p.obj(650, 100, "t b f")
    tl = p.obj(750, 100, "t b f")
    pa = p.obj(650, 130, "pack 0 f f")
    ea = p.obj(650, 160, "expr $f2*$f3")
    ml = p.obj(500, 190, "*~ 0")
    pl = p.obj(500, 220, "+~ 1")
    p.c(rsw, 0, ph); p.c(ph, 0, tri); p.c(tri, 0, ml)
    p.c(rd, 0, td); p.c(rl, 0, tl); p.c(td, 1, pa, 1); p.c(td, 0, pa, 0)
    p.c(tl, 1, pa, 2); p.c(tl, 0, pa, 0); p.c(pa, 0, ea); p.c(ea, 0, ml, 1); p.c(ml, 0, pl)
    f1 = p.obj(10, 260, "*~")
    f2 = p.obj(200, 260, "*~")
    o1 = p.obj(10, 300, "outlet~")
    o2 = p.obj(200, 300, "outlet~")
    p.text(10, 330, "links: f_clk oben \\, rechts: f_clk unten (Hz)")
    p.c(l1, 0, f1, 0); p.c(pl, 0, f1, 1); p.c(l2, 0, f2, 0); p.c(pl, 0, f2, 1)
    p.c(f1, 0, o1); p.c(f2, 0, o2)
    p.save("vrs-clock.pd")


# ---------------------------------------------------------------- Parameter
# name, lo, hi, default, Skalierung (expr ueber $f1) oder None, Beschreibung
PARAMS = [
    ("inp", 0, 127, 64, "pow(10{K}($f1-64)*0.375/20)", "Input Volume: input level -24...+24 dB (64 = 0 dB)"),
    ("regn", 0, 127, 0, "1-pow(1-$f1/127{K}2)", "Regeneration: feedback (dual pot, -log)"),
    ("key", 0, 3, 3, None, "Delay key: 0 = 50 \\, 1 = 100 \\, 2 = 200 \\, 3 = 400 ms (Reverb: E F G H)"),
    ("mode", 0, 1, 0, None, "Mode: 0 = echo \\, 1 = reverb"),
    ("fine", 0, 127, 0, "$f1/127", "Delay fine: 0 = longest time \\, 127 = shortest. Changes the clock: what is in the buffer goes higher/lower as in the original"),
    ("swp", 0, 127, 40, "0.1*pow(100{K}$f1/127)", "Sweep Freq: LFO 0.1...10 Hz"),
    ("dpth", 0, 127, 0, "$f1/127", "Depth: LFO depth on the clock"),
    ("tone", 0, 127, 64, "$f1/127", "Tone: 0 = dark \\, 127 = open"),
    ("rtrn", 0, 127, 100, "$f1/127", "Return: effect level L and R"),
    ("fx", 0, 1, 1, None, "Foot switch: 1 = effect on \\, 0 = input to the effect muted (the tail rings out)"),
    ("mix", 0, 127, 64, "$f1/127", "Mix: 0 = dry only \\, 127 = effect only"),
    ("vol", 0, 127, 100, f"if($f1<1{K}0{K}pow(10{K}($f1-100)*0.5/20))", "Output level: 100 = 0 dB \\, 0 = muted"),
    ("cmpd", 0, 1, 1, None, "Compander NE570 on (1) or off (0)"),
    ("ctau", 0, 127, 55, f"1000/(6.2831853*0.5*pow(200{K}$f1/127))", "Compander time constant 0.5...100 ms (55 = about 5 ms)"),
    ("trck", 0, 127, 64, "pow(10{K}($f1-64)/64*3/20)", "Expander tracking error: +-3 dB (64 = 0)"),
    ("nois", 0, 127, 50, "pow(10{K}(-100+$f1*0.5)/20)*0.3", "BBD noise: -100...-36 dB below nominal level"),
    ("drv", 0, 127, 64, "0.25*pow(16{K}$f1/127)", "BBD saturation: drive (64 = clipping from about +10 dB)"),
    ("aafc", 0, 127, 64, "pow(2{K}($f1-64)/64)", "Anti-aliasing filter: cutoff x0.5...x2"),
    ("aaq", 0, 127, 0, "$f1/127", "Anti-aliasing filter: 0 = Butterworth \\, 127 = as read from the schematic"),
    ("rkfc", 0, 127, 64, "pow(2{K}($f1-64)/64)", "Reconstruction filter: cutoff x0.5...x2"),
    ("rkq", 0, 127, 0, "$f1/127", "Reconstruction filter: 0 = Butterworth \\, 127 = as read from the schematic"),
    ("emph", 0, 127, 127, "$f1/127", "Pre-/de-emphasis: 0 = off \\, 127 = as in the schematic"),
    ("clka", 0, 127, 0, "$f1/127", "Clock artefacts: sample-and-hold at f_clk (aliasing)"),
    ("rclk", 0, 1, 0, None, "Clock of the upper row in reverb: 0 = 19 kHz \\, 1 = 30.75 kHz"),
    ("rnge", 0, 127, 64, "1+3*$f1/127", "Range of delay fine: clock factor max. 1...4 (64 = 2.5)"),
    ("lfom", 0, 127, 64, "0.3*$f1/127", "LFO swing at depth 127: 0...+-30 % of the clock"),
    ("gmax", 0, 127, 100, "1.2*$f1/127", "Maximum regeneration in echo (100 = just below self-oscillation)"),
    ("rvgm", 0, 127, 70, "1.2*$f1/127", "Maximum regeneration in reverb (70 = tail of about 5 s at full turn \\, self-oscillation from about 90)"),
    ("fxlv", 0, 127, 64, "pow(10{K}($f1-64)/64*12/20)", "Effect level trim: +-12 dB"),
    ("bias", 0, 127, 78, "($f1-64)/64*0.6", "BBD bias (P17/P18): 64 = symmetric \\, 78 = slightly asymmetric \\, else even harmonics (H2)"),
    ("iclp", 0, 127, 58, "0.3*pow(10{K}$f1*24/127/20)", "Input stage I001: clipping headroom 0...24 dB above nominal level (58 = about 11 dB)"),
    ("cclp", 0, 127, 54, "0.3*pow(10{K}$f1*24/127/20)", "Compressor output (P11): soft limit 0...24 dB above nominal level"),
]
SEND = {"ctau": "cdet", "aafc": "aa-fc", "aaq": "aa-q", "rkfc": "rk-fc", "rkq": "rk-q"}


def build_main():
    p = Patch(1500, 1100)
    N = len(PARAMS)
    p.text(10, 10, "vrs23~ - Dynacord VRS 23 Echo/Hall \\, Pd vanilla. Eingaenge: Audio \\, Messages. Ausgaenge: L \\, R \\, Events")

    # ---------------- Steuerung (Subpatch pd messages), Protokoll wie skandrums
    q = Patch()
    q.text(10, 10, "Messages wie skandrums: <name> <0-127> \\, params v0 v1 ... \\, ctl <index> <wert> \\, dump \\, dump vrs23 v0 ... \\, defaults \\, reset \\, print \\, get <name>")
    qin = q.obj(10, 40, "inlet")
    lb = q.obj(250, 40, "loadbang")
    dflt = q.msg(250, 65, "params " + " ".join(str(d) for _, _, _, d, _, _ in PARAMS))
    names = [n for n, *_ in PARAMS]
    rt = q.obj(10, 95, "route " + " ".join(names) + " params ctl dump defaults reset print get")
    q.c(qin, 0, rt); q.c(lb, 0, dflt); q.c(dflt, 0, rt)
    iP, iC, iD, iDef, iRes, iPr, iG = range(N, N + 7)
    up = q.obj(10, 125, "unpack " + " ".join("f" for _ in PARAMS))
    rc = q.obj(10, 150, "route " + " ".join(str(k) for k in range(N)))
    q.c(rt, iP, up); q.c(rt, iC, rc); q.c(rt, iDef, dflt); q.c(rt, iRes, dflt)
    for k, (n, lo, hi, d, sc, desc) in enumerate(PARAMS):
        x = 10 + 70 * (k % 15)
        y = 190 + (k // 15) * 150 + (k % 2) * 8
        cl = q.obj(x, y, f"clip {lo} {hi}")
        t = q.obj(x, y + 25, "t f f")
        v = q.obj(x + 25, y + 50, f"v $0-st-{n}")
        s_ = q.obj(x, y + 100, f"s $0-{SEND.get(n, n)}")
        q.c(rt, k, cl); q.c(up, k, cl); q.c(rc, k, cl); q.c(cl, 0, t); q.c(t, 1, v)
        if sc:
            ex = q.obj(x, y + 75, "expr " + sc.replace("{K}", K))
            q.c(t, 0, ex); q.c(ex, 0, s_)
        else:
            q.c(t, 0, s_)
    # dump / dump vrs23 ...
    dy = 520
    q.text(10, dy, "dump: alle Werte als 'dump vrs23 v0 ...' an den Event-Ausgang \\; 'dump vrs23 v0 ...' stellt sie wieder her")
    rd = q.obj(10, dy + 25, "route bang vrs23")
    q.c(rt, iD, rd); q.c(rd, 1, up)
    tr = q.obj(10, dy + 55, "t " + " ".join("b" for _ in range(N + 2)))
    q.c(rd, 0, tr)
    lpre = q.obj(10, dy + 170, "list prepend")
    tll = q.obj(10, dy + 195, "t l l")
    lacc = q.obj(150, dy + 220, "list append")
    clr = q.msg(800, dy + 90, "bang")
    q.c(tr, N + 1, clr); q.c(clr, 0, lpre, 1); q.c(clr, 0, lacc, 1)
    for k, n in enumerate(names):
        vv = q.obj(10 + 70 * (k % 10), dy + 90 + (k // 10) * 22, f"v $0-st-{n}")
        q.c(tr, N - k, vv); q.c(vv, 0, lpre, 0)
    q.c(lpre, 0, tll); q.c(tll, 1, lpre, 1); q.c(tll, 0, lacc, 1); q.c(tr, 0, lacc, 0)
    spd = q.obj(150, dy + 250, "spigot 1")
    lv = q.obj(150, dy + 275, "list prepend dump vrs23")
    ltr = q.obj(150, dy + 300, "list trim")
    qout = q.obj(150, dy + 360, "outlet")
    q.c(lacc, 0, spd); q.c(spd, 0, lv); q.c(lv, 0, ltr); q.c(ltr, 0, qout)
    # print: dieselbe Werteliste, aber ins Pd-Fenster statt an den Ausgang
    spp = q.obj(450, dy + 250, "spigot 0")
    tmpl = q.msg(450, dy + 275, "controls " + " ".join(f"{n} \\${k + 1}" for k, n in enumerate(names)))
    prn = q.obj(450, dy + 300, "print vrs23~")
    q.c(lacc, 0, spp); q.c(spp, 0, tmpl); q.c(tmpl, 0, prn)
    tp = q.obj(600, dy + 25, "t b b b b")
    on = q.msg(600, dy + 55, "0")
    off = q.msg(650, dy + 55, "1")
    srb = q.obj(700, dy + 55, "samplerate~")
    srp0 = q.obj(700, dy + 80, "list prepend samplerate")
    srp = q.obj(700, dy + 105, "list trim")
    q.c(srp0, 0, srp)
    q.c(rt, iPr, tp)
    q.c(tp, 3, on); q.c(on, 0, spd, 1); q.c(tp, 3, off); q.c(off, 0, spp, 1)
    q.c(tp, 2, tr)
    on2 = q.msg(750, dy + 55, "1")
    off2 = q.msg(800, dy + 55, "0")
    q.c(tp, 1, on2); q.c(on2, 0, spd, 1); q.c(tp, 1, off2); q.c(off2, 0, spp, 1)
    q.c(tp, 0, srb); q.c(srb, 0, srp0); q.c(srp, 0, prn)
    # get <name>: 'value <name> <wert>' an den Event-Ausgang
    gy = dy + 400
    q.text(10, gy, "get <name>: antwortet mit 'value <name> <wert>'")
    rg = q.obj(10, gy + 25, "route " + " ".join(names))
    q.c(rt, iG, rg)
    for k, n in enumerate(names):
        x = 10 + 90 * (k % 12)
        y = gy + 55 + (k // 12) * 60
        gv = q.obj(x, y, f"v $0-st-{n}")
        gp = q.obj(x, y + 25, f"list prepend value {n}")
        q.c(rg, k, gv); q.c(gv, 0, gp); q.c(gp, 0, ltr)

    ictl = p.obj(1100, 40, "inlet")
    msub = p.sub(1100, 70, "messages", q, 1150, 1150)
    odump = p.obj(1100, 100, "outlet")
    p.c(ictl, 0, msub); p.c(msub, 0, odump)

    # ---------------- Hilfsfunktion: Parameter -> line~
    def lin(x, y, recv, ms=20):
        r = p.obj(x, y, f"r $0-{recv}")
        m = p.msg(x, y + 22, f"\\$1 {ms}")
        l = p.obj(x, y + 44, "line~")
        p.c(r, 0, m); p.c(m, 0, l)
        return l

    def mul(x, y, sig, gain_sig):
        m = p.obj(x, y, "*~")
        p.c(sig, 0, m, 0); p.c(gain_sig, 0, m, 1)
        return m

    def add(x, y, a, b):
        s = p.obj(x, y, "+~")
        p.c(a, 0, s, 0); p.c(b, 0, s, 1)
        return s

    # ---------------- Engine
    X0, Y0 = 10, 800
    p.text(X0, Y0 - 25, "Signalweg: Eingang -> Pre-Emphasis -> E1/E2 -> Kompressor -> AA -> BBD -> Matrix -> Rek -> Expander -> A1/A2")
    ia = p.obj(X0, Y0, "inlet~")
    g_in = lin(X0 + 100, Y0, "inp")
    xin = mul(X0, Y0 + 70, ia, g_in)
    g_fx = lin(X0 + 100, Y0 + 90, "fx", 30)
    xg = mul(X0, Y0 + 160, xin, g_fx)
    pre = p.obj(X0, Y0 + 190, "vrs-preemph $0")
    p.c(xg, 0, pre)
    neg = p.obj(X0, Y0 + 220, "*~ -1")
    p.c(pre, 0, neg)

    # Matrix-Gewichte
    WX = 300
    rmo = p.obj(WX, 40 + 760, "r $0-mode")
    rke = p.obj(WX + 90, 40 + 760, "r $0-key")
    tmo = p.obj(WX, 65 + 760, "t b f")
    tke = p.obj(WX + 90, 65 + 760, "t b f")
    pw = p.obj(WX, 90 + 760, "pack 0 f f")
    p.c(rmo, 0, tmo); p.c(rke, 0, tke); p.c(tmo, 1, pw, 1); p.c(tmo, 0, pw, 0)
    p.c(tke, 1, pw, 2); p.c(tke, 0, pw, 0)
    ech, rev = "($f2<0.5)", "($f2>0.5)"
    w1 = ["A", "B", "C", "D", "J", "ser", "L1"]
    e1 = [rev, f"{ech}*($f3==0)", f"{ech}*($f3==1)", f"{ech}*($f3==2)", f"{ech}*($f3==3)", ech, ech]
    w2 = ["E", "F", "G", "H", "rev", "L2"]
    e2 = [f"{rev}*($f3==0)", f"{rev}*($f3==1)", f"{rev}*($f3==2)", f"{rev}*($f3==3)", rev, rev]
    W = {}
    for row, (names, exs) in enumerate(((w1, e1), (w2, e2))):
        ex = p.obj(WX, 120 + 760 + row * 100, "expr " + E.join(exs))
        p.c(pw, 0, ex)
        for k, nm in enumerate(names):
            m = p.msg(WX + 60 * k, 145 + 760 + row * 100, "\\$1 30")
            l = p.obj(WX + 60 * k, 170 + 760 + row * 100, "line~")
            p.c(ex, k, m); p.c(m, 0, l)
            W[nm] = l

    # Regeneration-Koeffizienten
    RX = 760
    rr = [p.obj(RX + 90 * k, 800, f"r $0-{n}") for k, n in enumerate(("mode", "regn", "gmax", "rvgm"))]
    tt = [p.obj(RX + 90 * k, 825, "t b f") for k in range(4)]
    pr = p.obj(RX, 850, "pack 0 f f f f")
    for k in range(4):
        p.c(rr[k], 0, tt[k]); p.c(tt[k], 1, pr, k + 1); p.c(tt[k], 0, pr, 0)
    g = f"($f3*if($f2<0.5{K}$f4{K}$f5))"
    # Reverb: R023 (A1) und R039 (A2) je 10k vor dem Tandempoti -> Faktor 0.82 auf allen g-Pfaden
    ce = [f"if($f2<0.5{K}-{g}{K}-0.82*{g})", f"if($f2<0.5{K}0{K}0.82*{g})",
          f"if($f2<0.5{K}0{K}-1)", f"if($f2<0.5{K}0{K}-0.82*{g})"]
    exr = p.obj(RX, 875, "expr " + E.join(ce))
    p.c(pr, 0, exr)
    C = {}
    for k, nm in enumerate(("c11", "c12", "c21", "c22")):
        m = p.msg(RX + 70 * k, 900, "\\$1 30")
        l = p.obj(RX + 70 * k, 925, "line~")
        p.c(exr, k, m); p.c(m, 0, l)
        C[nm] = l

    # Takte
    clk = p.obj(1100, 800, "vrs-clock $0")

    # A1/A2 werden weiter unten erzeugt; Platzhalter-Objekte jetzt anlegen
    BY = 1000
    # Laufwerk-Ausgaenge (Expander) - spaeter verbunden
    exp1 = p.obj(700, BY + 330, "vrs-exp $0")
    exp2 = p.obj(1000, BY + 330, "vrs-exp $0")

    # E1 = -pre + c11*A1 + c12*A2
    a1c11 = mul(X0, BY, exp1, C["c11"])
    a2c12 = mul(X0 + 60, BY, exp2, C["c12"])
    e1a = add(X0, BY + 30, neg, a1c11)
    e1 = add(X0, BY + 60, e1a, a2c12)
    # E2 = c21*A1 + c22*A2
    a1c21 = mul(X0 + 200, BY, exp1, C["c21"])
    a2c22 = mul(X0 + 260, BY, exp2, C["c22"])
    e2 = add(X0 + 200, BY + 30, a1c21, a2c22)

    k1 = p.obj(X0, BY + 90, "vrs-komp $0")
    k2 = p.obj(X0 + 200, BY + 90, "vrs-komp $0")
    aa1 = p.obj(X0, BY + 120, "vrs-sk3 $0 aa")
    aa2 = p.obj(X0 + 200, BY + 120, "vrs-sk3 $0 aa")
    p.c(e1, 0, k1); p.c(e2, 0, k2); p.c(k1, 0, aa1); p.c(k2, 0, aa2)

    row = p.obj(X0, BY + 150, "vrs-bbdw $0 o")
    p.c(aa1, 0, row, 0); p.c(clk, 0, row, 1)
    ro = p.obj(X0 + 400, BY + 150, "vrs-bbdr $0 o")
    p.c(clk, 0, ro, 0); p.c(ro, 5, row, 2); p.c(ro, 6, row, 3)
    # Eingang untere Reihe: Echo = Ende oben, Reverb = Kompressor 2
    ser = mul(X0 + 200, BY + 150, ro, W["ser"])
    p.c(ro, 4, ser, 0)  # Ende roh (Ausgang 4)
    # Korrektur: mul() verbindet Ausgang 0 - die richtige Verbindung ist Ausgang 4
    p.conns.remove(f"#X connect {ro} 0 {ser} 0;")
    rv = mul(X0 + 260, BY + 150, aa2, W["rev"])
    uin = add(X0 + 200, BY + 180, ser, rv)
    ruw = p.obj(X0 + 200, BY + 210, "vrs-bbdw $0 u")
    p.c(uin, 0, ruw, 0); p.c(clk, 1, ruw, 1)
    ru = p.obj(X0 + 600, BY + 150, "vrs-bbdr $0 u")
    p.c(clk, 1, ru, 0); p.c(ru, 5, ruw, 2); p.c(ru, 6, ruw, 3)

    # Abgriff-Matrix
    def tap(x, y, src, outlet, w):
        m = p.obj(x, y, "*~")
        p.c(src, outlet, m, 0); p.c(w, 0, m, 1)
        return m
    tA = tap(500, BY + 240, ro, 0, W["A"])
    tB = tap(540, BY + 240, ro, 1, W["B"])
    tC = tap(580, BY + 240, ro, 2, W["C"])
    tD = tap(620, BY + 240, ro, 3, W["D"])
    tJ = tap(660, BY + 240, ru, 3, W["J"])
    tE = tap(800, BY + 240, ru, 0, W["E"])
    tF = tap(840, BY + 240, ru, 1, W["F"])
    tG = tap(880, BY + 240, ru, 2, W["G"])
    tH = tap(920, BY + 240, ru, 3, W["H"])
    s1 = add(500, BY + 265, tA, tB); s1 = add(500, BY + 285, s1, tC)
    s1 = add(500, BY + 305, s1, tD); lw1 = add(500, BY + 325, s1, tJ)
    s2 = add(800, BY + 265, tE, tF); s2 = add(800, BY + 285, s2, tG)
    lw2 = add(800, BY + 305, s2, tH)

    # Rauschen, Taktartefakte, Rekonstruktion, Expander
    rn = p.obj(1150, BY + 240, "r $0-nois")
    n1 = p.obj(1150, BY + 265, "noise~")
    n2 = p.obj(1250, BY + 265, "noise~")
    nm1 = p.obj(1150, BY + 290, "*~ 0")
    nm2 = p.obj(1250, BY + 290, "*~ 0")
    p.c(rn, 0, nm1, 1); p.c(rn, 0, nm2, 1); p.c(n1, 0, nm1); p.c(n2, 0, nm2)
    lw1n = add(500, BY + 350, lw1, nm1)
    lw2n = add(800, BY + 350, lw2, nm2)
    sh1 = p.obj(500, BY + 375, "vrs-sh $0")
    sh2 = p.obj(800, BY + 375, "vrs-sh $0")
    p.c(lw1n, 0, sh1, 0); p.c(clk, 0, sh1, 1); p.c(lw2n, 0, sh2, 0); p.c(clk, 1, sh2, 1)
    rk1 = p.obj(500, BY + 400, "vrs-sk3 $0 rk")
    rk2 = p.obj(800, BY + 400, "vrs-sk3 $0 rk")
    p.c(sh1, 0, rk1); p.c(sh2, 0, rk2); p.c(rk1, 0, exp1); p.c(rk2, 0, exp2)

    # Ausgang: L = Echo A1 / Reverb A2, R = A1
    OX, OY = 10, BY + 470
    p.text(OX, OY - 22, "Ausgang: Effect L = A1 (Echo) bzw. A2 (Reverb) \\, Effect R = A1")
    wl1 = mul(OX, OY, exp1, W["L1"])
    wl2 = mul(OX + 60, OY, exp2, W["L2"])
    wetl = add(OX, OY + 25, wl1, wl2)
    ch = {}
    for side, (x, src) in {"L": (OX, wetl), "R": (OX + 300, exp1)}.items():
        tn = p.obj(x, OY + 55, "vrs-tone $0")
        p.c(src, 0, tn)
        g_r = lin(x + 100, OY + 55, "rtrn")
        ret = mul(x, OY + 125, tn, g_r)
        de = p.obj(x, OY + 150, "vrs-deemph $0")
        p.c(ret, 0, de)
        g_l = lin(x + 100, OY + 150, "fxlv")
        ch[side] = mul(x, OY + 220, de, g_l)
    rmix = p.obj(700, OY + 55, "r $0-mix")
    emix = p.obj(700, OY + 80, f"expr $f1{E}1-$f1")
    mw = p.msg(700, OY + 105, "\\$1 30")
    md = p.msg(780, OY + 105, "\\$1 30")
    lw = p.obj(700, OY + 130, "line~")
    ld = p.obj(780, OY + 130, "line~")
    p.c(rmix, 0, emix); p.c(emix, 0, mw); p.c(emix, 1, md); p.c(mw, 0, lw); p.c(md, 0, ld)
    dry = mul(OX + 600, OY + 250, xin, ld)
    g_v = lin(OX + 900, OY + 220, "vol")
    for k, side in enumerate(("L", "R")):
        x = OX + 300 * k
        wet = mul(x, OY + 250, ch[side], lw)
        s = add(x, OY + 275, wet, dry)
        v = mul(x, OY + 300, s, g_v)
        o = p.obj(x, OY + 330, "outlet~")
        p.c(v, 0, o)
    p.shift_y(700, -650)
    p.save("vrs23~.pd")


# ---------------------------------------------------------------- Hilfe (Layout wie skandrums)
import textwrap

CNV = "#X obj {x} {y} cnv 19 {w} {h} empty empty empty 20 12 0 30 {c} #000000 0;"


def label(p, x, y, text, size, color="#1e3791"):
    t = text.replace(" ", "\\ ")
    return p._add(f"#X obj {x} {y} cnv 10 10 10 empty empty {t} 24 6 0 {size} {color} #000000 0;")


def htext(p, x, y, lines, width=64):
    """Kommentar mit Zeilenumbruechen (Semikolon) wie in den skandrums-Hilfen."""
    clean = [l.replace("\\,", ",").replace("\\;", "") for l in lines]
    body = " \\; ".join(l.replace(",", " \\,").replace("$", "\\$") for l in clean)
    return p._add(f"#X text {x} {y} {body}, f {width};")


def wrap(first, rest, width=58):
    out = textwrap.wrap(first + " " + rest, width)
    return out


def build_help():
    p = Patch(800, 100)
    items_bg = []
    # Hintergrund wird am Ende mit der richtigen Hoehe eingesetzt
    bg = [p._add("BG0"), p._add("BG1"), p._add("BG2")]
    label(p, 60, 40, "vrs23~", 22)
    label(p, 60, 80, "Dynacord VRS 23 echo / reverb (1981) - bucket brigade simulation", 16)

    # --- Demo oben: Toggle -> metro -> Clap
    tg = p._add("#X obj 60 132 tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1;")
    mt = p.obj(60, 160, "metro 2000")
    bn = p._add("#X obj 66 198 bng 19 250 50 0 empty empty empty 17 7 0 10 #fcfcfc #000000 #000000;")
    sclap = p.obj(60, 228, "s $0-clap")
    p.c(tg, 0, mt); p.c(mt, 0, sclap); p.c(bn, 0, sclap)
    sv = p.obj(160, 228, "s $0-vrs23")
    for x, m in ((160, "dump"), (208, "defaults"), (284, "print")):
        mm = p.msg(x, 198, m)
        p.c(mm, 0, sv)
    p.text(340, 198, "<- play the clap")
    # Echo/Reverb-Schnellwahl
    for x, m in ((160, "mode 0"), (225, "mode 1")):
        mm = p.msg(x, 160, m)
        p.c(mm, 0, sv)
    p.text(290, 160, "Echo / Reverb")

    # --- Zahlenboxen -> Messages
    y = 270
    rows = []
    groups = [("Front panel", PARAMS[:12]), ("Sound", PARAMS[12:])]
    for title, plist in groups:
        label(p, 60, y, title, 14)
        y += 30
        for r in range(0, len(plist), 4):
            chunk = plist[r:r + 4]
            for k, (n, lo, hi, d, sc, desc) in enumerate(chunk):
                x = 60 + 112 * k
                fa = p._add(f"#X floatatom {x} {y} 4 {lo} {hi} 0 - \\$0-nb-{n} -;")
                mm = p.msg(x, y + 30, f"{n} \\$1")
                p.c(fa, 0, mm)
                rows.append((mm, y))
            s_ = p.obj(60, y + 58, "s $0-vrs23")
            for mm, yy in rows[-len(chunk):]:
                p.c(mm, 0, s_)
            y += 90
    # --- Objekt, Clap-Player, Ausgang
    y += 10
    rv = p.obj(150, y + 340, "r $0-vrs23")
    p.text(230, y + 20, "reference: vrs23-clap.wav")
    rcl = p.obj(150, y + 234, "r $0-clap")
    lbc = p.obj(230, y + 52, "loadbang")
    f0 = p.obj(310, y + 52, "f $0")
    rdm = p.msg(230, y + 78, "read -resize vrs23-clap.wav \\$1-clap")
    sf = p.obj(230, y + 104, "soundfiler")
    arr = p.obj(330, y + 260, "array define $0-clap")
    info = p.obj(310, y + 130, "unpack f")
    pk = p.obj(230, y + 156, "pack f f")
    ex = p.obj(230, y + 182, "expr $f1 \\; $f1/$f2*1000")
    pk2 = p.obj(230, y + 208, "pack f f")
    p.c(lbc, 0, f0); p.c(f0, 0, rdm); p.c(rdm, 0, sf); p.c(sf, 0, pk); p.c(sf, 1, info); p.c(info, 0, pk, 1)
    p.c(pk, 0, ex); p.c(ex, 0, pk2); p.c(ex, 1, pk2, 1)
    vl = p.obj(250, y + 260, "list")
    p.c(pk2, 0, vl, 1); p.c(rcl, 0, vl, 0)
    vm2 = p.msg(250, y + 286, "0 \\, \\$1 \\$2")
    vli = p.obj(250, y + 312, "vline~")
    tr4 = p.obj(250, y + 338, "tabread4~ $0-clap")
    p.c(vl, 0, vm2); p.c(vm2, 0, vli); p.c(vli, 0, tr4)
    obj = p.obj(60, y + 370, "vrs23~")
    p.c(rv, 0, obj, 1); p.c(tr4, 0, obj, 0)
    mL = p.obj(60, y + 406, "*~ 0.3")
    mR = p.obj(130, y + 406, "*~ 0.3")
    dac = p.obj(60, y + 442, "dac~")
    p.c(obj, 0, mL); p.c(obj, 1, mR); p.c(mL, 0, dac, 0); p.c(mR, 0, dac, 1)
    prt = p.obj(390, y + 406, "print VRS23")
    # Anzeige: Werte in die Zahlenboxen zurueckschreiben
    an = Patch()
    a_in = an.obj(10, 10, "inlet")
    a_in2 = an.obj(250, 10, "inlet")
    a_r = an.obj(10, 40, "route dump")
    a_r2 = an.obj(10, 70, "route vrs23")
    a_up = an.obj(10, 110, "unpack " + " ".join("f" for _ in PARAMS))
    an.c(a_in, 0, a_r); an.c(a_r, 0, a_r2); an.c(a_r2, 0, a_up); an.c(a_in2, 0, a_up)
    for k, (n, *_r) in enumerate(PARAMS):
        x = 10 + 80 * (k % 10)
        yy = 150 + (k // 10) * 60
        sm = an.msg(x, yy, "set \\$1")
        ss = an.obj(x, yy + 25, f"s $0-nb-{n}")
        an.c(a_up, k, sm); an.c(sm, 0, ss)
    an.text(10, 360, "Shows the current values in the number boxes (event outlet and skpreset)")
    anz = p.sub(250, y + 442, "numbers", an, 850, 400)
    p.c(obj, 2, prt); p.c(obj, 2, anz, 0)
    lbd = p.obj(250, y + 406, "loadbang")
    dmp = p.msg(330, y + 406, "dump")
    sdm = p.obj(330, y + 436, "s $0-vrs23")
    p.c(lbd, 0, dmp); p.c(dmp, 0, sdm)   # fill the number boxes at load

    # --- Presets (skpreset)
    y += 500
    label(p, 60, y, "Presets and steps", 14)
    hr = p._add(f"#X obj 60 {y + 34} hradio 19 1 0 16 empty empty empty 0 -8 0 10 #fcfcfc #000000 #000000 0;")
    skp = p.obj(60, y + 70, "skpreset vrs23-kit")
    td = p.obj(300, y + 70, "text define -k vrs23-kit")
    ssk = p.obj(60, y + 100, "s $0-vrs23")
    p.c(hr, 0, skp, 1); p.c(obj, 2, skp, 0); p.c(skp, 0, ssk); p.c(skp, 1, anz, 1)
    yt = y + 136
    htext(p, 82, yt, [
        "Choose slot 0-15: the current slot is stored \\, the new one",
        "loaded. An empty slot takes the current sound.",
        "The values are kept in [text define -k vrs23-kit] and saved",
        "with this patch. The same logic as in skandrums:",
        "outlet 2 -> skpreset left \\, skpreset left -> vrs23~ right."])
    y = yt + 120

    # --- Dokumentation
    def section(title, lines, y):
        label(p, 60, y, title, 14)
        htext(p, 82, y + 24, lines)
        return y + 24 + 19 * len(lines) + 40

    y = section("Inlets", ["0: audio (mono)", "1: messages (see Messages below)"], y)
    y = section("Outlets", ["0: audio left \\, 1: audio right",
                            "2: events: dump \\, value"], y)
    y = section("Messages", [
        "<control> <0-127>: set a control by name \\, e.g. regn 64",
        "ctl <index> <value> \\, params v0 v1 ...: set by index",
        "defaults \\, reset: all controls to their defaults",
        "get <control>: answers 'value <control> <v>' on outlet 2",
        "print: controls and sample rate to the Pd window",
        "dump: all controls on outlet 2 as 'dump vrs23 v0 ..'",
        "dump vrs23 v0 v1 ...: restore such a dump"], y)
    ctl_lines = []
    for k, (n, lo, hi, d, sc, desc) in enumerate(PARAMS):
        dsc = desc.replace("\\,", ",")
        rng = f"[{lo}-{hi}] " if (lo, hi) != (0, 127) else ""
        ctl_lines += textwrap.wrap(f"{k} {n} ({d}) {rng}{dsc}", 60, subsequent_indent="   ")
    y = section("Controls (index name default)", ctl_lines, y)
    y = section("Signal path", [
        "input -> pre-emphasis -> E1/E2 -> NE570 compressor ->",
        "anti-aliasing -> 2 x 5 BBD chips (768 clocks each) -> taps ->",
        "reconstruction -> NE570 expander -> A1/A2 -> tone \\, return.",
        "Echo: 10 chips in series \\, one clock \\, mono output.",
        "Reverb: rows separate \\, two clocks \\, cross-coupled.",
        "L = lower row \\, R = upper row.",
        "The buffer follows the clock: fine and sweep change the",
        "pitch of what is already in the bucket brigade."], y)
    y = section("Credits", [
        "Credits: Modelled on the Dynacord schematics 80 207 / 80 205",
        "(1981) and the service manual (1983) with help by Claude.",
        "skpreset from skandrums-pd (CC0). Reference: vrs23-clap.wav"], y)
    H = y + 20
    p.head = f"#N canvas 348 20 820 {H + 60} 12;"
    p.items[bg[0]] = CNV.format(x=40, y=20, w=700, h=H - 20, c="#fafaf2")
    p.items[bg[1]] = CNV.format(x=20, y=80, w=20, h=H - 80, c="#ffcfd6")
    p.items[bg[2]] = CNV.format(x=560, y=140, w=200, h=H - 140, c="#1e3791")
    p.save("vrs23~-help.pd")


if __name__ == "__main__":
    build_lp2(); build_sk3(); build_sh(); build_reihe(); build_komp(); build_exp()
    build_tone()
    build_emph("vrs-preemph.pd", 1880, "Pre-Emphasis (I001: R002 47k + R001 18k/C002 4n7): Hoehen bis +11 dB",
               "-2.17*$f1", "1+2*$f1")
    build_emph("vrs-deemph.pd", 360, "De-Emphasis (I009: 470k / 470k+470p): Shelf -6 dB zwischen 360 und 720 Hz",
               "0.5*$f1", "1-0.5*$f1")
    build_clock(); build_main(); build_help()
    print("geschrieben nach", OUT, sorted(os.listdir(OUT)))
