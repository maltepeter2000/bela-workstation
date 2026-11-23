// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Peter
//
// sverb~ -- the reverb of Signalsmith Basics (an 8-channel FDN with early reflections,
// damping and detune in the feedback loop) as a Pd object for bela-workstation.
// By default a send effect: dry 0, wet 1 (100 % wet). [sverb~ insert] starts with dry 1,
// wet 0.5 to use it in line.
//
//   inlet 1   signal: left (or mono); any: a message, see below
//   inlet 2   signal: right. Not connected: mono, the left input goes into both sides of
//             the reverb (the same as connecting one source to both inlets); the output
//             is stereo either way. "mono 1" / "mono 0" force it, "mono auto" (default)
//             follows the connection.
//   inlet 3   any: a message (the same set as inlet 1)
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: value <control> <0-127> <value> (answer to get),
//             dump sverb v0 .. v9 (answer to dump), display <title> <value> on every change
//             (for [lkdisplay] or an OLED)
//
// Controls (the protocol of vrs23~ and skandrums, 0-127; 64 is the middle of the range):
//   #  name      0 .. 64 .. 127              default (send)
//   0  dry       0 .. 1 .. 4 (gain)          0
//   1  wet       0 .. 1 .. 4 (gain)          1
//   2  room      10 .. 100 .. 200 ms         80 ms
//   3  decay     0.01 .. 2 .. 30 s (RT20)    1 s
//   4  early     0 .. 1 .. 2.5               1.5
//   5  detune    0 .. 5 .. 50                2
//   6  lowcut    10 .. 80 .. 500 Hz          80 Hz
//   7  lowdamp   1 .. 2 .. 10                1.5
//   8  highcut   1000 .. 5000 .. 20000 Hz    12000 Hz
//   9  highdamp  1 .. 2 .. 10                2.5
//
// Messages:
//   <control> <0-127>     set a control
//   val <control> <v>     set it in its own unit (ms, s, Hz, gain)
//   ctl <index> <0-127>   set control number index
//   params v0 .. v9       all controls at once (0-127)
//   get <control>         -> value <control> <0-127> <v>
//   dump                  -> dump sverb v0 .. v9;   dump sverb v0 .. v9 restores it
//   defaults / reset      the defaults of the mode (send or insert)
//   send / insert         dry 0 wet 1 / dry 1 wet 0.5 (the other controls stay)
//   preset ambient        the "ambient" preset of Signalsmith Basics (dry stays)
//   clear                 empties the reverb (the tail stops at once)
//   mono auto|0|1         mono input: from the connection (default), off, on
//   print                 all controls to the Pd window
//
// Room changes cross-fade between the old and the new delay lengths, and all controls are
// smoothed inside the reverb, so turning them does not click.

#include "m_pd.h"
#include "g_canvas.h"

#include "signalsmith-basics/reverb.h"

#include <cmath>
#include <cstring>
#include <new>
#include <vector>

