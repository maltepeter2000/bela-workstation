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
#if !defined(_WIN32)
#define _DEFAULT_SOURCE              /* sockets, nanosleep with -std=c99 */
#define _DARWIN_C_SOURCE
#endif
#include "m_pd.h"
#include "pdoly8_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#if !defined(_WIN32)
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define P8_OLED 1
#endif

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
       X_FMIG, X_DGLD, NEXT };
static const char *EXT_NAME[NEXT] = {
    "oct1", "oct2", "nois", "sprd", "mrph", "bnd", "mgf", "mgi", "ch1", "tone", "pw1", "pw2",
    "sync", "swp2", "fm1", "fmr", "fms", "wsh1", "wsh2", "wmx1", "wmx2", "leak", "a1", "d1", "bp1",
    "sl1", "su1", "r1", "a2", "d2", "bp2", "sl2", "su2", "r2", "port", "pmod", "cut", "res", "kbd",
    "pol", "egi", "trg", "env3", "a3", "d3", "bp3", "sl3", "su3", "r3", "mgd", "mgv", "mgr", "treb",
    "bass", "dtim", "dfb", "dmf", "dmi", "dlev", "mono", "pwm1", "pwm2", "swe", "swm", "ring",
    "fm2", "fmhz", "wsm", "wse", "drv", "vamp", "vcut", "drvp", "hmg", "rmf", "retr", "drft",
    "fmig", "dgld"};
static const float EXT_DEFAULT[NEXT] = {
    0, 0, 0, 0, 25, 2, 5, 0, 0, 1, 64, 64, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 31, 0, 31, 0, 0, 0,
    31, 0, 31, 0, 0, 0, 99, 0, 0, 2, 0, 2, 0, 0, 0, 31, 0, 31, 0, 0, 0, -1, 0, 0, 30, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 64, 1};

/* performance controls (see apply_perf) */
static const char *PERF_NAME[3] = {"atk", "rel", "vol"};
static const float PERF_DEFAULT[3] = {0, 0, 100};

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
    int      inited;               /* ch1 is set up once, then kept by defaults and presets */
    unsigned rnd;                  /* random voices */
    float   *buf; int bufn;
    t_clock *rep_clk;              /* reports changed settings as "info" on outlet 3 */
    unsigned char syx_ram[0xA80];  /* Poly-800 II memory: last dump received or read (template) */
    int      syx_has;              /* syx_ram holds a real dump */
    int      syx_ch;               /* MIDI channel 1-16 of the dump header (2n): from the dump, or syxch */
    unsigned char syx_in[0x1600]; int syx_n;   /* incoming sysex bytes ([sysexin] -> syxin) */
    t_canvas *canvas;
    t_float *rep_v[160]; float rep_last[160];
    int      txch;                 /* send channel for the controller's feedback, 0 = the receive channel */
    t_float  perf[3];              /* atk rel (-63..63, 0 = the sound as it is) vol (0..127): not in a sound */
    float    vol_gain;
#ifdef P8_OLED
    /* OLED via O2O: the audio thread only copies text (seqlock), a thread of
     * its own sends the OSC packet, so no network call runs in the audio
     * thread (on Bela that would cost a mode switch and clicks) */
    char     o_txt[2][2][48];      /* two buffers of two lines */
    volatile unsigned o_seq;       /* odd while writing */
    int      o_on, o_quit, o_sock;
    struct sockaddr_in o_to;
    pthread_t o_th;
#endif
} t_pdoly8;

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
    float g = x->vol_gain;
    for (int i = 0; i < n; i++) { out_l[i] = ol[i] * g; out_r[i] = or_[i] * g; }
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

static void rep_kick(t_pdoly8 *x);

static void apply_ctl_(t_pdoly8 *x, int i, t_float v);
static void apply_ctl(t_pdoly8 *x, int i, t_float v) { apply_ctl_(x, i, v); rep_kick(x); }
static void apply_ctl_(t_pdoly8 *x, int i, t_float v)
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

static void apply_ext_(t_pdoly8 *x, int i, t_float v);
static void apply_ext(t_pdoly8 *x, int i, t_float v) { apply_ext_(x, i, v); rep_kick(x); }
static void apply_ext_(t_pdoly8 *x, int i, t_float v)
{
    p8_engine_t *e = &x->e;
    switch (i) {
    case X_OCT1: case X_OCT2:
        v = v < -1 ? -1 : v > 4 ? 4 : (t_float)(int)v;          /* -1..1 chip, 2..4 feet shifted up */
        e->octave[i == X_OCT2] = (int)v; x->ext[i] = v; p8_retune(e); return;
    case X_NOIS: v = clampf_((float)v, 0.0f, 15.0f); e->noise_level = (float)v / 15.0f; break;
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
    case X_DGLD: v = v != 0; e->ddl_glide = v != 0; break;   /* delay: 1 time changes glide (default), 0 mute 40 ms as the original */
    case X_FMIG: v = clampf_((float)v, 0.0f, 127.0f); e->fm_ig = (float)v / 64.0f; break;   /* input gain, 64 = 1 */
    case X_FMS:  v = (t_float)(int)clampf_((float)v, 0.0f, 2.0f); e->fm_src = (int)v; break;   /* sine, other DCO, input */
    case X_WSM:  v = clampf_((float)v, 0.0f, 127.0f); e->ws_m = (float)v; break;   /* MG -> Walsh position */
    case X_WSE:  v = clampf_((float)v, -127.0f, 127.0f); e->ws_e = (float)v; break;   /* DEG3 -> Walsh position */
    case X_VAMP: v = clampf_((float)v, 0.0f, 127.0f); e->vel_amp = (float)v / 127.0f; break;      /* 127: level = velocity */
    case X_VCUT: v = clampf_((float)v, 0.0f, 127.0f); e->vel_cut = 5.0f * (float)v / 127.0f; break; /* 127: soft key 5 oct lower */
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
static void out_display(t_pdoly8 *x, t_symbol *name);

/* <name> <value>: one setting; reported as "display <label> <value>" for the Launchkey / OLED */
static void apply_perf(t_pdoly8 *x, int i, t_float v);
static void pdoly8_anything(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc < 1) { pd_error(x, "pdoly8~: %s needs a value", s->s_name); return; }
    t_float v = atom_getfloatarg(0, argc, argv);
    if (argv[0].a_type == A_SYMBOL) { pd_error(x, "pdoly8~: %s needs a number", s->s_name); return; }
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) { apply_ctl(x, i, v); out_display(x, s); return; }
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) { apply_ext(x, i, v); out_display(x, s); return; }
    for (int i = 0; i < 3; i++) if (s == gensym(PERF_NAME[i])) { apply_perf(x, i, v); out_display(x, s); return; }
    pd_error(x, "pdoly8~: unknown message '%s'", s->s_name);
}

static void pdoly8_ctl(t_pdoly8 *x, t_floatarg i, t_floatarg v) { apply_ctl(x, (int)i, v); }

/* 0-127 (a pot, CV or MIDI CC) to a parameter's range and back:
 * 'i' up to 15 values: whole numbers in equal zones, more: continuous (fractions from
 *     14-bit controllers pass), 'r' linear,
 * 'b' / 'c' bipolar -hi..hi with 0 at 64 (whole numbers / continuous), 'f' linear, 'l' logarithmic */
typedef struct { const char *name; char kind; float lo, hi;
                 const char *label, *unit; } t_p8range;   /* unit: "a|b|c" names the values, "@" a target */
