#!/usr/bin/env python3
"""Writes the Juno-106 measurement kit to juno~/test/:

  juno106test.pd    plays the test through [midiout]: before every step the patch as Juno-106
                    SysEx, then the notes. Record the synth it drives (a Juno-106 or an
                    emulation) and nothing else.
  juno106test.txt   the sequence for [qlist]
  juno106test.json  the same steps with their times, for the analysis (tools/juno106_analyse.py)

usage: python3 tools/make_juno106_test.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "juno~", "test")

SLIDERS = ["lforate", "lfodelay", "dcolfo", "pwm", "noise", "cutoff", "res", "env", "vcflfo",
           "kybd", "level", "attack", "decay", "sustain", "release", "sub"]

# A plain open sawtooth: 8', no chorus, high-pass at the Juno-106's flat position (1).
BASE = dict(lforate=64, lfodelay=0, dcolfo=0, pwm=0, noise=0, cutoff=127, res=0, env=0,
            vcflfo=0, kybd=0, level=100, attack=0, decay=0, sustain=127, release=10, sub=0,
            range=1, pulse=0, saw=1, chorus=0, pwmmode=1, envpol=0, vcamode=0, hpf=1)


def patch_bytes(p):
    b = [p[n] for n in SLIDERS]
    s16 = (1 << p["range"]) | (8 if p["pulse"] else 0) | (16 if p["saw"] else 0)
    if p["chorus"] == 0:
        s16 |= 0x20
    elif p["chorus"] == 1:
        s16 |= 0x40
    s17 = (1 if p["pwmmode"] else 0) | (2 if p["envpol"] else 0) | (4 if p["vcamode"] else 0)
    s17 |= (3 - p["hpf"]) << 3
    return b + [s16, s17]


STEPS = []          # (group, label, overrides, notes, hold s, tail s)


def step(group, label, notes, hold, tail, **ov):
    STEPS.append((group, label, ov, notes, hold, tail))


step("sync", "C4 open sawtooth", [60], 0.5, 1.5)
for c in (0, 16, 32, 48, 64, 80, 96, 112, 127):
    step("cutoff", f"cutoff {c}", [24], 1.5, 0.5, cutoff=c)
for r in (0, 32, 64, 80, 96, 112, 127):
    step("resonance", f"cutoff 64 res {r}", [36], 1.5, 0.5, cutoff=64, res=r)
for c in (24, 48, 72, 96, 112):
    step("selfosc", f"no oscillator, res 127, cutoff {c}", [36], 1.5, 0.5, cutoff=c, res=127, saw=0)
step("selfosc", "no oscillator, res 100, cutoff 64", [36], 1.5, 0.5, cutoff=64, res=100, saw=0)
for k in (127, 64):
    for n in (24, 48, 72):
        step("kybd", f"cutoff 48 kybd {k} note {n}", [n], 1.5, 0.5, cutoff=48, kybd=k)
for e in (32, 64, 96, 127):
    step("envdepth", f"cutoff 0 env {e}, decay 60", [36], 2.0, 1.0, cutoff=0, env=e, decay=60, sustain=0, release=30)
for a in (32, 64, 96, 127):
    step("attack", f"attack {a}", [48], 6.0, 1.0, attack=a)
for d in (32, 64, 96, 127):
    step("decay", f"decay {d} sustain 0", [48], 8.0, 1.0, decay=d, sustain=0)
for r in (32, 64, 96, 127):
    step("release", f"release {r}", [48], 0.5, 10.0, release=r)
step("vcamode", "VCA gate, release 127", [48], 0.5, 2.0, vcamode=1, release=127)
for h in (0, 1, 2, 3):
    step("hpf", f"hpf {h}", [36], 1.5, 0.5, hpf=h)
for ch in (0, 1, 2):
    step("chorus", f"chorus {['off', 'I', 'II'][ch]}", [48], 5.0, 0.5, chorus=ch)
for r in (0, 32, 64, 96, 127):
    step("lforate", f"LFO rate {r}, dcolfo 64", [60], 4.0, 0.5, lforate=r, dcolfo=64)
for d in (32, 127):
    step("dcolfo", f"dcolfo {d}, rate 64", [60], 4.0, 0.5, dcolfo=d)
for d in (64, 127):
    step("lfodelay", f"LFO delay {d}, rate 64, dcolfo 64", [60], 6.0, 0.5, lfodelay=d, dcolfo=64)
step("vcflfo", "vcflfo 64, cutoff 64, rate 64", [36], 4.0, 0.5, vcflfo=64, cutoff=64)
step("source", "square (pulse, pwm 0 manual)", [48], 2.0, 0.5, saw=0, pulse=1, pwm=0)
step("source", "pulse, pwm 64 manual", [48], 2.0, 0.5, saw=0, pulse=1, pwm=64)
step("source", "pulse, pwm 127 manual", [48], 2.0, 0.5, saw=0, pulse=1, pwm=127)
step("source", "pulse, pwm 64 LFO", [48], 4.0, 0.5, saw=0, pulse=1, pwm=64, pwmmode=0)
step("source", "sub 127 alone", [48], 2.0, 0.5, saw=0, sub=127)
step("source", "noise 127 alone", [48], 2.0, 0.5, saw=0, noise=127)
step("source", "saw + sub 127", [48], 2.0, 0.5, sub=127)
for lv in (32, 64, 127):
    step("level", f"VCA level {lv}", [48], 1.5, 0.5, level=lv)
step("sync", "C4 open sawtooth (end)", [60], 0.5, 1.5)


def main():
    os.makedirs(OUT, exist_ok=True)
    lines, plan, t, last = [], [], 1.0, 0.0
    for group, label, ov, notes, hold, tail in STEPS:
        p = dict(BASE, **ov)
        sx = [0xF0, 0x41, 0x30, 0x00, 0x00] + patch_bytes(p) + [0xF7]
        lines.append(f"{round((t - last) * 1000)} j106sx {' '.join(map(str, sx))};")
        last = t
        on = t + 0.4
        lines.append(f"{round((on - last) * 1000)} j106note " + " ".join(f"{n} 100" for n in notes) + ";")
        last = on
        off = on + hold
        lines.append(f"{round((off - last) * 1000)} j106note " + " ".join(f"{n} 0" for n in notes) + ";")
        last = off
        plan.append(dict(group=group, label=label, patch=patch_bytes(p), notes=notes,
                         on=round(on, 3), off=round(off, 3)))
        t = off + tail
    lines.append(f"{round((t - last) * 1000)} j106done bang;")
    open(os.path.join(OUT, "juno106test.txt"), "w").write("\n".join(lines) + "\n")
    json.dump(dict(length=round(t, 2), steps=plan), open(os.path.join(OUT, "juno106test.json"), "w"), indent=1)
    pd = """#N canvas 100 80 640 520 12;
