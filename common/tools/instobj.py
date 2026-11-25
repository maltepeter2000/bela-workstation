#!/usr/bin/env python3
"""The instrument object <x>.i~: an external with its editor, controller layer, Launchkey pads
and display, MIDI in and out, in one Pd file. The same module for every instrument; the
instrument's own generator (tools/gen_<x>_inst.py) only calls write() with its names.

    [<x>.i~ <channel> <pads channel>]
    |        |        \\
    [dac~     ]        (messages: info ..., display ...)

  argument 1   MIDI channel of the external and the controller layer (0 or none = 1)
  argument 2   channel of the Launchkey pads (0 or none = 10, the Drums Channel)
  inlet        notes and messages, the same as inlet 1 of the external
  outlet 1, 2  left, right
  outlet 3     the messages of the external's last outlet (info, name, display ...), without
               MIDI bytes (floats), which go to [midiout] inside

The window of the object is the editor (its top canvas; its title becomes <x>.i~). The wiring
lies below it, aligned with it, grouped, every wire running downwards (checked here). The parts
talk over a bus local to the instance ($0-<short> / $0-<short>-out), so several instances do
not get in each other's way. The other parts (controller layer, pads, pad translator, display)
become subpatches; their own $0 names get a prefix, so they stay apart as they were as
abstractions.

User banks kept by the external (banks=[(write, read, file), ...]): each file beside the patch
that holds the object (Pd's [pdcontrol] dir 1). At load it is read if it is there; otherwise the
bank beside the external stays (the starting point). The write / read messages without a file
name use that file, with a file name they go on unchanged. save = store, then the first write.
banks=[] leaves all of that to the external or the editor.

MIDI in: ("raw", inlet) raw bytes from [midiin] into that inlet, or ("notein",) notes as lists
(note velocity channel) and bend as "bend -1..1" into inlet 1.
"""
import re

OBJ_KINDS = ("obj", "msg", "text", "floatatom", "symbolatom", "listbox")


def read(path):
    with open(path) as fh:
        lines = fh.read().splitlines()
    assert lines and lines[0].startswith("#N canvas"), path
    return lines


def top_objects(lines):
    """(x, y) of every object on the top canvas, in Pd's order, and the top-level connections"""
    depth, objs, conns = 0, [], []
    for ln in lines[1:]:
        if ln.startswith("#N canvas"):
            depth += 1
            continue
        if ln.startswith("#X restore"):
            depth -= 1
            if depth == 0:
                p = ln.split()
                objs.append((int(p[2]), int(p[3])))
            continue
        if depth:
            continue
        p = re.split(r"(?<!\\) ", ln.rstrip(";"))
        if len(p) > 3 and p[0] == "#X" and p[1] in OBJ_KINDS:
            objs.append((int(p[2]), int(p[3])))
        elif len(p) > 5 and p[0] == "#X" and p[1] == "connect":
            conns.append(tuple(int(v) for v in p[2:6]))
    return objs, conns


def as_subpatch(lines, name, x, y):
    """an abstraction's lines as [pd name] (closed); its $0 names get the prefix name"""
    m = re.match(r"#N canvas (\d+) (\d+) (\d+) (\d+) \d+;$", lines[0])
    assert m, lines[0]
    body = [ln.replace("\\$0-", f"\\$0-{name}-") for ln in lines[1:]]
    return [f"#N canvas {m[1]} {m[2]} {m[3]} {m[4]} {name} 0;"] + body + [f"#X restore {x} {y} pd {name};"]


def subst(lines, rules, name):
    """regex replacements on whole lines, each must hit at least once"""
    out, hits = [], [0] * len(rules)
    for ln in lines:
        for k, (pat, rep) in enumerate(rules):
            ln, c = re.subn(pat, lambda m, rep=rep: m[1] + rep, ln)
            hits[k] += c
        out.append(ln)
    for (pat, _), c in zip(rules, hits):
        assert c, f"{name}: no match for {pat}"
    return out


