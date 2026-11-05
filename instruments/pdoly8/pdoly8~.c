/* SPDX-License-Identifier: BSD-3-Clause
 * pdoly8~ : 4-voice Korg Poly-800 tone generation for Pure Data (Mac,
 * Linux, Bela). Wraps pdoly8_core (MSM5232 dividers, foot buses, waveform
 * networks, band-limited).
 *
 * Inlet   1  messages: notes ("note p v", the list "p v", or "p v ch" from
 *            [notein]), controls, settings, presets, see pdoly8~-help.pd
 * Outlets 1, 2  signal left, right
 *         3     events: "dump pdoly8 <values>" (for [skpreset]), "value <name> <v>"
 * Message conventions follow cz4~ and skandrums-pd (dump / params / defaults / get).
 *
 * No envelopes, no filter in this version: a key gates its voice (with a
 * short de-click ramp); level shaping is left to an external VCA.
 */
#include "m_pd.h"
#include "pdoly8_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* The eight controls (0..127), one per pot or MIDI CC. */
enum { C_WAV1, C_WAV2, C_HAR1, C_HAR2, C_INTV, C_DETN, C_LEV1, C_LEV2, NCTL };
static const char *CTL_NAME[NCTL] = {"wav1", "wav2", "har1", "har2", "intv", "detn", "lev1", "lev2"};
static const float CTL_DEFAULT[NCTL] = {127, 127, 64, 64, 0, 0, 127, 0};

/* Settings with raw values, in dump order after the controls. */
enum { X_MODE, X_OCT1, X_OCT2, X_NOIS, X_SPRD, X_LEVL, X_MRPH, X_HOLD, X_A4, X_TUNE, X_BND,
       X_MGF, X_MGI, X_HSNP, X_CH1, X_TONE, X_PATH, X_NORM, X_PFAD, X_PW1, X_PW2, X_SYNC, X_SWP2, X_FM, X_FMR, X_FMS, X_WSH1, X_WSH2, X_WMX1, X_WMX2,
       X_ENV1, X_ENV2, X_LEAK, X_A1, X_D1, X_BP1, X_SL1, X_SU1, X_R1, X_A2, X_D2, X_BP2, X_SL2, X_SU2, X_R2, X_PORT, X_PMOD,
       X_CUT, X_RES, X_KBD, X_POL, X_EGI, X_TRG, X_ENV3, X_A3, X_D3, X_BP3, X_SL3, X_SU3, X_R3,
       X_MGD, X_MGV, X_MGR, X_TREB, X_BASS, X_DTIM, X_DFB, X_DMF, X_DMI, X_DLEV, NEXT };
static const char *EXT_NAME[NEXT] = {"mode", "oct1", "oct2", "nois", "sprd", "levl", "mrph", "hold", "a4",
                                     "tune", "bnd", "mgf", "mgi", "hsnp", "ch1", "tone", "path", "norm", "pfad", "pw1", "pw2", "sync", "swp2", "fm", "fmr", "fms", "wsh1", "wsh2", "wmx1", "wmx2",
                                     "env1", "env2", "leak", "a1", "d1", "bp1", "sl1", "su1", "r1", "a2", "d2", "bp2", "sl2", "su2", "r2", "port", "pmod",
                                     "cut", "res", "kbd", "pol", "egi", "trg", "env3", "a3", "d3", "bp3", "sl3", "su3", "r3",
                                     "mgd", "mgv", "mgr", "treb", "bass", "dtim", "dfb", "dmf", "dmi", "dlev"};
static const float EXT_DEFAULT[NEXT] = {2, 0, 0, 0, 0.5f, 0.5f, 8, 0, 440, 0, 2, 5, 0, 1, 0, 1, 0, 0, 1, 64, 64, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 31, 0, 31, 0, 0, 0, 31, 0, 31, 0, 0, 0,
                                       99, 0, 0, 2, 0, 2, 0, 0, 0, 31, 0, 31, 0,
                                       0, 0, -1, 0, 0, 30, 0, 0, 0, 0};

static t_class *pdoly8_class;

typedef struct _pdoly8 {
    t_object x_obj;
    t_outlet *x_dump;
    p8_engine_t e;
    t_float  ctl[NCTL], ext[NEXT];
    t_float  bend;                 /* -1..1 */
    int      held[P8_VOICES]; t_float key[P8_VOICES]; unsigned age[P8_VOICES]; int keys;
    unsigned clock;
    t_float  last;                 /* last note played, -1 none */
    int      restoring;
    float   *buf; int bufn;
} t_pdoly8;

