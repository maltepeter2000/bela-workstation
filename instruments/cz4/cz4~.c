/* SPDX-License-Identifier: BSD-3-Clause
 * cz4~ : paraphonic 4-voice Casio CZ phase-distortion voice for Pure Data
 * (Mac, Linux, Bela). Wraps cz_core (derived from melodee's cz_native.c,
 * uPD933 model by Devin Acker; see LICENSE-BSD-3-uPD933.txt).
 *
 * Inlet   1  messages: notes ("note p v", the list "p v", or "p v ch" routed
 *            by MIDI channel), controls, presets, see cz4~-help.pd
 * Outlets 1, 2  signal left, right
 *         3     events: "dump cz4 <values>" (for [skpreset]), "value <name> <v>"
 * Message conventions follow skandrums-pd (dump / params / defaults / get).
 *
 * Control is MIDI only in this version: no CV or gate inlets. A note-on
 * resets the phase of the voice it starts, as on the CZ. Line 2 follows
 * line 1 (trk 1) or plays its own notes from a second channel (trk 0).
 */
#include "m_pd.h"
#include "cz_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* The eight controls (0..127), one per pot, Machinedrum style. Both lines
 * always run with their own wave, wave shape and level. */
enum { C_WAV1, C_WAV2, C_DCW1, C_DCW2, C_DETN, C_AMP1, C_AMP2, C_MOD, NCTL };
static const char *CTL_NAME[NCTL] = {"wav1", "wav2", "dcw1", "dcw2", "detn", "amp1", "amp2", "mod"};
static const float CTL_DEFAULT[NCTL] = {0, 0, 6, 6, 0, 127, 0, 0};

/* Further settings with raw values, in dump order after the controls. */
enum { X_PAI1, X_PAI2, X_OCTV, X_SPRD, X_LEVL, X_DRFT, X_MRPH, X_HOLD, X_A4, X_DVEL, X_TRK, X_CH1, X_CH2,
       X_PMOR, X_SINE, X_ERA, X_FINE, X_PAT8, X_FADE, NEXT };
static const char *EXT_NAME[NEXT] = {"pai1", "pai2", "octv", "sprd", "levl", "drft", "mrph", "hold", "a4", "dvel",
                                     "trk", "ch1", "ch2", "pmor", "sine", "era", "fine", "pat8", "fade"};
static const float EXT_DEFAULT[NEXT] = {-1, -1, 0, 0.5f, 0.5f, 0, 8, 1, 440, 40, 1, 0, 2, 1, 1, 0, 64, 0, 4};

/* One paraphonic allocator per line: line 1 always, line 2 when it is split
 * off (trk 0) and plays its own notes from a second MIDI channel. */
typedef struct { int held[CZ_VOICES]; t_float key[CZ_VOICES]; unsigned age[CZ_VOICES]; int keys; } alloc_t;

static t_class *cz4_class;

typedef struct _cz4 {
    t_object x_obj;
    t_outlet *x_dump;
    cz_engine_t e;
    t_float  ctl[NCTL], ext[NEXT];
    t_float  vel[2];               /* velocity of the last note-on per line, 0..1 */
    float    dcw_prev[2];          /* DCW per line at the end of the last block */
    alloc_t  al[2];
    unsigned clock;
    float   *buf; int bufn;        /* out L, out R in float (Pd may run in double) */
} t_cz4;

static float clampf_(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* DCW of a line: its control plus the velocity share (dvel), 0..1 */
static float dcw_target(const t_cz4 *x, int line)
{
    t_float base = x->ctl[line ? C_DCW2 : C_DCW1];
    t_float vel = x->vel[(line && x->e.split) ? 1 : 0];
    return clampf_((float)(base / 127.0f + x->ext[X_DVEL] / 127.0f * vel), 0.0f, 1.0f);
}

/* ------------------------------------------------------------------ DSP */
static t_int *cz4_perform(t_int *w)
{
    t_cz4 *x = (t_cz4 *)(w[1]);
    t_sample *out_l = (t_sample *)(w[2]), *out_r = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    float *ol = x->buf, *or_ = x->buf + n, d1 = dcw_target(x, 0), d2 = dcw_target(x, 1);
    /* ramps from the last block's values: no zipper on pots or CC */
    cz_render2(&x->e, ol, or_, (unsigned)n, x->dcw_prev[0], d1, x->dcw_prev[1], d2);
    x->dcw_prev[0] = d1; x->dcw_prev[1] = d2;
    for (int i = 0; i < n; i++) { out_l[i] = ol[i]; out_r[i] = or_[i]; }
    return w + 5;
}

static void cz4_dsp(t_cz4 *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    float sr = (float)sp[0]->s_sr;
    if (sr > 0.0f && sr != x->e.fs) { x->e.fs = sr; x->e.lp_hz_cur = 0.0f; }
    if (2 * n > x->bufn) {
        x->buf = (float *)resizebytes(x->buf, (size_t)x->bufn * sizeof(float), (size_t)(2 * n) * sizeof(float));
        x->bufn = 2 * n;
    }
    dsp_add(cz4_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)n);
}