def bank_rewrite(lines, f):
    """[ufread f <default> <text>] and [write f( -> the file beside the patch ($0-path0)"""
    out = []
    uf = re.compile(r"^#X obj (\d+) (\d+) ufread " + re.escape(f) + r" (\S+) (\S+);$")
    wr = re.compile(r"^#X msg (\d+) (\d+) write " + re.escape(f) + ";$")
    hits = 0
    for ln in lines:
        m = uf.match(ln)
        if m:
            x, y, default, text = m[1], m[2], m[3], m[4]
            out += ["#N canvas 0 50 460 300 bankread 0;",
                    "#X text 10 270 the user bank beside the patch \\, else as before (ufread), f 60;",
                    "#X obj 10 10 inlet;", "#X obj 10 40 t b;", "#X obj 150 10 r \\$0-path0;",
                    "#X obj 150 40 t b s;", "#X obj 10 70 symbol;", "#X obj 10 100 file isfile;",
                    "#X obj 10 130 t b;", "#X obj 10 160 symbol;", "#X obj 10 190 list prepend read;",
                    "#X obj 10 220 list trim;", "#X obj 10 250 s " + text + ";",
                    f"#X obj 200 160 ufread {f} {default} {text};",
                    "#X connect 1 0 2 0;", "#X connect 2 0 5 0;", "#X connect 3 0 4 0;",
                    "#X connect 4 1 5 1;", "#X connect 4 1 8 1;", "#X connect 4 0 5 0;",
                    "#X connect 5 0 6 0;", "#X connect 6 0 7 0;", "#X connect 7 0 8 0;",
                    "#X connect 8 0 9 0;", "#X connect 9 0 10 0;", "#X connect 10 0 11 0;",
                    "#X connect 6 1 12 0;",
                    f"#X restore {x} {y} pd bankread;"]
            hits += 1
            continue
        m = wr.match(ln)
        if m:
            out += ["#N canvas 0 50 360 220 bankwrite 0;",
                    "#X obj 10 10 inlet;", "#X obj 10 40 t b;", "#X obj 120 10 r \\$0-path0;",
                    "#X obj 10 70 symbol;", "#X obj 10 100 list prepend write;",
                    "#X obj 10 130 list trim;", "#X obj 10 160 outlet;",
                    "#X connect 0 0 1 0;", "#X connect 1 0 3 0;", "#X connect 2 0 3 1;",
                    "#X connect 3 0 4 0;", "#X connect 4 0 5 0;", "#X connect 5 0 6 0;",
                    f"#X restore {m[1]} {m[2]} pd bankwrite;"]
            hits += 1
            continue
        out.append(ln)
    assert hits >= 2, f"{f}: ufread / write not found"
    return out