static float clampf_(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ------------------------------------------------------------------ DSP */
static t_int *pdoly8_perform(t_int *w)
{
    t_pdoly8 *x = (t_pdoly8 *)(w[1]);
    t_sample *out_l = (t_sample *)(w[2]), *out_r = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    float *ol = x->buf, *or_ = x->buf + n;
    p8_render(&x->e, ol, or_, (unsigned)n);
    for (int i = 0; i < n; i++) { out_l[i] = ol[i]; out_r[i] = or_[i]; }
    return w + 5;
}

static void pdoly8_dsp(t_pdoly8 *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    float sr = (float)sp[0]->s_sr;
    if (sr > 0.0f && sr != x->e.fs) p8_set_fs(&x->e, sr);
    if (2 * n > x->bufn) {
        x->buf = (float *)resizebytes(x->buf, (size_t)x->bufn * sizeof(float), (size_t)(2 * n) * sizeof(float));
        x->bufn = 2 * n;
    }
    dsp_add(pdoly8_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)n);
}

/* ------------------------------------------------------- applying values */
/* Group levels. Below full scale each is a plain level; when lev1 + lev2
 * goes above it, both are scaled so that the sum stays at full scale (as
 * cz4~ does), so DOUBLE cannot clip where one DCO alone does not. */
static void update_levels(t_pdoly8 *x)
{
    float a1 = (float)x->ctl[C_LEV1] / 127.0f, a2 = (float)x->ctl[C_LEV2] / 127.0f, sum = a1 + a2;
    if (x->e.mode != P8_DOUBLE) { a2 = 0.0f; sum = a1; }
    float k = sum > 1.0f ? 1.0f / sum : 1.0f;
    x->e.lev[0] = a1 * k; x->e.lev[1] = a2 * k;
}

static void apply_ctl(t_pdoly8 *x, int i, t_float v)
{
    if (i < 0 || i >= NCTL) return;
    v = clampf_((float)v, 0.0f, 127.0f);
    x->ctl[i] = v;
    p8_engine_t *e = &x->e;
    switch (i) {
    case C_WAV1: e->wav[0] = (float)v / 127.0f; break;
    case C_WAV2: e->wav[1] = (float)v / 127.0f; break;
    case C_HAR1: e->har[0] = (float)v; break;
    case C_HAR2: e->har[1] = (float)v; break;
    case C_INTV: e->interval = (int)lroundf((float)v * 12.0f / 127.0f); p8_retune(e); return;
    case C_DETN: {                     /* Poly-800 II service manual: steps 0-3 = 0, 6.5, 13, 26 cents (down) */
        static const float DET[4] = {0.0f, 6.5f, 13.0f, 26.0f};
        float q = clampf_((float)v, 0.0f, 127.0f) * 3.0f / 127.0f; int a = q >= 3.0f ? 2 : (int)q;
        e->detune_cents = DET[a] + (q - (float)a) * (DET[a + 1] - DET[a]); return;
    }
    case C_LEV1: case C_LEV2: update_levels(x); break;
    }
    p8_update(e);
}

