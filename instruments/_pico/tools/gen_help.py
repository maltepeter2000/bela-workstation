#!/usr/bin/env python3
"""Generate d50~-help.pd. Never edit the help patch by hand: change this
script and run it again. Names, ranges and CC pages come from tools/d50_params.py.

    python3 tools/gen_help.py            writes d50~/d50~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ctlayer                                    # noqa: E402
import d50_params as T                            # noqa: E402

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


OBJ = "d50~"
SHORT = "d50"
BUS = f"{DOLLAR}0-{SHORT}"
SUBTITLE = "Roland D-50 linear arithmetic synthesizer"

CC = ctlayer.cc_map(T.CC_ROWS)
PERF = list(T.CC_ROWS[0][1])
PERF_HELP = dict(T.MACRO_HELP)
PERF_HELP["reverb"] = "reverb balance of the patch"
PERF_HELP["balance"] = "balance of the two tones, lower at 0, upper at 100"
PERF_DEFAULT = {"cut": "0", "res": "0", "att": "0", "rel": "0", "volume": "80"}


def rng(nm):
    _, _, lo, hi, _, _ = T.CONTROLS[nm]
    return lo, hi


INLETS = [
    "1 float: note number. Velocity is taken from inlet 2",
    "  and velocity 0 is a note off.",
    "1 list: note, velocity and, if you like, the MIDI channel.",
    "1 anything: a message, see Messages.",
    "2 float: velocity for the next note, 0 to 127 (default 100).",
    "3 float or list: raw MIDI bytes, as midiin gives them.",
    "3 anything: a message, the same set as inlet 1.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: info, name, 0-127 and value text for every",
    "  change, the patch that plays, its names, the channels.",
    "3 float: SysEx bytes for midiout.",
]

MESSAGES = [
    "name v: set a control, in the values the D-50 shows.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "preset n: a factory preset, read only.",
    "user n: a slot of the user bank, 0 to 63.",
    "init: a plain sawtooth patch to start from.",
    "reload: the same patch again, edits dropped.",
    "store n: the playing patch into user slot n, in memory.",
    "userwrite: write the user bank to d50user-bank.syx.",
    "userread: read it again. Both also take a file name.",
    "usersend: the user bank as SysEx on outlet 3.",
    "presets, users: list the names.",
    "bend f: pitch wheel, -1 to 1, times bendrange.",
    "mod f: modulation lever, 0 to 1.",
    "aftertouch f: channel pressure, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback and device",
    "  number of SysEx dumps, 1 to 16.",
    "status: report patch, names, channels, voices and CPU.",
    "stop: all notes off.",
    "reset: all notes off and the effect tails emptied.",
]

MIDI = [
    "midiin: raw bytes into inlet 3. d50~ takes notes, bend,",
    "  aftertouch, program change, CC 1 (lever), 5 (portamento",
    "  time), 7 (volume), 64 (hold), 65 (portamento) and SysEx.",
    "midiout: the floats of outlet 3, SysEx answers and dumps.",
    "d50cc 1: the CC pages 20 to 119 on channel 1.",
    "Without midiin: send note lists and bend, mod, sustain.",
]

LAYER = [
    "d50cc is the controller layer: MIDI CC 20 to 119 in pages of 8, the "
    "same pages as for pdoly8~ and tx81z~. Page 1 is the performance row, "
    "page k is row k of the editor. It turns a CC into an n message and "
    "sends every change made elsewhere back to the controller as CC. Its "
    "argument is the MIDI channel, and it follows the channels d50~ reports.",
    "d50edit is the editor: click it in the object area above. All 117 "
    "controls in rows of 8, the factory presets and the user bank, the "
    "MIDI channels and SysEx. It explains itself in its readme.",
    "The D-50 has four partials, two per tone. A partial name such as "
    "cutoff acts on the selected partial, a tone name such as structure "
    "on the tone it belongs to. part 0 to 3 selects U1, U2, L1, L2. All "
    "names are listed at the end of this patch with their CC numbers.",
]

DATA = [
    "The object contains no Roland data. It loads its files from the "
    "folder of the external, or else from the folder of the patch.",
    "d50_pcm.bin: the PCM samples, written by tools/make_d50_data.py from "
    "your own ROM images. Without it the object stays silent and says so "
    "in the Pd window.",
    "Every .syx file beside it is read as a factory bank, a D-50 bulk "
    "dump of 64 patches. The files are loaded in alphabetical order, "
    "PND50-00.syx first, and their patches are numbered through. Without "
    "one the object plays eight built-in presets, which cannot be edited.",
    "d50user-bank.syx: the user bank, in the same format. Until it has "
    "been written the user bank holds 64 init patches.",
    "The engine runs at 32 kHz, the rate of the original, and is resampled "
    "to the rate of Pd.",
]

PRESETS = [
    "preset selects a factory preset, user a slot of the user bank. The "
    "third outlet answers with the number and the name.",
    "A patch change sets the first four controls of the performance row "
    "(cut, res, att, rel) back to 0.",
    "store 5 followed by userwrite keeps the playing patch in user slot 5 "
    "and on disk. The editor does both with save patch.",
]

EDIT_BLOCK = (
    "Patch editing",
    [[("part", 0, 3), ("cutoff", 0, 100), ("reso", 0, 30), ("level", 0, 100)],
     [("structure", 1, 7)],
     [("keymode", 0, 8)]],
    [],
    ["A few examples, the editor has them all. Values are the ones the "
     "D-50 shows, fine for example runs from -50 to 50. A name without a "
     "value reports the current one.",
     "A partial or tone edit is heard from the next note on. Chorus, "
     "equalizer, reverb and the balances follow at once, and cut also "
     "moves the notes that are sounding.",
     "The built-in presets cannot be edited. Load a bank, or start from "
     "init or a user slot.",
     "part (0): the partial that cutoff, reso, level and the other partial "
     "names act on. 0 to 3 are U1, U2, L1, L2.",
     "cutoff (patch): filter cutoff of the selected partial. Only synth "
     "partials have a filter, a PCM partial ignores it.",
     "reso (patch): filter resonance.",
     "level (patch): level of the partial.",
     "structure (patch): which partials of the tone are synth or PCM and "
     "whether they are ring modulated, see Tone controls.",
     "keymode (patch): whole, dual, split and their solo variants, see "
     "Patch controls."])

MEMORY_BLOCK = (
    "By address",
    [],
    ["l2 cutoff 30", "l2 cutoff", "dump", "param 13 60", "param 13", "sysexdump"],
    ["l2 cutoff 30: section, name and value set a parameter without "
     "changing the selection. The sections are u1, u2, upper, l1, l2, "
     "lower and patch. Without a value the message reports it.",
     "dump: reports the three names and every named parameter of every "
     "section, 311 messages.",
     "param a v: sets one of the 448 bytes of the patch by address, with "
     "the value a D-50 dump stores. The address is the section times 64 "
     "plus the offset, sections counted from 0 in the order above. param "
     "a reports the byte.",
     "name: patch name My Patch sets the patch name, up to 18 "
     "characters. upper name and lower name take up to 10.",
     "sysexdump: sends the playing patch as SysEx, see below."])

SYSEX = [
    "The object speaks the exclusive format of the D-50, so an editor or "
    "librarian made for the D-50 can program it, and it can exchange "
    "patches with a real D-50. The bytes arrive with the rest of the MIDI "
    "at the third inlet and leave the third outlet as floats.",
    "Data set (DT1) to address 00-00-00 edits the playing patch, the same "
    "448 bytes that param reaches. Data set to 02-00-00, the internal "
    "memory of the D-50, goes to the user bank: a bulk dump replaces it "
    "in memory, userwrite keeps it.",
    "A request (RQ1) for either area is answered at the third outlet. "
    "sysexdump sends the playing patch and usersend the user bank "
    "without being asked.",
    "sysex followed by the bytes of one message does the same from a "
    "list. The device number of incoming messages is not checked, "
    "answers carry the number of the request.",
]

BLOCKS = [
    ("Playing",
     [[("bend", -1, 1), ("mod", 0, 1), ("aftertouch", 0, 1), ("sustain", 0, 1)]],
     [],
     ["bend (0): pitch wheel, -1 to 1. The range in semitones is bendrange.",
      "mod (0): modulation lever, 0 to 1. What it does is part of the patch.",
      "aftertouch (0): channel pressure, 0 to 1.",
      "sustain (0): hold pedal. Notes released while it is 1 sound until it returns to 0."]),
    ("Additional settings",
     [[("tune", -50, 50), ("voices", 1, 8)],
      [("governor", 0, 1)]],
     [],
     ["tune (0): master tune in cents.",
      "voices (8): polyphony per tone. A patch that uses one tone plays "
      "twice as many notes.",
      "governor (0): when 1, the object ends the oldest released note "
      "early whenever a block takes longer to compute than it lasts. Held "
      "notes are never ended. Meant for small machines such as Bela."]),
]

CREDITS = ("Credits: Modelled on PicoFaceD5 from PicoVintageSynthCollection by "
           "Michi71; Pure Data adaptation with help by Claude")


def cc_of(names):
    ccs = [CC[n] for n in names if n in CC]
    if not ccs:
        return ""
    if len(ccs) == 1:
        return f", CC {ccs[0]}"
    return f", CC {ccs[0]} to {ccs[-1]}"


def param_lines(doc, table):
    """One entry per line, 'name lo..hi, CC n: text', continued with an indent."""
    named = [n for names, _ in doc for n in names.split()]
    assert sorted(named) == sorted(t[0] for t in table), set(named) ^ set(t[0] for t in table)
    lines = []
    for names, text in doc:
        ns = names.split()
        lo, hi = rng(ns[0])
        assert all(rng(n) == (lo, hi) for n in ns)
        label = ns[0] if len(ns) == 1 else f"{ns[0]} to {ns[-1]}"
        first, *rest = wrap(f"{label} {lo}..{hi}{cc_of(ns)}: {text}")
        lines.append(first)
        for r in wrap(" ".join(rest), MAXCH - 2) if rest else []:
            lines.append("  " + r)
    return lines


def perf_lines():
    lines = []
    for nm in PERF + ["part", "tune", "voices"]:
        lo, hi = rng(nm)
        first, *rest = wrap(f"{nm} {lo}..{hi}{cc_of([nm])}: {PERF_HELP[nm]}")
        lines.append(first)
        for r in wrap(" ".join(rest), MAXCH - 2) if rest else []:
            lines.append("  " + r)
    return lines

PARTIAL_DOC = [
    ("coarse", "pitch in semitones, 0 is C1"),
    ("fine", "fine tune"),
    ("keyfollow", "pitch keyfollow step, 11 is a semitone per key"),
    ("lfomode", "pitch LFO: off, +, -, aftertouch and lever"),
    ("penvmode", "pitch envelope: off, +, -"),
    ("bendmode", "bender: off, keyfollow, normal"),
    ("wave", "synth waveform: square, sawtooth"),
    ("pcm", "PCM sample"),
    ("pw", "pulse width"),
    ("pwvelo", "pulse width by velocity"),
    ("pwlfo", "pulse width LFO: +1, -1, +2, -2, +3, -3"),
    ("pwlfodepth", "pulse width LFO depth"),
    ("pwat", "pulse width by aftertouch"),
    ("cutoff", "filter cutoff"),
    ("reso", "filter resonance"),
    ("tvfkf", "cutoff keyfollow step"),
    ("tvfbiaspoint", "cutoff bias point and direction"),
    ("tvfbiaslevel", "cutoff bias level"),
    ("tvfenv", "filter envelope depth"),
    ("tvfvelo", "filter envelope depth by velocity"),
    ("tvfdepthkf", "filter envelope depth keyfollow"),
    ("tvftimekf", "filter envelope time keyfollow"),
    ("tvft1 tvft2 tvft3 tvft4 tvft5", "filter envelope times"),
    ("tvfl1 tvfl2 tvfl3", "filter envelope levels"),
    ("tvfsustain", "filter envelope sustain level"),
    ("tvfend", "filter envelope end level: 0, 100"),
    ("tvflfo", "filter LFO, as pwlfo"),
    ("tvflfodepth", "filter LFO depth"),
    ("tvfat", "cutoff by aftertouch"),
    ("level", "level of the partial"),
    ("velo", "level by velocity"),
    ("tvabiaspoint", "level bias point and direction"),
    ("tvabiaslevel", "level bias level"),
    ("tvat1 tvat2 tvat3 tvat4 tvat5", "level envelope times"),
    ("tval1 tval2 tval3", "level envelope levels"),
    ("tvasustain", "level envelope sustain level"),
    ("tvaend", "level envelope end level: 0, 100"),
    ("tvavelofollow", "level envelope time 1 by velocity"),
    ("tvatimekf", "level envelope time keyfollow"),
    ("tvalfo", "level LFO, as pwlfo"),
    ("tvalfodepth", "level LFO depth"),
    ("tvaat", "level by aftertouch"),
]

TONE_DOC = [
    ("structure", "S+S, S+S ring, P+S, P+S ring, S+P ring, P+P, P+P ring"),
    ("penvvelo", "pitch envelope by velocity"),
    ("penvtimekf", "pitch envelope time keyfollow"),
    ("penvt1 penvt2 penvt3 penvt4", "pitch envelope times"),
    ("penvl0 penvl1 penvl2", "pitch envelope levels"),
    ("penvsustain", "pitch envelope sustain level"),
    ("penvend", "pitch envelope end level"),
    ("pmodlfo", "pitch LFO depth"),
    ("pmodlever", "pitch LFO depth by the lever"),
    ("pmodat", "pitch LFO depth by aftertouch"),
    ("lfo1wave", "triangle, sawtooth, square, random"),
    ("lfo1rate", "LFO 1 rate"),
    ("lfo1delay", "LFO 1 delay"),
    ("lfo1sync", "off, on, key"),
    ("lfo2wave", "as lfo1wave"),
    ("lfo2rate", "LFO 2 rate"),
    ("lfo2delay", "LFO 2 delay"),
    ("lfo2sync", "off, on"),
    ("lfo3wave", "as lfo1wave"),
    ("lfo3rate", "LFO 3 rate"),
    ("lfo3delay", "LFO 3 delay"),
    ("lfo3sync", "off, on"),
    ("eqlowfreq", "low shelf frequency step"),
    ("eqlowgain", "low shelf gain in dB"),
    ("eqhighfreq", "high band frequency step"),
    ("eqhighq", "high band width step"),
    ("eqhighgain", "high band gain in dB"),
    ("chorustype", "chorus type"),
    ("chorusrate", "chorus rate"),
    ("chorusdepth", "chorus depth"),
    ("tchorus", "chorus balance of this tone"),
    ("mute", "partials sounding: none, first, second, both"),
    ("pbal", "balance of the two partials"),
]

PATCH_DOC = [
    ("keymode", "whole, dual, split, whole solo, dual solo, split "
                "upper solo, split lower solo, separate, separate solo"),
    ("split", "split point, 0 is C2"),
    ("portamode", "portamento on upper, lower, both"),
    ("ukeyshift lkeyshift", "key shift of each tone in semitones"),
    ("ufine lfine", "fine tune of each tone"),
    ("bendrange", "bender range in semitones"),
    ("atbend", "pitch bend by aftertouch in semitones"),
    ("portatime", "portamento time"),
    ("outmode", "how the tones go to reverb and outputs"),
    ("reverbtype", "reverb type"),
    ("reverb", "reverb balance"),
    ("pvol", "level of the patch, volume is the master"),
    ("balance", "balance of the two tones"),
    ("porta", "portamento switch"),
]


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



def build(p):
    """Draws everything below the backgrounds; returns the bottom y."""
    y = YEL_Y + M
    if p:
        p.heading(CX, y, OBJ, 22)
        p.heading(CX, y + 36, SUBTITLE, 16)
    y += 80

    # ---- demo row
    if p:
        tg = p.obj(CX, y, "tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1")
        me = p.obj(CX + 30, y, "metro 500")
        on = p.msg(CX + 110, y, f"57 100 {COMMA} 60 100 {COMMA} 64 100")
        de = p.obj(CX + 30, y + 30, "delay 250")
        off = p.msg(CX + 110, y + 30, f"57 0 {COMMA} 60 0 {COMMA} 64 0")
        s1 = p.obj(CX + 400, y + 30, f"s {BUS}")
        p.conn(tg, 0, me, 0)
        p.conn(me, 0, on, 0)
        p.conn(me, 0, de, 0)
        p.conn(de, 0, off, 0)
        p.conn(on, 0, s1, 0)
        p.conn(off, 0, s1, 0)
    y += 64
    if p:
        s2 = p.obj(CX + 400, y, f"s {BUS}")
        for i, txt in enumerate(["60 100", "60 0", "status", "presets", "stop", "reset"]):
            m = p.msg(CX + i * 62, y, txt)
            p.conn(m, 0, s2, 0)
    y += 40

    # ---- main controls: the performance row, two rows of four
    for r in range(2):
        if p:
            snd = p.obj(CX + 400, y + 50, f"s {BUS}")
            for c in range(4):
                nm = PERF[r * 4 + c]
                lo, hi = rng(nm)
                a = p.atom(CX + c * 112, y, 4, lo, hi)
                m = p.msg(CX + c * 112, y + 24, f"{nm} {DOLLAR}1")
                p.conn(a, 0, m, 0)
                p.conn(m, 0, snd, 0)
        y += 90 if r == 0 else 64

    # ---- object area: MIDI, controller layer, editor
    if p:
        ro = p.obj(CX + 130, y, f"r {BUS}-out")
        cc = p.obj(CX + 130, y + 30, "d50cc 1")
        sc = p.obj(CX + 130, y + 60, f"s {BUS}")
        p.conn(ro, 0, cc, 0)
        p.conn(cc, 0, sc, 0)
        p.obj(CX + 250, y + 30, f"d50edit {BUS}")
        mi = p.obj(CX + 400, y + 60, "midiin")
        rc = p.obj(CX, y + 60, f"r {BUS}")
        ob = p.obj(CX, y + 100, OBJ)
        p.conn(rc, 0, ob, 0)
        p.conn(mi, 0, ob, 2)
        gl = p.obj(CX, y + 134, "*~ 0.3")
        gr = p.obj(CX + 60, y + 134, "*~ 0.3")
        da = p.obj(CX, y + 164, "dac~")
        rf = p.obj(CX + 130, y + 134, "route float")
        mo = p.obj(CX + 130, y + 164, "midiout")
        so = p.obj(CX + 220, y + 164, f"s {BUS}-out")
        pr = p.obj(CX + 330, y + 164, "print D50")
        p.conn(ob, 0, gl, 0)
        p.conn(ob, 1, gr, 0)
        p.conn(gl, 0, da, 0)
        p.conn(gr, 0, da, 1)
        p.conn(ob, 2, rf, 0)
        p.conn(rf, 0, mo, 0)
        p.conn(rf, 1, so, 0)
        p.conn(rf, 1, pr, 0)
    y += 200

    # ---- text sections
    y = section(p, y, "Inlets", INLETS)
    y = section(p, y, "Outlets", OUTLETS)
    y = section(p, y, "Messages", MESSAGES)
    y = section(p, y, "MIDI", MIDI)
    y = section(p, y, "Controls",
                [f"{nm} ({PERF_DEFAULT.get(nm, 'patch')}): {PERF_HELP[nm]}." for nm in PERF],
                para=True)
    y = section(p, y, "Controller layer and editor", LAYER, para=True)
    y = section(p, y, "Data files", DATA, para=True)

    # ---- presets, with live objects
    if p:
        p.heading(CX, y, "Presets", 14)
    y += 28
    if p:
        s = p.obj(CX + 400, y + 24, f"s {BUS}")
        for k, (nm, hi) in enumerate((("preset", 127), ("user", 63))):
            a = p.atom(CX + k * 100, y, 4, 0, hi)
            m = p.msg(CX + k * 100, y + 24, f"{nm} {DOLLAR}1")
            p.conn(a, 0, m, 0)
            p.conn(m, 0, s, 0)
        r = p.obj(CX, y + 60, f"r {BUS}-out")
        ro = p.obj(CX, y + 84, "route preset user")
        p.conn(r, 0, ro, 0)
        for k in range(2):
            un = p.obj(CX + k * 200, y + 110, "unpack f s")
            fa = p.atom(CX + k * 200, y + 138, 4, 0, 0)
            sa = p.add(f"#X symbolatom {CX + 40 + k * 200} {y + 138} 20 0 0 0 - - - 0;")
            p.conn(ro, k, un, 0)
            p.conn(un, 0, fa, 0)
            p.conn(un, 1, sa, 0)
    y += 166
    if p:
        p.comment(TX, y, PRESETS, para=True)
    y += LINE * nlines(PRESETS) + GAP + 10

    # ---- settings blocks
    y = draw_block(p, *EDIT_BLOCK, y)
    y = draw_block(p, *MEMORY_BLOCK, y)
    y = section(p, y, "SysEx", SYSEX, para=True)
    for title, groups, examples, paras in BLOCKS:
        y = draw_block(p, title, groups, examples, paras, y)

    # ---- every control, with its CC number
    y = section(p, y, "Performance and setup controls", perf_lines())
    y = section(p, y, "Partial controls: the selected partial", param_lines(PARTIAL_DOC, T.PARTIAL))
    y = section(p, y, "Tone controls: its tone", param_lines(TONE_DOC, T.TONE))
    patch_doc = [d for d in PATCH_DOC]
    y = section(p, y, "Patch controls", param_lines(patch_doc, T.PATCH))

    y += 6
    if p:
        p.comment(TX, y, [CREDITS], para=True, semi=True)
    y += LINE * nlines([CREDITS]) + M
    return y


def main():
    bottom = build(None)
    p = Patch()
    p.cnv(YEL_X, YEL_Y, YEL_W, bottom - YEL_Y)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bottom - BLUE_TOP, bg=BLUE)
    assert build(p) == bottom
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                       OBJ, f"{OBJ}-help.pd")
    with open(out, "w") as f:
        f.write(f"#N canvas 348 20 820 {bottom + M} 12;\n")
        f.write("\n".join(p.lines) + "\n")
        f.write("\n".join(p.conns) + "\n")
    print(f"wrote {os.path.normpath(out)} ({len(p.lines)} objects, "
          f"{len(p.conns)} connections, height {bottom + M})")


if __name__ == "__main__":
    main()

