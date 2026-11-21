#!/usr/bin/env python3
"""Measures how loud every instrument plays its factory sounds, for the rack's level trims.

usage: python3 common/tools/measure_levels.py [instrument ...]   (default: all)

For each instrument Pd runs offline (pd -batch): the first N factory sounds one after the
other (presetstep), each with a C major chord (C4 E4 G4, velocity 100) held 1.6 s and 1.4 s
release. The RMS of the held part of every sound (0.3-1.5 s after note on) is measured; the
median over the sounds is the instrument's level, the trim brings it to REF dBFS. Needs
Pd 0.54 or newer, numpy and soundfile, and the instrument built for this machine (make).
d50~ needs d50_pcm.bin and tx81zn~ its ROM (tx81z-v1.6.bin) beside the external, cz4~ a
factory bank (.syx) there: without them the instrument is reported as not measured.

Writes the table to docs/levels.md (and prints it). The rack applies the trims.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
REF = -20.0          # dBFS RMS of the median sound after the trim
SEG = 3.0            # s per sound

# name: (object or wrapper, number of sounds, pd/ folder, wrapper patch text or None, note)
P8W = """#N canvas 0 0 400 300 10;
#X obj 10 10 inlet;
#X obj 10 40 p8nav;
#X obj 10 80 pdoly8~;
#X obj 10 120 outlet~;
#X obj 80 120 outlet~;
#X connect 0 0 1 0;
#X connect 1 0 2 0;
#X connect 2 0 3 0;
#X connect 2 1 4 0;
#X connect 2 2 1 1;
"""
INSTRUMENTS = {
    "juno": ("juno~", 32, "instruments/juno/pd", None, "built-in Juno-106 factory patches"),
    "d50": ("d50~", 32, "instruments/d50/pd", None, "needs d50_pcm.bin"),
    "cz4": ("cz4~", 32, "instruments/cz4/pd", None, "needs a factory bank (.syx) beside cz4~"),
    "pdoly8": ("p8w", 32, "instruments/pdoly8/pd", P8W, "Poly-800 II factory bank through p8nav"),
    "tx81z": ("tx81zn~", 32, "instruments/tx81z/pd", None, "needs tx81z-v1.6.bin (ROM presets A01-)"),
}


def patch(obj, n, wav):
    """the measuring patch: n sounds, presetstep before each; recorded into two arrays that
    soundfiler writes at the end (synchronous: writesf~ may not have flushed when pd -batch quits)"""
    bs = chr(92)
    size = int((n * SEG + 1) * 48000)
    L, C = ["#N canvas 0 0 800 600 10;"], []

    def add(t):
        L.append(t + ";")
        return len(L) - 2
    lb = add("#X obj 10 10 loadbang")
    ini = add(f"#X msg 10 40 {bs}; pd dsp 1 {bs}; rec bang")
    o = add(f"#X obj 10 200 {obj}")
    r = add("#X obj 10 170 r inst")
    wl = add("#X obj 10 300 tabwrite~ mlv-l")
    wr = add("#X obj 120 300 tabwrite~ mlv-r")
    rr = add("#X obj 200 270 r rec")
    add(f"#X obj 300 300 array define mlv-l {size}")
    add(f"#X obj 300 330 array define mlv-r {size}")
    C += [(lb, 0, ini, 0), (r, 0, o, 0), (o, 0, wl, 0), (o, 1, wr, 0), (rr, 0, wl, 0), (rr, 0, wr, 0)]
    d0 = add("#X obj 300 10 del 200")
    cnt = add("#X obj 300 40 f")
    inc = add("#X obj 340 40 + 1")
    sel = add(f"#X obj 300 70 moses {n}")
    tt = add("#X obj 300 100 t b b")
    on = add(f"#X msg 300 160 {bs}; inst presetstep 1 {bs}; inst 60 100 {bs}; inst 64 100 {bs}; inst 67 100")
    d1 = add("#X obj 450 100 del 1600")
    off = add(f"#X msg 450 130 {bs}; inst 60 0 {bs}; inst 64 0 {bs}; inst 67 0")
    d2 = add(f"#X obj 300 130 del {int(SEG * 1000)}")
    fin = add(f"#X msg 380 100 {bs}; mlv-sf write -bytes 4 {wav} mlv-l mlv-r {bs}; pd quit")
    sv = add("#X obj 380 230 r mlv-sf")
    sfl = add("#X obj 380 260 soundfiler")
    C += [(sv, 0, sfl, 0)]
    C += [(lb, 0, d0, 0), (d0, 0, cnt, 0), (cnt, 0, inc, 0), (inc, 0, cnt, 1), (cnt, 0, sel, 0),
          (sel, 0, tt, 0), (tt, 1, on, 0), (tt, 0, d1, 0), (d1, 0, off, 0), (tt, 0, d2, 0),
          (d2, 0, cnt, 0), (sel, 1, fin, 0)]
    return "\n".join(L + [f"#X connect {a} {b} {c} {d};" for a, b, c, d in C]) + "\n"


def measure(name):
    obj, n, folder, wrapper, note = INSTRUMENTS[name]
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "out.wav")
        if wrapper:
            open(os.path.join(tmp, obj + ".pd"), "w").write(wrapper)
        pp = os.path.join(tmp, "measure.pd")
        open(pp, "w").write(patch(obj, n, wav))
        subprocess.run(["pd", "-batch", "-nosound", "-nomidi", "-stderr",
                        "-path", os.path.join(REPO, folder), "-path", tmp, pp],
                       capture_output=True, timeout=600)
        if not os.path.exists(wav):
            return None, note
        d, sr = sf.read(wav)
    seg, start = int(SEG * sr), int(0.2 * sr)
    rms, peak = [], []
    for i in range(n):
        a = d[start + i * seg + int(0.3 * sr): start + i * seg + int(1.5 * sr)]
        if len(a) == 0:
            break
        rms.append(20 * np.log10(np.sqrt(np.mean(a ** 2)) + 1e-12))
        peak.append(20 * np.log10(np.max(np.abs(d[start + i * seg: start + (i + 1) * seg])) + 1e-12))
    rms = np.array(rms)
    ok = rms > -80
    if ok.sum() < 2 or rms[ok].max() - rms[ok].min() < 0.1:   # silent, or the same sound each time
        return None, note
    med = float(np.median(rms[ok]))
    return dict(n=int(ok.sum()), of=len(rms), median=med, lo=float(rms[ok].min()),
                hi=float(rms[ok].max()), peak=float(max(peak)), trim=REF - med), note


def main():
    names = sys.argv[1:] or list(INSTRUMENTS)
    rows = []
    for nm in names:
        r, note = measure(nm)
        if r:
            rows.append(f"| {nm} | {r['n']}/{r['of']} | {r['median']:.1f} | {r['lo']:.1f} .. {r['hi']:.1f} "
                        f"| {r['peak']:.1f} | {r['trim']:+.1f} | {note} |")
        else:
            rows.append(f"| {nm} | - | - | - | - | - | not measured: {note} |")
    text = (f"# Levels\n\nMade by `common/tools/measure_levels.py` (run it again after changes, and "
            f"on a machine with the ROMs for d50 and tx81z). Each instrument plays its first factory "
            f"sounds with a C major chord at velocity 100; RMS of the held part, dBFS, at the "
            f"instrument's default volume. **Trim** brings the median to {REF:.0f} dBFS: the rack "
            f"puts it in front of the mixer.\n\n"
            "| Instrument | Sounds | Median RMS | Range | Peak | Trim dB | Note |\n"
            "|---|---|---|---|---|---|---|\n" + "\n".join(rows) + "\n")
    print(text)
    if not sys.argv[1:]:
        open(os.path.join(REPO, "docs", "levels.md"), "w").write(text)


if __name__ == "__main__":
    main()
