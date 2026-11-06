/* SPDX-License-Identifier: BSD-3-Clause
 * pdoly8~ : 4-voice Korg Poly-800 tone generation for Pure Data (Mac,
 * Linux, Bela). Wraps pdoly8_core (MSM5232 dividers, foot buses, waveform
 * networks, band-limited).
 *
 * Inlet   1  messages: notes ("note p v", the list "p v", or "p v ch" from
 *            [notein]), controls, settings, presets, see pdoly8~-help.pd
 * Inlet   2  signal: external modulator for FM (fms 2), about -1..1
 * Outlets 1, 2  signal left, right
 *         3     events: "dump pdoly8 <values>" (for [skpreset]), "value <name> <v>"
 * Message conventions follow cz4~ and skandrums-pd (dump / params / defaults / get).
 *
 * DEG1-3, VCF, MG, EQ and digital delay as on the Poly-800 II.
 */
#include "m_pd.h"
#include "pdoly8_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* The eight controls (0..127), one per pot or MIDI CC: timbre and envelope
 * of each DCO, interval, detune, levels. env1 / env2 also take -1 (own DEG
 * values a1 ... r2). */
enum { C_HAR1, C_HAR2, C_ENV1, C_ENV2, C_INTV, C_DETN, C_LEV1, C_LEV2, NCTL };
static const char *CTL_NAME[NCTL] = {"har1", "har2", "env1", "env2", "intv", "detn", "lev1", "lev2"};
static const float CTL_DEFAULT[NCTL] = {45, 45, 0, 0, 0, 0, 127, 0};

/* Settings with raw values, in dump order after the controls. */
enum {
       X_OCT1, X_OCT2, X_NOIS, X_SPRD, X_MRPH, X_BND, X_MGF, X_MGI, X_CH1, X_TONE,
       X_PW1, X_PW2, X_SYNC, X_SWP2, X_FM, X_FMR, X_FMS, X_WSH1, X_WSH2, X_WMX1,
       X_WMX2, X_LEAK, X_A1, X_D1, X_BP1, X_SL1, X_SU1, X_R1, X_A2, X_D2,
       X_BP2, X_SL2, X_SU2, X_R2, X_PORT, X_PMOD, X_CUT, X_RES, X_KBD, X_POL,
       X_EGI, X_TRG, X_ENV3, X_A3, X_D3, X_BP3, X_SL3, X_SU3, X_R3, X_MGD,
       X_MGV, X_MGR, X_TREB, X_BASS, X_DTIM, X_DFB, X_DMF, X_DMI, X_DLEV, X_MONO,
       X_PWM1, X_PWM2, X_SWE, X_SWM, X_RING, X_FM2, X_FMHZ, X_WSM, X_WSE, X_DRV, X_VAMP, X_VCUT, X_DRVP, X_HMG, X_RMF, X_RETR, X_DRFT,
       X_A4, X_D4, X_BP4, X_SL4, X_SU4, X_R4, X_ENV4, X_TRG4, X_D4T1, X_D4A1, X_D4T2, X_D4A2, X_L2R, X_L2W, X_L2K, X_L2F, X_L2T1, X_L2A1, X_L2T2, X_L2A2, X_L3R, X_L3W, X_L3K, X_L3F, X_L3T1, X_L3A1, X_L3T2, X_L3A2, X_FMIG, NEXT };
static const char *EXT_NAME[NEXT] = {
    "oct1", "oct2", "nois", "sprd", "mrph", "bnd", "mgf", "mgi", "ch1", "tone", "pw1", "pw2",
    "sync", "swp2", "fm1", "fmr", "fms", "wsh1", "wsh2", "wmx1", "wmx2", "leak", "a1", "d1",
    "bp1", "sl1", "su1", "r1", "a2", "d2", "bp2", "sl2", "su2", "r2", "port", "pmod",
    "cut", "res", "kbd", "pol", "egi", "trg", "env3", "a3", "d3", "bp3", "sl3", "su3",
    "r3", "mgd", "mgv", "mgr", "treb", "bass", "dtim", "dfb", "dmf", "dmi", "dlev", "mono",
    "pwm1", "pwm2", "swe", "swm", "ring", "fm2", "fmhz", "wsm", "wse", "drv", "vamp", "vcut", "drvp", "hmg", "rmf", "retr", "drft",
    "a4", "d4", "bp4", "sl4", "su4", "r4", "env4", "trg4", "d4t1", "d4a1", "d4t2", "d4a2", "l2r", "l2w", "l2k", "l2f", "l2t1", "l2a1", "l2t2", "l2a2", "l3r", "l3w", "l3k", "l3f", "l3t1", "l3a1", "l3t2", "l3a2", "fmig"};
