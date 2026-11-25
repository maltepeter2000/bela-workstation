// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2026 Peter
//
// cz4~ -- a Casio CZ-1 for Pure Data: 16 voices of phase distortion with two
// lines each, six 8-step envelopes per voice, the CZ-1's operation memory
// (split, tone mix, portamento, glide), chorus, and the CZ SysEx with its
// handshake, both ways.
//
//   inlet 1   float: note (velocity from inlet 2; velocity 0 = note off)
//             list:  note velocity [channel]
//             any:   a message, see below
//   inlet 2   float: velocity for the next note, 0..127 (default 100)
//   inlet 3   float or list: raw MIDI bytes, as [midiin] / [sysexin] give them:
//             notes, bend, aftertouch, program change, CC 1 7 64, CZ SysEx
//             any:   a message (the same set as inlet 1)
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: info <name> <0-127> <text> for every change, preset /
//             user / op names, ...   floats: SysEx bytes for [midiout]
//
// Controls (tools/cz4_params.py; the same for [cz4cc] and [cz4edit]):
//   <name> <value>      set a control, in the values the CZ shows
//   <name>              report it
//   n <name> <0-127>    set it from a pot or a MIDI CC
//   ninfo <name>        report it as info
// The line, envelope and tone controls act on the selection: line (1/2),
// env (DCO/DCW/DCA), edit (tone A/B).
//
// Tones:      preset n, user n, init, reload, compare 0/1, store [n],
//             name <text>, copyline <from> <to>, copyenv <line> <env> <line> <env>,
//             userwrite [file], userread [file], usersend, presets, users
// Operation:  op n, opstore [n], opinit, opwrite [file], opread [file], ops
// SysEx:      sysex b b ..., sysexdump [prog], czget <prog>, czsend [prog],
//             czmodel 1|101, channel n, txchannel n
// Playing:    bend f (-1..1), mod f, at f (0..1), sustain 0/1, stop, reset, status
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <dirent.h>
#endif

extern "C" {
#include "m_pd.h"
}

#include "cz4_core.h"