/* ------------------------------------------------------- applying values */
/* line 2 detune: detn 0 = unison .. 127 = one octave up, continuous (about
 * 9.4 cents a step); fine 64 = 0, about 0.8 cent a step
 * (+-50 cents over the range) */
static void update_detune(t_cz4 *x)
{
    x->e.detune = (float)x->ctl[C_DETN] * (12.0f / 127.0f) + (float)(x->ext[X_FINE] - 64.0f) * (1.0f / 128.0f);
}

/* Line levels. Below full scale each amp is a plain level; when amp1 + amp2
 * goes above it, both are scaled so that the sum stays at full scale (as the
 * Cizzle drops its level for chords): two lines at full level cannot clip
 * where one line alone does not. */
static void update_amps(t_cz4 *x)
{
    float a1 = (float)x->ctl[C_AMP1] / 127.0f, a2 = (float)x->ctl[C_AMP2] / 127.0f, sum = a1 + a2;
    float k = sum > 1.0f ? 1.0f / sum : 1.0f;
    x->e.line_amp[0] = a1 * k; x->e.line_amp[1] = a2 * k;
}

static void apply_ctl(t_cz4 *x, int i, t_float v)
{
    if (i < 0 || i >= NCTL) return;
    v = clampf_((float)v, 0.0f, 127.0f);
    x->ctl[i] = v;
    cz_engine_t *e = &x->e;
    switch (i) {
    case C_WAV1: e->line[0].pos = (float)v / 127.0f * CZ_MORPH_MAX; break;
    case C_WAV2: e->line[1].pos = (float)v / 127.0f * CZ_MORPH_MAX; break;
    case C_DETN: update_detune(x); break;
    case C_AMP1: case C_AMP2: update_amps(x); break;                  /* amp2 with ring: the ring product */
    case C_MOD:                                                        /* off / ring / noise */
        e->mod = v < 43.0f ? 0 : v < 86.0f ? CZ_MOD_RING : CZ_MOD_NOISE; break;
    default: break;                                                   /* dcw1, dcw2: read per block */
    }
}

static void set_pair(cz_line_t *l, t_float f)
{
    if (f < 0) l->pair_on = 0;
    else { l->pair = clampf_((float)f, 0.0f, CZ_MORPH_MAX); l->pair_on = 1; }
}