static const float EXT_DEFAULT[NEXT] = {
    0, 0, 0, 0, 8, 2, 5, 0, 0, 1, 64, 64, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 31, 0, 0, 0,
    31, 0, 31, 0, 0, 0, 99, 0, 0, 2, 0, 2, 0, 0, 0,
    31, 0, 31, 0, 0, 0, -1, 0, 0, 30, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, 31, 0, 31, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 64};

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
    t_float  stack[16]; int nstack; /* mono: keys held, last on top */
    int      restoring;
    float   *buf; int bufn;
} t_pdoly8;

/* modulation targets of DEG4, LFO2, LFO3 (the order of P8_T_* in pdoly8_core.h) */
static const char *TGT_NAME[P8_NT] = {
    "off", "pitch", "pitch2", "detn", "har1", "har2", "wsh1", "wsh2", "wmx1", "wmx2", "pw1", "pw2",
    "fm1", "fm2", "fmr", "ring", "swe", "lev1", "lev2", "amp", "pan", "nois", "cut", "res", "drv",
    "mgr", "mgi", "l2r", "l2d", "l3r", "l3d", "dtim", "dlev"};

static float clampf_(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* ------------------------------------------------------------------ DSP */
static t_int *pdoly8_perform(t_int *w)
{
    t_pdoly8 *x = (t_pdoly8 *)(w[1]);
    t_sample *in = (t_sample *)(w[2]), *out_l = (t_sample *)(w[3]), *out_r = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    float *ol = x->buf, *or_ = x->buf + n, *fi = x->buf + 2 * n;
    for (int i = 0; i < n; i++) fi[i] = in[i];          /* the input may share memory with an output */
    x->e.fm_in = fi;
    p8_render(&x->e, ol, or_, (unsigned)n);
    x->e.fm_in = 0;
    for (int i = 0; i < n; i++) { out_l[i] = ol[i]; out_r[i] = or_[i]; }
    return w + 6;
}

static void pdoly8_dsp(t_pdoly8 *x, t_signal **sp)
{
    int n = sp[0]->s_n;
    float sr = (float)sp[0]->s_sr;
    if (sr > 0.0f && sr != x->e.fs) p8_set_fs(&x->e, sr);
    if (3 * n > x->bufn) {
        x->buf = (float *)resizebytes(x->buf, (size_t)x->bufn * sizeof(float), (size_t)(3 * n) * sizeof(float));
        x->bufn = 3 * n;
    }
    dsp_add(pdoly8_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, (t_int)n);
}

/* ------------------------------------------------------- applying values */
/* Group levels. Below full scale each is a plain level; when lev1 + lev2
 * goes above it, both are scaled so that the sum stays at full scale (as
 * cz4~ does), so DOUBLE cannot clip where one DCO alone does not. */
static void update_levels(t_pdoly8 *x)
{
    float a1 = (float)x->ctl[C_LEV1] / 127.0f, a2 = (float)x->ctl[C_LEV2] / 127.0f, sum = a1 + a2;
    float k = sum > 1.0f ? 1.0f / sum : 1.0f;
    x->e.lev[0] = a1 * k; x->e.lev[1] = a2 * k;
}

static void apply_ctl(t_pdoly8 *x, int i, t_float v)
{
    if (i < 0 || i >= NCTL) return;
    p8_engine_t *e = &x->e;
    if (i == C_ENV1 || i == C_ENV2) {           /* DEG morph 0..127, -1 = own values (a1 ... r2) */
        int g = i == C_ENV2;
        v = v < 0 ? -1 : clampf_((float)v, 0.0f, 127.0f);
        x->ctl[i] = v; e->env[g] = (float)v;
        if (v < 0) for (int k = 0; k < 6; k++) e->deg[g][k] = (float)x->ext[(g ? X_A2 : X_A1) + k];
        p8_update(e);
        if (v >= 0) for (int k = 0; k < 6; k++) x->ext[(g ? X_A2 : X_A1) + k] = e->deg[g][k];
        return;
    }
    v = i == C_INTV ? (t_float)(int)lroundf(clampf_((float)v, 0.0f, 12.0f)) : clampf_((float)v, 0.0f, 127.0f);
    x->ctl[i] = v;
    switch (i) {
    case C_HAR1: e->har[0] = (float)v; break;
    case C_HAR2: e->har[1] = (float)v; break;
    case C_INTV: e->interval = (int)v; p8_retune(e); return;          /* semitones 0..12, as stored on the Poly-800 II */
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
    case X_OCT1: case X_OCT2:
        v = v < -1 ? -1 : v > 4 ? 4 : (t_float)(int)v;          /* -1..1 chip, 2..4 feet shifted up */
        e->octave[i == X_OCT2] = (int)v; x->ext[i] = v; p8_retune(e); return;
    case X_NOIS: v = (t_float)(int)clampf_((float)v, 0.0f, 15.0f); e->noise_level = (float)v / 15.0f; break;
    case X_SPRD: v = clampf_((float)v, 0.0f, 127.0f); e->spread = (float)v / 127.0f; x->ext[i] = v; p8_update(e); return;   /* stereo spread of the 4 voices */
    case X_MRPH: v = v < 0 ? 0 : v; p8_set_smooth(e, (float)v); break;
    case X_BND:  v = clampf_((float)v, 0.0f, 12.0f); e->bend_semi = (float)(x->bend * v); break;
    case X_MGF:  v = clampf_((float)v, 0.05f, 30.0f); e->mg_hz = (float)v; break;
    case X_MGI:  v = clampf_((float)v, 0.0f, 200.0f);   /* cents; measured 12.4 per step of 83, 186 at 15 */ e->mg_cents = (float)v; break;
    case X_CH1:  v = (t_float)(int)clampf_((float)v, 0.0f, 16.0f); break;     /* 0 = any channel */
    case X_TONE: v = v != 0; e->tone = v != 0; break;                     /* measured output response */
    case X_PW1: case X_PW2: {                     /* 0..127: width 3 % .. 97 %, 64 = square */
        v = clampf_((float)v, 0.0f, 127.0f);
        float d = (float)v - 64.0f;
        e->pw_t[i == X_PW2] = 0.5f + 0.47f * (d >= 0.0f ? d / 63.0f : d / 64.0f);
        x->ext[i] = v; p8_update(e); return;
    }
    case X_SYNC: v = v != 0; e->sync = v != 0; break;                     /* DCO2 restarts with DCO1 */
    case X_SWP2: v = clampf_((float)v, 0.0f, 72.0f); e->swp2 = (float)v; break;   /* semitones */
    case X_SWE:  v = clampf_((float)v, 0.0f, 72.0f); e->sw_e = (float)v; break;    /* DEG3 -> DCO2, semitones */
    case X_SWM:  v = clampf_((float)v, 0.0f, 24.0f); e->sw_m = (float)v; break;    /* MG -> DCO2, +-semitones */
    case X_PWM1: case X_PWM2:                    /* MG -> pulse width, 127 = +-47 % */
        v = clampf_((float)v, 0.0f, 127.0f); e->pwm_d[i == X_PWM2] = 0.47f * (float)v / 127.0f; break;
    case X_RING: v = clampf_((float)v, 0.0f, 127.0f); e->rmod = (float)v / 127.0f; break;
    case X_FM: case X_FM2: {                     /* index 0..16, 1 at about 32 */
        v = clampf_((float)v, 0.0f, 127.0f);
        float q = (float)v / 127.0f; if (i == X_FM) e->fm = 16.0f * q * q; else e->fm2 = 16.0f * q * q; break;
    }
    case X_FMR:  v = clampf_((float)v, 0.125f, 16.0f); e->fm_ratio = (float)v; break;
    case X_FMHZ: v = clampf_((float)v, -1000.0f, 1000.0f); e->fm_hz = (float)v; break;
    case X_FMIG: v = clampf_((float)v, 0.0f, 127.0f); e->fm_ig = (float)v / 64.0f; break;   /* input gain, 64 = 1 */
    case X_FMS:  v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); e->fm_src = (int)v; break;   /* sine, other DCO, input */
    case X_WSM:  v = clampf_((float)v, 0.0f, 127.0f); e->ws_m = (float)v; break;   /* MG -> Walsh position */
    case X_WSE:  v = clampf_((float)v, -127.0f, 127.0f); e->ws_e = (float)v; break;   /* DEG3 -> Walsh position */
    case X_VAMP: v = clampf_((float)v, 0.0f, 127.0f); e->vel_amp = (float)v / 127.0f; break;      /* 127: level = velocity */
    case X_VCUT: v = clampf_((float)v, 0.0f, 127.0f); e->vel_cut = 5.0f * (float)v / 127.0f; break; /* 127: soft key 5 oct lower */
    case X_ENV4: {                               /* DEG4 morph 0..127, -1 = own values (a4 ...) */
        v = v < 0 ? -1 : clampf_((float)v, 0.0f, 127.0f);
        e->env[3] = (float)v; x->ext[i] = v; p8_update(e);
        for (int k = 0; k < 6; k++) x->ext[X_A4 + k] = e->deg[3][k];
        return;
    }
    case X_A4: case X_D4: case X_BP4: case X_SL4: case X_SU4: case X_R4: {
        v = clampf_((float)v, 0.0f, 31.0f); x->ext[i] = v;
        if (x->restoring && e->env[3] >= 0.0f) return;
        e->deg[3][i - X_A4] = (float)v; e->env[3] = -1.0f; x->ext[X_ENV4] = -1; p8_update(e);
        return;
    }
    case X_TRG4: v = v < 1.5f ? 1 : 2; e->trig4 = (int)v; break;                   /* DEG4 single / multi */
    case X_D4T1: case X_D4T2: case X_L2T1: case X_L2T2: case X_L3T1: case X_L3T2: {   /* targets 0..32 */
        v = (t_float)(int)clampf_((float)v, 0.0f, (float)(P8_NT - 1)); x->ext[i] = v;
        int *t = i == X_D4T1 ? &e->d4_tgt[0] : i == X_D4T2 ? &e->d4_tgt[1] : i == X_L2T1 ? &e->lfo[0].tgt[0]
               : i == X_L2T2 ? &e->lfo[0].tgt[1] : i == X_L3T1 ? &e->lfo[1].tgt[0] : &e->lfo[1].tgt[1];
        *t = (int)v; p8_update(e); return;
    }
    case X_D4A1: case X_D4A2: case X_L2A1: case X_L2A2: case X_L3A1: case X_L3A2: {   /* amounts -127..127 */
        v = clampf_((float)v, -127.0f, 127.0f); x->ext[i] = v;
        float *a = i == X_D4A1 ? &e->d4_amt[0] : i == X_D4A2 ? &e->d4_amt[1] : i == X_L2A1 ? &e->lfo[0].amt[0]
                 : i == X_L2A2 ? &e->lfo[0].amt[1] : i == X_L3A1 ? &e->lfo[1].amt[0] : &e->lfo[1].amt[1];
        *a = (float)v / 127.0f; p8_update(e); return;
    }
    case X_L2R: case X_L3R: v = clampf_((float)v, 0.01f, 50.0f); e->lfo[i == X_L3R].hz = (float)v; break;   /* Hz */
    case X_L2W: case X_L3W: v = (t_float)(int)clampf_((float)v, 0.0f, 5.0f); e->lfo[i == X_L3W].wave = (int)v; break;
    case X_L2K: case X_L3K: v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); e->lfo[i == X_L3K].retrig = (int)v; break;
    case X_L2F: case X_L3F: v = clampf_((float)v, 0.0f, 10.0f); e->lfo[i == X_L3F].fade = (float)v; break;   /* s */
    case X_DRFT: v = clampf_((float)v, 0.0f, 127.0f); e->drift = (float)v / 127.0f; break;   /* analogue drift */
    case X_HMG: v = clampf_((float)v, 0.0f, 127.0f); e->h_m = (float)v; break;   /* MG -> har1 / har2, +- steps */
    case X_RMF: v = (t_float)(int)clampf_((float)v, 0.0f, 14.0f); e->rm_mask = P8_WALSH[(int)v]; break;   /* ring modulator on the Walsh loop */
    case X_RETR: v = v != 0; break;              /* mono: 1 every key restarts the envelopes, 0 legato */
    case X_DRVP: v = v != 0; e->drv_pre = v != 0; break;                  /* overdrive 0 post, 1 pre VCF */
    case X_DRV:  v = clampf_((float)v, 0.0f, 127.0f); p8_set_drive(e, (float)v / 127.0f); break;
    case X_WSH1: case X_WSH2:                    /* Walsh loop position 0..127 */
        v = clampf_((float)v, 0.0f, 127.0f); e->wpos[i == X_WSH2] = (float)v; x->ext[i] = v; p8_update(e); return;
    case X_ENV3: {                               /* DEG3 morph 0..127, -1 = own values (a3 ...) */
        v = v < 0 ? -1 : clampf_((float)v, 0.0f, 127.0f);
        e->env[2] = (float)v; x->ext[i] = v; p8_update(e);
        for (int k = 0; k < 6; k++) x->ext[X_A3 + k] = e->deg[2][k];
        return;
    }
    case X_LEAK: v = v != 0; e->leak = v != 0; break;          /* released voices at -55 dB */
    case X_PORT: v = clampf_((float)v, 0.0f, 127.0f);              /* 0 off .. 127 = 3 s */
        p8_set_port(e, 3.0f * ((float)v / 127.0f) * ((float)v / 127.0f)); break;
    case X_PMOD: v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); break;   /* 0 last note, 1 own, 2 legato */
    case X_MONO:                                 /* 0 poly, 1 mono (2: mono with retr 0, older presets) */
        v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f);
        if (v == 2) { v = 1; x->ext[X_RETR] = 0; }
        if ((int)v != (int)x->ext[X_MONO]) { for (int k = 0; k < P8_VOICES; k++) { p8_voice_off(e, k); x->held[k] = 0; } x->keys = 0; x->nstack = 0; }
        break;
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
        e->deg[g][k] = (float)v; e->env[g] = -1.0f;
        if (g == 2) x->ext[X_ENV3] = -1; else x->ctl[g ? C_ENV2 : C_ENV1] = -1;
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
            p8_voice_off(&x->e, v);
        }
}