namespace {

using Reverb = signalsmith::basics::ReverbFloat;

constexpr int kControls = 10;

// A control: its range as low / middle / high (the middle at 64) and its default.
// The curve x -> value goes through all three points: v = lo + (hi-lo) * a x / (1 - x + a x)
// with a = m / (1-m), m = (mid-lo)/(hi-lo); a = 1 is a straight line.
struct Control {
    const char* name;
    const char* title;   // for the display
    const char* unit;
    float lo, mid, hi;
    float send_default, insert_default;
};

const Control kTable[kControls] = {
    {"dry",      "Dry",       "",    0.f,    1.f,    4.f,     0.f,     1.f},
    {"wet",      "Wet",       "",    0.f,    1.f,    4.f,     1.f,     0.5f},
    {"room",     "Room",      "ms",  10.f,   100.f,  200.f,   80.f,    80.f},
    {"decay",    "Decay",     "s",   0.01f,  2.f,    30.f,    1.f,     1.f},
    {"early",    "Early",     "",    0.f,    1.f,    2.5f,    1.5f,    1.5f},
    {"detune",   "Detune",    "",    0.f,    5.f,    50.f,    2.f,     2.f},
    {"lowcut",   "Low cut",   "Hz",  10.f,   80.f,   500.f,   80.f,    80.f},
    {"lowdamp",  "Low damp",  "",    1.f,    2.f,    10.f,    1.5f,    1.5f},
    {"highcut",  "High cut",  "Hz",  1000.f, 5000.f, 20000.f, 12000.f, 12000.f},
    {"highdamp", "High damp", "",    1.f,    2.f,    10.f,    2.5f,    2.5f},
};

double curve_a(const Control& c) {
    double m = (c.mid - c.lo) / double(c.hi - c.lo);
    return m / (1 - m);
}

double to_value(const Control& c, double cc) {          // 0..127 -> unit
    double x = cc / 127.0;
    x = x < 0 ? 0 : (x > 1 ? 1 : x);
    double a = curve_a(c);
    double t = a * x / (1 - x + a * x);
    return c.lo + (c.hi - c.lo) * t;
}

double to_cc(const Control& c, double v) {              // unit -> 0..127
    double t = (v - c.lo) / double(c.hi - c.lo);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    double a = curve_a(c);
    double x = t / (a - t * (a - 1));
    return x * 127.0;
}

int find_control(t_symbol* s) {
    for (int i = 0; i < kControls; ++i)
        if (!std::strcmp(s->s_name, kTable[i].name)) return i;
    return -1;
}

t_class* sverb_class = nullptr;
t_class* sverb_proxy_class = nullptr;

struct t_sverb;

struct t_sverb_proxy {
    t_pd pd;
    t_sverb* owner;
};

struct t_sverb {
    t_object obj;
    t_float f;                    // the main signal inlet's float
    t_canvas* canvas;             // where the object lies, to see whether inlet 2 is connected
    t_inlet* right;
    t_outlet* out;
    t_sverb_proxy proxy;