static void apply_ext(t_cz4 *x, int i, t_float v)
{
    cz_engine_t *e = &x->e;
    switch (i) {
    case X_PAI1: if (v >= 0) v = clampf_((float)v, 0.0f, CZ_MORPH_MAX); set_pair(&e->line[0], v); break;
    case X_PAI2: if (v >= 0) v = clampf_((float)v, 0.0f, CZ_MORPH_MAX); set_pair(&e->line[1], v); break;
    case X_OCTV: v = v < -1 ? -1 : v > 1 ? 1 : (t_float)(int)v; e->octave = (int)v; break;
    case X_SPRD: v = clampf_((float)v, 0.0f, 1.0f); e->spread = (float)v; break;
    case X_LEVL: v = clampf_((float)v, 0.0f, 2.0f); e->level = (float)v; break;
    case X_DRFT: v = clampf_((float)v, 0.0f, 1.0f); e->vint.drift = (float)v; break;
    case X_MRPH: v = v < 0 ? 0 : v; e->morph_ms = (float)v; break;
    case X_HOLD:
        v = v != 0;
        if (v == 0) for (int k = 0; k < CZ_VOICES; k++) {
            if (!x->al[0].held[k]) cz_voice_off(e, k);
            if (!x->al[1].held[k]) cz_line2_off(e, k);
        }
        break;
    case X_A4: if (v < 300 || v > 600) return; e->a4 = (float)v; break;
    case X_DVEL: v = clampf_((float)v, 0.0f, 127.0f); break;          /* read per block */
    case X_TRK:                                   /* 1: line 2 follows line 1; 0: own notes */
        v = v != 0;
        if (v != 0) {
            for (int k = 0; k < CZ_VOICES; k++) { x->al[1].held[k] = 0; cz_line2_off(e, k); }
            x->al[1].keys = 0;
        }
        e->split = v == 0;
        break;
    case X_CH1: v = (t_float)(int)clampf_((float)v, 0.0f, 16.0f); break;  /* 0 = any channel */
    case X_CH2: v = (t_float)(int)clampf_((float)v, 1.0f, 16.0f); break;
    case X_PMOR: v = v != 0; e->phase_morph = v != 0; break;       /* 0: CZ-style crossfade */
    case X_SINE: v = v != 0; e->sine0 = v != 0; break;             /* 0: chip behaviour at DCW 0 */
    case X_ERA:  v = clampf_((float)v, 0.0f, 127.0f); cz_set_era(e, (float)v / 127.0f); break;
    case X_FINE: v = clampf_((float)v, 0.0f, 127.0f); x->ext[X_FINE] = v; update_detune(x); break;
    case X_PAT8: v = v != 0; e->patent8 = v != 0; break;            /* 8-bit patent resonance */
    case X_FADE: v = clampf_((float)v, 1.0f, 100.0f); e->fade_ms = (float)v; break;   /* voice fade, ms */
    default: return;
    }
    x->ext[i] = v;
}

/* --------------------------------------------------- voice allocation */
/* Paraphonic, per line: a note-on with no key down starts a new phrase and
 * clears that line's old voices; otherwise a free (released) voice is taken,
 * else the oldest held one is stolen. With hold on, released voices keep
 * sounding until the next phrase, so an external VCA's release keeps the
 * whole chord. Line 1 drives the voice (and line 2 with it, trk 1); with
 * trk 0 line 2 is allocated on its own. */
static void line_on(t_cz4 *x, int line, int v, t_float pitch)
{
    if (line) { cz_line2_on(&x->e, v, (float)pitch); return; }
    if (x->e.split) { x->e.v[v].ph[0] = x->e.v[v].tog[0] = 0; }   /* line 1 phase only */
    else cz_voice_reset(&x->e, v);
    cz_voice_on(&x->e, v, (float)pitch);
}
static void line_off(t_cz4 *x, int line, int v)
{
    if (line) cz_line2_off(&x->e, v); else cz_voice_off(&x->e, v);
}

static void note_on(t_cz4 *x, int line, t_float pitch, t_float vel)
{
    alloc_t *a = &x->al[line];
    int v, pick = -1;
    x->vel[line] = clampf_((float)vel / 127.0f, 0.0f, 1.0f);
    for (v = 0; v < CZ_VOICES; v++) if (a->held[v] && a->key[v] == pitch) return;
    if (a->keys == 0) for (v = 0; v < CZ_VOICES; v++) line_off(x, line, v);
    for (v = 0; v < CZ_VOICES; v++)
        if (!a->held[v] && (pick < 0 || a->age[v] < a->age[pick])) pick = v;
    if (pick < 0) {                                   /* all held: steal the oldest */
        for (v = 0; v < CZ_VOICES; v++) if (pick < 0 || a->age[v] < a->age[pick]) pick = v;
        a->keys--;
    }
    a->held[pick] = 1; a->key[pick] = pitch; a->age[pick] = ++x->clock; a->keys++;
    line_on(x, line, pick, pitch);
}

static void note_off(t_cz4 *x, int line, t_float pitch)
{
    alloc_t *a = &x->al[line];
    for (int v = 0; v < CZ_VOICES; v++)
        if (a->held[v] && a->key[v] == pitch) {
            a->held[v] = 0; a->keys--; a->age[v] = ++x->clock;
            if (x->ext[X_HOLD] == 0) line_off(x, line, v);
        }
}

static void play(t_cz4 *x, int line, t_float pitch, t_float vel)
{
    if (line && !x->e.split) return;                  /* line 2 follows line 1 */
    if (vel > 0) note_on(x, line, pitch, vel); else note_off(x, line, pitch);
}