/* mono: one voice, last-note priority. retr 1
 * retriggers the envelopes on every key; legato only on the first key of a
 * phrase - further keys (and returning to a held one) only change the pitch,
 * with portamento if it is on. */
static void mono_on(t_pdoly8 *x, t_float pitch)
{
    int i, n = x->nstack;
    for (i = 0; i < n; i++) if (x->stack[i] == pitch) break;
    if (i < n) { for (; i < n - 1; i++) x->stack[i] = x->stack[i + 1]; n--; }
    if (n == 16) { for (i = 0; i < 15; i++) x->stack[i] = x->stack[i + 1]; n--; }
    int legato = x->ext[X_RETR] == 0 && n > 0;
    x->stack[n++] = pitch; x->nstack = n;
    int pm = (int)x->ext[X_PMOD];
    t_float from = (pm == 2 && !legato && n == 1) ? -1 : x->last;
    if (legato) p8_voice_pitch(&x->e, 0, (float)pitch, (float)from);
    else p8_voice_glide(&x->e, 0, (float)pitch, (float)from);
    x->held[0] = 1; x->key[0] = pitch; x->keys = 1;
    x->last = pitch;
}

static void mono_off(t_pdoly8 *x, t_float pitch)
{
    int i, n = x->nstack;
    for (i = 0; i < n; i++) if (x->stack[i] == pitch) break;
    if (i == n) return;
    int top = i == n - 1;
    for (; i < n - 1; i++) x->stack[i] = x->stack[i + 1];
    x->nstack = --n;
    if (n == 0) { p8_voice_off(&x->e, 0); x->held[0] = 0; x->keys = 0; return; }
    if (!top) return;
    t_float back = x->stack[n - 1];                  /* back to the key still held */
    if (x->ext[X_RETR] == 0) p8_voice_pitch(&x->e, 0, (float)back, (float)x->last);
    else p8_voice_glide(&x->e, 0, (float)back, (float)x->last);
    x->key[0] = back; x->last = back;
}

