#!/usr/bin/env python3
"""The instrument parameters of the eps~ editor in one table: the header
pd/engine/eps_edit_params.h, the editor rows of [epsedit] and the help are generated from it.

Unlike the controls of eps_params.py (offsets over the instrument), these edit the loaded
instrument itself: the instrument header, the selected layer, the selected wavesample and its
selected envelope (the SELECT bar of the editor). The editor sends  e <name> <0-127>,  eps~
answers with  info <name> <0-127> <text>  like for the controls.

  scope  i instrument   l layer   m layer bit in a patch / key-up mask
         w wavesample   e envelope of the wavesample (ENV1 / 2 / 3 as selected)
         p sample position (coarse, % of the sample)   f the same, fine (+-63 samples)
  kind   how the value is shown: n number, s signed, s8 signed but stored as a raw byte,
         note, src (mod source), cents, range (wave mod range), bend,
         or a list of names "a|b|c" (lo..hi)

usage: python3 tools/eps_edit_params.py   (writes pd/engine/eps_edit_params.h)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

ONOFF = "off|on"
LOOPMODE = "fwd|bwd|loop|bidir|untilrel"
WMTYPE = "off|looppos|start|start+lp|loopstrt|loopend|lpstrt-x|transwav"
LFOWAVE = "tri|sin/tri|sine|+tri|+sine|saw|square"
LFOMODE = "normal|reset|human"
ENVMODE = "normal|finite|repeat"

# name, label (<= 12), scope, C++ field (I instrument, L layer, W wavesample, E envelope),
# lo, hi, kind, help
PARAMS = [
    # instrument
    ("i_trans", "Transpose", "i", "I.transpose", -36, 36, "s", "transposition of the instrument in semitones"),
    ("i_keylo", "Key low", "i", "I.keyLow", 21, 108, "note", "lowest key the instrument plays"),
    ("i_keyhi", "Key high", "i", "I.keyHigh", 21, 108, "note", "highest key the instrument plays"),
    ("i_press", "Pressure", "i", "I.pressureMode", 0, 3, "n", "pressure mode as stored (0..3)"),
    ("i_psel", "Patch mode", "i", "I.patchSelectMode", 0, 3, "n", "patch select mode as stored"),
    ("i_prog", "MIDI prog", "i", "I.midiProgram", 0, 127, "n", "MIDI program as stored"),
    ("i_mout", "MIDI out ch", "i", "I.midiOutChannel", 0, 16, "n", "MIDI out channel as stored"),
    # layer
    ("l_vello", "Vel low", "l", "L.velocityLow", 0, 127, "n", "lowest velocity the layer plays"),
    ("l_velhi", "Vel high", "l", "L.velocityHigh", 0, 127, "n", "highest velocity the layer plays"),
    ("l_glmode", "Glide mode", "l", "L.glideMode", 0, 5, "n", "glide mode as stored (eps~: any mode is mono with glide)"),
    ("l_gltime", "Glide time", "l", "L.glideTime", 0, 99, "n", "glide time"),
    ("l_delay", "Delay", "l", "L.delayTime", 0, 99, "n", "delay before the layer sounds"),
    ("l_dlyvel", "Delay vel", "l", "L.delayModByVelocity", -127, 127, "s8", "velocity modulation of the delay"),
    ("l_restrk", "Restrike", "l", "L.restrikeDecay", 0, 99, "n", "restrike decay"),
    ("l_legato", "Legato", "l", "L.legatoLayer", 0, 1, ONOFF, "legato layer"),
    ("l_in00", "In 00", "m", "0", 0, 1, ONOFF, "the layer plays in patch 00"),
    ("l_in0x", "In 0*", "m", "1", 0, 1, ONOFF, "the layer plays in patch 0*"),
    ("l_inx0", "In *0", "m", "2", 0, 1, ONOFF, "the layer plays in patch *0"),
    ("l_inxx", "In **", "m", "3", 0, 1, ONOFF, "the layer plays in patch **"),
    ("l_keyup", "Key up", "m", "4", 0, 1, ONOFF, "the layer sounds on key up"),
    ("l_stlink", "Stereo link", "l", "L.stereoLink", 0, 1, ONOFF, "stereo link"),
    ("l_ptable", "Pitch table", "l", "L.pitchTable", 0, 1, "n", "pitch table (not played by eps~)"),
    # wavesample: points
    ("w_sst", "Smp start", "p", "0", 0, 127, "n", "sample start, coarse"),
    ("w_sstf", "fine", "f", "0", 0, 127, "n", "sample start, fine"),
    ("w_send", "Smp end", "p", "1", 0, 127, "n", "sample end, coarse"),
    ("w_sendf", "fine", "f", "1", 0, 127, "n", "sample end, fine"),
    ("w_lst", "Loop start", "p", "2", 0, 127, "n", "loop start, coarse"),
    ("w_lstf", "fine", "f", "2", 0, 127, "n", "loop start, fine"),
    ("w_lend", "Loop end", "p", "3", 0, 127, "n", "loop end, coarse"),
    ("w_lendf", "fine", "f", "3", 0, 127, "n", "loop end, fine"),
    # wavesample: loop and wave modulation
    ("w_loop", "Loop mode", "w", "W.loopMode", 0, 4, LOOPMODE, "loop mode"),
    ("w_keylo", "WS key low", "w", "W.keyLow", 21, 108, "note", "lowest key of the wavesample"),
    ("w_keyhi", "WS key high", "w", "W.keyHigh", 21, 108, "note", "highest key of the wavesample"),
    ("w_wmsrc", "Wave mod", "w", "W.waveModRaw[0]", 0, 15, "src", "source of the wave modulation"),
    ("w_wmamt", "Wave amt", "w", "W.waveModRaw[1]", -127, 127, "s8", "amount of the wave modulation"),
    ("w_wmtype", "Wave type", "w", "W.waveModRaw[3]", 0, 7, WMTYPE, "type of the wave modulation"),
    ("w_wmrng", "Wave range", "w", "W.waveModRaw[2]", 0, 15, "range", "range of the wave modulation"),
    # wavesample: pitch
    ("w_root", "Root key", "w", "W.rootNote", 21, 108, "note", "root key"),
    ("w_fine", "Fine", "w", "W.fineTune", -127, 127, "cents", "fine tune, 1/128 semitone"),
    ("w_bend", "Bend range", "w", "W.pitchBendRange", 0, 13, "bend", "bender range in semitones, 13 = global"),
    ("w_penv", "ENV1 pitch", "w", "W.pitchEnvAmount", -127, 127, "s", "pitch envelope amount"),
    ("w_plfo", "LFO pitch", "w", "W.lfoPitchAmount", -127, 127, "s", "LFO to pitch"),
    ("w_psrc", "Pitch mod", "w", "W.pitchModSource", 0, 15, "src", "pitch modulation source"),
    ("w_pamt", "Pitch amt", "w", "W.pitchModAmount", -127, 127, "s", "pitch modulation amount"),
    ("w_rand", "Random", "w", "W.randomModAmount", 0, 127, "n", "random pitch amount"),
    # wavesample: filters
    ("w_fmode", "Filt mode", "w", "W.filterMode", 0, 3, "n", "filter mode (0, 1: filter 2 is a high-pass)"),
    ("w_cut1", "Cutoff 1", "w", "W.filterCutoff[0]", 0, 150, "n", "cutoff of filter 1"),
    ("w_key1", "Key 1", "w", "W.filterKeyAmount[0]", -127, 127, "s", "key tracking of filter 1"),
    ("w_env1", "Env 1", "w", "W.filterEnvAmount[0]", -127, 127, "s", "ENV2 amount on filter 1"),
    ("w_fsrc1", "Mod 1", "w", "W.filterModSource[0]", 0, 15, "src", "modulation source of filter 1"),
    ("w_famt1", "Mod amt 1", "w", "W.filterModAmount[0]", -127, 127, "s", "modulation amount of filter 1"),
    ("w_cut2", "Cutoff 2", "w", "W.filterCutoff[1]", 0, 150, "n", "cutoff of filter 2"),
    ("w_key2", "Key 2", "w", "W.filterKeyAmount[1]", -127, 127, "s", "key tracking of filter 2"),
    ("w_env2", "Env 2", "w", "W.filterEnvAmount[1]", -127, 127, "s", "ENV2 amount on filter 2"),
    ("w_fsrc2", "Mod 2", "w", "W.filterModSource[1]", 0, 15, "src", "modulation source of filter 2"),
    ("w_famt2", "Mod amt 2", "w", "W.filterModAmount[1]", -127, 127, "s", "modulation amount of filter 2"),
    # wavesample: amp
    ("w_vol", "WS volume", "w", "W.volume", 0, 127, "n", "volume of the wavesample"),
    ("w_pan", "Pan", "w", "W.panSigned", -128, 127, "s", "pan (EPS-16 PLUS / ASR)"),
    ("w_boost", "Boost", "w", "W.boost", 0, 1, ONOFF, "+12 dB"),
    ("w_asrc", "Amp mod", "w", "W.ampModSource", 0, 15, "src", "amp modulation source"),
    ("w_aamt", "Amp amt", "w", "W.ampModAmount", -127, 127, "s8", "amp modulation amount"),
    ("w_pnsrc", "Pan mod", "w", "W.panModSource", 0, 15, "src", "pan modulation source"),
    ("w_pnamt", "Pan amt", "w", "W.panModAmount", -127, 127, "s8", "pan modulation amount"),
    ("w_bus", "Out bus", "w", "W.outputBus", 0, 7, "n", "output bus as stored"),
    # wavesample: LFO and random
    ("w_lwave", "LFO wave", "w", "W.lfoWaveform", 0, 6, LFOWAVE, "LFO waveform"),
    ("w_lspeed", "LFO speed", "w", "W.lfoSpeed", 0, 99, "n", "LFO speed"),
    ("w_ldepth", "LFO depth", "w", "W.lfoDepth", 0, 127, "n", "LFO depth"),
    ("w_ldelay", "LFO delay", "w", "W.lfoDelay", 0, 99, "n", "LFO delay"),
    ("w_lmode", "LFO mode", "w", "W.lfoMode", 0, 2, LFOMODE, "LFO restart mode"),
    ("w_lrsrc", "Rate mod", "w", "W.lfoRateModSource", 0, 15, "src", "LFO rate modulation source"),
    ("w_lramt", "Rate amt", "w", "W.lfoRateModAmount", -127, 127, "s", "LFO rate modulation amount"),
    ("w_rfreq", "Random freq", "w", "W.randomFreq", 0, 99, "n", "rate of the random pitch"),
    # wavesample: crossfade, LFO depth modulation
    ("w_xfa", "Xfade A", "w", "W.crossfade[0]", 0, 127, "n", "crossfade point A"),
    ("w_xfb", "Xfade B", "w", "W.crossfade[1]", 0, 127, "n", "crossfade point B"),
    ("w_xfc", "Xfade C", "w", "W.crossfade[2]", 0, 127, "n", "crossfade point C"),
    ("w_xfd", "Xfade D", "w", "W.crossfade[3]", 0, 127, "n", "crossfade point D"),
    ("w_xfcrv", "Xfade on", "w", "W.volumeModCrossfadeCurve", 0, 1, ONOFF, "amp modulation as crossfade"),
    ("w_ldsrc", "Depth mod", "w", "W.lfoDepthModSource", 0, 15, "src", "LFO depth modulation source"),
    ("w_ldamt", "Depth amt", "w", "W.lfoDepthModAmount", -127, 127, "s", "LFO depth modulation amount"),
    # envelope (ENV1 pitch, ENV2 filter, ENV3 amp as selected)
    ("e_l0h", "L0 hard", "e", "E.hardLevel[0]", 0, 127, "n", "start level, hard"),
    ("e_l1h", "L1 hard", "e", "E.hardLevel[1]", 0, 127, "n", "level 1, hard"),
    ("e_l2h", "L2 hard", "e", "E.hardLevel[2]", 0, 127, "n", "level 2, hard"),
    ("e_l3h", "L3 hard", "e", "E.hardLevel[3]", 0, 127, "n", "level 3, hard"),
    ("e_l4h", "L4 hard", "e", "E.hardLevel[4]", 0, 127, "n", "sustain level, hard"),
    ("e_l5", "L5 release", "e", "E.level5", -127, 127, "s", "level change of the first release stage"),
    ("e_mode", "Env mode", "e", "E.mode", 0, 2, ENVMODE, "envelope mode"),
    ("e_type", "Soft curve", "e", "E.type", 0, 4, "n", "soft velocity curve as stored"),
    ("e_l0s", "L0 soft", "e", "E.softLevel[0]", 0, 127, "n", "start level, soft"),
    ("e_l1s", "L1 soft", "e", "E.softLevel[1]", 0, 127, "n", "level 1, soft"),
    ("e_l2s", "L2 soft", "e", "E.softLevel[2]", 0, 127, "n", "level 2, soft"),
    ("e_l3s", "L3 soft", "e", "E.softLevel[3]", 0, 127, "n", "level 3, soft"),
    ("e_l4s", "L4 soft", "e", "E.softLevel[4]", 0, 127, "n", "sustain level, soft"),
    ("e_t1vel", "Attack vel", "e", "E.time1VelSens", 0, 99, "n", "velocity shortens time 1"),
    ("e_kbs", "Key scale", "e", "E.kbTimeScaling", -127, 127, "s8", "keyboard time scaling"),
    ("e_vsw", "Vel switch", "e", "E.velocitySwitch", 0, 1, "n", "velocity switch as stored"),
    ("e_t1", "Time 1", "e", "E.time[1]", 0, 99, "n", "time 1"),
    ("e_t2", "Time 2", "e", "E.time[2]", 0, 99, "n", "time 2"),
    ("e_t3", "Time 3", "e", "E.time[3]", 0, 99, "n", "time 3"),
    ("e_t4", "Time 4", "e", "E.time[4]", 0, 99, "n", "time 4"),
    ("e_t5", "T5 release", "e", "E.time[5]", 0, 99, "n", "release time 1 (to L5)"),
    ("e_t6", "T6 release", "e", "E.time[6]", 0, 99, "n", "release time 2 (to zero)"),
]

# Editor rows, eight cells each (None = empty); the column they go into.
ROWS_LEFT = [
    ("INSTRUMENT", ["i_trans", "i_keylo", "i_keyhi", "i_press", "i_psel", "i_prog", "i_mout", None]),
    ("LAYER", ["l_vello", "l_velhi", "l_glmode", "l_gltime", "l_delay", "l_dlyvel", "l_restrk", "l_legato"]),
    ("LAYER PATCHES", ["l_in00", "l_in0x", "l_inx0", "l_inxx", "l_keyup", "l_stlink", "l_ptable", None]),
    ("WS POINTS", ["w_sst", "w_sstf", "w_send", "w_sendf", "w_lst", "w_lstf", "w_lend", "w_lendf"]),
    ("WS LOOP / WAVE MOD", ["w_loop", "w_keylo", "w_keyhi", "w_wmsrc", "w_wmamt", "w_wmtype", "w_wmrng", None]),
    ("WS PITCH", ["w_root", "w_fine", "w_bend", "w_penv", "w_plfo", "w_psrc", "w_pamt", "w_rand"]),
]
ROWS_RIGHT = [
    ("WS FILTER 1", ["w_fmode", "w_cut1", "w_key1", "w_env1", "w_fsrc1", "w_famt1", None, None]),
    ("WS FILTER 2", [None, "w_cut2", "w_key2", "w_env2", "w_fsrc2", "w_famt2", None, None]),
    ("WS AMP", ["w_vol", "w_pan", "w_boost", "w_asrc", "w_aamt", "w_pnsrc", "w_pnamt", "w_bus"]),
    ("WS LFO", ["w_lwave", "w_lspeed", "w_ldepth", "w_ldelay", "w_lmode", "w_lrsrc", "w_lramt", "w_rfreq"]),
    ("WS XFADE / LFO DEPTH", ["w_xfa", "w_xfb", "w_xfc", "w_xfd", "w_xfcrv", "w_ldsrc", "w_ldamt", None]),
    ("ENV LEVELS HARD", ["e_l0h", "e_l1h", "e_l2h", "e_l3h", "e_l4h", "e_l5", "e_mode", "e_type"]),
    ("ENV LEVELS SOFT", ["e_l0s", "e_l1s", "e_l2s", "e_l3s", "e_l4s", "e_t1vel", "e_kbs", "e_vsw"]),
    ("ENV TIMES", ["e_t1", "e_t2", "e_t3", "e_t4", "e_t5", "e_t6", None, None]),
]

P = {p[0]: p for p in PARAMS}
_listed = [n for _, ns in ROWS_LEFT + ROWS_RIGHT for n in ns if n]
assert len(_listed) == len(set(_listed)) and set(_listed) == set(P), set(P) ^ set(_listed)
for _p in PARAMS:
    assert len(_p[1]) <= 12, _p[1]
    if "|" in _p[6]:
        assert len(_p[6].split("|")) == _p[5] - _p[4] + 1, _p


def label(nm):
    return P[nm][1]


def write_header(path):
    L = ["// Generated by tools/eps_edit_params.py -- do not edit.",
         "// The instrument parameters of the editor: name, label, scope (i instrument, l layer,",
         "// m layer mask bit, w wavesample, e envelope, p/f sample point coarse/fine), lo, hi,",
         "// how the value is shown, index (masks and points).",
         "#pragma once",
         "namespace eps {",
         "struct EditParam { const char *name, *label; char scope; int lo, hi; const char *kind; int index; };",
         "static const EditParam kEditParams[] = {"]
    for nm, lbl, scope, field, lo, hi, kind, _ in PARAMS:
        idx = int(field) if scope in "mpf" else -1
        L.append(f'    {{"{nm}", "{lbl}", \'{scope}\', {lo}, {hi}, "{kind}", {idx}}},')
    L.append("};")
    L.append(f"constexpr int kEditParamCount = {len(PARAMS)};")
    L.append("")
    L.append("// The field of an i, l, w or e parameter (nullptr for the others); defined where")
    L.append("// EPS_EDIT_FIELDS is set (eps_edit.cpp).")
    L.append("int *editField(int id, Instrument &I, Layer *Lp, WaveSample *Wp, Envelope *Ep);")
    L.append("#ifdef EPS_EDIT_FIELDS")
    L.append("int *editField(int id, Instrument &I, Layer *Lp, WaveSample *Wp, Envelope *Ep) {")
    L.append("    switch (id) {")
    for i, (nm, lbl, scope, field, lo, hi, kind, _) in enumerate(PARAMS):
        if scope == "i":
            L.append(f"        case {i}: return &{field};")
        elif scope == "l":
            L.append(f"        case {i}: if (!Lp) return nullptr; {{ Layer &L = *Lp; return &{field}; }}")
        elif scope == "w":
            L.append(f"        case {i}: if (!Wp) return nullptr; {{ WaveSample &W = *Wp; return &{field}; }}")
        elif scope == "e":
            L.append(f"        case {i}: if (!Ep) return nullptr; {{ Envelope &E = *Ep; return &{field}; }}")
    L.append("        default: return nullptr;")
    L.append("    }")
    L.append("}")
    L.append("#endif")
    L.append("} // namespace eps")
    open(path, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "engine", "eps_edit_params.h")
    write_header(out)
    print("wrote", out, f"({len(PARAMS)} parameters)")