static void cz4_note(t_cz4 *x, t_floatarg pitch, t_floatarg vel) { play(x, 0, pitch, vel); }
static void cz4_note2(t_cz4 *x, t_floatarg pitch, t_floatarg vel) { play(x, 1, pitch, vel); }

/* "p v" plays line 1; "p v ch" (e.g. [notein] -> [pack f f f]) is routed by
 * channel: ch2 goes to line 2 when it is split off, ch1 (0 = any) to line 1. */
static void cz4_list(t_cz4 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc < 2) return;
    t_float p = atom_getfloatarg(0, argc, argv), v = atom_getfloatarg(1, argc, argv);
    if (argc < 3) { play(x, 0, p, v); return; }
    int ch = (int)atom_getfloatarg(2, argc, argv);
    if (x->e.split && ch == (int)x->ext[X_CH2]) play(x, 1, p, v);
    else if ((int)x->ext[X_CH1] == 0 || ch == (int)x->ext[X_CH1]) play(x, 0, p, v);
}

static void cz4_panic(t_cz4 *x)
{
    for (int v = 0; v < CZ_VOICES; v++) {
        x->al[0].held[v] = x->al[1].held[v] = 0;
        cz_voice_off(&x->e, v); cz_line2_off(&x->e, v);
    }
    x->al[0].keys = x->al[1].keys = 0;
}

static void cz4_phase(t_cz4 *x) { cz_gate_reset(&x->e); }

/* direct voice control, bypassing the allocator (e.g. with your own [poly]) */
static void cz4_voice(t_cz4 *x, t_floatarg v, t_floatarg pitch)
{
    int i = (int)v;
    if (i >= 0 && i < CZ_VOICES) line_on(x, 0, i, pitch);
}
static void cz4_voiceoff(t_cz4 *x, t_floatarg v) { cz_voice_off(&x->e, (int)v); }

/* ------------------------------------------------- named value messages */
/* Any selector that names a control or a setting: "wav1 64", "ring 1" ... */
static void cz4_anything(t_cz4 *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc < 1) { pd_error(x, "cz4~: %s needs a value", s->s_name); return; }
    t_float v = atom_getfloatarg(0, argc, argv);
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) { apply_ctl(x, i, v); return; }
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) { apply_ext(x, i, v); return; }
    pd_error(x, "cz4~: unknown message '%s'", s->s_name);
}

/* "ctl <index> <value>": controls by number, handy for MIDI CC mapping */
static void cz4_ctl(t_cz4 *x, t_floatarg i, t_floatarg v) { apply_ctl(x, (int)i, v); }

/* "params v0 v1 ...": controls by index */
static void cz4_params(t_cz4 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    for (int i = 0; i < argc && i < NCTL; i++) apply_ctl(x, i, atom_getfloatarg(i, argc, argv));
}

static void set_defaults(t_cz4 *x)
{
    for (int i = 0; i < NCTL; i++) apply_ctl(x, i, CTL_DEFAULT[i]);
    for (int i = 0; i < NEXT; i++) apply_ext(x, i, EXT_DEFAULT[i]);
}
static void cz4_defaults(t_cz4 *x) { set_defaults(x); }
static void cz4_reset(t_cz4 *x) { cz4_panic(x); set_defaults(x); cz_gate_reset(&x->e); }

/* "get <name>": answer "value <name> <v>" on the event outlet */
static void cz4_get(t_cz4 *x, t_symbol *name)
{
    t_atom a[2];
    for (int i = 0; i < NCTL + NEXT; i++) {
        const char *n = i < NCTL ? CTL_NAME[i] : EXT_NAME[i - NCTL];
        if (name == gensym(n)) {
            SETSYMBOL(a, name); SETFLOAT(a + 1, i < NCTL ? x->ctl[i] : x->ext[i - NCTL]);
            outlet_anything(x->x_dump, gensym("value"), 2, a);
            return;
        }
    }
    pd_error(x, "cz4~: get: unknown name '%s'", name->s_name);
}

/* ------------------------------------------------------- presets */
/* "dump" sends "dump cz4 v0 v1 ..." (controls, then settings) out of the
 * event outlet, as the skandrums voices do; "dump cz4 v0 ..." restores it.
 * Lines of other voices are ignored, so [skpreset] works unchanged. */