static void play(t_pdoly8 *x, t_float pitch, t_float vel)
{
    if (vel > 0) {                               /* velocity: for the voice about to play */
        int pick = 0;
        if (x->ext[X_MONO] <= 0) {               /* same choice as note_on */
            int v, found = -1;
            for (v = 0; v < P8_VOICES; v++) if (x->held[v] && x->key[v] == pitch) found = v;
            if (found >= 0) pick = found;
            else {
                pick = -1;
                for (v = 0; v < P8_VOICES; v++) if (!x->held[v] && (pick < 0 || x->age[v] < x->age[pick])) pick = v;
                if (pick < 0) for (v = 0; v < P8_VOICES; v++) if (pick < 0 || x->age[v] < x->age[pick]) pick = v;
            }
        }
        p8_voice_vel(&x->e, pick, (float)vel / 127.0f);
    }
    if (x->ext[X_MONO] > 0) { if (vel > 0) mono_on(x, pitch); else mono_off(x, pitch); return; }
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
    x->keys = 0; x->nstack = 0;
}

/* pitch bend -1..1, scaled by bnd (semitones); acts on the master clock */
/* delay presets: delay flanger | chorus1 | chorus2 | slap | long | off
 * (time, feedback, mod rate, mod depth, level in Poly-800 II steps) */