static void apply_ext(t_pdoly8 *x, int i, t_float v)
{
    p8_engine_t *e = &x->e;
    switch (i) {
    case X_MODE:
        v = v < 1.5f ? 1 : 2;
        e->mode = (int)v; x->ext[i] = v; update_levels(x); p8_update(e); p8_retune(e); return;
    case X_OCT1: case X_OCT2:
        v = v < -1 ? -1 : v > 1 ? 1 : (t_float)(int)v;
        e->octave[i == X_OCT2] = (int)v; x->ext[i] = v; p8_retune(e); return;
    case X_NOIS: v = (t_float)(int)clampf_((float)v, 0.0f, 15.0f); e->noise_level = (float)v / 15.0f; break;
    case X_SPRD: v = clampf_((float)v, 0.0f, 1.0f); e->spread = (float)v; x->ext[i] = v; p8_update(e); return;
    case X_LEVL: v = clampf_((float)v, 0.0f, 2.0f); e->level = (float)v; break;
    case X_MRPH: v = v < 0 ? 0 : v; p8_set_smooth(e, (float)v); break;
    case X_HOLD:
        v = v != 0;
        if (v == 0) for (int k = 0; k < P8_VOICES; k++) if (!x->held[k]) p8_voice_off(e, k);
        break;
    case X_A4:   if (v < 300 || v > 600) return; e->a4 = (float)v; break;
    case X_TUNE: v = clampf_((float)v, -50.0f, 50.0f); e->tune_cents = (float)v; break;
    case X_BND:  v = clampf_((float)v, 0.0f, 12.0f); e->bend_semi = (float)(x->bend * v); break;
    case X_MGF:  v = clampf_((float)v, 0.05f, 30.0f); e->mg_hz = (float)v; break;
    case X_MGI:  v = clampf_((float)v, 0.0f, 200.0f);   /* cents; measured 12.4 per step of 83, 186 at 15 */ e->mg_cents = (float)v; break;
    case X_HSNP: v = clampf_((float)v, 0.0f, 4.0f); e->hsnap = (float)v; x->ext[i] = v; p8_update(e); return;
    case X_CH1:  v = (t_float)(int)clampf_((float)v, 0.0f, 16.0f); break;     /* 0 = any channel */
    case X_TONE: v = v != 0; e->tone = v != 0; break;                     /* measured output response */
    case X_PATH: v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); e->path = (int)v; x->ext[i] = v; p8_update(e); return;
    case X_NORM: v = clampf_((float)v, 0.0f, 1.0f); e->norm = (float)v; x->ext[i] = v; p8_update(e); return;
    case X_PFAD: v = v != 0; e->pfade = v != 0; x->ext[i] = v; p8_update(e); return;
    case X_PW1: case X_PW2: {                     /* 0..127: width 3 % .. 97 %, 64 = square */
        v = clampf_((float)v, 0.0f, 127.0f);
        float d = (float)v - 64.0f;
        e->pw_t[i == X_PW2] = 0.5f + 0.47f * (d >= 0.0f ? d / 63.0f : d / 64.0f);
        x->ext[i] = v; p8_update(e); return;
    }
    case X_SYNC: v = v != 0; e->sync = v != 0; break;                     /* DCO2 restarts with DCO1 */
    case X_SWP2: v = clampf_((float)v, 0.0f, 48.0f); e->swp2 = (float)v; break;   /* semitones */
    case X_FM:   v = clampf_((float)v, 0.0f, 127.0f);                    /* index 0..8, 1 at about 45 */
        e->fm = 8.0f * ((float)v / 127.0f) * ((float)v / 127.0f); break;
    case X_FMR:  v = clampf_((float)v, 0.125f, 16.0f); e->fm_ratio = (float)v; break;
    case X_FMS:  v = v != 0; e->fm_src = v != 0; break;
    case X_WSH1: case X_WSH2:                    /* Walsh loop position 0..127 */
        v = clampf_((float)v, 0.0f, 127.0f); e->wpos[i == X_WSH2] = (float)v; x->ext[i] = v; p8_update(e); return;
    case X_ENV1: case X_ENV2: case X_ENV3: {     /* DEG morph 0..127, -1 = own values (a1 ...) */
        int g = i == X_ENV3 ? 2 : i == X_ENV2;
        v = v < 0 ? -1 : clampf_((float)v, 0.0f, 127.0f);
        e->env[g] = (float)v; x->ext[i] = v; p8_update(e);
        for (int k = 0; k < 6; k++) x->ext[(g == 2 ? X_A3 : g ? X_A2 : X_A1) + k] = e->deg[g][k];
        return;
    }
    case X_LEAK: v = v != 0; e->leak = v != 0; break;          /* released voices at -55 dB */
    case X_PORT: v = clampf_((float)v, 0.0f, 127.0f);              /* 0 off .. 127 = 3 s */
        p8_set_port(e, 3.0f * ((float)v / 127.0f) * ((float)v / 127.0f)); break;
    case X_PMOD: v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); break;   /* 0 last note, 1 own, 2 legato */
    case X_CUT: v = clampf_((float)v, 0.0f, 99.0f); e->vcf_c = (float)v; break;      /* VCF cutoff 0..99 */
    case X_RES: v = clampf_((float)v, 0.0f, 15.0f); e->vcf_r = (float)v; break;      /* resonance 0..15 */
    case X_KBD: v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); e->kbd = (int)v; break;   /* off, half, full */
    case X_POL: case X_EGI: {                    /* polarity 1 / 2 and EG INT 0..15 -> signed depth */
        if (i == X_POL) v = v < 1.5f ? 1 : 2; else v = clampf_((float)v, 0.0f, 15.0f);
        x->ext[i] = v;
        e->egint = (float)x->ext[X_EGI] * (x->ext[X_POL] < 1.5f ? -1.0f : 1.0f);
        return;
    }
    case X_TRG: v = v < 1.5f ? 1 : 2; e->trig3 = (int)v; break;                     /* DEG3 single / multi */
    case X_A1: case X_D1: case X_BP1: case X_SL1: case X_SU1: case X_R1:
    case X_A2: case X_D2: case X_BP2: case X_SL2: case X_SU2: case X_R2:
    case X_A3: case X_D3: case X_BP3: case X_SL3: case X_SU3: case X_R3: {   /* own DEG values 0..31 */
        int g = i >= X_A3 ? 2 : i >= X_A2, k = i - (g == 2 ? X_A3 : g ? X_A2 : X_A1);
        v = clampf_((float)v, 0.0f, 31.0f);
        x->ext[i] = v;
        if (x->restoring && e->env[g] >= 0.0f) return;          /* a dump with a morph position */
        e->deg[g][k] = (float)v; e->env[g] = -1.0f; x->ext[g == 2 ? X_ENV3 : g ? X_ENV2 : X_ENV1] = -1;
        p8_update(e);
        return;
    }
    case X_MGD: {                                /* MG delay 0..15 (82), measured: 0, 0.47, 0.95, 1.55 s at 0/5/10/15 */
        static const float T[4] = {0.0f, 0.47f, 0.95f, 1.55f};
        v = clampf_((float)v, 0.0f, 15.0f); float q = (float)v / 5.0f; int a = q >= 3.0f ? 2 : (int)q;
        e->mg_delay = T[a] + (q - (float)a) * (T[a + 1] - T[a]); break;
    }
    case X_MGV: v = clampf_((float)v, 0.0f, 15.0f); e->mg_vcf = 7.9f * (float)v / 15.0f; break;   /* MG -> VCF (84): +-7.9 oct at 15, measured */
    case X_MGR: {                                /* MG frequency 0..15 (81), measured; sets mgf. -1 = leave mgf */
        static const float S[7] = {0, 3, 6, 8, 9, 12, 15}, HZ[7] = {0.291f, 1.262f, 3.39f, 5.22f, 6.493f, 11.09f, 19.08f};
        if (v < 0) { v = -1; break; }
        v = clampf_((float)v, 0.0f, 15.0f);
        int a = 0; while (a < 5 && (float)v > S[a + 1]) a++;
        float t = ((float)v - S[a]) / (S[a + 1] - S[a]);
        e->mg_hz = HZ[a] * powf(HZ[a + 1] / HZ[a], t); x->ext[X_MGF] = e->mg_hz; break;
    }
    case X_TREB: case X_BASS:                    /* EQ steps -6..6 (+-10 dB) */
        v = (t_float)(int)lroundf(clampf_((float)v, -6.0f, 6.0f)); x->ext[i] = v;
        p8_set_eq(e, (float)x->ext[X_TREB], (float)x->ext[X_BASS]); return;
    case X_DTIM: case X_DFB: case X_DMF: case X_DMI: case X_DLEV: {   /* digital delay, Poly-800 II units */
        static const float MX[5] = {99, 15, 31, 31, 15};
        v = clampf_((float)v, 0.0f, MX[i - X_DTIM]); x->ext[i] = v;
        p8_set_ddl(e, (float)x->ext[X_DTIM], (float)x->ext[X_DFB], (float)x->ext[X_DMF], (float)x->ext[X_DMI], (float)x->ext[X_DLEV]);
        return;
    }
    case X_WMX1: case X_WMX2:                    /* feet -> Walsh, 0..127 */
        v = clampf_((float)v, 0.0f, 127.0f); e->wmix[i == X_WMX2] = (float)v / 127.0f; x->ext[i] = v; p8_update(e); return;
    default: return;
    }
    x->ext[i] = v;
}