static const t_p8range RANGE[] = {
    {"har1",'i',0,127,"Timbre 1",""},
    {"har2",'i',0,127,"Timbre 2",""},
    {"env1",'i',0,127,"Env 1 morph",""},
    {"env2",'i',0,127,"Env 2 morph",""},
    {"intv",'i',0,12,"Interval","st"},
    {"detn",'i',0,127,"Detune",""},
    {"lev1",'i',0,127,"Level 1",""},
    {"lev2",'i',0,127,"Level 2",""},
    {"cut",'i',0,99,"Cutoff",""},
    {"res",'i',0,15,"Resonance",""},
    {"egi",'i',0,15,"EG depth",""},
    {"kbd",'i',0,2,"Key track","off|half|full"},
    {"env3",'i',0,127,"Env 3 morph",""},
    {"vcut",'i',0,127,"Vel>cutoff",""},
    {"drv",'i',0,127,"Drive",""},
    {"nois",'i',0,15,"Noise",""},
    {"oct1",'i',-1,4,"Octave 1","-1|0|+1|+2|+3|+4"},
    {"oct2",'i',-1,4,"Octave 2","-1|0|+1|+2|+3|+4"},
    {"pw1",'i',0,127,"Pulse width 1",""},
    {"pw2",'i',0,127,"Pulse width 2",""},
    {"pwm1",'i',0,127,"PWM 1",""},
    {"pwm2",'i',0,127,"PWM 2",""},
    {"hmg",'i',0,127,"MG>timbre",""},
    {"drft",'i',0,127,"Drift",""},
    {"wsh1",'i',0,127,"Walsh pos 1",""},
    {"wsh2",'i',0,127,"Walsh pos 2",""},
    {"wmx1",'i',0,127,"Walsh mix 1",""},
    {"wmx2",'i',0,127,"Walsh mix 2",""},
    {"wsm",'i',0,127,"MG>Walsh",""},
    {"wse",'b',-127,127,"EG3>Walsh",""},
    {"sync",'i',0,1,"Sync","off|on"},
    {"swp2",'i',0,72,"DCO2 pitch","st"},
    {"swe",'i',0,72,"EG3>DCO2","st"},
    {"swm",'i',0,24,"MG>DCO2","st"},
    {"ring",'i',0,127,"Ring mod",""},
    {"rmf",'i',0,14,"Ring source",""},
    {"fm1",'i',0,127,"FM depth 1",""},
    {"fm2",'i',0,127,"FM depth 2",""},
    {"fmr",'q',0.125f,16,"FM ratio","x"},
    {"fmhz",'c',-1000,1000,"FM offset","Hz"},
    {"fms",'i',0,2,"FM source","sine|other DCO|input"},
    {"fmig",'i',0,127,"FM input gain",""},
    {"dgld",'i',0,1,"Delay changes","mute|glide"},
    {"mgr",'i',0,15,"MG rate",""},
    {"mgi",'r',0,200,"Vibrato","ct"},
    {"mgd",'i',0,15,"MG delay",""},
    {"mgv",'i',0,15,"MG>cutoff",""},
    {"dtim",'i',0,99,"Delay time",""},
    {"dfb",'i',0,15,"Delay feedback",""},
    {"dmf",'i',0,31,"Delay mod rate",""},
    {"dmi",'i',0,31,"Delay mod depth",""},
    {"dlev",'i',0,15,"Delay level",""},
    {"port",'i',0,127,"Portamento",""},
    {"mono",'i',0,1,"Voice mode","poly|mono"},
    {"vamp",'i',0,127,"Vel>level",""},
    {"pmod",'i',0,2,"Glide mode","last|own|legato"},
    {"retr",'i',0,1,"Mono retrig","legato|retrig"},
    {"pol",'i',1,2,"EG polarity","down|up"},
    {"trg",'i',1,2,"Env 3 trigger","single|multi"},
    {"drvp",'i',0,1,"Drive position","post|pre"},
    {"mgf",'l',0.1f,30,"MG rate","Hz"},
    {"bnd",'i',0,12,"Bend range","st"},
    {"mrph",'i',0,100,"Smoothing","ms"},
    {"tone",'i',0,1,"Output stage","off|on"},
    {"leak",'i',0,1,"Leak","off|on"},
    {"treb",'i',-6,6,"Treble",""},
    {"bass",'i',-6,6,"Bass",""},
    {"sprd",'i',0,127,"Stereo spread",""},
    {"a1",'z',0,31,"Env 1 attack",""},
    {"d1",'z',0,31,"Env 1 decay",""},
    {"bp1",'z',0,31,"Env 1 break",""},
    {"sl1",'z',0,31,"Env 1 slope",""},
    {"su1",'z',0,31,"Env 1 sustain",""},
    {"r1",'z',0,31,"Env 1 release",""},
    {"a2",'z',0,31,"Env 2 attack",""},
    {"d2",'z',0,31,"Env 2 decay",""},
    {"bp2",'z',0,31,"Env 2 break",""},
    {"sl2",'z',0,31,"Env 2 slope",""},
    {"su2",'z',0,31,"Env 2 sustain",""},
    {"r2",'z',0,31,"Env 2 release",""},
    {"a3",'z',0,31,"Env 3 attack",""},
    {"d3",'z',0,31,"Env 3 decay",""},
    {"bp3",'z',0,31,"Env 3 break",""},
    {"sl3",'z',0,31,"Env 3 slope",""},
    {"su3",'z',0,31,"Env 3 sustain",""},
    {"r3",'z',0,31,"Env 3 release",""},
    {"ch1",'z',0,16,"MIDI channel","all|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16"},
    {"atk",'b',-63,63,"Attack all",""},
    {"rel",'b',-63,63,"Release all",""},
    {"vol",'i',0,127,"Volume",""}};
#define NRANGE ((int)(sizeof RANGE / sizeof RANGE[0]))
typedef char p8_rep_fits[(sizeof RANGE / sizeof RANGE[0]) <= 160 ? 1 : -1];

static const t_p8range *find_range(t_symbol *s)
{
    for (int i = 0; i < NRANGE; i++) if (s == gensym(RANGE[i].name)) return &RANGE[i];
    return 0;
}

/* performance controls: the same three in every instrument's page 1 (with cut, res, mgi,
 * dlev, egi); laid over the sound, not stored in it. A sound recalled sets atk and rel to 0. */

static t_float *find_value(t_pdoly8 *x, t_symbol *s)
{
    for (int i = 0; i < NCTL; i++) if (s == gensym(CTL_NAME[i])) return &x->ctl[i];
    for (int i = 0; i < NEXT; i++) if (s == gensym(EXT_NAME[i])) return &x->ext[i];
    for (int i = 0; i < 3; i++) if (s == gensym(PERF_NAME[i])) return &x->perf[i];
    return 0;
}

static void apply_perf(t_pdoly8 *x, int i, t_float v)
{
    if (i == 2) {                                /* vol: 100 = 0 dB, square law, 127 = +4 dB */
        v = clampf_((float)v, 0.0f, 127.0f); x->perf[2] = v;
        x->vol_gain = ((float)v / 100.0f) * ((float)v / 100.0f);
    } else {
        v = (t_float)(int)lroundf(clampf_((float)v, -63.0f, 63.0f)); x->perf[i] = v;
        if (i == 0) x->e.perf_att = (float)v * 31.0f / 63.0f; else x->e.perf_rel = (float)v * 31.0f / 63.0f;
        p8_update(&x->e);
    }
    rep_kick(x);
}

/* a sound recalled: attack and release back to the sound */
static void perf_reset(t_pdoly8 *x)
{
    if (x->perf[0] != 0) apply_perf(x, 0, 0);
    if (x->perf[1] != 0) apply_perf(x, 1, 0);
}

/* fmr from a pot / CC: harmonic ratios in steps, so 1 (and every other one) can be found again;
 * inharmonic FM: fmhz, or any ratio by message (fmr 1.41) */