namespace {

t_class* cz4_class = nullptr;
t_class* cz4_proxy_class = nullptr;

const char kUserFile[] = "cz4user-bank.syx";
const char kOpFile[] = "cz4user-op.syx";

struct t_cz4;

struct t_cz4_proxy {
    t_pd pd;
    t_cz4* owner;
};

// The CZ handshake. As a device: a computer sends "F0 44 00 00 7n 10|11 pp",
// we answer "F0 44 00 00 7n 30", it sends "7n 31", we send the tone and F7, it
// sends F7. Or it sends "F0 44 00 00 7n 20|21 pp", we answer "... 30", it sends
// the tone and F7, we send F7. As a host (czget, czsend) the other way round.
enum DevState { kIdle, kWait31, kRecv, kHostGet, kHostGetData, kHostSend, kHostSendDone };

struct Midi {
    std::vector<uint8_t> sx;   // the SysEx message being collected
    bool in_sx = false;
    int status = 0;
    int data[2] = {0, 0};
    int have = 0;
    DevState dev = kIdle;
    int prog = 0;
    bool len144 = true;
};

struct Shown {
    std::vector<int> value;
    int source[2] = {-1, -1}, index[2] = {-1, -1}, op = -2;
    bool edited = false;
};

struct t_cz4 {
    t_object obj;
    cz4::Synth* synth;
    t_float velocity;
    t_cz4_proxy proxy;
    t_outlet* info;
    Midi* midi;
    Shown* shown;
    t_clock* timeout;
    int channel;               // receive, 0 = all
    int txchannel;             // send, 1..16
    int model;                 // 1: CZ-1 (144 bytes), 101: CZ-101 family (128)
    int last_preset;           // the factory tone and the user slot the edit tone played or was
    int last_user;             // stored last: presetstep / userstep count from there, save stores there
    int store_to;              // savearm .. save: the user slot chosen, -1 none
};

// ---- data files -------------------------------------------------------------

bool g_data_tried = false;
std::string g_data_dir;

bool file_exists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void load_data() {
    if (g_data_tried) return;
    g_data_tried = true;
    const char* h = class_gethelpdir(cz4_class);
    if (!h || !*h) return;
    g_data_dir = h;
    std::vector<std::string> files;
#ifndef _WIN32
    if (DIR* d = opendir(g_data_dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() < 5 || n == kUserFile || n == kOpFile) continue;
            std::string ext = n.substr(n.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".syx") files.push_back(n);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());
#endif
    for (const std::string& n : files) {
        std::string err;
        const int got = cz4::load_syx((g_data_dir + "/" + n).c_str(), err);
        if (got < 0) pd_error(nullptr, "cz4~: %s: %s", n.c_str(), err.c_str());
        else post("cz4~: %s: %d tones", n.c_str(), got);
    }
    for (const char* f : {kUserFile, kOpFile}) {
        const std::string p = g_data_dir + "/" + f;
        if (!file_exists(p)) continue;
        std::string err;
        const int got = f == kUserFile ? cz4::load_user(p.c_str(), err) : cz4::load_ops(p.c_str(), err);
        if (got < 0) pd_error(nullptr, "cz4~: %s: %s", f, err.c_str());
        else post("cz4~: %s: %d %s", f, got, f == kUserFile ? "tones" : "operation slots");
    }
}

std::string data_path(int argc, t_atom* argv, const char* dflt) {
    if (argc > 0 && argv[0].a_type == A_SYMBOL) {
        const std::string f = atom_getsymbol(argv)->s_name;
        if (!f.empty() && (f[0] == '/' || (f.size() > 1 && f[1] == ':'))) return f;
        return g_data_dir + "/" + f;
    }
    return g_data_dir + "/" + dflt;
}

// ---- output -----------------------------------------------------------------

void out_value(t_cz4* x, const char* key, t_float v) {
    t_atom a;
    SETFLOAT(&a, v);
    outlet_anything(x->info, gensym(key), 1, &a);
}

void out_slot(t_cz4* x, const char* key, int index, const char* name) {
    t_atom a[2];
    SETFLOAT(&a[0], index);
    SETSYMBOL(&a[1], gensym(name));
    outlet_anything(x->info, gensym(key), 2, a);
}

void out_sysex(void* ctx, const uint8_t* msg, int len) {
    t_cz4* x = static_cast<t_cz4*>(ctx);
    for (int i = 0; i < len; ++i) outlet_float(x->info, msg[i]);
}

void out_bytes(t_cz4* x, std::initializer_list<int> b) {
    for (int v : b) outlet_float(x->info, v);
}

void out_info(t_cz4* x, int i) {
    const cz4::Control& c = cz4::control(i);
    const int v = x->synth->get_control(c);
    x->shown->value[static_cast<size_t>(i)] = v;
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(c.name));
    SETFLOAT(&a[1], cz4::control_to127(c, v));
    SETSYMBOL(&a[2], gensym(x->synth->control_text(c, v).c_str()));
    outlet_anything(x->info, gensym("info"), 3, a);
}

// "11" ... "88": the user slots in the CZ-1's numbering
std::string slot_label(int slot) {
    return std::string(1, static_cast<char>('1' + slot / 8)) + static_cast<char>('1' + slot % 8);
}

// One line for a display (the Launchkey's popup, an OLED): which tone plays,
// what was turned, what was stored. Same form in every instrument.
void out_display(t_cz4* x, const std::string& title, const std::string& value) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(title.c_str()));
    SETSYMBOL(&a[1], gensym(value.c_str()));
    outlet_anything(x->info, gensym("display"), 2, a);
}

void out_source(t_cz4* x, int t) {
    cz4::Synth& y = *x->synth;
    const char* key = t ? "toneb" : "tonea";
    t_atom a[3];
    const int src = y.source(t);
    SETSYMBOL(&a[0], gensym(src == cz4::Synth::kFactory ? "preset" : src == cz4::Synth::kUser ? "user"
                            : src == cz4::Synth::kSysex ? "sysex" : "init"));
    SETFLOAT(&a[1], y.patch(t));
    SETSYMBOL(&a[2], gensym(y.name(t).c_str()));
    outlet_anything(x->info, gensym(key), 3, a);
    if (t == y.edit_tone()) {
        const int idx = y.patch(t);
        if (src == cz4::Synth::kFactory) out_display(x, "Preset " + std::to_string(idx), y.name(t));
        else if (src == cz4::Synth::kUser) out_display(x, "User " + slot_label(idx), y.name(t));
        else if (src == cz4::Synth::kSysex) out_display(x, "SysEx", y.name(t));
        else out_display(x, "Init", "");
        t_atom n;
        SETSYMBOL(&n, gensym(y.name(t).c_str()));
        outlet_anything(x->info, gensym("name"), 1, &n);
    }
}