/* --------------------------------------------------- voice allocation */
/* As cz4~: a note-on with no key down starts a new phrase and clears the
 * old voices; otherwise a free (released) voice is taken, else the oldest
 * held one is stolen. With hold on, released voices keep sounding until the
 * next phrase, so an external VCA's release keeps the whole chord. The
 * chip's counters run freely, so a note-on does not reset any phase. */
static void note_on(t_pdoly8 *x, t_float pitch)
{
    int v, pick = -1;
    for (v = 0; v < P8_VOICES; v++) if (x->held[v] && x->key[v] == pitch) return;
    if (x->keys == 0) for (v = 0; v < P8_VOICES; v++) p8_voice_off(&x->e, v);
    for (v = 0; v < P8_VOICES; v++)
        if (!x->held[v] && (pick < 0 || x->age[v] < x->age[pick])) pick = v;
    if (pick < 0) {
        for (v = 0; v < P8_VOICES; v++) if (pick < 0 || x->age[v] < x->age[pick]) pick = v;
        x->keys--;
    }
    /* portamento source: the last note played (pmod 0), this voice's own
     * last note (1), or the last note only while another key is held (2) */
    int pm = (int)x->ext[X_PMOD];
    t_float from = pm == 1 ? (x->e.tg[pick].note > 0 ? x->e.tg[pick].note : -1) :
                   (pm == 2 && x->keys == 0) ? -1 : x->last;
    x->held[pick] = 1; x->key[pick] = pitch; x->age[pick] = ++x->clock; x->keys++;
    p8_voice_glide(&x->e, pick, (float)pitch, (float)from);
    x->last = pitch;
}