static const float FMR_Q[] = {0.125f, 0.25f, 1.0f / 3, 0.5f, 2.0f / 3, 0.75f, 1, 1.25f, 4.0f / 3, 1.5f, 2, 2.5f,
                              3, 3.5f, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
#define NFMR ((int)(sizeof FMR_Q / sizeof FMR_Q[0]))

static float range_from127(const t_p8range *r, float c)
{
    c = clampf_(c, 0.0f, 127.0f);
    if (r->kind == 'q') return FMR_Q[(int)fminf(floorf(c * NFMR / 128.0f), NFMR - 1)];
    if (r->kind == 'l') return r->lo * powf(r->hi / r->lo, c / 127.0f);
    if (r->kind == 'f') return r->lo + (r->hi - r->lo) * c / 127.0f;
    if (r->kind == 'r') return r->lo + (r->hi - r->lo) * c / 127.0f;
    if (r->kind == 'b' || r->kind == 'c') return clampf_((c - 64.0f) * r->hi / 63.0f, -r->hi, r->hi);
    if (r->kind == 'i' && r->hi - r->lo >= 15.0f) return r->lo + (r->hi - r->lo) * c / 127.0f;   /* continuous */
    /* 'z' and short 'i': integers in zones of equal width */
    float n = r->hi - r->lo + 1.0f;                      /* zones of equal width */
    return r->lo + fminf(floorf(c * n / 128.0f), n - 1.0f);
}

static float range_to127(const t_p8range *r, float v)
{
    float c;
    if (r->kind == 'q') {                                /* nearest step (in octaves), middle of its zone */
        int b = 0;
        for (int i = 1; i < NFMR; i++)
            if (fabsf(log2f(fmaxf(v, 1e-3f) / FMR_Q[i])) < fabsf(log2f(fmaxf(v, 1e-3f) / FMR_Q[b]))) b = i;
        return floorf((b + 0.5f) * 128.0f / NFMR);
    }
    if (r->kind == 'l') c = 127.0f * logf(fmaxf(v, r->lo) / r->lo) / logf(r->hi / r->lo);
    else if (r->kind == 'f' || r->kind == 'r') c = 127.0f * (v - r->lo) / (r->hi - r->lo);
    else if (r->kind == 'b' || r->kind == 'c') c = 64.0f + v * 63.0f / r->hi;
    else if (r->kind == 'i' && r->hi - r->lo >= 15.0f) c = 127.0f * (v - r->lo) / (r->hi - r->lo);
    else { float n = r->hi - r->lo + 1.0f; c = (v - r->lo + 0.5f) * 128.0f / n; }   /* middle of the zone */
    return floorf(clampf_(c, 0.0f, 127.0f) + 0.5f);
}

#ifdef P8_OLED
static void oled_put(t_pdoly8 *x, const char *l1, const char *l2)
{
    if (!x->o_on) return;
    unsigned s = x->o_seq;
    __atomic_store_n(&x->o_seq, s + 1, __ATOMIC_RELEASE);          /* odd: writing */
    int b = ((s + 2) >> 1) & 1;
    snprintf(x->o_txt[b][0], sizeof x->o_txt[b][0], "%s", l1);
    snprintf(x->o_txt[b][1], sizeof x->o_txt[b][1], "%s", l2);
    __atomic_store_n(&x->o_seq, s + 2, __ATOMIC_RELEASE);
}

static int osc_str(char *p, const char *s)                   /* OSC string, padded to 4 */
{
    int n = (int)strlen(s) + 1, m = (n + 3) & ~3;
    memset(p, 0, (size_t)m); memcpy(p, s, (size_t)(n - 1));
    return m;
}

static void *oled_thread(void *arg)
{
    t_pdoly8 *x = (t_pdoly8 *)arg;
    unsigned sent = 0;
    struct timespec ts = {0, 40 * 1000 * 1000};             /* at most every 40 ms */
    while (!x->o_quit) {
        nanosleep(&ts, 0);
        unsigned s = __atomic_load_n(&x->o_seq, __ATOMIC_ACQUIRE);
        if (s == sent || (s & 1)) continue;
        char l[2][48]; int b = (s >> 1) & 1;
        memcpy(l, x->o_txt[b], sizeof l);
        if (__atomic_load_n(&x->o_seq, __ATOMIC_ACQUIRE) != s) continue;   /* changed while copying */
        char pk[256]; int n = 0;
        n += osc_str(pk + n, "/display-strings-and-numbers");
        n += osc_str(pk + n, ",sss");
        n += osc_str(pk + n, l[0]); n += osc_str(pk + n, "\\n"); n += osc_str(pk + n, l[1]);
        sendto(x->o_sock, pk, (size_t)n, 0, (struct sockaddr *)&x->o_to, sizeof x->o_to);
        sent = s;
    }
    return 0;
}

/* oled <host> <port>: send the changed setting (and oledtext) to O2O from a thread of its own */
static void pdoly8_oled(t_pdoly8 *x, t_symbol *host, t_floatarg port)
{
    if (x->o_on) return;
    x->o_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (x->o_sock < 0) { pd_error(x, "pdoly8~: oled: no socket"); return; }
    memset(&x->o_to, 0, sizeof x->o_to);
    x->o_to.sin_family = AF_INET; x->o_to.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host->s_name, &x->o_to.sin_addr) != 1) { pd_error(x, "pdoly8~: oled: bad address"); close(x->o_sock); return; }
    x->o_quit = 0; x->o_on = 1;
    if (pthread_create(&x->o_th, 0, oled_thread, x) != 0) { x->o_on = 0; close(x->o_sock); pd_error(x, "pdoly8~: oled: no thread"); }
}

/* oledtext <line 1> <line 2>: show two lines (e.g. the program) */
static void pdoly8_oledtext(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    char a[48] = "", b[48] = "";
    if (argc > 0) atom_string(argv, a, sizeof a);
    if (argc > 1) atom_string(argv + 1, b, sizeof b);
    oled_put(x, a, b);
}
#endif

static int disp_text(t_pdoly8 *x, t_symbol *name, const char **label, char *buf, size_t nb);

/* n <name> <0-127>: set a parameter from a pot, CV or CC; "none" is ignored */
static void pdoly8_n(t_pdoly8 *x, t_symbol *name, t_floatarg c)
{
    if (name == gensym("none")) return;
    const t_p8range *r = find_range(name);
    if (!r) { pd_error(x, "pdoly8~: n: no 0-127 range for '%s'", name->s_name); return; }
    t_atom a; SETFLOAT(&a, range_from127(r, c));
    pdoly8_anything(x, name, 1, &a);
#ifdef P8_OLED
    if (x->o_on) { const char *l; char v[48]; if (disp_text(x, name, &l, v, sizeof v)) oled_put(x, l, v); }
#endif
}

/* disp <name>: outlet 3 gets "disp <label> <value>", two symbols for a display
 * (OLED): the setting's name and its value with unit, switch name or target */
static int disp_text(t_pdoly8 *x, t_symbol *name, const char **label, char *buf, size_t nb)
{
    const t_p8range *r = find_range(name);
    t_float *pv = find_value(x, name);
    if (!r || !pv) return 0;
    float v = *pv;
    *label = r->label;
    if (strchr(r->unit, '|')) {                     /* named values: lo, lo+1, ... */
        int k = (int)lroundf(v - r->lo), n = 0;
        const char *p = r->unit, *q;
        while (n < k && (q = strchr(p, '|'))) { p = q + 1; n++; }
        q = strchr(p, '|'); int len = q ? (int)(q - p) : (int)strlen(p);
        snprintf(buf, nb, "%.*s", len, p);
    }
    else if ((r->kind == 'i' && r->hi - r->lo < 15.0f) || r->kind == 'z')
        snprintf(buf, nb, "%.0f%s%s", v, r->unit[0] ? " " : "", r->unit);
    else if (r->kind == 'l' || fabsf(v) < 10.0f) snprintf(buf, nb, "%.3g%s%s", v, r->unit[0] ? " " : "", r->unit);
    else snprintf(buf, nb, "%.1f%s%s", v, r->unit[0] ? " " : "", r->unit);
    return 1;
}

static void pdoly8_disp(t_pdoly8 *x, t_symbol *name)
{
    const char *l; char buf[64];
    if (!disp_text(x, name, &l, buf, sizeof buf)) return;
    t_atom a[2]; SETSYMBOL(a, gensym(l)); SETSYMBOL(a + 1, gensym(buf));
    outlet_anything(x->x_dump, gensym("disp"), 2, a);
}

