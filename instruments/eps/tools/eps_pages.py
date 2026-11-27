#!/usr/bin/env python3
"""The Launchkey surface of eps~: the encoder pages P1-P2 (PLAY) and E1-E19 (EDIT), the eight
EDIT areas and the pads, in one table. pd/engine/eps_pages.h, the help and the editor read it.

  PLAY  P1 PERFORM, P2 PLAY: the controls of eps_params.py (offsets over the instrument), on
        CC page 1 and 2 of the controller layer (the same numbers for the Bela)
  EDIT  E1-E20: the parameters of the loaded instrument (eps_edit_params.py), the sampler (E18)
        the wavesample commands (E19) and the effect (E20); the area pads choose the page, the encoders send
        whatever CC page the Launchkey is on: encoder n is always cell n of the page shown

A cell: (short name for the legend, <= 4; long name for the display, <= 10; target)
  c:<control>   a control of eps_params.py        e:<param>  a parameter of eps_edit_params.py
  r:<setting>   rec <setting> (the wave recorder)  x:<arg>    cmd <arg> (the wavesample commands)
  k:<command>   turned: that wavesample command chosen on the COMMAND page (CMD twice runs it)
  a:<message>   turned: the message is sent (copy and paste the envelope)
  f:<k>         parameter k of the effect page shown (fxcell k)
  -             not there yet (shown as --)        None       empty
usage: python3 tools/eps_pages.py   (writes pd/engine/eps_pages.h)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import eps_params as T                            # noqa: E402
import eps_edit_params as E                       # noqa: E402

# id, title (<= 10), area (-1 = PLAY, else the EDIT area), cells
PAGES = [
    ("P1", "PERFORM", -1, [("CUT", "CUTOFF", "c:cut"), ("CUT2", "CUTOFF 2", "c:cut2"),
                           ("FATT", "F ATTACK", "c:fatt"), ("FDEC", "F DECAY", "c:fdec"),
                           ("FREL", "F RELEASE", "c:frel"), ("AATT", "A ATTACK", "c:att"),
                           ("ADEC", "A DECAY", "c:dec"), ("AREL", "A RELEASE", "c:rel")]),
    ("P2", "PLAY", -1, [("VOL", "VOLUME", "c:volume"), ("TRNS", "TRANSPOSE", "c:transpose"),
                        ("WAVE", "WAVE POS", "c:wave"), ("FENV", "FILTER ENV", "c:fenv"),
                        ("FX", "FX MIX", "c:fxmix"), None, None, None]),
    ("E1", "INST", 0, [("TRNS", "TRANSPOSE", "e:i_trans"), ("KLO", "KEY LOW", "e:i_keylo"),
                       ("KHI", "KEY HIGH", "e:i_keyhi"), ("PRES", "PRESSURE", "e:i_press"),
                       ("PMOD", "PATCH MODE", "e:i_psel"), ("PROG", "MIDI PROG", "e:i_prog"),
                       ("MOUT", "MIDI OUT", "e:i_mout"), ("XCTL", "XCTRL CC", "c:xcc")]),
    ("E2", "LAYER", 1, [("VLO", "VEL LOW", "e:l_vello"), ("VHI", "VEL HIGH", "e:l_velhi"),
                        ("GLMD", "GLIDE MODE", "e:l_glmode"), ("GLTM", "GLIDE TIME", "e:l_gltime"),
                        ("DLY", "DELAY", "e:l_delay"), ("DVEL", "DELAY VEL", "e:l_dlyvel"),
                        ("RSTK", "RESTRIKE", "e:l_restrk"), ("LEG", "LEGATO", "e:l_legato")]),
    ("E3", "PATCHES", 1, [("IN00", "IN 00", "e:l_in00"), ("IN0*", "IN 0*", "e:l_in0x"),
                          ("IN*0", "IN *0", "e:l_inx0"), ("IN**", "IN **", "e:l_inxx"),
                          ("KYUP", "KEY UP", "e:l_keyup"), ("STLK", "STEREO LNK", "e:l_stlink"),
                          ("PTBL", "PITCH TBL", "e:l_ptable"), None]),
    ("E4", "WAVE", 2, [("STRT", "SMP START", "e:w_sst"), ("STF", "START FINE", "e:w_sstf"),
                       ("END", "SMP END", "e:w_send"), ("ENF", "END FINE", "e:w_sendf"),
                       ("LSTR", "LOOP START", "e:w_lst"), ("LSF", "LSTRT FINE", "e:w_lstf"),
                       ("LEND", "LOOP END", "e:w_lend"), ("LEF", "LEND FINE", "e:w_lendf")]),
    ("E5", "LOOP", 2, [("LOOP", "LOOP MODE", "e:w_loop"), ("LPOS", "LOOP POS", "e:w_lpos"),
                       ("LPF", "LPOS FINE", "e:w_lposf"), ("ALF", "AUTO LOOP", "c:alf"),
                       ("KLO", "WS KEY LOW", "e:w_keylo"), ("KHI", "WS KEY HI", "e:w_keyhi"),
                       None, None]),
    ("E6", "WAVE MOD", 2, [("WMSR", "WAVE SRC", "e:w_wmsrc"), ("WMAM", "WAVE AMT", "e:w_wmamt"),
                           ("WMTY", "WAVE TYPE", "e:w_wmtype"), ("WMRG", "WAVE RANGE", "e:w_wmrng"),
                           None, None, None, None]),
    ("E7", "PITCH", 3, [("ROOT", "ROOT KEY", "e:w_root"), ("FINE", "FINE TUNE", "e:w_fine"),
                        ("BEND", "BEND RANGE", "e:w_bend"), ("PENV", "ENV1>PITCH", "e:w_penv"),
                        ("PLFO", "LFO>PITCH", "e:w_plfo"), ("PSRC", "PITCH SRC", "e:w_psrc"),
                        ("PAMT", "PITCH AMT", "e:w_pamt"), ("RAND", "RANDOM", "e:w_rand")]),
    ("E8", "PITCH TBL", 3, [("TBL", "TABLE", "-"), ("KEY", "TABLE KEY", "-"),
                            ("SEMI", "SEMITONE", "-"), ("FINE", "TABLE FINE", "-"),
                            None, None, None, None]),
    ("E9", "FILTER", 4, [("MODE", "FILT MODE", "e:w_fmode"), ("CUT1", "CUTOFF 1", "e:w_cut1"),
                         ("KEY1", "KEY 1", "e:w_key1"), ("ENV1", "ENV 1", "e:w_env1"),
                         ("CUT2", "CUTOFF 2", "e:w_cut2"), ("KEY2", "KEY 2", "e:w_key2"),
                         ("ENV2", "ENV 2", "e:w_env2"), None]),
    ("E10", "FILT MOD", 4, [("SRC1", "F1 SOURCE", "e:w_fsrc1"), ("AMT1", "F1 AMOUNT", "e:w_famt1"),
                            ("SRC2", "F2 SOURCE", "e:w_fsrc2"), ("AMT2", "F2 AMOUNT", "e:w_famt2"),
                            None, None, None, None]),
    ("E11", "AMP", 5, [("VOL", "WS VOLUME", "e:w_vol"), ("PAN", "PAN", "e:w_pan"),
                       ("BOOS", "BOOST", "e:w_boost"), ("ASRC", "AMP SRC", "e:w_asrc"),
                       ("AAMT", "AMP AMT", "e:w_aamt"), ("PNSR", "PAN SRC", "e:w_pnsrc"),
                       ("PNAM", "PAN AMT", "e:w_pnamt"), ("BUS", "OUT BUS", "e:w_bus")]),
    ("E12", "XFADE", 5, [("XFA", "XFADE A", "e:w_xfa"), ("XFB", "XFADE B", "e:w_xfb"),
                         ("XFC", "XFADE C", "e:w_xfc"), ("XFD", "XFADE D", "e:w_xfd"),
                         ("XFON", "XFADE ON", "e:w_xfcrv"), None, None, None]),
    ("E13", "LFO", 6, [("WAVE", "LFO WAVE", "e:w_lwave"), ("SPD", "LFO SPEED", "e:w_lspeed"),
                       ("DPTH", "LFO DEPTH", "e:w_ldepth"), ("DLY", "LFO DELAY", "e:w_ldelay"),
                       ("MODE", "LFO MODE", "e:w_lmode"), ("RSRC", "RATE SRC", "e:w_lrsrc"),
                       ("RAMT", "RATE AMT", "e:w_lramt"), ("RNDF", "RAND FREQ", "e:w_rfreq")]),
    ("E14", "LFO DEPTH", 6, [("DSRC", "DEPTH SRC", "e:w_ldsrc"), ("DAMT", "DEPTH AMT", "e:w_ldamt"),
                             None, None, None, None, None, None]),
    ("E15", "ENV", 7, [("T1", "TIME 1", "e:e_t1"), ("T2", "TIME 2", "e:e_t2"),
                       ("T3", "TIME 3", "e:e_t3"), ("T4", "TIME 4", "e:e_t4"),
                       ("L1", "LEVEL 1", "e:e_l1h"), ("L2", "LEVEL 2", "e:e_l2h"),
                       ("L3", "LEVEL 3", "e:e_l3h"), ("L4", "LEVEL 4", "e:e_l4h")]),
    ("E16", "RELEASE", 7, [("T5", "RELEASE 1", "e:e_t5"), ("T6", "RELEASE 2", "e:e_t6"),
                           ("L0", "LEVEL 0", "e:e_l0h"), ("L5", "REL LEVEL", "e:e_l5"),
                           ("MODE", "ENV MODE", "e:e_mode"), ("TYPE", "SOFT CURVE", "e:e_type"),
                           ("PSTE", "PASTE ENV", "a:env paste"), ("COPY", "COPY ENV", "a:env copy")]),
    ("E17", "SOFT", 7, [("L0S", "L0 SOFT", "e:e_l0s"), ("L1S", "L1 SOFT", "e:e_l1s"),
                        ("L2S", "L2 SOFT", "e:e_l2s"), ("L3S", "L3 SOFT", "e:e_l3s"),
                        ("L4S", "L4 SOFT", "e:e_l4s"), ("T1V", "ATTACK VEL", "e:e_t1vel"),
                        ("KBS", "KEY SCALE", "e:e_kbs"), ("VSW", "VEL SWITCH", "e:e_vsw")]),
    ("E18", "SAMPLE", 8, [("RATE", "RATE", "r:rate"), ("THRS", "THRESHOLD", "r:thresh"),
                          ("PRE", "PRE TRIG", "r:pre"), ("IN", "INPUT", "r:input"),
                          ("DEST", "DEST", "r:dest"), ("MON", "MONITOR", "r:monitor"),
                          None, None]),
    ("E19", "COMMAND", 9, [("CMD", "COMMAND", "x:cmd"), ("RNG", "RANGE", "x:range"),
                           ("DPTH", "DEPTH", "x:depth"), ("ZONE", "FADE ZONE", "x:zone"),
                           ("SCL1", "SCALE STRT", "x:sstart"), ("SCL2", "SCALE END", "x:send"),
                           ("RATE", "NEW RATE", "x:rate"), ("OPT", "OPTION", "x:opt")]),
    ("E20", "EFFECT", 10, [(f"FX{k + 1}", f"FX {k + 1}", f"f:{k}") for k in range(8)]),
]

# The EDIT areas: the top row of pads 36-43 (pressed again: the next page of the area); 8 and 9
# are the pages of the SMPL and CMD pads, 10 the effect: INST pressed again (E1, then the pages
# of eight effect parameters, then E1 again). Its cells are the effect's parameters, named by
# the effect loaded (f:<k> = parameter k of the effect page shown).
AREAS = ["INST", "LAYER", "WAVE", "PITCH", "FILTER", "AMP", "LFO", "ENV", "SAMPLE", "COMMAND",
         "EFFECT"]

# The pads (notes 36-51 on the pad channel): name for the legend (<= 5)
# 51 (bottom right) is MODE in both layers: PLAY <-> EDIT, blue in PLAY, purple in EDIT
PADS_PLAY = {36: ">", 38: "INIT", 39: "PAT>", 41: "SMPL", 42: "OPEN",
             44: "<", 46: "SAVE", 47: "PAT<", 48: "SEL", 50: "EXIT", 51: "MODE"}
PADS_EDIT = {36 + k: AREAS[k] for k in range(8)}
PADS_EDIT.update({44: "SEL", 45: "LAY>", 46: "WS>", 47: "ENV#", 48: "O/N", 49: "CMD",
                  50: "SMPL", 51: "MODE"})

REC = {"rate", "thresh", "pre", "input", "dest", "monitor"}
CMD = {"cmd", "range", "depth", "zone", "sstart", "send", "rate", "opt"}

# checks
assert len(AREAS) == 11
for pid, title, area, cells in PAGES:
    assert len(cells) == 8 and len(title) <= 10, pid
    assert area < 0 or area < len(AREAS)
    for c in cells:
        if c is None:
            continue
        s, ln, tg = c
        assert len(s) <= 4 and len(ln) <= 10, c
        k, _, n = tg.partition(":")
        assert tg == "-" or (k == "c" and n in T.CONTROLS) or (k == "e" and n in E.P) or \
            (k == "r" and n in REC) or (k == "x" and n in CMD) or (k in "ka" and n) or \
            (k == "f" and n.isdigit()), c
for a in range(len(AREAS)):
    assert any(p[2] == a for p in PAGES), AREAS[a]
for _p in list(PADS_PLAY.values()) + list(PADS_EDIT.values()):
    assert len(_p) <= 7, _p


def area_pages(a):
    return [i for i, p in enumerate(PAGES) if p[2] == a]


def write_header(path):
    L = ["// Generated by tools/eps_pages.py -- do not edit.",
         "// The Launchkey surface of eps~: pages P1-P2 (PLAY) and E1-E19 (EDIT), cells of",
         "// (short name, long name, target kind, target); kind c control, e edit parameter,",
         "// r rec setting, x cmd argument, k a command to choose, a a message to send, '-' not",
         "// there yet, 0 empty.",
         "#pragma once",
         "namespace eps {",
         "struct PageCell { const char *shortName, *longName; char kind; const char *target; };",
         "struct Page { const char *id, *title; int area; PageCell cell[8]; };",
         "static const Page kPages[] = {"]
    for pid, title, area, cells in PAGES:
        cs = []
        for c in cells:
            if c is None:
                cs.append('{"", "", 0, ""}')
            else:
                s, ln, tg = c
                k, _, n = tg.partition(":")
                cs.append(f'{{"{s}", "{ln}", \'{k}\', "{n}"}}')
        L.append(f'    {{"{pid}", "{title}", {area}, {{' + ", ".join(cs) + "}},")
    L.append("};")
    L.append(f"constexpr int kPageCount = {len(PAGES)};")
    L.append("struct Area { const char *name; int first, count; };")
    L.append("static const Area kAreas[] = {")
    for a, nm in enumerate(AREAS):
        ps = area_pages(a)
        assert ps == list(range(ps[0], ps[0] + len(ps)))
        L.append(f'    {{"{nm}", {ps[0]}, {len(ps)}}},')
    L.append("};")
    L.append(f"constexpr int kAreaCount = {len(AREAS)};")
    L.append(f"constexpr int kAreaSample = {AREAS.index('SAMPLE')}, kAreaCommand = {AREAS.index('COMMAND')}, "
             f"kAreaEffect = {AREAS.index('EFFECT')};")
    for nm, tab in (("kPadsPlay", PADS_PLAY), ("kPadsEdit", PADS_EDIT)):
        L.append(f"static const char *const {nm}[16] = {{" +
                 ", ".join(f'"{tab.get(36 + i, "")}"' for i in range(16)) + "};")
    L.append("} // namespace eps")
    open(path, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "engine", "eps_pages.h")
    write_header(out)
    print("wrote", out, f"({len(PAGES)} pages)")