static void note_off(t_pdoly8 *x, t_float pitch)
{
    for (int v = 0; v < P8_VOICES; v++)
        if (x->held[v] && x->key[v] == pitch) {
            x->held[v] = 0; x->keys--; x->age[v] = ++x->clock;
            if (x->ext[X_HOLD] == 0) p8_voice_off(&x->e, v);
        }
}

static void play(t_pdoly8 *x, t_float pitch, t_float vel)
{
    if (vel > 0) note_on(x, pitch); else note_off(x, pitch);
}

static void pdoly8_note(t_pdoly8 *x, t_floatarg pitch, t_floatarg vel) { play(x, pitch, vel); }

static void pdoly8_list(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc < 2) return;
    t_float p = atom_getfloatarg(0, argc, argv), v = atom_getfloatarg(1, argc, argv);
    if (argc >= 3) {
        int ch = (int)atom_getfloatarg(2, argc, argv);
        if ((int)x->ext[X_CH1] != 0 && ch != (int)x->ext[X_CH1]) return;
    }
    play(x, p, v);
}

static void pdoly8_panic(t_pdoly8 *x)
{
    for (int v = 0; v < P8_VOICES; v++) { x->held[v] = 0; p8_voice_off(&x->e, v); }
    x->keys = 0;
}

/* pitch bend -1..1, scaled by bnd (semitones); acts on the master clock */
static void pdoly8_bend(t_pdoly8 *x, t_floatarg f)
{
    x->bend = clampf_((float)f, -1.0f, 1.0f);
    x->e.bend_semi = (float)(x->bend * x->ext[X_BND]);
}

static void pdoly8_voice(t_pdoly8 *x, t_floatarg v, t_floatarg pitch)
{
    int i = (int)v;
    if (i >= 0 && i < P8_VOICES) p8_voice_on(&x->e, i, (float)pitch);
}
static void pdoly8_voiceoff(t_pdoly8 *x, t_floatarg v) { p8_voice_off(&x->e, (int)v); }

/* ------------------------------------------------- named value messages */
static void pdoly8_anything(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc < 1) { pd_error(x, "pdoly8~: %s needs a value", s->s_name); return; }
    t_float v = atom_getfloatarg(0, argc, argv);
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) { apply_ctl(x, i, v); return; }
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) { apply_ext(x, i, v); return; }
    pd_error(x, "pdoly8~: unknown message '%s'", s->s_name);
}

static void pdoly8_ctl(t_pdoly8 *x, t_floatarg i, t_floatarg v) { apply_ctl(x, (int)i, v); }

static void pdoly8_params(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    for (int i = 0; i < argc && i < NCTL; i++) apply_ctl(x, i, atom_getfloatarg(i, argc, argv));
}

static void set_defaults(t_pdoly8 *x)
{
    x->restoring = 1;
    for (int i = 0; i < NEXT; i++) apply_ext(x, i, EXT_DEFAULT[i]);
    x->restoring = 0;
    for (int i = 0; i < NCTL; i++) apply_ctl(x, i, CTL_DEFAULT[i]);
}
static void pdoly8_defaults(t_pdoly8 *x) { set_defaults(x); }
static void pdoly8_reset(t_pdoly8 *x) { pdoly8_panic(x); set_defaults(x); pdoly8_bend(x, 0); p8_settle(&x->e); }