/* display <label> <value> on outlet 3 after a setting was changed by a message or n (the
 * Launchkey display through [lkdisplay], as juno~, d50~ and cz4~ do) */
static void out_display(t_pdoly8 *x, t_symbol *name)
{
    const char *l; char buf[64];
    if (!disp_text(x, name, &l, buf, sizeof buf)) return;
    t_atom a[2]; SETSYMBOL(a, gensym(l)); SETSYMBOL(a + 1, gensym(buf));
    outlet_anything(x->x_dump, gensym("display"), 2, a);
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

/* info <name> <0-127> <value text> on outlet 3: the pot position and the value with its unit */
static void emit_info(t_pdoly8 *x, const t_p8range *r, t_float v)
{
    const char *l; char buf[64];
    if (!disp_text(x, gensym(r->name), &l, buf, sizeof buf)) return;
    char nb[96]; size_t j = 0;                           /* spaces -> no-break space: one clean symbol */
    for (const char *c = buf; *c && j < sizeof nb - 3; c++) {
        if (*c == ' ') { nb[j++] = (char)0xc2; nb[j++] = (char)0xa0; } else nb[j++] = *c;
    }
    nb[j] = 0;
    t_atom a[3]; SETSYMBOL(a, gensym(r->name)); SETFLOAT(a + 1, range_to127(r, v)); SETSYMBOL(a + 2, gensym(nb));
    outlet_anything(x->x_dump, gensym("info"), 3, a);
}

/* ninfo <name>: info for one setting now */
static void pdoly8_ninfo(t_pdoly8 *x, t_symbol *name)
{
    const t_p8range *r = find_range(name);
    t_float *v = find_value(x, name);
    if (r && v) emit_info(x, r, *v);
}

/* every change (message, n, preset, defaults) is reported once as info, in the same logical
 * time (clock 0): the controller layer sends it back to the controller, the editor shows it */
static void rep_tick(t_pdoly8 *x)
{
    for (int i = 0; i < NRANGE; i++) {
        if (!x->rep_v[i] || *x->rep_v[i] == x->rep_last[i]) continue;
        x->rep_last[i] = *x->rep_v[i];
        emit_info(x, &RANGE[i], *x->rep_v[i]);
        if (!strcmp(RANGE[i].name, "ch1")) {          /* the controller layer follows the channel */
            t_atom a; SETFLOAT(&a, *x->rep_v[i]);
            outlet_anything(x->x_dump, gensym("channel"), 1, &a);
        }
    }
}
static void rep_kick(t_pdoly8 *x) { if (x->rep_clk) clock_delay(x->rep_clk, 0); }

static void pdoly8_params(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    for (int i = 0; i < argc && i < NCTL; i++) apply_ctl(x, i, atom_getfloatarg(i, argc, argv));
}

/* the MIDI channel (ch1) is part of the setup, not of a sound: defaults and presets keep it;
 * the first time it is 1 (one channel per instrument, see the controller layer) */
static void set_defaults(t_pdoly8 *x)
{
    float ch = x->inited ? x->ext[X_CH1] : 1;
    x->restoring = 1;
    for (int i = 0; i < NEXT; i++) apply_ext(x, i, i == X_CH1 ? ch : EXT_DEFAULT[i]);
    x->restoring = 0;
    for (int i = 0; i < NCTL; i++) apply_ctl(x, i, CTL_DEFAULT[i]);
    if (!x->inited) apply_perf(x, 2, PERF_DEFAULT[2]);   /* vol: set up once, like ch1 */
    perf_reset(x);
    x->inited = 1;
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
/* Preset lines before 25 Nov 2026 carry DEG4, LFO2 and LFO3 (28 values) between drft and fmig:
 * a line with more values than the current layout is such a line, its block is skipped.
 * src_index: position j of the current layout (controls, then settings) -> index into the
 * nvals values of the line, or -1 when the line is shorter. */
#define OLD_MOD_VALUES 28
static int src_index(int nvals, int j)
{
    const int skip = nvals > NCTL + NEXT ? OLD_MOD_VALUES : 0;
    const int k = j < NCTL + X_FMIG ? j : j + skip;
    return k < nvals ? k : -1;
}

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
    const int nvals = argc - 1;
    x->restoring = 1;
    for (int i = 0; i < NEXT; i++) {
        const int k = src_index(nvals, NCTL + i);
        if (k >= 0 && i != X_CH1) apply_ext(x, i, atom_getfloatarg(1 + k, argc, argv));
    }
    x->restoring = 0;
    for (int i = 0; i < NCTL; i++) {
        const int k = src_index(nvals, i);
        if (k >= 0) apply_ctl(x, i, atom_getfloatarg(1 + k, argc, argv));
    }
    perf_reset(x);
}


/* ------------------------------------------------- Poly-800 II sysex (as tools/p8sysex.py)
 * Data dump: F0 42 21 0C, the RAM (2688 bytes) as 5376 nibbles (low first), the 8-bit sum of the
 * RAM as two nibbles, F7. 64 programs of 26 bytes from 400H, fields = one bit stream, LSB first.
 *   syxin <byte>       bytes from [sysexin]; a complete dump -> "bank <n> pdoly8 v ..." for every
 *                      program on outlet 3, then "syxstatus received"
 *   syxread <file>     the same from a .syx file
 *   syxbank <n> pdoly8 v ...   put a preset into program n (0-63) of the stored memory
 *   syxsend            the stored memory as a dump: "syxbyte <b>" on outlet 3 (-> [midiout])
 *   syxch <1-16>       the Poly-800's MIDI channel (header byte 2n); a dump received or read sets it
 *   syxtemplate <file> only the memory outside the programs from a .syx file (no programs out)
 *   syxwrite <file>    the stored memory as a .syx file
 * The memory outside the programs (sequencer, chord memory, MIDI channel) is kept from the last
 * dump received or read, so syxsend / syxwrite need one first. */
#define SYX_RAM 0xA80
#define SYX_PBASE 0x400
#define SYX_PSIZE 26
typedef struct { const char *name; int byte, lo, n; } t_syxf;
static const t_syxf SYXF[] = {
    {"oct1", 0, 0, 2}, {"oct2", 0, 2, 2}, {"wave1", 0, 4, 2}, {"wave2", 0, 6, 2},
    {"feet1", 1, 0, 4}, {"feet2", 1, 4, 4}, {"detune", 2, 0, 2}, {"mode", 2, 6, 1},
    {"interval", 3, 0, 4}, {"noise", 3, 4, 4}, {"egint", 4, 0, 4}, {"polarity", 4, 4, 1},
    {"level1", 4, 5, 5}, {"level2", 5, 2, 5}, {"cutoff", 6, 0, 7}, {"trigger", 6, 7, 1},
    {"mgfreq", 7, 0, 4}, {"mgdelay", 7, 4, 4}, {"mgdco", 8, 0, 4}, {"mgvcf", 8, 4, 4},
    {"a1", 9, 0, 5}, {"d1", 9, 5, 5}, {"bp1", 10, 2, 5}, {"sl1", 10, 7, 5}, {"su1", 11, 4, 5}, {"r1", 12, 1, 5},
    {"a2", 12, 6, 5}, {"d2", 13, 3, 5}, {"bp2", 14, 0, 5}, {"sl2", 14, 5, 5}, {"su2", 15, 2, 5}, {"r2", 15, 7, 5},
    {"a3", 16, 4, 5}, {"d3", 17, 1, 5}, {"bp3", 17, 6, 5}, {"sl3", 18, 3, 5}, {"su3", 19, 0, 5}, {"r3", 19, 5, 5},
    {"resonance", 20, 2, 4}, {"kbd", 20, 6, 2}, {"ddltime", 21, 0, 7}, {"ddlfb", 22, 0, 4},
    {"ddllevel", 22, 4, 4}, {"ddlmf", 23, 0, 5}, {"ddlmi", 24, 0, 5}, {"treble", 25, 0, 4}, {"bass", 25, 4, 4}};
#define NSYXF ((int)(sizeof SYXF / sizeof SYXF[0]))
static const int SYX_TIMB[15] = {0x8, 0x9, 0xB, 0xA, 0xE, 0xF, 0xD, 0xC, 0xC, 0xE, 0xA, 0xB, 0xF, 0xD, 0x9};
static const float SYX_MGSTEP[7] = {0, 3, 6, 8, 9, 12, 15};
static const float SYX_MGHZ[7] = {0.291f, 1.262f, 3.39f, 5.22f, 6.493f, 11.09f, 19.08f};
static const float SYX_KPOS[8] = {0, 1, 24, 48, 72, 92, 108, 127};
static const float SYX_KNOT[8][6] = {{0, 0, 31, 0, 31, 0}, {0, 3, 0, 0, 0, 3}, {0, 14, 0, 0, 0, 12},
    {0, 10, 20, 24, 0, 14}, {2, 16, 24, 0, 24, 26}, {16, 0, 31, 0, 31, 18}, {20, 22, 26, 0, 26, 24},
    {30, 26, 26, 0, 26, 31}};
#define SYX_SQ16 0.402717f                       /* 10^(-7.9/20): 16' alone, square net vs saw net */

static int syx_field(const char *n) { for (int i = 0; i < NSYXF; i++) if (!strcmp(SYXF[i].name, n)) return i; return -1; }
static int vidx(const char *n)                     /* index in a preset line (controls, then settings) */
{
    for (int i = 0; i < NCTL; i++) if (!strcmp(CTL_NAME[i], n)) return i;
    for (int i = 0; i < NEXT; i++) if (!strcmp(EXT_NAME[i], n)) return NCTL + i;
    return -1;
}
static int getbits(const unsigned char *b, int byte, int lo, int n)
{
    int v = 0;
    for (int k = 0; k < n; k++) { int p = byte * 8 + lo + k; v |= ((b[p / 8] >> (p % 8)) & 1) << k; }
    return v;
}
static void setbits(unsigned char *b, int byte, int lo, int n, int v)
{
    for (int k = 0; k < n; k++) {
        int p = byte * 8 + lo + k;
        if ((v >> k) & 1) b[p / 8] |= (unsigned char)(1 << (p % 8)); else b[p / 8] &= (unsigned char)~(1 << (p % 8));
    }
}
static int feet_msb(int f) { int m = 0; for (int k = 0; k < 4; k++) m |= ((f >> k) & 1) << (3 - k); return m; }
static float mg_hz(float step)
{
    int a = 0;
    while (a < 5 && step > SYX_MGSTEP[a + 1]) a++;
    float t = (step - SYX_MGSTEP[a]) / (SYX_MGSTEP[a + 1] - SYX_MGSTEP[a]);
    return SYX_MGHZ[a] * powf(SYX_MGHZ[a + 1] / SYX_MGHZ[a], t);
}
static float rnd(float v, float q) { return roundf(v / q) * q; }

/* program n of the memory -> preset line v[NCTL + NEXT] */
static void syx_to_line(const unsigned char *ram, int n, float *v)
{
    const unsigned char *p = ram + SYX_PBASE + SYX_PSIZE * n;
    int f[NSYXF];
    for (int i = 0; i < NSYXF; i++) f[i] = getbits(p, SYXF[i].byte, SYXF[i].lo, SYXF[i].n);
#define F(name) f[syx_field(name)]
#define V(name) v[vidx(name)]
    for (int i = 0; i < NCTL; i++) v[i] = CTL_DEFAULT[i];
    for (int i = 0; i < NEXT; i++) v[NCTL + i] = EXT_DEFAULT[i];
    int dbl = F("mode") == 1;
    for (int g = 1; g <= 2; g++) {
        char k[8];
        int wave = f[syx_field(g == 1 ? "wave1" : "wave2")], m = feet_msb(f[syx_field(g == 1 ? "feet1" : "feet2")]);
        float har = 45, lf = 0; int shift = 0;
        if (m) {
            while (!(m & 8)) { m = (m << 1) & 0xF; shift++; }
            lf = 1;
            if (m == 0x8) { har = 0; if (wave == 0) lf = SYX_SQ16; }
            else for (int i = 0; i < 15; i++)
                if (SYX_TIMB[i] == m && (i < 8) == (wave == 1)) { har = roundf(i * 127.0f / 14.0f); break; }
        }
        float lev = f[syx_field(g == 1 ? "level1" : "level2")] * 127.0f / 31.0f * lf;
        if (g == 2 && !dbl) lev = 0;
        snprintf(k, sizeof k, "har%d", g); V(k) = har;
        snprintf(k, sizeof k, "oct%d", g); { int o = f[syx_field(g == 1 ? "oct1" : "oct2")] - 1 + shift; V(k) = o > 4 ? 4 : o; }
        snprintf(k, sizeof k, "lev%d", g); V(k) = roundf(lev);
        snprintf(k, sizeof k, "env%d", g); V(k) = -1;
    }
    static const float DET[4] = {0, 42, 85, 127};
    V("intv") = F("interval"); V("detn") = DET[F("detune") & 3]; V("nois") = F("noise");
    V("cut") = F("cutoff"); V("res") = F("resonance"); V("kbd") = F("kbd");
    V("pol") = F("polarity") + 1; V("egi") = F("egint"); V("trg") = F("trigger") + 1; V("env3") = -1;
    static const char *ST[6] = {"a", "d", "bp", "sl", "su", "r"};
    for (int g = 1; g <= 3; g++) for (int k = 0; k < 6; k++) {
        char nm[8]; snprintf(nm, sizeof nm, "%s%d", ST[k], g);
        V(nm) = f[syx_field(nm)];
    }
    V("mgr") = F("mgfreq"); V("mgf") = rnd(mg_hz(F("mgfreq")), 0.001f); V("mgd") = F("mgdelay");
    V("mgi") = rnd(F("mgdco") * 12.4f, 0.1f); V("mgv") = F("mgvcf");
    V("dtim") = F("ddltime"); V("dfb") = F("ddlfb"); V("dmf") = F("ddlmf"); V("dmi") = F("ddlmi"); V("dlev") = F("ddllevel");
    V("treb") = F("treble") - 6; V("bass") = F("bass") - 6;
}

static void env_morph_(float pos, float *out)
{
    if (pos < 0.5f) { for (int k = 0; k < 6; k++) out[k] = SYX_KNOT[0][k]; return; }
    pos = clampf_(pos, 1, 127); int a = 1;
    while (a < 6 && pos > SYX_KPOS[a + 1]) a++;
    float x0 = SYX_KPOS[a], x1 = SYX_KPOS[a + 1], sn = fminf(1.0f, 0.45f * (x1 - x0));
    float lo = a == 1 ? x0 : x0 + sn, hi = a + 1 == 7 ? x1 : x1 - sn;
    float t = pos <= lo ? 0 : pos >= hi ? 1 : (pos - lo) / (hi - lo);
    for (int k = 0; k < 6; k++) out[k] = SYX_KNOT[a][k] + t * (SYX_KNOT[a + 1][k] - SYX_KNOT[a][k]);
}

/* preset line -> program n of the memory (the nearest Poly-800 values) */
static void syx_from_line(unsigned char *ram, int n, const float *v)
{
    int f[NSYXF];
    unsigned char *p = ram + SYX_PBASE + SYX_PSIZE * n;
    for (int i = 0; i < NSYXF; i++) f[i] = getbits(p, SYXF[i].byte, SYXF[i].lo, SYXF[i].n);
    static const char *ST[6] = {"a", "d", "bp", "sl", "su", "r"};
    for (int g = 1; g <= 2; g++) {
        char k[8]; int oc;
        snprintf(k, sizeof k, "oct%d", g); oc = (int)V(k);
        snprintf(k, sizeof k, "har%d", g);
        int i = (int)clampf_(roundf(V(k) * 14.0f / 127.0f), 0, 14), m = SYX_TIMB[i], wave = i < 8;
        for (int e = oc - 1; e > 0; e--) m >>= 1;
        int feet = 0; for (int b = 0; b < 4; b++) feet |= ((m >> (3 - b)) & 1) << b;
        f[syx_field(g == 1 ? "wave1" : "wave2")] = wave;
        f[syx_field(g == 1 ? "feet1" : "feet2")] = feet;
        f[syx_field(g == 1 ? "oct1" : "oct2")] = (oc < 1 ? oc : 1) + 1;
        snprintf(k, sizeof k, "lev%d", g); float lev = V(k);
        if (feet == 1 && wave == 0) lev /= SYX_SQ16;
        f[syx_field(g == 1 ? "level1" : "level2")] = (int)clampf_(roundf(lev * 31.0f / 127.0f), 0, 31);
        snprintf(k, sizeof k, "env%d", g); float env = V(k), d[6];
        if (env >= 0) env_morph_(env, d);
        else for (int s = 0; s < 6; s++) { char nm[8]; snprintf(nm, sizeof nm, "%s%d", ST[s], g); d[s] = V(nm); }
        for (int s = 0; s < 6; s++) { char nm[8]; snprintf(nm, sizeof nm, "%s%d", ST[s], g); f[syx_field(nm)] = (int)clampf_(roundf(d[s]), 0, 31); }
    }
    {
        float d[6];
        if (V("env3") >= 0) env_morph_(V("env3"), d);
        else for (int s = 0; s < 6; s++) { char nm[8]; snprintf(nm, sizeof nm, "%s3", ST[s]); d[s] = V(nm); }
        for (int s = 0; s < 6; s++) { char nm[8]; snprintf(nm, sizeof nm, "%s3", ST[s]); f[syx_field(nm)] = (int)clampf_(roundf(d[s]), 0, 31); }
    }
    F("mode") = V("lev2") > 0;
    F("interval") = (int)clampf_(roundf(V("intv")), 0, 12);
    F("detune") = (int)clampf_(roundf(V("detn") * 3.0f / 127.0f), 0, 3);
    F("noise") = (int)clampf_(V("nois"), 0, 15);
    F("cutoff") = (int)clampf_(roundf(V("cut")), 0, 99); F("resonance") = (int)clampf_(roundf(V("res")), 0, 15);
    F("kbd") = (int)clampf_(V("kbd"), 0, 2);
    F("polarity") = (int)V("pol") - 1 > 0; F("egint") = (int)clampf_(roundf(V("egi")), 0, 15); F("trigger") = (int)V("trg") - 1 > 0;
    if (V("mgr") >= 0) F("mgfreq") = (int)clampf_(V("mgr"), 0, 15);
    else {
        int b = 0; float hz = fmaxf(V("mgf"), 1e-3f);
        for (int s = 1; s < 16; s++) if (fabsf(logf(hz) - logf(mg_hz(s))) < fabsf(logf(hz) - logf(mg_hz(b)))) b = s;
        F("mgfreq") = b;
    }
    F("mgdelay") = (int)clampf_(roundf(V("mgd")), 0, 15);
    F("mgdco") = (int)clampf_(roundf(V("mgi") / 12.4f), 0, 15); F("mgvcf") = (int)clampf_(roundf(V("mgv")), 0, 15);
    F("ddltime") = (int)clampf_(roundf(V("dtim")), 0, 99); F("ddlfb") = (int)clampf_(roundf(V("dfb")), 0, 15);
    F("ddlmf") = (int)clampf_(roundf(V("dmf")), 0, 31); F("ddlmi") = (int)clampf_(roundf(V("dmi")), 0, 31);
    F("ddllevel") = (int)clampf_(roundf(V("dlev")), 0, 15);
    F("treble") = (int)clampf_(roundf(V("treb")) + 6, 0, 15); F("bass") = (int)clampf_(roundf(V("bass")) + 6, 0, 15);
    for (int i = 0; i < NSYXF; i++) setbits(p, SYXF[i].byte, SYXF[i].lo, SYXF[i].n, f[i]);
#undef F
#undef V
}

static void syx_status(t_pdoly8 *x, const char *s)
{
    t_atom a; SETSYMBOL(&a, gensym(s));
    outlet_anything(x->x_dump, gensym("syxstatus"), 1, &a);
}

/* a dump (from F0 to F7) -> the memory and the 64 programs as presets */
static void syx_take(t_pdoly8 *x, const unsigned char *d, int n, int emit)
{
    int i = 0;
    while (i + 4 <= n && !(d[i] == 0xF0 && d[i + 1] == 0x42 && (d[i + 2] & 0xF0) == 0x20 && d[i + 3] == 0x0C)) i++;
    if (i + 4 + 2 * SYX_RAM + 2 > n) { syx_status(x, "no Poly-800 II dump"); return; }
    const unsigned char *b = d + i + 4;
    unsigned sum = 0;
    for (int k = 0; k < SYX_RAM; k++) { x->syx_ram[k] = (unsigned char)((b[2 * k] & 15) | ((b[2 * k + 1] & 15) << 4)); sum += x->syx_ram[k]; }
    unsigned cs = (b[2 * SYX_RAM] & 15) | ((b[2 * SYX_RAM + 1] & 15) << 4);
    x->syx_has = 1;
    if (!emit) { syx_status(x, "ready"); return; }   /* template only: the programs stay as they are */
    x->syx_ch = (d[i + 2] & 15) + 1;
    t_atom a[2 + NCTL + NEXT]; float v[NCTL + NEXT];
    for (int p = 0; p < 64; p++) {
        syx_to_line(x->syx_ram, p, v);
        SETFLOAT(a, p); SETSYMBOL(a + 1, gensym("pdoly8"));
        for (int k = 0; k < NCTL + NEXT; k++) SETFLOAT(a + 2 + k, v[k]);
        outlet_anything(x->x_dump, gensym("bank"), 2 + NCTL + NEXT, a);
    }
    t_atom c; SETFLOAT(&c, x->syx_ch);
    outlet_anything(x->x_dump, gensym("syxch"), 1, &c);
    syx_status(x, (sum & 0xFF) == cs ? "64 programs received" : "received, check sum differs");
}

static void pdoly8_syxin(t_pdoly8 *x, t_floatarg f)
{
    int b = (int)f & 0xFF;
    if (b == 0xF0) x->syx_n = 0;
    if (x->syx_n < (int)sizeof x->syx_in) x->syx_in[x->syx_n++] = (unsigned char)b;
    if (b == 0xF7) { syx_take(x, x->syx_in, x->syx_n, 1); x->syx_n = 0; }
}

static void syx_file(t_pdoly8 *x, t_symbol *file, int emit)
{
    char dir[MAXPDSTRING], *base; int fd = canvas_open(x->canvas, file->s_name, "", dir, &base, MAXPDSTRING, 1);
    if (fd < 0) { pd_error(x, "pdoly8~: syxread: can't open %s", file->s_name); syx_status(x, "file not found"); return; }
    sys_close(fd);
    char path[2 * MAXPDSTRING + 2]; snprintf(path, sizeof path, "%s/%s", dir, base);
    FILE *fp = sys_fopen(path, "rb");
    if (!fp) { syx_status(x, "file not found"); return; }
    unsigned char *d = (unsigned char *)getbytes(0x4000);
    int n = (int)fread(d, 1, 0x4000, fp); fclose(fp);
    syx_take(x, d, n, emit);
    freebytes(d, 0x4000);
}
static void pdoly8_syxread(t_pdoly8 *x, t_symbol *file) { syx_file(x, file, 1); }
/* syxtemplate <file>: only the memory outside the programs (sequencer, chord memory ...) from a
 * .syx file, nothing goes out; the editor loads the factory dump this way at start */
static void pdoly8_syxtemplate(t_pdoly8 *x, t_symbol *file) { syx_file(x, file, 0); }

static void pdoly8_syxbank(t_pdoly8 *x, t_symbol *s, int argc, t_atom *argv)
{
    (void)s;
    if (argc < 3 || argv[1].a_type != A_SYMBOL || argv[1].a_w.w_symbol != gensym("pdoly8")) return;
    int n = (int)atom_getfloatarg(0, argc, argv);
    if (n < 0 || n > 63) return;
    float v[NCTL + NEXT];
    for (int i = 0; i < NCTL; i++) v[i] = CTL_DEFAULT[i];
    for (int i = 0; i < NEXT; i++) v[NCTL + i] = EXT_DEFAULT[i];
    for (int i = 0; i < NCTL + NEXT; i++) {
        const int k = src_index(argc - 2, i);
        if (k >= 0) v[i] = atom_getfloatarg(k + 2, argc, argv);
    }
    syx_from_line(x->syx_ram, n, v);
}

static int syx_bytes(t_pdoly8 *x, unsigned char *out)
{
    int k = 0; unsigned sum = 0;
    out[k++] = 0xF0; out[k++] = 0x42; out[k++] = (unsigned char)(0x20 | ((x->syx_ch - 1) & 15)); out[k++] = 0x0C;
    for (int i = 0; i < SYX_RAM; i++) { out[k++] = x->syx_ram[i] & 15; out[k++] = x->syx_ram[i] >> 4; sum += x->syx_ram[i]; }
    out[k++] = sum & 15; out[k++] = (sum >> 4) & 15; out[k++] = 0xF7;
    return k;
}

/* txchannel <0-16>: the channel the controller layer sends its feedback on (0 = the receive
 * channel ch1). Reported, so the layer follows it; the same message in every instrument. */
static void pdoly8_txchannel(t_pdoly8 *x, t_floatarg f)
{
    x->txch = (int)clampf_(f, 0, 16);
    t_atom a; SETFLOAT(&a, x->txch);
    outlet_anything(x->x_dump, gensym("txchannel"), 1, &a);
}

/* syxch <1-16>: the MIDI channel of the Poly-800 II (it only takes a dump on its own channel) */
static void pdoly8_syxch(t_pdoly8 *x, t_floatarg f)
{
    x->syx_ch = (int)clampf_(f, 1, 16);
    t_atom a; SETFLOAT(&a, x->syx_ch);
    outlet_anything(x->x_dump, gensym("syxch"), 1, &a);
}

static void pdoly8_syxsend(t_pdoly8 *x)
{
    if (!x->syx_has) { syx_status(x, "no template: presets/poly800mk2-factory.syx missing"); return; }
    unsigned char d[4 + 2 * SYX_RAM + 3];
    int n = syx_bytes(x, d);
    t_atom a;
    for (int i = 0; i < n; i++) { SETFLOAT(&a, d[i]); outlet_anything(x->x_dump, gensym("syxbyte"), 1, &a); }
    syx_status(x, "64 programs sent");
}

static void pdoly8_syxwrite(t_pdoly8 *x, t_symbol *file)
{
    if (!x->syx_has) { syx_status(x, "no template: presets/poly800mk2-factory.syx missing"); return; }
    char path[MAXPDSTRING];
    canvas_makefilename(x->canvas, file->s_name, path, MAXPDSTRING);
    FILE *fp = sys_fopen(path, "wb");
    if (!fp) { pd_error(x, "pdoly8~: syxwrite: can't write %s", path); syx_status(x, "can't write"); return; }
    unsigned char d[4 + 2 * SYX_RAM + 3];
    fwrite(d, 1, (size_t)syx_bytes(x, d), fp); fclose(fp);
    syx_status(x, "written");
}


/* ------------------------------------------------- random voices (as tx81z~)
 * random <pluck|bass|lead|brass|pad|strings|total> [seed]: a new sound from the defaults, with
 * ranges per kind for the timbres, envelopes, filter, modulation and delay. MIDI channel stays. */
static float rnd01(t_pdoly8 *x)
{
    x->rnd ^= x->rnd << 13; x->rnd ^= x->rnd >> 17; x->rnd ^= x->rnd << 5;
    return (x->rnd >> 8) * (1.0f / 16777216.0f);
}
static float rf(t_pdoly8 *x, float lo, float hi) { return lo + (hi - lo) * rnd01(x); }
static int ri(t_pdoly8 *x, int lo, int hi) { return lo + (int)fminf((float)(hi - lo), floorf(rnd01(x) * (hi - lo + 1))); }
static int chance(t_pdoly8 *x, int pct) { return ri(x, 1, 100) <= pct; }
static float pickf(t_pdoly8 *x, const float *v, int n) { return v[ri(x, 0, n - 1)]; }
#define PICK(...) pickf(x, (const float[]){__VA_ARGS__}, (int)(sizeof((const float[]){__VA_ARGS__}) / sizeof(float)))

static void setv(t_pdoly8 *x, const char *name, float v)
{
    for (int i = 0; i < NCTL; i++) if (!strcmp(CTL_NAME[i], name)) { apply_ctl(x, i, v); return; }
    for (int i = 0; i < NEXT; i++) if (!strcmp(EXT_NAME[i], name)) { apply_ext(x, i, v); return; }
}

static void chorus(t_pdoly8 *x, int pct)
{
    if (!chance(x, pct)) return;
    setv(x, "dtim", ri(x, 4, 18)); setv(x, "dfb", ri(x, 0, 3)); setv(x, "dmf", ri(x, 4, 14));
    setv(x, "dmi", ri(x, 10, 26)); setv(x, "dlev", ri(x, 6, 12));
}

static void pdoly8_random(t_pdoly8 *x, t_symbol *kind, t_floatarg seed)
{
    const char *k = kind->s_name;
    if (seed != 0) x->rnd = (unsigned)seed * 2654435761u | 1;
    if (strcmp(k, "pluck") && strcmp(k, "bass") && strcmp(k, "lead") && strcmp(k, "brass") && strcmp(k, "pad")
        && strcmp(k, "strings") && strcmp(k, "total")) {
        pd_error(x, "pdoly8~: random pluck / bass / lead / brass / pad / strings / total"); return;
    }
    set_defaults(x);
    float saw = ri(x, 0, 63), any = ri(x, 0, 127);
    setv(x, "har1", any); setv(x, "har2", chance(x, 50) ? any : ri(x, 0, 127));
    setv(x, "lev1", 127); setv(x, "kbd", ri(x, 1, 2)); setv(x, "vamp", ri(x, 20, 70));
    if (!strcmp(k, "pluck")) {
        setv(x, "env1", ri(x, 16, 44)); setv(x, "env2", ri(x, 16, 44)); setv(x, "env3", ri(x, 16, 48));
        setv(x, "cut", ri(x, 20, 55)); setv(x, "res", ri(x, 0, 7)); setv(x, "egi", ri(x, 7, 15));
        setv(x, "oct2", PICK(0, 0, 1)); setv(x, "intv", PICK(0, 0, 7, 12)); setv(x, "detn", ri(x, 0, 40));
        setv(x, "lev2", ri(x, 0, 110)); setv(x, "vcut", ri(x, 20, 70));
        if (chance(x, 30)) { setv(x, "dtim", ri(x, 30, 70)); setv(x, "dfb", ri(x, 2, 7)); setv(x, "dlev", ri(x, 4, 9)); }
    } else if (!strcmp(k, "bass")) {
        setv(x, "oct1", -1); setv(x, "oct2", PICK(-1, 0)); setv(x, "intv", PICK(0, 0, 12, 7));
        setv(x, "env1", ri(x, 20, 60)); setv(x, "env2", ri(x, 20, 60)); setv(x, "env3", ri(x, 14, 40));
        setv(x, "cut", ri(x, 10, 40)); setv(x, "res", ri(x, 2, 11)); setv(x, "egi", ri(x, 8, 15));
        setv(x, "lev2", ri(x, 0, 90)); setv(x, "detn", ri(x, 0, 20)); setv(x, "drv", ri(x, 0, 60));
        if (chance(x, 30)) { setv(x, "mono", 1); setv(x, "port", ri(x, 10, 40)); }
    } else if (!strcmp(k, "lead")) {
        setv(x, "oct1", PICK(0, 0, 1)); setv(x, "oct2", PICK(0, 1));
        setv(x, "env1", ri(x, 64, 80)); setv(x, "env2", ri(x, 64, 80)); setv(x, "env3", ri(x, 50, 80));
        setv(x, "cut", ri(x, 40, 80)); setv(x, "res", ri(x, 2, 9)); setv(x, "egi", ri(x, 3, 10));
        setv(x, "lev2", ri(x, 60, 127)); setv(x, "detn", ri(x, 20, 60));
        setv(x, "mgr", ri(x, 5, 9)); setv(x, "mgd", ri(x, 3, 8)); setv(x, "mgi", rf(x, 10, 40));
        if (chance(x, 50)) { setv(x, "mono", 1); setv(x, "port", ri(x, 10, 40)); }
        if (chance(x, 30)) { setv(x, "sync", 1); setv(x, "swp2", ri(x, 0, 12)); setv(x, "swe", ri(x, 12, 36)); }
        setv(x, "drv", ri(x, 0, 50));
    } else if (!strcmp(k, "brass")) {
        setv(x, "har1", saw); setv(x, "har2", saw);
        setv(x, "env1", ri(x, 84, 100)); setv(x, "env2", ri(x, 84, 100)); setv(x, "env3", ri(x, 80, 100));
        setv(x, "cut", ri(x, 25, 45)); setv(x, "res", ri(x, 0, 4)); setv(x, "egi", ri(x, 8, 13));
        setv(x, "intv", PICK(0, 0, 12)); setv(x, "lev2", ri(x, 60, 127)); setv(x, "detn", ri(x, 30, 60));
        setv(x, "mgr", ri(x, 5, 8)); setv(x, "mgd", ri(x, 4, 9)); setv(x, "mgi", rf(x, 5, 15));
    } else if (!strcmp(k, "pad")) {
        setv(x, "env1", ri(x, 104, 127)); setv(x, "env2", ri(x, 104, 127)); setv(x, "env3", ri(x, 100, 127));
        setv(x, "cut", ri(x, 40, 75)); setv(x, "res", ri(x, 0, 6)); setv(x, "egi", ri(x, 2, 8));
        setv(x, "intv", PICK(0, 7, 12, 0)); setv(x, "lev2", ri(x, 80, 127)); setv(x, "detn", ri(x, 40, 90));
        setv(x, "pwm1", ri(x, 0, 60)); setv(x, "pwm2", ri(x, 0, 60)); setv(x, "mgr", ri(x, 2, 6));
        setv(x, "mgi", rf(x, 0, 15)); setv(x, "sprd", ri(x, 60, 127)); chorus(x, 80);
    } else if (!strcmp(k, "strings")) {
        setv(x, "har1", ri(x, 0, 40)); setv(x, "har2", ri(x, 0, 40));
        setv(x, "env1", ri(x, 90, 110)); setv(x, "env2", ri(x, 90, 110)); setv(x, "env3", ri(x, 90, 110));
        setv(x, "cut", ri(x, 35, 60)); setv(x, "egi", ri(x, 3, 8)); setv(x, "lev2", ri(x, 100, 127));
        setv(x, "detn", ri(x, 50, 100)); setv(x, "intv", PICK(0, 0, 12));
        setv(x, "mgr", ri(x, 5, 8)); setv(x, "mgd", ri(x, 5, 10)); setv(x, "mgi", rf(x, 8, 20));
        setv(x, "sprd", ri(x, 70, 127)); chorus(x, 90);
    } else {                                          /* total: anything that still sounds */
        setv(x, "oct1", ri(x, -1, 2)); setv(x, "oct2", ri(x, -1, 2)); setv(x, "intv", ri(x, 0, 12));
        setv(x, "detn", ri(x, 0, 127)); setv(x, "lev1", ri(x, 90, 127)); setv(x, "lev2", ri(x, 0, 127));
        setv(x, "env1", ri(x, 0, 127)); setv(x, "env2", ri(x, 0, 127)); setv(x, "env3", ri(x, 0, 127));
        setv(x, "cut", ri(x, 15, 99)); setv(x, "res", ri(x, 0, 15)); setv(x, "egi", ri(x, 0, 15));
        setv(x, "pw1", ri(x, 0, 127)); setv(x, "pw2", ri(x, 0, 127));
        setv(x, "pwm1", ri(x, 0, 80)); setv(x, "pwm2", ri(x, 0, 80));
        if (chance(x, 30)) { setv(x, "sync", 1); setv(x, "swp2", ri(x, 0, 24)); setv(x, "swe", ri(x, 0, 48)); }
        if (chance(x, 30)) setv(x, "ring", ri(x, 20, 110));
        if (chance(x, 30)) { setv(x, "fm1", ri(x, 10, 80)); setv(x, "fmr", PICK(0.5f, 1, 2, 3, 4, 1.5f)); }
        if (chance(x, 20)) { setv(x, "wmx1", ri(x, 20, 80)); setv(x, "wsh1", ri(x, 0, 127)); }
        setv(x, "mgr", ri(x, 0, 15)); setv(x, "mgi", rf(x, 0, 40)); setv(x, "mgv", ri(x, 0, 8));
        setv(x, "drv", ri(x, 0, 80)); setv(x, "nois", chance(x, 15) ? ri(x, 1, 8) : 0);
        if (chance(x, 40)) { setv(x, "dtim", ri(x, 0, 99)); setv(x, "dfb", ri(x, 0, 12)); setv(x, "dmf", ri(x, 0, 31));
                             setv(x, "dmi", ri(x, 0, 31)); setv(x, "dlev", ri(x, 3, 12)); }
    }
}
#undef PICK

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
    for (int i = 0; i < NRANGE && i < 160; i++) {
        x->rep_v[i] = find_value(x, gensym(RANGE[i].name)); x->rep_last[i] = -1e30f;   /* first tick: all (no NaN: -ffast-math on the Bela) */
    }
    x->rep_clk = clock_new(x, (t_method)rep_tick);
    x->canvas = canvas_getcurrent();
    x->syx_ch = 1;
    x->rnd = (unsigned)time(NULL) * 2654435761u | 1;
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

static void pdoly8_free(t_pdoly8 *x)
{
    clock_free(x->rep_clk);
#ifdef P8_OLED
    if (x->o_on) { x->o_quit = 1; pthread_join(x->o_th, 0); close(x->o_sock); x->o_on = 0; }
#endif
    freebytes(x->buf, (size_t)x->bufn * sizeof(float));
}

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
    class_addmethod(pdoly8_class, (t_method)pdoly8_ninfo, gensym("ninfo"), A_SYMBOL, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxin, gensym("syxin"), A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxread, gensym("syxread"), A_SYMBOL, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxwrite, gensym("syxwrite"), A_SYMBOL, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxtemplate, gensym("syxtemplate"), A_SYMBOL, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxbank, gensym("syxbank"), A_GIMME, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxsend, gensym("syxsend"), 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_syxch, gensym("syxch"), A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_txchannel, gensym("txchannel"), A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_random, gensym("random"), A_SYMBOL, A_DEFFLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_disp, gensym("disp"), A_SYMBOL, 0);
#ifdef P8_OLED
    class_addmethod(pdoly8_class, (t_method)pdoly8_oled, gensym("oled"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(pdoly8_class, (t_method)pdoly8_oledtext, gensym("oledtext"), A_GIMME, 0);
#endif
    class_addmethod(pdoly8_class, (t_method)pdoly8_delay, gensym("delay"), A_SYMBOL, 0);
    M1(voiceoff); M1(bend); M0(panic); M0(defaults); M0(reset); M0(print);
}