// Everything that changed since outlet 3 last spoke, in the logical time of
// the change (the controller layer tells its own encoder by that).
void report(t_cz4* x, bool quiet = false) {
    cz4::Synth& y = *x->synth;
    Shown& s = *x->shown;
    {
        const int et = y.edit_tone();
        if (y.source(et) == cz4::Synth::kFactory) x->last_preset = y.patch(et);
        else if (y.source(et) == cz4::Synth::kUser) x->last_user = y.patch(et);
    }
    for (int t = 0; t < 2; ++t) {
        if (y.source(t) == s.source[t] && y.patch(t) == s.index[t]) continue;
        s.source[t] = y.source(t);
        s.index[t] = y.patch(t);
        if (!quiet) out_source(x, t);
    }
    if (y.op_slot() != s.op) {
        s.op = y.op_slot();
        if (!quiet) out_value(x, "op", s.op);
    }
    const bool ed = y.edited();
    if (ed != s.edited) {
        s.edited = ed;
        if (!quiet) out_value(x, "edited", ed);
    }
    for (int i = 0; i < cz4::control_count(); ++i) {
        const cz4::Control& c = cz4::control(i);
        const int v = y.get_control(c);
        if (v == s.value[static_cast<size_t>(i)]) continue;
        if (quiet) { s.value[static_cast<size_t>(i)] = v; continue; }
        out_info(x, i);
    }
}

void force_source(t_cz4* x) { x->shown->source[0] = x->shown->source[1] = -2; }

// ---- SysEx ------------------------------------------------------------------

int dev_channel(t_cz4* x) { return (x->txchannel - 1) & 15; }

void send_tone_data(t_cz4* x, const uint8_t* tone144, bool len144) {
    const int nb = len144 ? cz4::kToneBytes : CZ_TONE_OLD;
    std::vector<uint8_t> nib(static_cast<size_t>(2 * nb));
    cz_nibbles_out(tone144, nb, nib.data());
    for (uint8_t b : nib) outlet_float(x->info, b);
    outlet_float(x->info, 0xF7);
}

void start_timeout(t_cz4* x) { clock_delay(x->timeout, 3000); }

void cz4_timeout(t_cz4* x) {
    if (x->midi->dev != kIdle) {
        pd_error(x, "cz4~: SysEx handshake timed out");
        x->midi->dev = kIdle;
    }
}

// The CZ's tone in the payload of a message: 256 or 288 nibbles.
bool payload_tone(const std::vector<uint8_t>& m, size_t from, uint8_t* tone144) {
    if (m.size() < from + 1) return false;
    const size_t n = m.size() - 1 - from;      // without F7
    uint8_t raw[cz4::kToneBytes];
    if (n == 2u * cz4::kToneBytes) {
        if (!cz_nibbles_in(&m[from], cz4::kToneBytes, raw)) return false;
        std::memcpy(tone144, raw, cz4::kToneBytes);
    } else if (n == 2u * CZ_TONE_OLD) {
        if (!cz_nibbles_in(&m[from], CZ_TONE_OLD, raw)) return false;
        cz_tone_from128(tone144, raw, "CZ-101 TONE");
    } else {
        return false;
    }
    return cz_tone_valid(tone144) != 0;
}

bool cz_header(const std::vector<uint8_t>& m, t_cz4* x) {
    return m.size() >= 6 && m[0] == 0xF0 && m[1] == 0x44 && m[2] == 0 && m[3] == 0 && (m[4] & 0xF0) == 0x70 &&
           (!x->channel || (m[4] & 0x0F) + 1 == x->channel);
}

