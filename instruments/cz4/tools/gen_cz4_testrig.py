#!/usr/bin/env python3
"""Generates cz4/cz4test.pd (next to cz4~, data in cz4/test/): plays the comparison tests of tools/cz4_tests.py to a
CZ (Arturia CZ V or a real one) over MIDI and to cz4~ at the same time, and records both:
test/rec/czv-testNN.wav (the audio input) and test/rec/cz4-testNN.wav (cz4~).

usage: python3 tools/gen_cz4_testrig.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import cz4_tests as T                                 # noqa: E402

D = "\\$"
N = len(T.TESTS)


class P:
    def __init__(self):
        self.lines = ["#N canvas 40 40 1000 760 12;"]
        self.n = 0
        self.conns = []

    def add(self, s):
        self.lines.append(s if s.endswith(";") else s + ";")
        self.n += 1
        return self.n - 1

    def obj(self, x, y, t):
        return self.add(f"#X obj {x} {y} {t}")

    def msg(self, x, y, t):
        t = t.replace("$", "\\$").replace(", ", " \\, ").replace("; ", " \\; ")
        if t.startswith(";"):
            t = "\\;" + t[1:]
        return self.add(f"#X msg {x} {y} {t}")

    def text(self, x, y, t, width=0):
        t = t.replace(",", " \\,").replace(";", " \\;").replace("$", "\\$")
        return self.add(f"#X text {x} {y} {t}" + (f", f {width}" if width else ""))

    def c(self, a, o, b, i):
        self.conns.append(f"#X connect {a} {o} {b} {i};")

    def save(self, path):
        open(path, "w").write("\n".join(self.lines + self.conns) + "\n")


def build():
    p = P()
    p.text(20, 40, "cz4~ against Arturia CZ V: comparison recordings", 0)
    howto = [
        "1. Import the test tones into CZ V: test/tones/cz101/test01.syx ... test12.syx "
        "(CZ-101 data, the usual form). If CZ V does not take them, try tones/dump, tones/cz1 "
        "or tones/cz1dump, or the banks test/cz4test-bank-*.syx. With CZ-101 data set the "
        "velocity by hand for test 5 (DCA 15, DCW 8 on line 1). "
        "Leave the CZ V settings at their defaults (volume, effects off, no unison, poly).",
        "2. Route: Pd MIDI out -> CZ V MIDI in (IAC driver on the Mac), CZ V audio out -> Pd "
        "audio in 1+2 (a loopback such as BlackHole 2ch, or the interface). The meter below "
        "shows the input level. Set channel to CZ V's MIDI channel.",
        "3. Choose a test, select its tone in CZ V, press run. The sequence goes to CZ V and to "
        "cz4~ at once, both are recorded into test/rec/ (czv-testNN.wav, cz4-testNN.wav). "
        "With all on, the next test is chosen when one ends: select its tone in CZ V, then "
        "press run (no hurry: nothing plays before). Only with pc on (CZ V follows program "
        "change, program = test number - 1) does the next test start by itself.",
        "4. Test 7 needs portamento on (time 50) and bend range 2 in CZ V. All others: "
        "portamento off. Then: python3 test/cz_compare.py test/rec (in the repository: "
        "tools/cz_compare.py cz4/test/rec) writes test/rec/report.md with plots. Needs numpy.",
    ]
    y = 64
    for line in howto:
        p.text(20, y, line, 120)
        y += 54

    # ---- controls
    y = 300
    p.text(20, y, "test")
    p.add(f"#X floatatom 60 {y} 3 1 {N} 0 - czt-selv czt-sel")
    p.text(110, y, "run")
    p.add(f"#X obj 140 {y} bng 19 250 50 0 czt-run empty empty 0 -10 0 12 #fcfcfc #000000 #000000")
    p.text(180, y, "stop")
    p.add(f"#X obj 220 {y} bng 19 250 50 0 czt-stop empty empty 0 -10 0 12 #fcfcfc #000000 #000000")
    p.add(f"#X obj 270 {y} tgl 19 0 czt-all empty all 23 10 0 12 #fcfcfc #000000 #000000 0 1")
    p.add(f"#X obj 330 {y} tgl 19 1 czt-rec czt-recv record 23 10 0 12 #fcfcfc #000000 #000000 1 1")
    p.add(f"#X obj 420 {y} tgl 19 0 czt-pc empty pc 23 10 0 12 #fcfcfc #000000 #000000 0 1")
    p.add(f"#X obj 480 {y} tgl 19 1 czt-hear czt-hearv hear_cz4~ 23 10 0 12 #fcfcfc #000000 #000000 1 1")
    p.text(600, y, "channel")
    p.add(f"#X floatatom 660 {y} 3 1 16 0 - czt-chv czt-ch")
    p.text(20, y + 30, "now:")
    p.add(f"#X symbolatom 60 {y + 30} 22 0 0 0 - czt-namev -")
    p.text(260, y + 30, "input dB")
    p.add(f"#X floatatom 330 {y + 30} 5 0 0 0 - czt-inv -")
    p.text(400, y + 30, "cz4~ dB")
    p.add(f"#X floatatom 460 {y + 30} 5 0 0 0 - czt-outv -")
    p.text(20, y + 60, "tests: " + ", ".join(f"{k + 1} {t['name'][4:].lower()}" for k, t in
                                              enumerate(T.TESTS)), 120)

    # ---- logic (below the fold)
    Y = 800
    lb = p.obj(20, Y, "loadbang")
    tl = p.obj(20, Y + 30, "t b b b")
    p.c(lb, 0, tl, 0)
    dflt = p.msg(200, Y + 60, "; czt-selv 1; czt-chv 1; czt-recv 1; czt-hearv 1")
    p.c(tl, 2, dflt, 0)
    pdc_m = p.msg(20, Y + 60, "dir")
    pdc = p.obj(20, Y + 90, "pdcontrol")
    p.c(tl, 0, pdc_m, 0); p.c(pdc_m, 0, pdc, 0)
    mf = p.obj(20, Y + 120, "makefilename %s/test/cz4test-bank.syx")
    p.c(pdc, 0, mf, 0)
    # rec/ for the recordings
    mkn = p.obj(110, Y + 150, "makefilename %s/test/rec")
    fmk = p.obj(110, Y + 180, "file mkdir")
    p.c(pdc, 0, mkn, 0); p.c(mkn, 0, fmk, 0)
    lp = p.obj(20, Y + 150, "list prepend userread")
    lt = p.obj(20, Y + 180, "list trim")
    p.c(mf, 0, lp, 0); p.c(lp, 0, lt, 0)
    tocz = p.obj(20, Y + 210, "s czt-cz4")
    p.c(lt, 0, tocz, 0)

    # the instrument
    rcz = p.obj(400, Y, "r czt-cz4")
    cz = p.obj(400, Y + 260, "cz4~")
    p.c(rcz, 0, cz, 0)
    # notes
    rn = p.obj(500, Y, "r czt-note")
    p.c(rn, 0, cz, 0)
    un = p.obj(600, Y + 30, "unpack f f")
    p.c(rn, 0, un, 0)
    nout = p.obj(600, Y + 60, "noteout 1")
    p.c(un, 0, nout, 0); p.c(un, 1, nout, 1)
    # CC: to CZ V; CC 1 also to cz4~ as mod
    rcc = p.obj(700, Y, "r czt-cc")
    ucc = p.obj(700, Y + 30, "unpack f f")
    p.c(rcc, 0, ucc, 0)
    cout = p.obj(700, Y + 90, "ctlout")
    swap = p.obj(700, Y + 60, "swap")
    p.c(ucc, 0, swap, 1); p.c(ucc, 1, swap, 0)
    p.c(swap, 0, cout, 0); p.c(swap, 1, cout, 1)
    rmod = p.obj(800, Y + 30, "route 1")
    p.c(rcc, 0, rmod, 0)
    dm = p.obj(800, Y + 60, "/ 127")
    mm = p.msg(800, Y + 90, "mod $1")
    p.c(rmod, 0, dm, 0); p.c(dm, 0, mm, 0); p.c(mm, 0, cz, 0)
    # bend
    rb = p.obj(900, Y, "r czt-bend")
    bo = p.obj(900, Y + 60, "bendout")
    p.c(rb, 0, bo, 0)
    db = p.obj(980, Y + 30, "/ 8192")
    bm = p.msg(980, Y + 60, "bend $1")
    p.c(rb, 0, db, 0); p.c(db, 0, bm, 0); p.c(bm, 0, cz, 0)
    # aftertouch
    ra = p.obj(1080, Y, "r czt-at")
    to = p.obj(1080, Y + 60, "touchout")
    p.c(ra, 0, to, 0)
    da = p.obj(1160, Y + 30, "/ 127")
    am = p.msg(1160, Y + 60, "at $1")
    p.c(ra, 0, da, 0); p.c(da, 0, am, 0); p.c(am, 0, cz, 0)
    # channel to all outputs
    rch = p.obj(1260, Y, "r czt-ch")
    for o, inl in ((nout, 2), (cout, 2), (bo, 1), (to, 1)):
        p.c(rch, 0, o, inl)
    pgm = p.obj(1260, Y + 120, "pgmout")
    p.c(rch, 0, pgm, 1)

    # run: number -> cz4~ tone, CZ V program, files, sequence
    rrun = p.obj(20, Y + 300, "r czt-run")
    rsel = p.obj(120, Y + 300, "r czt-sel")
    fsel = p.obj(20, Y + 330, "f 1")
    p.c(rrun, 0, fsel, 0); p.c(rsel, 0, fsel, 1)
    trun = p.obj(20, Y + 360, "t f f f f f")
    p.c(fsel, 0, trun, 0)
    # 5: cz4~ back to a known state and its test tone
    m5 = p.msg(500, Y + 400, "reset, opinit, user $1")
    m5a = p.obj(500, Y + 370, "- 1")
    p.c(trun, 4, m5a, 0); p.c(m5a, 0, m5, 0)
    p.c(m5, 0, cz, 0)
    # 4: CZ V: all notes off and program change when pc is on
    off = p.msg(700, Y + 400, "0 123")
    p.c(trun, 3, off, 0)
    offu = p.obj(700, Y + 430, "unpack f f")
    p.c(off, 0, offu, 0)
    offs = p.obj(700, Y + 460, "swap")
    p.c(offu, 0, offs, 1); p.c(offu, 1, offs, 0)
    p.c(offs, 0, cout, 0); p.c(offs, 1, cout, 1)
    rpc = p.obj(860, Y + 370, "r czt-pc")
    spig = p.obj(820, Y + 430, "spigot")
    p.c(trun, 3, spig, 0); p.c(rpc, 0, spig, 1)
    p.c(spig, 0, pgm, 0)
    # 3: the name of the test in the display
    sel = p.obj(160, Y + 460, "sel " + " ".join(str(k + 1) for k in range(N)))
    p.c(trun, 2, sel, 0)
    shown = p.obj(160, Y + 560, "s czt-namev")
    for k, t in enumerate(T.TESTS):
        m = p.msg(160 + (k % 4) * 40, Y + 490 + (k // 4) * 22, "set " + t["name"].replace(" ", "_"))
        p.c(sel, k, m, 0)
        p.c(m, 0, shown, 0)
    # 2: open the files
    rrec = p.obj(300, Y + 440, "r czt-rec")
    sprec = p.obj(300, Y + 500, "spigot 1")
    p.c(trun, 1, sprec, 0); p.c(rrec, 0, sprec, 1)
    tr2 = p.obj(300, Y + 530, "t f f")
    p.c(sprec, 0, tr2, 0)
    fa = p.obj(300, Y + 570, "makefilename test/rec/czv-test%02d.wav")
    fb = p.obj(560, Y + 570, "makefilename test/rec/cz4-test%02d.wav")
    p.c(tr2, 0, fa, 0); p.c(tr2, 1, fb, 0)
    oa = p.msg(300, Y + 600, "open -bytes 3 $1, start")
    ob = p.msg(560, Y + 600, "open -bytes 3 $1, start")
    p.c(fa, 0, oa, 0); p.c(fb, 0, ob, 0)
    adc = p.obj(300, Y + 660, "adc~ 1 2")
    wa = p.obj(300, Y + 700, "writesf~ 2")
    wb = p.obj(560, Y + 700, "writesf~ 2")
    p.c(oa, 0, wa, 0); p.c(ob, 0, wb, 0)
    p.c(adc, 0, wa, 0); p.c(adc, 1, wa, 1)
    p.c(cz, 0, wb, 0); p.c(cz, 1, wb, 1)
    # 1: the sequence, after 300 ms (program change, files)
    dl = p.obj(20, Y + 400, "delay 300")
    fq = p.obj(20, Y + 430, "f")
    p.c(trun, 0, fq, 1); p.c(trun, 0, dl, 0); p.c(dl, 0, fq, 0)
    qn = p.obj(20, Y + 460, "makefilename test/seq/test%02d.txt")
    qm = p.msg(20, Y + 490, "read $1, bang")
    q = p.obj(20, Y + 520, "qlist")
    p.c(fq, 0, qn, 0); p.c(qn, 0, qm, 0); p.c(qm, 0, q, 0)

    # end of a test: stop the files, next one if all
    rend = p.obj(20, Y + 620, "r czt-end")
    rstop = p.obj(120, Y + 620, "r czt-stop")
    sm = p.msg(120, Y + 650, "stop")
    p.c(rstop, 0, sm, 0); p.c(sm, 0, q, 0)
    stp = p.msg(200, Y + 700, "stop")
    p.c(rend, 0, stp, 0); p.c(sm, 0, stp, 0)
    p.c(stp, 0, wa, 0); p.c(stp, 0, wb, 0)
    rst = p.msg(120, Y + 680, "reset")
    p.c(rstop, 0, rst, 0); p.c(rst, 0, cz, 0)
    p.c(rstop, 0, off, 0)
    pr = p.obj(20, Y + 760, "print cz4test")
    tend = p.obj(20, Y + 650, "t b b")
    p.c(rend, 0, tend, 0)
    fcur = p.obj(20, Y + 680, "f")
    p.c(fsel, 0, fcur, 1)
    p.c(tend, 1, fcur, 0)
    tdone = p.obj(20, Y + 710, "t f f")
    p.c(fcur, 0, tdone, 0)
    dmsg = p.msg(20, Y + 735, "done test $1")
    p.c(tdone, 1, dmsg, 0); p.c(dmsg, 0, pr, 0)
    rall = p.obj(160, Y + 740, "r czt-all")
    nxt = p.obj(60, Y + 790, f"moses {N}")
    sall = p.obj(60, Y + 760, "spigot")
    p.c(tdone, 0, sall, 0); p.c(rall, 0, sall, 1)
    p.c(sall, 0, nxt, 0)
    inc = p.obj(60, Y + 820, "+ 1")
    p.c(nxt, 0, inc, 0)
    # the next test is chosen at once; it starts by itself only when CZ V follows program
    # change (pc on), otherwise it waits for run: time to select the tone in CZ V
    tn = p.obj(60, Y + 850, "t f f f")
    p.c(inc, 0, tn, 0)
    ss = p.obj(260, Y + 880, "s czt-sel")
    sv = p.msg(360, Y + 880, "set $1")
    svs = p.obj(360, Y + 910, "s czt-selv")
    p.c(tn, 2, ss, 0); p.c(tn, 2, sv, 0); p.c(sv, 0, svs, 0)
    p.c(tn, 2, sel, 0)
    nmsg = p.msg(160, Y + 880, "next: test $1 - select its tone in CZ V then press run")
    rpc2 = p.obj(160, Y + 910, "r czt-pc")
    sp2 = p.obj(160, Y + 940, "spigot 1")
    inv = p.obj(260, Y + 940, "== 0")
    p.c(rpc2, 0, inv, 0); p.c(inv, 0, sp2, 1)
    p.c(tn, 1, sp2, 0); p.c(sp2, 0, nmsg, 0); p.c(nmsg, 0, pr, 0)
    spa = p.obj(60, Y + 880, "spigot")
    p.c(tn, 0, spa, 0); p.c(rpc2, 0, spa, 1)
    dn = p.obj(60, Y + 910, "pipe 1500")
    p.c(spa, 0, dn, 0)
    dnb = p.obj(60, Y + 940, "t b")
    p.c(dn, 0, dnb, 0)
    sr = p.obj(60, Y + 970, "s czt-run")
    p.c(dnb, 0, sr, 0)
    fin = p.msg(120, Y + 820, "all tests done")
    p.c(nxt, 1, fin, 0); p.c(fin, 0, pr, 0)

    # listen and meters
    hr = p.obj(400, Y + 300, "r czt-hear")
    ml = p.obj(400, Y + 330, "*~")
    mr = p.obj(460, Y + 330, "*~")
    p.c(cz, 0, ml, 0); p.c(cz, 1, mr, 0); p.c(hr, 0, ml, 1); p.c(hr, 0, mr, 1)
    dac = p.obj(400, Y + 360, "dac~")
    p.c(ml, 0, dac, 0); p.c(mr, 0, dac, 1)
    ei = p.obj(100, Y + 960, "env~ 4096")
    p.c(adc, 0, ei, 0)
    eo = p.obj(200, Y + 960, "env~ 4096")
    p.c(cz, 0, eo, 0)
    for k, (e, dest) in enumerate(((ei, "czt-inv"), (eo, "czt-outv"))):
        sub = p.obj(100 + 100 * k, Y + 990, "- 100")
        p.c(e, 0, sub, 0)
        s = p.obj(100 + 100 * k, Y + 1020, f"s {dest}")
        p.c(sub, 0, s, 0)

    return p


if __name__ == "__main__":
    p = build()
    out = os.path.join(ROOT, "cz4", "cz4test.pd")
    p.save(out)
    print("wrote", out)