    Reverb* fx;
    std::vector<float>* in;       // copy of the inputs (Pd may hand in the output buffers)
    double sr;
    int block;
    bool insert;                  // mode of the defaults
    int mono_mode;                // -1 auto, 0 off, 1 on
    bool mono;                    // in effect
    float cc[kControls];          // 0..127, fractional allowed
};

// ---- controls ---------------------------------------------------------------

void apply(t_sverb* x, int i) {
    double v = to_value(kTable[i], x->cc[i]);
    Reverb& r = *x->fx;
    switch (i) {
        case 0: r.dry = v; break;
        case 1: r.wet = v; break;
        case 2: r.roomMs = v; break;
        case 3: r.rt20 = v; break;
        case 4: r.early = v; break;
        case 5: r.detune = v; break;
        case 6: r.lowCutHz = v; break;
        case 7: r.lowDampRate = v; break;
        case 8: r.highCutHz = v; break;
        case 9: r.highDampRate = v; break;
    }
}

void format_value(const Control& c, double v, char* buf, size_t n) {
    if (!std::strcmp(c.unit, "Hz") && v >= 1000) snprintf(buf, n, "%.1f kHz", v / 1000);
    else if (!std::strcmp(c.unit, "s") && v < 0.995) snprintf(buf, n, "%.0f ms", v * 1000);
    else if (*c.unit) snprintf(buf, n, v < 10 ? "%.2f %s" : "%.0f %s", v, c.unit);
    else snprintf(buf, n, "%.2f", v);
}

void display(t_sverb* x, int i) {
    char buf[32];
    format_value(kTable[i], to_value(kTable[i], x->cc[i]), buf, sizeof buf);
    t_atom a[2];
    SETSYMBOL(a, gensym(kTable[i].title));
    SETSYMBOL(a + 1, gensym(buf));
    outlet_anything(x->out, gensym("display"), 2, a);
}

void set_cc(t_sverb* x, int i, double cc, bool show = true) {
    if (i < 0 || i >= kControls) return;
    x->cc[i] = static_cast<float>(cc < 0 ? 0 : (cc > 127 ? 127 : cc));
    apply(x, i);
    if (show) display(x, i);
}

void set_defaults(t_sverb* x) {
    for (int i = 0; i < kControls; ++i) {
        const Control& c = kTable[i];
        set_cc(x, i, to_cc(c, x->insert ? c.insert_default : c.send_default), false);
    }
}

void report(t_sverb* x, int i) {
    t_atom a[3];
    SETSYMBOL(a, gensym(kTable[i].name));
    SETFLOAT(a + 1, x->cc[i]);
    SETFLOAT(a + 2, static_cast<t_float>(to_value(kTable[i], x->cc[i])));
    outlet_anything(x->out, gensym("value"), 3, a);
}

void dump(t_sverb* x) {
    t_atom a[kControls + 1];
    SETSYMBOL(a, gensym("sverb"));
    for (int i = 0; i < kControls; ++i) SETFLOAT(a + 1 + i, x->cc[i]);
    outlet_anything(x->out, gensym("dump"), kControls + 1, a);
}

void print(t_sverb* x) {
    post("sverb~: %s, %s input", x->insert ? "insert" : "send",
         x->mono ? "mono" : "stereo");
    for (int i = 0; i < kControls; ++i) {
        char buf[32];
        format_value(kTable[i], to_value(kTable[i], x->cc[i]), buf, sizeof buf);
        post("  %d %-9s %6.1f  %s", i, kTable[i].name, x->cc[i], buf);
    }
}

void preset_ambient(t_sverb* x) {
    // the values of ReverbSTFX::presets("ambient")
    const float v[kControls] = {0, 0.85f, 80, 8.5f, 0.55f, 8.5f, 50, 1.5f, 7200, 2};
    for (int i = 1; i < kControls; ++i) set_cc(x, i, to_cc(kTable[i], v[i]), false);
}

void command(t_sverb* x, t_symbol* s, int argc, t_atom* argv) {
    int i = find_control(s);
    if (i >= 0) {
        if (argc > 0) set_cc(x, i, atom_getfloat(argv));
        else report(x, i);
        return;
    }
    const char* m = s->s_name;
    if (!std::strcmp(m, "val") && argc >= 2) {
        int k = find_control(atom_getsymbol(argv));
        if (k < 0) { pd_error(x, "sverb~: val: no control %s", atom_getsymbol(argv)->s_name); return; }
        set_cc(x, k, to_cc(kTable[k], atom_getfloat(argv + 1)));
    } else if (!std::strcmp(m, "ctl") && argc >= 2) {
        set_cc(x, static_cast<int>(atom_getfloat(argv)), atom_getfloat(argv + 1));
    } else if (!std::strcmp(m, "params")) {
        for (int k = 0; k < argc && k < kControls; ++k) set_cc(x, k, atom_getfloat(argv + k), false);
    } else if (!std::strcmp(m, "get") && argc >= 1) {
        int k = find_control(atom_getsymbol(argv));
        if (k >= 0) report(x, k);
    } else if (!std::strcmp(m, "dump")) {
        if (argc == 0) dump(x);
        else if (atom_getsymbol(argv) == gensym("sverb"))
            for (int k = 0; k + 1 < argc && k < kControls; ++k) set_cc(x, k, atom_getfloat(argv + 1 + k), false);
    } else if (!std::strcmp(m, "defaults") || !std::strcmp(m, "reset")) {
        set_defaults(x);
    } else if (!std::strcmp(m, "send") || !std::strcmp(m, "insert")) {
        x->insert = (m[0] == 'i');
        set_cc(x, 0, to_cc(kTable[0], x->insert ? kTable[0].insert_default : kTable[0].send_default), false);
        set_cc(x, 1, to_cc(kTable[1], x->insert ? kTable[1].insert_default : kTable[1].send_default), false);
    } else if (!std::strcmp(m, "preset") && argc >= 1) {
        if (atom_getsymbol(argv) == gensym("ambient")) preset_ambient(x);
        else pd_error(x, "sverb~: no preset %s (there is: ambient)", atom_getsymbol(argv)->s_name);
    } else if (!std::strcmp(m, "clear")) {
        x->fx->reset();
    } else if (!std::strcmp(m, "mono") && argc >= 1) {
        if (argv[0].a_type == A_SYMBOL) x->mono_mode = -1;   // auto
        else x->mono_mode = atom_getfloat(argv) != 0 ? 1 : 0;
        canvas_update_dsp();                                   // takes effect in sverb_dsp
    } else if (!std::strcmp(m, "print")) {
        print(x);
    } else {
        pd_error(x, "sverb~: unknown message %s", m);
    }
}

void proxy_anything(t_sverb_proxy* p, t_symbol* s, int argc, t_atom* argv) {
    command(p->owner, s, argc, argv);
}

// ---- DSP --------------------------------------------------------------------

bool right_connected(t_sverb* x) {
    if (!x->canvas) return true;
    t_linetraverser t;
    linetraverser_start(&t, x->canvas);
    while (linetraverser_next(&t))
        if (t.tr_ob2 == &x->obj && t.tr_inno == 1) return true;
    return false;
}

t_int* sverb_perform(t_int* w) {
    t_sverb* x = reinterpret_cast<t_sverb*>(w[1]);
    const t_sample* inl = reinterpret_cast<t_sample*>(w[2]);
    const t_sample* inr = reinterpret_cast<t_sample*>(w[3]);
    t_sample* outl = reinterpret_cast<t_sample*>(w[4]);
    t_sample* outr = reinterpret_cast<t_sample*>(w[5]);
    const int n = static_cast<int>(w[6]);

    float* l = x->in->data();
    float* r = l + x->block;
    if (x->mono) inr = inl;
    for (int i = 0; i < n; ++i) { l[i] = inl[i]; r[i] = inr[i]; }
    const float* ins[2] = {l, r};
    float* outs[2] = {outl, outr};
    x->fx->process(ins, outs, n);
    return w + 7;
}

void sverb_dsp(t_sverb* x, t_signal** sp) {
    const double sr = sp[0]->s_sr;
    const int n = sp[0]->s_n;
    if (sr != x->sr || n > x->block) {
        x->sr = sr;
        x->block = n > x->block ? n : x->block;
        x->fx->configure(sr, x->block, 2);
        x->in->assign(2 * x->block, 0.f);
        for (int i = 0; i < kControls; ++i) apply(x, i);
        x->fx->reset();
    }
    x->mono = x->mono_mode < 0 ? !right_connected(x) : x->mono_mode == 1;
    dsp_add(sverb_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
            static_cast<t_int>(n));
}

// ---- lifetime ---------------------------------------------------------------

void* sverb_new(t_symbol*, int argc, t_atom* argv) {
    t_sverb* x = reinterpret_cast<t_sverb*>(pd_new(sverb_class));
    x->f = 0;
    x->canvas = canvas_getcurrent();
    x->fx = new Reverb();
    x->in = new std::vector<float>();
    x->sr = 0;
    x->block = 0;
    x->insert = false;
    x->mono_mode = -1;
    x->mono = false;
    for (int i = 0; i < argc; ++i)
        if (argv[i].a_type == A_SYMBOL) {
            t_symbol* s = argv[i].a_w.w_symbol;
            if (s == gensym("insert")) x->insert = true;
            else if (s == gensym("send")) x->insert = false;
            else if (s == gensym("ambient")) {}
            else pd_error(x, "sverb~: unknown argument %s (send, insert, ambient)", s->s_name);
        }
    set_defaults(x);
    for (int i = 0; i < argc; ++i)
        if (argv[i].a_type == A_SYMBOL && argv[i].a_w.w_symbol == gensym("ambient")) preset_ambient(x);

    x->right = inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    x->proxy.pd = sverb_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);
    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->out = outlet_new(&x->obj, nullptr);
    return x;
}

void sverb_free(t_sverb* x) {
    delete x->fx;
    delete x->in;
}

}  // namespace

extern "C" {
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void sverb_tilde_setup(void) {
    sverb_class = class_new(gensym("sverb~"), reinterpret_cast<t_newmethod>(sverb_new),
                            reinterpret_cast<t_method>(sverb_free), sizeof(t_sverb),
                            CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(sverb_class, t_sverb, f);
    class_addanything(sverb_class, reinterpret_cast<t_method>(command));
    class_addmethod(sverb_class, reinterpret_cast<t_method>(sverb_dsp), gensym("dsp"), A_CANT, 0);

    sverb_proxy_class = class_new(gensym("sverb~ messages"), nullptr, nullptr,
                                  sizeof(t_sverb_proxy), CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(sverb_proxy_class, reinterpret_cast<t_method>(proxy_anything));
}
}