static void pdoly8_delay(t_pdoly8 *x, t_symbol *s)
{
    static const struct { const char *n; float v[5]; } P[] = {
        {"flanger", { 0, 11,  8, 31, 15}},   /* 4 ms, slow sweep 1:5, feedback */
        {"chorus1", {20,  0, 24, 21, 12}},   /* 13 ms, fast 1.8 Hz, shallow */
        {"chorus2", {24,  0, 14, 26, 12}},   /* 16 ms, slow 0.4 Hz, wider */
        {"slap",    {60,  0,  0,  0, 10}},   /* 120 ms, one echo */
        {"long",    {85,  9,  0,  0,  9}},   /* 0.5 s, repeats */
        {"off",     {30,  0,  0,  0,  0}},
    };
    for (unsigned k = 0; k < sizeof P / sizeof P[0]; k++)
        if (!strcmp(s->s_name, P[k].n)) {
            for (int j = 0; j < 5; j++) apply_ext(x, X_DTIM + j, P[k].v[j]);
            return;
        }
    pd_error(x, "pdoly8~: delay: unknown preset %s (flanger chorus1 chorus2 slap long off)", s->s_name);
}

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
    if (argv[0].a_type == A_SYMBOL) {            /* a target by name: d4t1 cut, l2t2 pitch ... */
        int t = -1;
        for (int k = 0; k < P8_NT; k++) if (argv[0].a_w.w_symbol == gensym(TGT_NAME[k])) t = k;
        if (t < 0) { pd_error(x, "pdoly8~: %s: unknown target %s", s->s_name, argv[0].a_w.w_symbol->s_name); return; }
        v = t;
    }
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) { apply_ctl(x, i, v); return; }
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) { apply_ext(x, i, v); return; }
    pd_error(x, "pdoly8~: unknown message '%s'", s->s_name);
}