static void cz4_dump(t_cz4 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc == 0) {
        t_atom a[1 + NCTL + NEXT];
        SETSYMBOL(a, gensym("cz4"));
        for (int i = 0; i < NCTL; i++) SETFLOAT(a + 1 + i, x->ctl[i]);
        for (int i = 0; i < NEXT; i++) SETFLOAT(a + 1 + NCTL + i, x->ext[i]);
        outlet_anything(x->x_dump, gensym("dump"), 1 + NCTL + NEXT, a);
        return;
    }
    if (argv[0].a_type != A_SYMBOL || argv[0].a_w.w_symbol != gensym("cz4")) return;
    for (int i = 1; i < argc && i <= NCTL + NEXT; i++) {
        t_float v = atom_getfloatarg(i, argc, argv);
        if (i <= NCTL) apply_ctl(x, i - 1, v); else apply_ext(x, i - 1 - NCTL, v);
    }
}

/* ---------------------------------------- vintage stages, tone import */
static void vint_done(t_cz4 *x) { cz_vintage_update(&x->e); }
static void cz4_quant(t_cz4 *x, t_floatarg f) { x->e.vint.quant = clampf_((float)f, 0.0f, 1.0f); vint_done(x); }
static void cz4_chiprate(t_cz4 *x, t_floatarg f) { x->e.vint.chip_hz = f < 0 ? 0.0f : (float)f; vint_done(x); }
static void cz4_bits(t_cz4 *x, t_floatarg f) { int b = (int)f; x->e.vint.dac_bits = b < 4 ? 4 : b > 16 ? 16 : b; vint_done(x); }
static void cz4_dacerr(t_cz4 *x, t_floatarg f) { x->e.vint.dac_err = clampf_((float)f, 0.0f, 1.0f); vint_done(x); }
static void cz4_hiss(t_cz4 *x, t_floatarg f) { x->e.vint.noise_db = (float)f; vint_done(x); }
static void cz4_lpf(t_cz4 *x, t_floatarg f) { x->e.vint.lpf_hz = f < 0 ? 0.0f : (float)f; vint_done(x); }
static void cz4_drive(t_cz4 *x, t_floatarg f) { x->e.vint.drive = clampf_((float)f, 0.0f, 1.0f); vint_done(x); }

/* "tone b0 ... b127 [.. b143]": decoded native CZ-1 tone bytes */
static void cz4_tone(t_cz4 *x, t_symbol *s, int argc, t_atom *argv)
{
    uint8_t b[144];
    (void)s;
    if (argc < 128) { pd_error(x, "cz4~: tone needs 128 or 144 byte values, got %d", argc); return; }
    if (argc > 144) argc = 144;
    for (int i = 0; i < argc; i++) b[i] = (uint8_t)atom_getfloatarg(i, argc, argv);
    if (cz_import_tone(&x->e, b, (unsigned)argc)) { pd_error(x, "cz4~: invalid CZ tone data"); return; }
    /* The CZ line modes become wave and mix settings: 1 = line 1 only,
     * 2 = line 2 only, 1+1' = line 2 copies line 1, 1+2' = both. */
    cz_engine_t *e = &x->e;
    int mode = e->line_mode;
    if (mode == CZ_LINE_1_1) e->line[1] = e->line[0];
    e->line_mode = CZ_LINE_1_2;
    /* write the imported state back into the controls so dump stays true */
    apply_ctl(x, C_WAV1, clampf_(e->line[0].pos / CZ_MORPH_MAX * 127.0f, 0.0f, 127.0f));
    apply_ctl(x, C_WAV2, clampf_(e->line[1].pos / CZ_MORPH_MAX * 127.0f, 0.0f, 127.0f));
    {   /* the CZ's detune (1/64 semitone steps) goes to detn, the rest of
         * the rounding to fine; a downward or wider CZ detune is clamped to
         * unison or one octave up */
        float total = clampf_(e->detune, 0.0f, 12.0f), d = roundf(total * (127.0f / 12.0f));
        x->ext[X_FINE] = clampf_(64.0f + (total - d * (12.0f / 127.0f)) * 128.0f, 0.0f, 127.0f);
        apply_ctl(x, C_DETN, d);
    }
    apply_ctl(x, C_AMP1, mode == CZ_LINE_2 ? 0 : mode == CZ_LINE_1 ? 127 : 64);
    apply_ctl(x, C_AMP2, mode == CZ_LINE_1 ? 0 : mode == CZ_LINE_2 ? 127 : 64);
    apply_ctl(x, C_MOD, (e->mod & CZ_MOD_RING) ? 64 : (e->mod & CZ_MOD_NOISE) ? 106 : 0);
    x->ext[X_PAI1] = e->line[0].pair_on ? e->line[0].pair : -1;
    x->ext[X_PAI2] = e->line[1].pair_on ? e->line[1].pair : -1;
    x->ext[X_OCTV] = e->octave;
}