def build(obj, ext, short, edit, cc, edit_title, banks, pads=None, nav=None, lkdisplay=None,
          midi=("raw", 2), pads_channel_rule=True, more_edits=(), save=True, store_writes=False,
          editor_bank=None, channel_msg="channel"):
    """obj: 'juno.i~', ext: the external with its arguments ('juno~'), short: bus name,
    edit / cc / pads / nav / lkdisplay: paths of the generated abstractions,
    edit_title: the title of the bare editor ('juno~ editor'), more_edits: [(path, name)] more
    editors on the same bus, as subpatches [pd name] (click to open). save: 'save' becomes store,
    then the first write (False: 'save' goes to the external unchanged, e.g. tx81z~'s battery RAM).
    store_writes: after every store the first bank is written beside the patch as well (for an
    external whose store writes its own file). editor_bank: the file of a user bank kept in Pd
    by the editor and the pad translator with [ufread <file> <default> <text>] and [write <file>(
    (pdoly8~): those become the file beside the patch (ufread as before when it is not there).
    channel_msg: the external's message for the receive channel ('ch1' for pdoly8~)."""
    bus = "\\$0-" + short
    extname = ext.split()[0]
    title_re = r"^(#X obj \d+ \d+ cnv 10 10 10 empty empty )" + re.escape(edit_title.replace(" ", "\\ ")) + "(?= )"
    ed = subst(read(edit), [
        (r"^(#X obj \d+ \d+ s )\\\$1;$", bus + ";"),
        (r"^(#X obj \d+ \d+ r )\\\$1-out;$", bus + "-out;"),
        (title_re, obj),
    ], edit)
    ccl = read(cc)                               # $1 = channel, the same as argument 1
    pdl = None
    if pads:
        pdl = read(pads)
        if pads_channel_rule:                    # the pads' channel is argument 2
            pdl = subst(pdl, [(r"^(#X obj \d+ \d+ f )\\\$1;$", "\\$2;")], pads)
    nvl = read(nav) if nav else None
    if editor_bank:
        ed = bank_rewrite(ed, editor_bank)
    lkl = read(lkdisplay) if lkdisplay else None

    n = len(top_objects(ed)[0])
    head = re.match(r"#N canvas (\d+) (\d+) (\d+) (\d+) (\d+);$", ed[0])
    width = int(head[3])
    y0 = int(head[4]) + 20                       # below the window: the editor's own height
    lines = [f"#N canvas {head[1]} {head[2]} {head[3]} {head[4]} {head[5]};"] + ed[1:]
    conns, blocks = [], []

    def add(block):
        blocks.append(block if isinstance(block, list) else [block])
        return n + len(blocks) - 1

    def con(a, o, b, i):
        conns.append(f"#X connect {a} {o} {b} {i};")

    def note(x, y, text, f):
        return add(f"#X text {x} {y} {text}, f {f};")

    X, Y = 10, y0
    XR = X + 390                                 # the right column
    parts = " \\, controller layer" + (" \\, pads" if pads else "") + (" \\, display" if lkdisplay else "")
    args = "<channel> <pads channel>" if pads else "<channel>"
    argtext = "Argument 1: MIDI channel (0 or none = 1)"
    if pads:
        argtext += " \\, argument 2: Launchkey pads (0 or none = 10)"
    files = " and ".join([f for _, _, f in banks] + ([editor_bank] if editor_bank else []))
    nfiles = len(banks) + (1 if editor_bank else 0)
    banktext = (f" The user bank{'s are' if nfiles > 1 else ' is'} {files} beside the patch that "
                f"holds {obj}." if nfiles else "")
    note(X, Y, f"{obj} {args}: {extname} with the editor above{parts} \\, MIDI in and out. "
         f"{argtext}.{banktext} Generated by common/tools/instobj.py \\, do not edit by hand.", 104)
    Y += 70

    # ---- messages in (left): the bank messages without a file -> the files beside the patch
    if banks:
        note(X, Y, "messages in: save and the bank messages without a file use the files beside the patch", 60)
    else:
        note(X, Y, "messages in", 60)
    Y += 40
    inl = add(f"#X obj {X} {Y} inlet;")
    rb = add(f"#X obj {X + 60} {Y} r {bus};")
    to_ext = []                                  # (object, outlet) -> external inlet 1
    if banks:
        sel = (["save"] if save else []) + (["store"] if store_writes else []) + \
            [m for w, r, _ in banks for m in (w, r)]
        ru = add(f"#X obj {X} {Y + 30} route {' '.join(sel)};")
        for a in (inl, rb):
            con(a, 0, ru, 0)
        to_ext.append((ru, len(sel)))
        first_write = []                         # what bangs the path of the first bank
        if save:
            sv = add(f"#X obj {X} {Y + 60} t b b;")
            st = add(f"#X msg {X + 30} {Y + 120} store;")
            con(ru, sel.index("save"), sv, 0); con(sv, 1, st, 0)
            to_ext.append((st, 0))
            first_write.append((sv, 0))
        if store_writes:                         # store [n] on, then the file beside the patch
            tb_ = add(f"#X obj {X} {Y + 88} t b a;")
            lp = add(f"#X obj {X} {Y + 150} list prepend store;")
            lt = add(f"#X obj {X} {Y + 175} list trim;")
            con(ru, sel.index("store"), tb_, 0); con(tb_, 1, lp, 0); con(lp, 0, lt, 0)
            to_ext.append((lt, 0))
            first_write.append((tb_, 0))
        off = len(sel) - 2 * len(banks)
        for k, (w, r, _) in enumerate(banks):
            by = Y + 60 + k * 100
            bw = add(f"#X obj {X + 70} {by} route bang;")
            br = add(f"#X obj {X + 150} {by} route bang;")
            rp = add(f"#X obj {X + 240} {by} r \\$0-path{k};")
            sw = add(f"#X obj {X + 70} {by + 35} symbol;")    # bare: the path
            aw = add(f"#X obj {X + 120} {by + 35} symbol;")   # with a file name: that one
            sr = add(f"#X obj {X + 170} {by + 35} symbol;")
            ar = add(f"#X obj {X + 220} {by + 35} symbol;")
            mw = add(f"#X msg {X + 70} {by + 65} {w} \\$1;")
            mr = add(f"#X msg {X + 170} {by + 65} {r} \\$1;")
            con(ru, off + 2 * k, bw, 0); con(ru, off + 1 + 2 * k, br, 0)
            if k == 0:
                for a, o in first_write:         # save / store: then the first bank written
                    con(a, o, sw, 0)
            con(bw, 0, sw, 0); con(bw, 1, aw, 0)
            con(br, 0, sr, 0); con(br, 1, ar, 0)
            con(rp, 0, sw, 1); con(rp, 0, sr, 1)
            con(sw, 0, mw, 0); con(aw, 0, mw, 0)
            con(sr, 0, mr, 0); con(ar, 0, mr, 0)
            to_ext += [(mw, 0), (mr, 0)]
        my = Y + 60 + len(banks) * 100 + (60 if store_writes else 0)
    else:
        to_ext += [(inl, 0), (rb, 0)]
        my = Y + 40

    # ---- MIDI in (left, above the external)
    if midi[0] == "raw":
        mi = add(f"#X obj {X + 290} {my - 35} midiin;")
        midi_src = [(mi, 0, midi[1])]
    else:
        ni = add(f"#X obj {X + 130} {my - 35} notein;")
        pk = add(f"#X obj {X + 130} {my - 10} pack f f f;")
        bi = add(f"#X obj {X + 215} {my - 35} bendin;")
        bx = add(f"#X obj {X + 215} {my - 10} expr (\\$f1 - 8192) / 8192;")
        bm = add(f"#X msg {X + 215} {my + 15} bend \\$1;")
        for k in range(3):
            con(ni, k, pk, k)
        con(bi, 0, bx, 0); con(bx, 0, bm, 0)
        midi_src = [(pk, 0, 0), (bm, 0, 0)]
        my += 15

    # ---- the external and the outlets (left)
    jy = my + 30
    jn = add(f"#X obj {X} {jy} {ext};")
    for a, o in to_ext:
        con(a, o, jn, 0)
    for a, o, i in midi_src:
        con(a, o, jn, i)
    o1 = add(f"#X obj {X} {jy + 60} outlet~;")
    o2 = add(f"#X obj {X + 60} {jy + 60} outlet~;")
    rf = add(f"#X obj {X + 130} {jy + 30} route float;")
    mo = add(f"#X obj {X + 130} {jy + 60} midiout;")
    so = add(f"#X obj {X + 210} {jy + 60} s {bus}-out;")
    o3 = add(f"#X obj {X + 320} {jy + 60} outlet;")
    con(jn, 0, o1, 0); con(jn, 1, o2, 0); con(jn, 2, rf, 0)
    con(rf, 0, mo, 0)                            # MIDI bytes: SysEx, thru
    con(rf, 1, so, 0); con(rf, 1, o3, 0)

    # ---- at load (right): channel; per bank its path, read it if it is there
    ly = Y - 40
    note(XR, ly, "at load: MIDI channel" + (" \\, user bank" if banks or editor_bank else ""), 40)
    ly += 40
    lb = add(f"#X obj {XR} {ly} loadbang;")
    tl = add(f"#X obj {XR} {ly + 25} t b b;")
    fa = add(f"#X obj {XR + 80} {ly + 50} f \\$1;")
    mx = add(f"#X obj {XR + 80} {ly + 75} max 1;")
    ch = add(f"#X msg {XR + 80} {ly + 100} {channel_msg} \\$1;")
    sc1 = add(f"#X obj {XR + 80} {ly + 125} s {bus};")
    con(lb, 0, tl, 0)
    con(tl, 1, fa, 0); con(fa, 0, mx, 0); con(mx, 0, ch, 0); con(ch, 0, sc1, 0)
    cy = ly + 160
    if editor_bank:
        dm = add(f"#X msg {XR} {ly + 50} dir 1;")
        pc = add(f"#X obj {XR} {ly + 75} pdcontrol;")
        mf = add(f"#X obj {XR} {ly + 155} makefilename %s/{editor_bank};")
        sp_ = add(f"#X obj {XR} {ly + 180} s \\$0-path0;")
        con(tl, 0, dm, 0); con(dm, 0, pc, 0); con(pc, 0, mf, 0); con(mf, 0, sp_, 0)
        cy = ly + 215
    if banks:
        dm = add(f"#X msg {XR} {ly + 50} dir 1;")
        pc = add(f"#X obj {XR} {ly + 75} pdcontrol;")
        con(tl, 0, dm, 0); con(dm, 0, pc, 0)
        fan = add(f"#X obj {XR} {ly + 155} t " + " ".join(["s"] * len(banks)) + ";")
        con(pc, 0, fan, 0)
        for k, (_, r, f) in enumerate(banks):
            bx = XR                              # one chain below the other
            by = ly + 185 + k * 130
            mf = add(f"#X obj {bx} {by} makefilename %s/{f};")
            con(fan, len(banks) - 1 - k, mf, 0)
            ts = add(f"#X obj {bx} {by + 25} t s s;")
            fi = add(f"#X obj {bx} {by + 50} file isfile;")
            sp_ = add(f"#X obj {bx + 120} {by + 50} s \\$0-path{k};")
            ur = add(f"#X msg {bx} {by + 75} {r};")
            sc2 = add(f"#X obj {bx} {by + 100} s {bus};")
            con(mf, 0, ts, 0)
            con(ts, 1, sp_, 0)                   # first the path
            con(ts, 0, fi, 0); con(fi, 0, ur, 0); con(ur, 0, sc2, 0)   # then: read it, if there
        cy = ly + 185 + len(banks) * 130

    # ---- controller layer, pads, pad translator, Launchkey display (right, below): by the bus
    cy = max(cy, jy + 40)
    note(XR, cy, "controller layer" + (" \\, pads" if pads else "") + (" \\, display" if lkdisplay else ""), 40)
    cy += 30
    ro = add(f"#X obj {XR} {cy} r {bus}-out;")
    pcc = add(as_subpatch(ccl, "cc", XR, cy + 30))
    sc = add(f"#X obj {XR} {cy + 60} s {bus};")
    con(ro, 0, pcc, 0); con(pcc, 0, sc, 0)       # info in, n <name> <value> out
    disp = (ro, 0)                               # what the display shows
    xp = XR + max(70, 6 * len(f"r $0-{short}-out") + 20)   # beside the receive, not over it
    if pdl:
        ppd = add(as_subpatch(pdl, "pads", xp, cy))
        if nvl:                                  # pads -> translator -> instrument, display
            nsub = as_subpatch(nvl, "nav", xp, cy + 30)
            if editor_bank:
                nsub = nsub[:1] + bank_rewrite(nsub[1:-1], editor_bank) + nsub[-1:]
            pnv = add(nsub)
            sp = add(f"#X obj {xp} {cy + 60} s {bus};")
            con(ppd, 0, pnv, 0); con(ro, 0, pnv, 1); con(pnv, 0, sp, 0)
            disp = (pnv, 1)
        else:
            sp = add(f"#X obj {xp} {cy + 30} s {bus};")
            con(ppd, 0, sp, 0)                   # pads (their save goes to the bank beside the patch)
    if lkl:
        plk = add(as_subpatch(lkl, "lkdisplay", xp + 80, cy + 90))
        ml = add(f"#X obj {xp + 80} {cy + 120} midiout;")
        con(disp[0], disp[1], plk, 0); con(plk, 0, ml, 0)
        if pdl:
            con(ppd, 1, plk, 0)                  # the pads' own display lines (own pads, init armed)

    # ---- more editors on the same bus (right, below): click to open
    if more_edits:
        ey = cy + 170
        note(XR, ey, "more editors on the same bus: click to open", 50)
        for k, (path, name) in enumerate(more_edits):
            el = subst(read(path), [(r"^(#X obj \d+ \d+ s )\\\$1;$", bus + ";"),
                                    (r"^(#X obj \d+ \d+ r )\\\$1-out;$", bus + "-out;")], path)
            add(as_subpatch(el, name, XR + k * 100, ey + 30))

    for b in blocks:
        lines += b
    out = lines + conns
    check(out, n, width)
    return out


def check(lines, first, width):
    """every wire of the wiring part runs downwards; everything within the editor's width"""
    objs, conns = top_objects(lines)
    for a, _, b, _ in conns:
        if a >= first or b >= first:
            assert objs[b][1] > objs[a][1], f"wire {a} -> {b} does not run downwards"
    for x, _ in objs[first:]:
        assert 10 <= x <= width, f"object at x {x} outside the editor"


def write(path, **kw):
    out = build(**kw)
    with open(path, "w") as fh:
        fh.write("\n".join(out) + "\n")
    print("wrote", path)