// A byte of an open SysEx message: the handshake answers before the message ends.
void sysex_progress(t_cz4* x) {
    Midi& m = *x->midi;
    // A copy: the answers below can come back at once (another cz4~, a loop in
    // the patch) and change m.sx. The state is set before anything goes out.
    const std::vector<uint8_t> s(m.sx.begin(), m.sx.begin() + std::min<size_t>(m.sx.size(), 9));
    if (!cz_header(s, x)) return;
    const int n = static_cast<int>(m.sx.size());
    const int ch = s[4] & 0x0F;
    if (n == 6 && s[5] == 0x30 && m.dev == kHostGet) {       // the CZ is ready to send
        m.dev = kHostGetData;
        start_timeout(x);
        out_bytes(x, {0x70 | ch, 0x31});
    } else if (n == 6 && s[5] == 0x30 && m.dev == kHostSend) {   // the CZ is ready to receive
        uint8_t t[cz4::kToneBytes];
        std::memcpy(t, x->synth->tone(), cz4::kToneBytes);
        m.dev = kHostSendDone;
        start_timeout(x);
        send_tone_data(x, t, m.len144);
    } else if (n == 7 && (s[5] == 0x10 || s[5] == 0x11) && m.dev == kIdle) {   // send request
        m.prog = s[6];
        m.len144 = s[5] == 0x11;
        m.dev = kWait31;
        start_timeout(x);
        out_bytes(x, {0xF0, 0x44, 0x00, 0x00, 0x70 | ch, 0x30});
    } else if (n == 9 && m.dev == kWait31 && s[7] == (0x70 | ch) && s[8] == 0x31) {
        uint8_t t[cz4::kToneBytes];
        if (!x->synth->tone_for_prog(m.prog, t)) std::memcpy(t, x->synth->tone(0), cz4::kToneBytes);
        send_tone_data(x, t, m.len144);
    } else if (n == 7 && (s[5] == 0x20 || s[5] == 0x21) && m.dev == kIdle) {   // receive request
        m.prog = s[6];
        m.dev = kRecv;
        start_timeout(x);
        out_bytes(x, {0xF0, 0x44, 0x00, 0x00, 0x70 | ch, 0x30});
    }
}

// A complete message (F0 .. F7).
void sysex_done(t_cz4* x) {
    Midi& m = *x->midi;
    const std::vector<uint8_t> s = m.sx;       // a copy, as above
    const DevState st = m.dev;
    m.dev = kIdle;
    clock_unset(x->timeout);
    if (!cz_header(s, x)) return;
    uint8_t t[cz4::kToneBytes];
    if (st == kRecv) {
        outlet_float(x->info, 0xF7);              // closes our "30" message
        if (payload_tone(s, 7, t)) x->synth->receive_prog(m.prog, t);
        else pd_error(x, "cz4~: received SysEx is not a CZ tone");
        report(x);
    } else if (st == kHostGetData) {
        outlet_float(x->info, 0xF7);
        if (payload_tone(s, 6, t)) {
            char nm[CZ_NAME_LEN + 1];
            cz_name_get(t, nm);
            if (m.prog >= 0x20 && m.prog < 0x20 + cz4::kUserSlots) {
                x->synth->receive_prog(m.prog, t);
                post("cz4~: czget: tone '%s' into user slot %d", nm, m.prog - 0x20);
            } else {
                x->synth->load_tone(t);
                post("cz4~: czget: tone '%s' received", nm);
            }
        } else {
            pd_error(x, "cz4~: czget: no CZ tone in the answer");
        }
        report(x);
    } else if (st == kWait31 || st == kHostSendDone) {
        // the other side closed its message: done
    } else if (s.size() > 7 && (s[5] == 0x20 || s[5] == 0x21) && payload_tone(s, 7, t)) {
        // a librarian message without handshake
        x->synth->receive_prog(s[6], t);
        report(x);
    }
}

// ---- messages ---------------------------------------------------------------

void select_program(t_cz4* x, int pgm) {
    cz4::Synth& y = *x->synth;
    if (y.source() == cz4::Synth::kUser && pgm < cz4::kUserSlots) y.select_user(pgm);
    else y.select_patch(pgm);
    force_source(x);
}

int env_index(t_atom* a) {
    if (a->a_type == A_SYMBOL) {
        const char* s = atom_getsymbol(a)->s_name;
        if (!std::strcmp(s, "DCO") || !std::strcmp(s, "dco")) return 0;
        if (!std::strcmp(s, "DCW") || !std::strcmp(s, "dcw")) return 1;
        return 2;
    }
    return std::clamp(static_cast<int>(atom_getfloat(a)), 0, 2);
}