static void pdoly8_ctl(t_pdoly8 *x, t_floatarg i, t_floatarg v) { apply_ctl(x, (int)i, v); }

/* 0-127 (a pot, CV or MIDI CC) to a parameter's range and back:
 * 'i' whole numbers lo..hi in equal zones, 'r' whole numbers linear (wider ranges),
 * 'b' / 'c' bipolar -hi..hi with 0 at 64 (whole numbers / continuous), 'f' linear, 'l' logarithmic */
typedef struct { const char *name; char kind; float lo, hi; } t_p8range;
static const t_p8range RANGE[] = {
    {"har1",'i',0,127}, {"har2",'i',0,127}, {"env1",'i',0,127}, {"env2",'i',0,127}, {"intv",'i',0,12},
    {"detn",'i',0,127}, {"lev1",'i',0,127}, {"lev2",'i',0,127},
    {"cut",'i',0,99}, {"res",'i',0,15}, {"egi",'i',0,15}, {"kbd",'i',0,2}, {"env3",'i',0,127}, {"vcut",'i',0,127},
    {"drv",'i',0,127}, {"nois",'i',0,15},
    {"oct1",'i',-1,4}, {"oct2",'i',-1,4}, {"pw1",'i',0,127}, {"pw2",'i',0,127}, {"pwm1",'i',0,127}, {"pwm2",'i',0,127},
    {"hmg",'i',0,127}, {"drft",'i',0,127},
    {"wsh1",'i',0,127}, {"wsh2",'i',0,127}, {"wmx1",'i',0,127}, {"wmx2",'i',0,127}, {"wsm",'i',0,127}, {"wse",'b',-127,127},
    {"sync",'i',0,1}, {"swp2",'i',0,72}, {"swe",'i',0,72}, {"swm",'i',0,24}, {"ring",'i',0,127}, {"rmf",'i',0,14},
    {"fm1",'i',0,127}, {"fm2",'i',0,127}, {"fmr",'l',0.125f,16}, {"fmhz",'c',-1000,1000}, {"fms",'i',0,2}, {"fmig",'i',0,127},
    {"mgr",'i',0,15}, {"mgi",'r',0,200}, {"mgd",'i',0,15}, {"mgv",'i',0,15}, {"env4",'i',0,127}, {"trg4",'i',1,2},
    {"d4t1",'i',0,P8_NT - 1}, {"d4a1",'b',-127,127}, {"d4t2",'i',0,P8_NT - 1}, {"d4a2",'b',-127,127},
    {"l2r",'l',0.01f,50}, {"l2w",'i',0,5}, {"l2k",'i',0,2}, {"l2f",'f',0,10},
    {"l2t1",'i',0,P8_NT - 1}, {"l2a1",'b',-127,127}, {"l2t2",'i',0,P8_NT - 1}, {"l2a2",'b',-127,127},
    {"l3r",'l',0.01f,50}, {"l3w",'i',0,5}, {"l3k",'i',0,2}, {"l3f",'f',0,10},
    {"l3t1",'i',0,P8_NT - 1}, {"l3a1",'b',-127,127}, {"l3t2",'i',0,P8_NT - 1}, {"l3a2",'b',-127,127},
    {"dtim",'i',0,99}, {"dfb",'i',0,15}, {"dmf",'i',0,31}, {"dmi",'i',0,31}, {"dlev",'i',0,15},
    {"port",'i',0,127}, {"mono",'i',0,1}, {"vamp",'i',0,127},
    {"pmod",'i',0,2}, {"retr",'i',0,1}, {"pol",'i',1,2}, {"trg",'i',1,2}, {"drvp",'i',0,1}, {"mgf",'l',0.1f,30},
    {"bnd",'i',0,12}, {"mrph",'i',0,100}, {"tone",'i',0,1}, {"leak",'i',0,1},
    {"treb",'i',-6,6}, {"bass",'i',-6,6}, {"sprd",'i',0,127}};
