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


STEPS_1 = STEPS
STEPS = []
# ---- second pass: what the first one could not measure, or measured through the
# demo noise of the plug-in. Self-oscillation (no oscillator, res 127) gives the
# filter's frequency directly.
step("sync", "C4 open sawtooth", [60], 0.5, 1.5)
for c in range(0, 128, 8):
    step("selfosc_curve", f"self-oscillation, cutoff {c}", [60], 2.5 if c < 24 else 1.5, 0.5, saw=0, res=127, cutoff=c)
step("selfosc_curve", "self-oscillation, cutoff 127", [60], 1.5, 0.5, saw=0, res=127, cutoff=127)
for r in (100, 104, 108, 112, 116, 120):
    step("threshold", f"no oscillator, cutoff 64, res {r}", [60], 1.5, 0.5, saw=0, res=r, cutoff=64)
for k in (127, 64, 32):
    for n in (36, 60, 84):
        step("kybd_so", f"self-oscillation, cutoff 48, kybd {k}, note {n}", [n], 1.5, 0.5, saw=0, res=127, cutoff=48, kybd=k)
for e in (16, 32, 64, 96, 127):
    step("env_so", f"self-oscillation, cutoff 0, env {e}, sustain 127", [60], 1.5, 0.5, saw=0, res=127, cutoff=0, env=e)
for v in (32, 64, 127):
    step("vcflfo_so", f"self-oscillation, cutoff 64, vcflfo {v}, rate 64", [60], 4.0, 0.5, saw=0, res=127, cutoff=64, vcflfo=v)
for ch in (1, 2):
    step("chorus_so", f"self-oscillation, cutoff 72, chorus {['', 'I', 'II'][ch]}", [60], 8.0, 0.5, saw=0, res=127, cutoff=72, chorus=ch)
for h in (0, 1):
    step("hpf_low", f"C1 sawtooth, hpf {h}", [24], 2.0, 0.5, hpf=h)
for a in (64, 96):
    step("attack", f"attack {a}", [48], 5.0, 1.0, attack=a)
for d in (96, 112, 127):
    step("decay", f"decay {d} sustain 0", [48], 22.0, 1.0, decay=d, sustain=0)
for r in (112, 127):
    step("release", f"release {r}", [48], 0.5, 22.0, release=r)
for r, hold in ((0, 12.0), (16, 6.0), (127, 3.0)):
    step("lforate", f"LFO rate {r}, dcolfo 64", [60], hold, 0.5, lforate=r, dcolfo=64)
step("dcolfo", "dcolfo 32, rate 64", [60], 4.0, 0.5, dcolfo=32)
step("sync", "C4 open sawtooth (end)", [60], 0.5, 1.5)
STEPS_2 = STEPS


# ---- third pass: voice assignment and portamento. The synth's assign mode and portamento are
# panel settings, not patch data, so each run is recorded with them set by hand (see README).
# A long release makes it audible which voice a note takes: a tail that stops was taken over.
PAT = []        # (group, label, overrides, [(t, note, velocity)], length)


def pattern(group, label, events, length, **ov):
    PAT.append((group, label, ov, events, length))


LONG = dict(release=100, attack=0, decay=0, sustain=127)
pattern("sync", "C4 open sawtooth", [(0, 60, 100), (0.5, 60, 0)], 2.0)
pattern("assign", "two notes in turn: does the first tail go on?", [(0, 48, 100), (0.5, 48, 0), (1.5, 55, 100), (2.0, 55, 0)], 6.0, **LONG)
pattern("assign", "the same key twice", [(0, 48, 100), (0.5, 48, 0), (1.5, 48, 100), (2.0, 48, 0)], 6.0, **LONG)
pattern("assign", "seven notes in turn, six voices", [(0.4 * k, 48 + 2 * k, 100) for k in range(7)] + [(0.4 * k + 0.3, 48 + 2 * k, 0) for k in range(7)], 8.0, **LONG)
pattern("assign", "seven held notes", [(0.3 * k, 48 + 2 * k, 100) for k in range(7)] + [(3.0, 48 + 2 * k, 0) for k in range(7)], 8.0, release=10)
pattern("assign", "legato: second key while the first is held, then the first released",
        [(0, 48, 100), (0.8, 55, 100), (1.6, 48, 0), (2.4, 55, 0)], 4.0, release=10)
pattern("assign", "legato: first key released before the second",
        [(0, 48, 100), (0.8, 55, 100), (1.6, 55, 0), (2.4, 48, 0)], 4.0, release=10)