#X text 20 14 juno106test - plays the Juno-106 measurement through midiout;
#X text 20 40 1) In Pd's MIDI settings choose the output that reaches the synth (e.g. IAC). 2) Start recording the synth. 3) Click start. Patches go out as Juno-106 SysEx \\, notes on channel 1. Takes about %d seconds. stop ends it., f 70;
#X msg 20 130 start;
#X msg 90 130 stop;
#X msg 20 170 read juno106test.txt \\, rewind \\, bang;
#X obj 20 210 qlist;
#X obj 300 130 r j106sx;
#X obj 300 160 t b a;
#X obj 300 190 until;
#X obj 360 190 list;
#X obj 300 220 list split 1;
#X obj 300 250 midiout;
#X obj 300 300 r j106note;
#X obj 300 330 t b a;
#X obj 300 360 until;
#X obj 360 360 list;
#X obj 300 390 list split 2;
#X obj 300 420 unpack f f;
#X obj 300 460 noteout 1;
#X obj 120 260 r j106done;
#X obj 120 290 print juno106test;
#X msg 90 170 stop;
#X msg 160 170 0 123;
#X obj 160 210 ctlout 1;
#X connect 2 0 4 0;
#X connect 4 0 5 0;
#X connect 3 0 21 0;
#X connect 3 0 22 0;
#X connect 21 0 5 0;
#X connect 22 0 23 0;
#X connect 6 0 7 0;
#X connect 7 1 9 1;
#X connect 7 0 8 0;
#X connect 8 0 9 0;
#X connect 9 0 10 0;
#X connect 10 0 11 0;
#X connect 10 1 9 1;
#X connect 10 2 8 1;
#X connect 12 0 13 0;
#X connect 13 1 15 1;
#X connect 13 0 14 0;
#X connect 14 0 15 0;
#X connect 15 0 16 0;
#X connect 16 0 17 0;
#X connect 16 1 15 1;
#X connect 16 2 14 1;
#X connect 17 0 18 0;
#X connect 17 1 18 1;
#X connect 19 0 20 0;
""" % round(t)
    open(os.path.join(OUT, "juno106test.pd"), "w").write(pd)
    print(f"{len(plan)} steps, {t:.0f} s")


if __name__ == "__main__":
    main()