#define NRANGE ((int)(sizeof RANGE / sizeof RANGE[0]))

static const t_p8range *find_range(t_symbol *s)
{
    for (int i = 0; i < NRANGE; i++) if (s == gensym(RANGE[i].name)) return &RANGE[i];
    return 0;
}

static t_float *find_value(t_pdoly8 *x, t_symbol *s)
{
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) return &x->ctl[i];
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) return &x->ext[i];
    return 0;
}

static float range_from127(const t_p8range *r, float c)
{
    c = clampf_(c, 0.0f, 127.0f);
    if (r->kind == 'l') return r->lo * powf(r->hi / r->lo, c / 127.0f);
    if (r->kind == 'f') return r->lo + (r->hi - r->lo) * c / 127.0f;
    if (r->kind == 'r') return floorf(r->lo + (r->hi - r->lo) * c / 127.0f + 0.5f);
    if (r->kind == 'b' || r->kind == 'c') {
        float v = clampf_((c - 64.0f) * r->hi / 63.0f, -r->hi, r->hi);
        return r->kind == 'b' ? floorf(v + 0.5f) : v;
    }
    if (r->hi - r->lo == 127.0f) return r->lo + floorf(c + 0.5f);
    float n = r->hi - r->lo + 1.0f;                      /* zones of equal width */
    return r->lo + fminf(floorf(c * n / 128.0f), n - 1.0f);
}