void cz4_command(t_cz4* x, t_symbol* s, int argc, t_atom* argv) {
    cz4::Synth& y = *x->synth;
    const char* c = s->s_name;
    const t_float f = argc > 0 ? atom_getfloat(argv) : 0;
    const int n = static_cast<int>(f);
    auto is = [c](const char* k) { return std::strcmp(c, k) == 0; };
    const cz4::Control* touched = nullptr;   // shown on the display afterwards
    bool stored = false;

    if (const cz4::Control* ctl = cz4::find_control(c)) {
        if (argc > 0) { y.set_control(*ctl, n); touched = ctl; }
        else out_info(x, static_cast<int>(ctl - &cz4::control(0)));
    } else if (is("n")) {
        if (argc < 2 || argv[0].a_type != A_SYMBOL) return;
        const char* name = atom_getsymbol(argv)->s_name;
        const cz4::Control* nc = cz4::find_control(name);
        if (!nc) {
            if (std::strcmp(name, "none") != 0 && std::strcmp(name, "-") != 0)
                pd_error(x, "cz4~: n: no control '%s'", name);
            return;
        }
        const int i = static_cast<int>(nc - &cz4::control(0));
        const int before = y.get_control(*nc);
        y.set_control(*nc, cz4::control_from127(*nc, atom_getfloat(argv + 1)));
        touched = nc;
        if (y.get_control(*nc) == before) out_info(x, i);
    } else if (is("ninfo")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        if (const cz4::Control* nc = cz4::find_control(atom_getsymbol(argv)->s_name))
            out_info(x, static_cast<int>(nc - &cz4::control(0)));
    } else if (is("dump")) {
        for (int i = 0; i < cz4::control_count(); ++i) out_info(x, i);
    } else if (is("preset")) {
        if (argc < 1 || !y.select_patch(n)) pd_error(x, "cz4~: preset: expected 0..%d", cz4::patch_count() - 1);
        else force_source(x);
    } else if (is("user")) {
        if (argc < 1 || !y.select_user(n)) pd_error(x, "cz4~: user: expected 0..%d", cz4::kUserSlots - 1);
        else force_source(x);
    } else if (is("init")) {
        y.select_init();
        force_source(x);
    } else if (is("reload")) {
        y.reload();
        force_source(x);
    } else if (is("compare")) {
        y.set_compare(n != 0);
        out_value(x, "compare", y.compare());
    } else if (is("store")) {
        if (!y.store(argc > 0 ? n : -1)) pd_error(x, "cz4~: store: expected 0..%d", cz4::kUserSlots - 1);
        else { force_source(x); stored = true; }
    } else if (is("presetstep") || is("userstep")) {
        // the next (or n-th next) tone, counted from the one played last, wrapping around
        const int by = argc > 0 ? n : 1;
        if (is("presetstep")) {
            const int cnt = cz4::patch_count();
            if (cnt > 0) y.select_patch(((x->last_preset + by) % cnt + cnt) % cnt);
        } else {
            y.select_user(((x->last_user + by) % cz4::kUserSlots + cz4::kUserSlots) % cz4::kUserSlots);
        }
        force_source(x);
    } else if (is("savearm")) {
        // the pads' store with a question: show the slot, saveslot moves it, save stores there
        x->store_to = x->last_user;
        out_display(x, "Store to?", "User " + slot_label(x->store_to) + " " + cz4::user_name(x->store_to));
    } else if (is("saveslot")) {
        if (x->store_to >= 0) {
            x->store_to = ((x->store_to + (argc > 0 ? n : 1)) % cz4::kUserSlots + cz4::kUserSlots) % cz4::kUserSlots;
            out_display(x, "Store to?", "User " + slot_label(x->store_to) + " " + cz4::user_name(x->store_to));
        }
    } else if (is("savecancel")) {
        if (x->store_to >= 0) out_display(x, "Store", "cancelled");
        x->store_to = -1;
    } else if (is("save")) {
        // the edit tone into the slot chosen (savearm), else the user slot played or stored last,
        // and write the user bank
        stored = y.store(x->store_to >= 0 ? x->store_to : x->last_user);
        x->store_to = -1;
        if (stored) {
            force_source(x);
            if (!g_data_dir.empty()) {
                std::string err;
                const std::string path = g_data_dir + "/" + kUserFile;
                if (!cz4::write_user(path.c_str(), err)) pd_error(x, "cz4~: save: %s", err.c_str());
            }
        }
    } else if (is("name")) {
        std::string nm;
        for (int i = 0; i < argc; ++i) {
            char buf[MAXPDSTRING];
            atom_string(argv + i, buf, MAXPDSTRING);
            if (i) nm += ' ';
            nm += buf;
        }
        y.set_name(nm.c_str());
        force_source(x);
    } else if (is("copyline")) {
        if (argc < 2) { pd_error(x, "cz4~: copyline <from 1|2> <to 1|2>"); return; }
        y.copy_line(std::clamp(n, 1, 2) - 1, std::clamp(static_cast<int>(atom_getfloat(argv + 1)), 1, 2) - 1);
    } else if (is("copyenv")) {
        if (argc < 4) { pd_error(x, "cz4~: copyenv <line> <env> <line> <env>"); return; }
        y.copy_env(std::clamp(n, 1, 2) - 1, env_index(argv + 1),
                   std::clamp(static_cast<int>(atom_getfloat(argv + 2)), 1, 2) - 1, env_index(argv + 3));
    } else if (is("userwrite") || is("opwrite")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const bool ops = is("opwrite");
        const std::string path = data_path(argc, argv, ops ? kOpFile : kUserFile);
        if (!(ops ? cz4::write_ops(path.c_str(), err) : cz4::write_user(path.c_str(), err)))
            pd_error(x, "cz4~: %s: %s", c, err.c_str());
        else post("cz4~: %s written to %s", ops ? "operation memory" : "user bank", path.c_str());
    } else if (is("userread") || is("opread")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const bool ops = is("opread");
        const std::string path = data_path(argc, argv, ops ? kOpFile : kUserFile);
        const int got = ops ? cz4::load_ops(path.c_str(), err) : cz4::load_user(path.c_str(), err);
        if (got < 0) pd_error(x, "cz4~: %s: %s", c, err.c_str());
        else post("cz4~: %d %s from %s", got, ops ? "operation slots" : "tones", path.c_str());
    } else if (is("usersend")) {
        y.dump_user(out_sysex, x, dev_channel(x));
    } else if (is("presets")) {
        const int count = cz4::patch_count();
        for (int i = 0; i < count; ++i) out_slot(x, "preset", i, cz4::patch_name(i));
        out_value(x, "presets", count);
    } else if (is("users")) {
        for (int i = 0; i < cz4::kUserSlots; ++i) out_slot(x, "user", i, cz4::user_name(i));
        out_value(x, "users", cz4::kUserSlots);
    } else if (is("op")) {
        if (argc < 1 || !y.select_op(n)) pd_error(x, "cz4~: op: expected 0..%d", cz4::kOpSlots - 1);
        force_source(x);
        x->shown->op = -2;
    } else if (is("opstore")) {
        if (!y.store_op(argc > 0 ? n : -1)) pd_error(x, "cz4~: opstore: expected 0..%d", cz4::kOpSlots - 1);
        x->shown->op = -2;
    } else if (is("opinit")) {
        y.init_op();
        force_source(x);
        x->shown->op = -2;
    } else if (is("ops")) {
        int used = 0;
        for (int i = 0; i < cz4::kOpSlots; ++i) if (cz4::op_used(i)) { out_value(x, "opused", i); ++used; }
        out_value(x, "ops", used);
    } else if (is("sysex")) {
        Midi& m = *x->midi;
        m.sx.clear();
        if (argc > 0 && atom_getfloat(argv) != 0xF0) m.sx.push_back(0xF0);
        for (int i = 0; i < argc; ++i)
            m.sx.push_back(static_cast<uint8_t>(static_cast<int>(atom_getfloat(argv + i)) & 0xFF));
        if (m.sx.empty() || m.sx.back() != 0xF7) m.sx.push_back(0xF7);
        uint8_t t[cz4::kToneBytes];
        if (cz_header(m.sx, x) && m.sx.size() > 7 && payload_tone(m.sx, 7, t)) y.receive_prog(m.sx[6], t);
        else if (cz_header(m.sx, x) && m.sx.size() > 6 && payload_tone(m.sx, 6, t)) y.load_tone(t);
        else pd_error(x, "cz4~: sysex: not a CZ tone message");
        force_source(x);
    } else if (is("sysexdump")) {
        y.dump_tone(out_sysex, x, dev_channel(x), argc > 0 ? n : 0x60);
    } else if (is("czget") || is("czsend")) {
        // host: ask a CZ for a tone, or send it the edit tone
        Midi& m = *x->midi;
        const bool get = is("czget");
        m.prog = argc > 0 ? n : 0x60;
        m.len144 = x->model == 1;
        m.dev = get ? kHostGet : kHostSend;
        const int cmd = (get ? 0x10 : 0x20) | (m.len144 ? 1 : 0);
        out_bytes(x, {0xF0, 0x44, 0x00, 0x00, 0x70 | dev_channel(x), cmd, m.prog & 0x7F});
        start_timeout(x);
        return;
    } else if (is("czmodel")) {
        x->model = n == 101 ? 101 : 1;
        out_value(x, "czmodel", x->model);
    } else if (is("channel")) {
        x->channel = std::clamp(n, 0, 16);
        out_value(x, "channel", x->channel);
    } else if (is("txchannel")) {
        x->txchannel = std::clamp(n, 1, 16);
        out_value(x, "txchannel", x->txchannel);
    } else if (is("status")) {
        out_source(x, 0);
        out_source(x, 1);
        out_value(x, "op", y.op_slot());
        out_value(x, "channel", x->channel);
        out_value(x, "txchannel", x->txchannel);
        out_value(x, "czmodel", x->model);
        out_value(x, "sustain", y.sustain());
        out_value(x, "sounding", y.voices_sounding());
        out_value(x, "presets", cz4::patch_count());
    }
    else if (is("bend")) y.set_bend(f);
    else if (is("mod")) y.set_mod(f);
    else if (is("at")) y.set_aftertouch(f);
    else if (is("sustain")) y.set_sustain(f != 0);
    else if (is("stop")) y.all_notes_off();
    else if (is("reset")) y.silence();
    else { pd_error(x, "cz4~: no method for '%s'", c); return; }
    report(x);
    if (touched) out_display(x, touched->label, y.control_text(*touched, y.get_control(*touched)));
    if (stored) {
        const int et = y.edit_tone();
        out_display(x, "Stored", "User " + slot_label(y.patch(et)) + " " + y.name(et));
    }
}