static void pdoly8_get(t_pdoly8 *x, t_symbol *name)
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
    pd_error(x, "pdoly8~: get: unknown name '%s'", name->s_name);
}

/* ------------------------------------------------------- presets */
static void pdoly8_dump(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc == 0) {
        t_atom a[1 + NCTL + NEXT];
        SETSYMBOL(a, gensym("pdoly8"));
        for (int i = 0; i < NCTL; i++) SETFLOAT(a + 1 + i, x->ctl[i]);
        for (int i = 0; i < NEXT; i++) SETFLOAT(a + 1 + NCTL + i, x->ext[i]);
        outlet_anything(x->x_dump, gensym("dump"), 1 + NCTL + NEXT, a);
        return;
    }
    if (argv[0].a_type != A_SYMBOL || argv[0].a_w.w_symbol != gensym("pdoly8")) return;
    /* settings first (mode decides the levels), then the controls */
    x->restoring = 1;
    for (int i = 1 + NCTL; i < argc && i <= NCTL + NEXT; i++) apply_ext(x, i - 1 - NCTL, atom_getfloatarg(i, argc, argv));
    x->restoring = 0;
    for (int i = 1; i < argc && i <= NCTL; i++) apply_ctl(x, i - 1, atom_getfloatarg(i, argc, argv));
}

static void pdoly8_print(t_pdoly8 *x)
{
    char line[1024]; int n = 0;
    post("pdoly8~: sample rate %g, %s, 4 voices", x->e.fs, x->e.mode == P8_DOUBLE ? "DOUBLE" : "WHOLE");
    for (int i = 0; i < NCTL; i++) n += snprintf(line + n, sizeof line - (size_t)n, "%s %g  ", CTL_NAME[i], x->ctl[i]);
    post("pdoly8~: %s", line);
    n = 0;
    for (int i = 0; i < NEXT && n < (int)sizeof line - 32; i++) n += snprintf(line + n, sizeof line - (size_t)n, "%s %g  ", EXT_NAME[i], x->ext[i]);
    post("pdoly8~: %s", line);
    for (int v = 0; v < P8_VOICES; v++)
        post("pdoly8~: voice %d note %g (code %d / %d) %s", v, x->e.tg[v].note, x->e.tg[v].code, x->e.tg[v + 4].code,
             x->held[v] ? "held" : p8_voice_sounding(&x->e, v) ? "released, sounding" : "off");
}

/* ------------------------------------------------------------ lifecycle */
static void *pdoly8_new(void)
{
    t_pdoly8 *x = (t_pdoly8 *)pd_new(pdoly8_class);
    float sr = (float)sys_getsr();
    p8_init(&x->e, sr > 0.0f ? sr : 44100.0f);
    x->last = -1;
    set_defaults(x);
    p8_settle(&x->e);
    x->bufn = 2 * 64;
    x->buf = (float *)getbytes((size_t)x->bufn * sizeof(float));
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_dump = outlet_new(&x->x_obj, &s_anything);
    return x;
}

static void pdoly8_free(t_pdoly8 *x) { freebytes(x->buf, (size_t)x->bufn * sizeof(float)); }

#define M1(name) class_addmethod(pdoly8_class, (t_method)pdoly8_##name, gensym(#name), A_FLOAT, 0)
#define M0(name) class_addmethod(pdoly8_class, (t_method)pdoly8_##name, gensym(#name), 0)

#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void pdoly8_tilde_setup(void)
{
    pdoly8_class = class_new(gensym("pdoly8~"), (t_newmethod)pdoly8_new, (t_method)pdoly8_free,
                             sizeof(t_pdoly8), CLASS_DEFAULT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(pdoly8_class, (t_method)pdoly8_list);
    class_addanything(pdoly8_class, (t_method)pdoly8_anything);
    class_addmethod(pdoly8_class, (t_method)pdoly8_note, gensym("note"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_ctl, gensym("ctl"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_voice, gensym("voice"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_dump, gensym("dump"), A_GIMME, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_params, gensym("params"), A_GIMME, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_get, gensym("get"), A_SYMBOL, 0);
    M1(voiceoff); M1(bend); M0(panic); M0(defaults); M0(reset); M0(print);
}