static void cz4_print(t_cz4 *x)
{
    char line[256]; int n = 0;
    post("cz4~: sample rate %g, %d voices, line 2 %s", x->e.fs, CZ_VOICES, x->e.split ? "on its own (trk 0)" : "follows line 1");
    for (int i = 0; i < NCTL; i++) n += snprintf(line + n, sizeof line - (size_t)n, "%s %g  ", CTL_NAME[i], x->ctl[i]);
    post("cz4~: %s", line);
    n = 0;
    for (int i = 0; i < NEXT; i++) n += snprintf(line + n, sizeof line - (size_t)n, "%s %g  ", EXT_NAME[i], x->ext[i]);
    post("cz4~: %s", line);
    for (int v = 0; v < CZ_VOICES; v++) {
        const cz_voice_t *c = &x->e.v[v];
        if (x->e.split)
            post("cz4~: voice %d line 1 note %g %s / line 2 note %g %s", v, c->note,
                 x->al[0].held[v] ? "held" : c->gain_target > 0 ? "released" : "off",
                 c->note2, x->al[1].held[v] ? "held" : c->gain2_target > 0 ? "released" : "off");
        else
            post("cz4~: voice %d note %g %s", v, c->note,
                 x->al[0].held[v] ? "held" : c->gain_target > 0 ? "released, sounding" : "off");
    }
}

/* ------------------------------------------------------------ lifecycle */
static void *cz4_new(void)
{
    t_cz4 *x = (t_cz4 *)pd_new(cz4_class);
    float sr = (float)sys_getsr();
    cz_init(&x->e, sr > 0.0f ? sr : 44100.0f);
    x->e.line_mode = CZ_LINE_1_2;                 /* both lines, own waves; mix decides */
    set_defaults(x);
    x->dcw_prev[0] = dcw_target(x, 0); x->dcw_prev[1] = dcw_target(x, 1);
    x->bufn = 2 * 64;
    x->buf = (float *)getbytes((size_t)x->bufn * sizeof(float));
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_dump = outlet_new(&x->x_obj, &s_anything);
    return x;
}

static void cz4_free(t_cz4 *x) { freebytes(x->buf, (size_t)x->bufn * sizeof(float)); }

#define M1(name) class_addmethod(cz4_class, (t_method)cz4_##name, gensym(#name), A_FLOAT, 0)
#define M0(name) class_addmethod(cz4_class, (t_method)cz4_##name, gensym(#name), 0)

#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void cz4_tilde_setup(void)
{
    cz4_class = class_new(gensym("cz4~"), (t_newmethod)cz4_new, (t_method)cz4_free,
                          sizeof(t_cz4), CLASS_DEFAULT, 0);
    class_addmethod(cz4_class, (t_method)cz4_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(cz4_class, (t_method)cz4_list);
    class_addanything(cz4_class, (t_method)cz4_anything);
    class_addmethod(cz4_class, (t_method)cz4_note, gensym("note"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(cz4_class, (t_method)cz4_note2, gensym("note2"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(cz4_class, (t_method)cz4_ctl, gensym("ctl"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(cz4_class, (t_method)cz4_voice, gensym("voice"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(cz4_class, (t_method)cz4_dump, gensym("dump"), A_GIMME, 0);
    class_addmethod(cz4_class, (t_method)cz4_params, gensym("params"), A_GIMME, 0);
    class_addmethod(cz4_class, (t_method)cz4_get, gensym("get"), A_SYMBOL, 0);
    class_addmethod(cz4_class, (t_method)cz4_tone, gensym("tone"), A_GIMME, 0);
    M1(voiceoff); M0(panic); M0(phase); M0(defaults); M0(reset); M0(print);
    M1(quant); M1(chiprate); M1(bits); M1(dacerr); M1(hiss); M1(lpf); M1(drive);
}