void cz4_note(t_cz4* x, t_float note, t_float velocity) {
    const int n = static_cast<int>(note);
    if (velocity > 0) x->synth->note_on(n, static_cast<int>(velocity));
    else x->synth->note_off(n);
}

void cz4_float(t_cz4* x, t_float f) { cz4_note(x, f, x->velocity); }

void cz4_list(t_cz4* x, t_symbol*, int argc, t_atom* argv) {
    if (argc >= 3 && x->channel && static_cast<int>(atom_getfloat(argv + 2)) != x->channel) return;
    if (argc >= 2) cz4_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) cz4_float(x, atom_getfloat(argv));
}

void cz4_proxy_anything(t_cz4_proxy* p, t_symbol* s, int argc, t_atom* argv) { cz4_command(p->owner, s, argc, argv); }

// ---- raw MIDI ---------------------------------------------------------------

void midi_message(t_cz4* x, int status, int d1, int d2) {
    cz4::Synth& y = *x->synth;
    if (x->channel && (status & 0x0F) + 1 != x->channel) return;
    switch (status & 0xF0) {
        case 0x90:
            if (d2 > 0) y.note_on(d1, d2);
            else y.note_off(d1);
            return;
        case 0x80: y.note_off(d1); return;
        case 0xB0:
            switch (d1) {
                case 1: y.set_mod(d2 / 127.0f); return;
                case 7:
                    if (const cz4::Control* c = cz4::find_control("volume")) y.set_control(*c, d2);
                    break;
                case 64: y.set_sustain(d2 >= 64); return;
                case 120: y.silence(); return;
                case 123: y.all_notes_off(); return;
                default: return;          // the CC pages belong to [cz4cc]
            }
            break;
        case 0xC0: select_program(x, d1); break;
        case 0xD0: y.set_aftertouch(d1 / 127.0f); return;
        case 0xE0: y.set_bend(((d2 << 7 | d1) - 8192) / 8192.0f); return;
        default: return;
    }
    report(x);
}