pattern("porta", "jumps C3 C5 C3, separate notes", [(0, 48, 100), (0.9, 48, 0), (1.0, 72, 100), (1.9, 72, 0), (2.0, 48, 100), (2.9, 48, 0)], 4.0, release=10)
pattern("porta", "jumps C3 C5 C3, legato", [(0, 48, 100), (1.0, 72, 100), (1.01, 48, 0), (2.0, 48, 100), (2.01, 72, 0), (2.9, 48, 0)], 4.0, release=10)
pattern("porta", "chord C-E-G to C-E-G an octave up", [(0, 48, 100), (0, 52, 100), (0, 55, 100), (0.9, 48, 0), (0.9, 52, 0), (0.9, 55, 0),
                                                       (1.0, 60, 100), (1.0, 64, 100), (1.0, 67, 100), (2.4, 60, 0), (2.4, 64, 0), (2.4, 67, 0)], 4.0, release=10)
pattern("sync", "C4 open sawtooth (end)", [(0, 60, 100), (0.5, 60, 0)], 2.0)


def build_patterns(name):
    lines, plan, t, last = [], [], 1.0, 0.0
    for group, label, ov, events, length in PAT:
        p = dict(BASE, **ov)
        sx = [0xF0, 0x41, 0x30, 0x00, 0x00] + patch_bytes(p) + [0xF7]
        lines.append(f"{round((t - last) * 1000)} j106sx {' '.join(map(str, sx))};")
        last = t
        t0 = t + 0.4
        for et, n, v in sorted(events):
            lines.append(f"{round((t0 + et - last) * 1000)} j106note {n} {v};")
            last = t0 + et
        plan.append(dict(group=group, label=label, patch=patch_bytes(p), start=round(t0, 3),
                         events=[[round(t0 + et, 3), n, v] for et, n, v in sorted(events)]))
        t = t0 + length
    lines.append(f"{round((t - last) * 1000)} j106done bang;")
    return lines, plan, t


def build(STEPS, name, patterns=False):
    if patterns:
        lines, plan, t = build_patterns(name)
        return write(name, lines, plan, t)
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
    return write(name, lines, plan, t)


def write(name, lines, plan, t):
    open(os.path.join(OUT, name + ".txt"), "w").write("\n".join(lines) + "\n")
    json.dump(dict(length=round(t, 2), steps=plan), open(os.path.join(OUT, name + ".json"), "w"), indent=1)
    pd = """#N canvas 100 80 680 560 12;
#X text 20 14 juno106test - plays the Juno-106 measurement through midiout;
#X text 20 40 1) Pd MIDI output: the port that reaches the synth (e.g. IAC). 2) Pd audio input: the synth's output (e.g. BlackHole) - or record elsewhere and leave rec off. 3) Click start. Patches go out as Juno-106 SysEx \\, notes on channel 1. About %d seconds. With rec on \\, juno106test-rec.wav is written next to this patch., f 76;
#X msg 20 150 start;
#X msg 90 150 stop;
#X msg 20 190 read juno106test.txt \\, rewind \\, bang;
#X obj 20 230 qlist;
#X obj 320 150 r j106sx;
#X obj 320 180 t b a;
#X obj 320 210 until;
#X obj 380 210 list;
#X obj 320 240 list split 1;
#X obj 320 270 midiout;
#X obj 320 320 r j106note;
#X obj 320 350 t b a;
#X obj 320 380 until;
#X obj 380 380 list;
#X obj 320 410 list split 2;
#X obj 320 440 unpack f f;
#X obj 320 480 noteout 1;
#X obj 120 280 r j106done;
#X obj 120 310 print juno106test;
#X msg 90 190 stop;
#X msg 160 190 0 123;
#X obj 160 230 ctlout 1;
#X obj 20 370 tgl 19 0 empty empty rec 22 10 0 12 #fcfcfc #000000 #000000 1 1;
#X obj 20 400 spigot;
#X msg 20 430 open -bytes 3 juno106test-rec.wav \\, start;
#X obj 20 470 s j106rec;
#X obj 120 340 t b;
#X msg 160 400 stop;
#X obj 20 510 adc~ 1 2;
#X obj 120 510 r j106rec;
#X obj 20 540 writesf~ 2;
#X msg 160 430 \\; pd dsp 1;
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
#X connect 2 0 25 0;
#X connect 24 0 25 1;
#X connect 25 0 26 0;
#X connect 26 0 27 0;
#X connect 19 0 28 0;
#X connect 3 0 28 0;
#X connect 28 0 29 0;
#X connect 29 0 32 0;
#X connect 30 0 32 0;
#X connect 30 1 32 1;
#X connect 31 0 32 0;
#X connect 25 0 33 0;
""" % round(t)
    open(os.path.join(OUT, name + ".pd"), "w").write(pd.replace("juno106test", name))
    print(f"{name}: {len(plan)} steps, {t:.0f} s")


def main():
    os.makedirs(OUT, exist_ok=True)
    build(STEPS_1, "juno106test")
    build(STEPS_2, "juno106test2")
    build(None, "juno106test3", patterns=True)


if __name__ == "__main__":
    main()