static float range_to127(const t_p8range *r, float v)
{
    float c;
    if (r->kind == 'l') c = 127.0f * logf(fmaxf(v, r->lo) / r->lo) / logf(r->hi / r->lo);
    else if (r->kind == 'f' || r->kind == 'r') c = 127.0f * (v - r->lo) / (r->hi - r->lo);
    else if (r->kind == 'b' || r->kind == 'c') c = 64.0f + v * 63.0f / r->hi;
    else if (r->hi - r->lo == 127.0f) c = v - r->lo;
    else { float n = r->hi - r->lo + 1.0f; c = (v - r->lo + 0.5f) * 128.0f / n; }   /* middle of the zone */
    return floorf(clampf_(c, 0.0f, 127.0f) + 0.5f);
}

/* n <name> <0-127>: set a parameter from a pot, CV or CC; "none" is ignored */
static void pdoly8_n(t_pdoly8 *x, t_symbol *name, t_floatarg c)
{
    if (name == gensym("none")) return;
    const t_p8range *r = find_range(name);
    if (!r) { pd_error(x, "pdoly8~: n: no 0-127 range for '%s'", name->s_name); return; }
    t_atom a; SETFLOAT(&a, range_from127(r, c));
    t_float *old = find_value(x, name), before = old ? *old : 0;
    pdoly8_anything(x, name, 1, &a);
    const char *nm = name->s_name;                       /* targets: name them in the console */
    if (old && *old != before && strlen(nm) == 4 && nm[2] == 't' && (nm[0] == 'd' || nm[0] == 'l'))
        post("pdoly8~: %s %s", nm, TGT_NAME[(int)*old]);
}

/* nget <name> <tag>: outlet 3 gets "norm <tag> <0-127>", the parameter's value as a pot position */
static void pdoly8_nget(t_pdoly8 *x, t_symbol *name, t_floatarg tag)
{
    const t_p8range *r = find_range(name);
    t_float *v = find_value(x, name);
    if (!r || !v) return;
    t_atom a[2]; SETFLOAT(a, tag); SETFLOAT(a + 1, range_to127(r, *v));
    outlet_anything(x->x_dump, gensym("norm"), 2, a);
}

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
    post("pdoly8~: sample rate %g, 4 voices of 2 DCOs", x->e.fs);
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
    x->e.mode = P8_DOUBLE;                       /* always 4 voices of DCO1 + DCO2; lev1 / lev2 set the mix */
    x->e.path = 3;                               /* one-knob timbre */
    x->e.pfade = 1; x->e.norm = 0.0f;
    x->e.hsnap = 1.0f;                           /* exact original combinations +-1 step around each */            /* fading foot linear in power; original level changes */
    x->e.level = 0.5f;                           /* fixed output level (headroom for 4 voices, both DCOs) */
    x->last = -1;
    set_defaults(x);
    p8_settle(&x->e);
    x->bufn = 3 * 64;
    x->buf = (float *)getbytes((size_t)x->bufn * sizeof(float));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);   /* inlet 2: external FM signal (fms 2) */
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
    class_addmethod(pdoly8_class, (t_method)pdoly8_n, gensym("n"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_nget, gensym("nget"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_delay, gensym("delay"), A_SYMBOL, 0);
    M1(voiceoff); M1(bend); M0(panic); M0(defaults); M0(reset); M0(print);
}