void cz4_proxy_float(t_cz4_proxy* p, t_float f) {
    t_cz4* x = p->owner;
    Midi& m = *x->midi;
    const int b = static_cast<int>(f) & 0xFF;
    if (b >= 0xF8) return;
    if (b == 0xF0) { m.in_sx = true; m.sx.assign(1, 0xF0); return; }
    if (m.in_sx) {
        if (b < 0x80) {
            if (m.sx.size() < 400) m.sx.push_back(static_cast<uint8_t>(b));
            sysex_progress(x);
            return;
        }
        m.in_sx = false;
        if (b == 0xF7) {
            m.sx.push_back(0xF7);
            sysex_done(x);
            return;
        }
    }
    if (b == 0xF7 && (m.dev == kHostSendDone || m.dev == kWait31)) {   // the closing F7 alone
        m.dev = kIdle;
        clock_unset(x->timeout);
        if (m.sx.size()) post("cz4~: SysEx transfer done");
        return;
    }
    if (b >= 0xF0) { m.status = 0; return; }
    if (b >= 0x80) { m.status = b; m.have = 0; return; }
    if (!m.status) return;
    m.data[m.have++] = b;
    const int need = ((m.status & 0xF0) == 0xC0 || (m.status & 0xF0) == 0xD0) ? 1 : 2;
    if (m.have < need) return;
    m.have = 0;
    midi_message(x, m.status, m.data[0], need == 2 ? m.data[1] : 0);
}

void cz4_proxy_list(t_cz4_proxy* p, t_symbol*, int argc, t_atom* argv) {
    for (int i = 0; i < argc; ++i) cz4_proxy_float(p, atom_getfloat(argv + i));
}

// ---- dsp --------------------------------------------------------------------

t_int* cz4_perform(t_int* w) {
    t_cz4* x = reinterpret_cast<t_cz4*>(w[1]);
    t_sample* l = reinterpret_cast<t_sample*>(w[2]);
    t_sample* r = reinterpret_cast<t_sample*>(w[3]);
    const int n = static_cast<int>(w[4]);
#if PD_FLOATSIZE == 32
    x->synth->render(l, r, n);
#else
    float bl[64], br[64];
    for (int i = 0; i < n; i += 64) {
        const int m = std::min(64, n - i);
        x->synth->render(bl, br, m);
        for (int k = 0; k < m; ++k) { l[i + k] = bl[k]; r[i + k] = br[k]; }
    }
#endif
    return w + 5;
}

void cz4_dsp(t_cz4* x, t_signal** sp) {
    x->synth->set_sample_rate(sp[0]->s_sr);
    dsp_add(cz4_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(sp[0]->s_n));
}

// ---- lifetime ---------------------------------------------------------------

void* cz4_new(t_symbol*, int argc, t_atom* argv) {
    t_cz4* x = reinterpret_cast<t_cz4*>(pd_new(cz4_class));
    load_data();
    x->velocity = 100;
    x->channel = 0;
    x->txchannel = 1;
    x->model = 1;
    x->last_preset = x->last_user = 0;
    x->store_to = -1;
    x->midi = new Midi();
    x->shown = new Shown();
    x->shown->value.assign(static_cast<size_t>(cz4::control_count()), INT_MIN);
    x->synth = new cz4::Synth();
    x->timeout = clock_new(x, reinterpret_cast<t_method>(cz4_timeout));

    floatinlet_new(&x->obj, &x->velocity);
    x->proxy.pd = cz4_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);

    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->info = outlet_new(&x->obj, nullptr);

    if (argc > 0 && argv[0].a_type == A_FLOAT) x->synth->select_patch(static_cast<int>(atom_getfloat(argv)));
    report(x, true);
    return x;
}

void cz4_free(t_cz4* x) {
    clock_free(x->timeout);
    delete x->synth;
    delete x->midi;
    delete x->shown;
}

}  // namespace

extern "C" {
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void cz4_tilde_setup(void) {
    cz4_class = class_new(gensym("cz4~"), reinterpret_cast<t_newmethod>(cz4_new),
                          reinterpret_cast<t_method>(cz4_free), sizeof(t_cz4), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(cz4_class, reinterpret_cast<t_method>(cz4_float));
    class_addlist(cz4_class, reinterpret_cast<t_method>(cz4_list));
    class_addanything(cz4_class, reinterpret_cast<t_method>(cz4_command));
    class_addmethod(cz4_class, reinterpret_cast<t_method>(cz4_dsp), gensym("dsp"), A_CANT, 0);

    cz4_proxy_class = class_new(gensym("cz4~ midi"), nullptr, nullptr, sizeof(t_cz4_proxy),
                                CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(cz4_proxy_class, reinterpret_cast<t_method>(cz4_proxy_anything));
    class_addfloat(cz4_proxy_class, reinterpret_cast<t_method>(cz4_proxy_float));
    class_addlist(cz4_proxy_class, reinterpret_cast<t_method>(cz4_proxy_list));
}
}
