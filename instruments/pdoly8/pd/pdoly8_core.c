/* SPDX-License-Identifier: BSD-3-Clause
 * pdoly8_core : Korg Poly-800 tone generation, see pdoly8_core.h
 *
 * The MSM5232 scale ROM (programmable counter value and binary divider per
 * pitch code) is the chip's data as documented in MAME's msm5232.cpp
 * (Jarek Burczynski, Hiromitsu Shioya); the code here is written from the
 * Poly-800 service manual and does not use MAME code.
 */
#include "pdoly8_core.h"
#include <math.h>
#include <string.h>

#define P8_PI       3.14159265f
#define P8_REF_CLK  2119040.0f    /* MSM5232 clock that gives 440 Hz on 8' for code 0x21 */
#define P8_TG_CLK   1059520.0f    /* Poly-800 CLK1/CLK2, about 1 MHz (manual); A4 = code 0x2D.
                                    Measured: 2' sticks to 4' from 16' = 1 kHz up, which pins
                                    this clock, not the 2 MHz one */
#define P8_CODES    85            /* pitch codes 0x00..0x54 */
#define P8_OUT      0.25f         /* output scale before 'level' */
#define P8_HP_HZ    54.0f         /* measured output high-pass, one pole */
#ifndef P8_OUTPOLE
#define P8_OUTPOLE  5000.0f       /* output stage behind the VCF, one pole (fitted to the first recording) */
#endif
#ifndef P8_FC99
#define P8_FC99     20000.0f
#endif
#define P8_MG_LP    14.5f         /* MG -> pitch: one-pole low-pass (fitted: depth falls to 47 % at 19 Hz) */
#define P8_EQ_TR_F0 4256.0f       /* treble shelf corner, fitted (full boost +3 dB at 1.8 kHz; manual: 2 kHz) */
#define P8_EQ_BS_F0 115.0f        /* bass shelf corner, fitted (full boost +3 dB at 316 Hz; manual: 300 Hz) */
#define P8_DDL_LP   7000.0f       /* band limit of the delay path, 2 poles (provisional) */
#define P8_DDL_T0   4.24f         /* delay time at 0, ms: 4 ms in the manual, 6 % longer measured */
#define P8_DDL_FB15 0.70f         /* loop gain at feedback 15 (measured: -3.1 dB per echo) */
#define P8_DDL_GATE 0.004f        /* echo gate threshold (about -48 dB re full scale) */
#define P8_DDL_MF0   0.05f        /* delay mod rate at 0, Hz (measured: 11 -> 0.26 Hz, 21 -> 1.15 Hz; half the manual's 0.1-10 Hz) */
#define P8_NORM_REF 2.1203f       /* sqrt(sum w^2) of the square net, all feet */
#define P8_GATE_MS  1.5f          /* de-click ramp of a voice gate */
#define P8_D        (P8_TAPS / 2)

/* ------------------------------------------------------------ MSM5232 ROM */
/* Code 0 is (506, 7); then 12 semitones per binary-divider step, counters
 * 478 .. 253 (253 = 506 / 2 is the octave). The binary divider n is the
 * counter bit of the 16' output; 8', 4', 2' are bits n-1, n-2, n-3 and stick
 * at bit 0 in the top octaves, as on the chip. */
static const uint16_t P8_CNT[12] = {478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253};

static void rom(int code, float *counter, int *n)
{
    if (code <= 0) { *counter = 506.0f; *n = 7; return; }
    *counter = (float)P8_CNT[(code - 1) % 12];
    *n = 7 - (code - 1) / 12;
}

/* frequency of bit k: the counter steps at 2 * clock / counter (MAME:
 * 2119040 Hz, code 0x21 -> 301, n 5 -> 8' = bit 4 = 440 Hz) */
float p8_code_hz16(int code, float clock_hz)
{
    float c; int n;
    rom(code, &c, &n);
    return 2.0f * clock_hz / c / (float)(2u << n);
}

/* --------------------------------------------------- foot path (har) */
/* 15 knots, each one original foot combination (bits 16' 8' 4' 2'); from one
 * knot to the next exactly one foot fades in or out. The lowest foot never
 * goes back down, and the timbre moves as little as possible: 16' alone ...
 * all four feet in the middle (64) ... 2' alone. */
const uint8_t P8_KNOT[P8_KNOTS] = {0x8, 0xC, 0xE, 0xA, 0xB, 0x9, 0xD, 0xF, 0x7, 0x5, 0x4, 0x6, 0x2, 0x3, 0x1};

static float vcf_cut_oct(float c);
#define P8_RMAX 12                                           /* ring flips per sample and voice */
static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* Musical ring (path 1): the 8 combinations that keep the 16' as root, a
 * closed loop, one foot per step, brightness up and down again: 16', 16'4',
 * 16'4'2', 16'2', 16'8'2', all, 16'8'4', 16'8', and back to 16'. */
static const uint8_t P8_LOOP8[8] = {0x8, 0xA, 0xB, 0x9, 0xD, 0xF, 0xE, 0xC};

/* Figure eight (path 2): the ring through the square net, at 16'8' over to
 * the saw net (only the weights change), the ring backwards, and at 16'
 * (square and saw differ only in level there) back to the start. */
static const uint8_t P8_FIG8[16] = {0x8, 0xA, 0xB, 0x9, 0xD, 0xF, 0xE, 0xC,
                                    0xC, 0xE, 0xF, 0xD, 0x9, 0xB, 0xA, 0x8};

/* One-knob timbre (path 3): the 15 distinct original timbres from dark to
 * bright. Without the 16' a combination is only a transposition (8' 4' 2' =
 * 16' 8' 4' an octave up; reach those with the octave), and 16' alone is the
 * same in both nets, so these are all of them. Saw net first (dark), at
 * 16' 8' over to the square net; one foot or the net changes per step.
 * Brightness (spectral centroid with the measured weights, harmonics of the
 * 16'): 2.7 2.9 3.3 3.1 3.4 3.6 3.3 3.1 | 3.8 5.5 5.9 9.1 8.0 7.8 9.4. */
static const uint8_t P8_TIMB[15] = {0x8, 0x9, 0xB, 0xA, 0xE, 0xF, 0xD, 0xC,     /* saw net */
                                    0xC, 0xE, 0xA, 0xB, 0xF, 0xD, 0x9};         /* square net */

/* position h (0..127) on a knot list: knot a, knot b, fraction t, with a
 * plateau of hsnap steps around each knot; cyclic lists wrap at 127 -> 0 */
static void knot_pos(float h, float hsnap, int n, int cyclic, int *ka, int *kb, float *t)
{
    const float step = 127.0f / (float)(cyclic ? n : n - 1);
    h = clampf(h, 0.0f, 127.0f);
    int seg = (int)(h / step);
    if (cyclic) { if (seg > n - 1) seg = n - 1; }
    else if (seg > n - 2) seg = n - 2;
    float x0 = (float)seg * step;
    float s = clampf(hsnap, 0.0f, step * 0.45f);
    float lo = x0 + s, hi = x0 + step - s;
    if (h <= lo + 1e-3f) *t = 0.0f; else if (h >= hi - 1e-3f) *t = 1.0f; else *t = (h - lo) / (hi - lo);
    *ka = seg; *kb = cyclic ? (seg + 1) % n : seg + 1;
}

/* Walsh morph: the 15 products of the feet (Walsh functions, foot subsets
 * as in P8_KNOT) on a closed loop by sequency, i.e. by where their energy
 * lies (harmonic 1 ... 8 of the 16'): up through one of each pair, down
 * through the other, neighbours one harmonic apart. */
const uint8_t P8_WALSH[15] = {0x8, 0x4, 0xA, 0x2, 0xB, 0x5, 0x9, 0x1, 0xD, 0x7, 0xF, 0x3, 0xE, 0x6, 0xC};

void p8_path_gains(int path, float h, float hsnap, int pfade, float g[P8_FEET], float *net)
{
    const uint8_t *k = path == 1 ? P8_LOOP8 : path == 2 ? P8_FIG8 : path == 3 ? P8_TIMB : P8_KNOT;
    int n = path == 1 ? 8 : path == 2 ? 16 : path == 3 ? 15 : P8_KNOTS, a, b;
    float t;
    knot_pos(h, hsnap, n, path == 1 || path == 2, &a, &b, &t);
    for (int f = 0; f < P8_FEET; f++) {
        float fa = (float)((k[a] >> (3 - f)) & 1), fb = (float)((k[b] >> (3 - f)) & 1);
        float x = fa + t * (fb - fa);
        g[f] = pfade ? sqrtf(x) : x;                         /* sqrt: the fading foot's power is linear */
    }
    if (net) {                                               /* paths 2, 3: the net is part of the path (1 = saw) */
        float na, nb;
        if (path == 3) { na = (float)(a < 8); nb = (float)(b < 8); }
        else { na = (float)(a >= 8); nb = (float)(b >= 8); }
        *net = path >= 2 ? na + t * (nb - na) : -1.0f;
    }
}

void p8_foot_gains(float h, float hsnap, float g[P8_FEET])
{
    p8_path_gains(0, h, hsnap, 0, g, 0);
}

/* ------------------------------------------------ band-limited impulses */
/* Kaiser-windowed sinc, 32 taps, cut-off 0.42 fs, rows for 64 sub-sample
 * positions (+1 for interpolation), each row normalised to sum 1 so that an
 * integrated impulse is an exact step. */
static float H[P8_PHASES + 1][P8_TAPS];
static int H_ready;

static float bessel_i0(float x)
{
    float s = 1.0f, t = 1.0f, q = x * x * 0.25f;
    for (int k = 1; k < 30; k++) { t *= q / (float)(k * k); s += t; if (t < 1e-9f * s) break; }
    return s;
}

static void blep_init(void)
{
    if (H_ready) return;
    const float fc = 0.42f, beta = 8.0f, i0b = bessel_i0(beta);
    for (int p = 0; p <= P8_PHASES; p++) {
        float sum = 0.0f, tau = (float)p / (float)P8_PHASES;
        for (int k = 0; k < P8_TAPS; k++) {
            float x = (float)k - (float)P8_D + tau;              /* event tau samples ago */
            float r = x / (float)P8_D, w = 0.0f;
            if (r > -1.0f && r < 1.0f) w = bessel_i0(beta * sqrtf(1.0f - r * r)) / i0b;
            float a = 2.0f * P8_PI * fc * x;
            float s = fabsf(x) < 1e-6f ? 2.0f * fc : sinf(a) / (P8_PI * x);
            H[p][k] = s * w; sum += H[p][k];
        }
        for (int k = 0; k < P8_TAPS; k++) H[p][k] /= sum;
    }
    H_ready = 1;
}

/* a sharp step d, tau samples ago (0..1), into both channels */
static void step_sharp(p8_engine_t *e, float dl, float dr, float tau)
{
    float p = tau * (float)P8_PHASES;
    int ip = (int)p;
    if (ip >= P8_PHASES) ip = P8_PHASES - 1;
    float fr = p - (float)ip;
    const float *h0 = H[ip], *h1 = H[ip + 1];
    float *rl = e->ring[0], *rr = e->ring[1];
    for (int k = 0; k < P8_TAPS; k++) {
        float h = h0[k] + fr * (h1[k] - h0[k]);
        unsigned j = (e->rpos + (unsigned)k) & (P8_RING - 1);
        rl[j] += dl * h; rr[j] += dr * h;
    }
}

/* a smooth change (gain ramps): no edge, so no impulse needed, same delay */
static void step_smooth(p8_engine_t *e, float dl, float dr)
{
    unsigned j = (e->rpos + P8_D) & (P8_RING - 1);
    e->ring[0][j] += dl; e->ring[1][j] += dr;
}

/* ------------------------------------------------------------- setup */
static void deg_coeffs(p8_engine_t *e, int g);

void p8_set_smooth(p8_engine_t *e, float ms)
{
    e->smooth_ms = ms < 0.0f ? 0.0f : ms;
    e->ksm = e->smooth_ms <= 0.0f ? 1.0f : 1.0f - expf(-1.0f / (e->smooth_ms * 0.001f * e->fs));
}

void p8_set_fs(p8_engine_t *e, float fs)
{
    e->fs = fs > 1000.0f ? fs : 44100.0f;
    e->dc_k = 1.0f - 2.0f * P8_PI * 5.0f / e->fs;
    e->hp_k = 1.0f / (1.0f + 2.0f * P8_PI * P8_HP_HZ / e->fs);
    {   float g = tanf(P8_PI * P8_OUTPOLE / e->fs); e->op_g = g / (1.0f + g); }
    p8_set_smooth(e, e->smooth_ms);
    p8_set_port(e, e->port_s);
    for (int g = 0; g < 3; g++) deg_coeffs(e, g);
    p8_set_eq(e, e->eq_tr, e->eq_bs);
    {   float g = tanf(P8_PI * P8_DDL_LP / e->fs); e->ddl_lpg = g / (1.0f + g); }
    e->ddl_rel = expf(-1.0f / (0.02f * e->fs));                    /* gate envelope: 20 ms release */
    p8_set_ddl(e, e->ddl_time, e->ddl_fb, e->ddl_mf, e->ddl_mi, e->ddl_lev);
}

/* ------------------------------------------------------ EQ and delay */
/* EQ (Poly-800 II, 76/77): treble and bass -6..+6 in 13 steps. Measured
 * (noise through the EQ): first-order shelves with fixed corners, gain per
 * step not linear: about 4 dB at +-3 and 10 dB at +-6 (1.10 k + 0.089 k^2). */
static float eq_db(float k) { float a = fabsf(k), g = 1.10f * a + 0.089f * a * a; return k < 0.0f ? -g : g; }
static void shelf(float *c, float fs, float f0, float gdb, int high)
{
    float G = powf(10.0f, gdb / 20.0f);
    float K = tanf(P8_PI * f0 / fs);
    /* high: H(s) = (G s + w0) / (s + w0); low: H(s) = (s + G w0) / (s + w0); cut = 1 / boost */
    int cut = G < 1.0f; if (cut) G = 1.0f / G;
    float b0, b1, a0, a1;
    if (high) { b0 = G + K; b1 = K - G; } else { b0 = 1.0f + G * K; b1 = G * K - 1.0f; }
    a0 = 1.0f + K; a1 = K - 1.0f;
    if (cut) { float t0 = b0, t1 = b1; b0 = a0; b1 = a1; a0 = t0; a1 = t1; }
    c[0] = b0 / a0; c[1] = b1 / a0; c[2] = a1 / a0;
}

static void eq_coeffs(p8_engine_t *e)
{
    shelf(e->eq_c[0], e->fs, P8_EQ_TR_F0, eq_db(e->sm.tr), 1);
    shelf(e->eq_c[1], e->fs, P8_EQ_BS_F0, eq_db(e->sm.bs), 0);
}

void p8_set_eq(p8_engine_t *e, float treble, float bass)
{
    e->eq_tr = clampf(treble, -6.0f, 6.0f); e->eq_bs = clampf(bass, -6.0f, 6.0f);
    eq_coeffs(e);                                            /* the render loop follows the smoothed values */
}

/* DDL (service manual): time 0..99 = 4..1024 ms, feedback 0..15 = 0..100 %,
 * mod frequency 0..31 = 0.1..10 Hz, mod intensity 0..31, effect level
 * 0..15 (15 = as loud as the direct sound). Curves provisional (exponential
 * in time and rate) until measured. */
void p8_set_ddl(p8_engine_t *e, float time, float fb, float mf, float mi, float lev)
{
    e->ddl_time = clampf(time, 0.0f, 99.0f); e->ddl_fb = clampf(fb, 0.0f, 15.0f);
    e->ddl_mf = clampf(mf, 0.0f, 31.0f); e->ddl_mi = clampf(mi, 0.0f, 31.0f); e->ddl_lev = clampf(lev, 0.0f, 15.0f);
    e->ddl_ms = P8_DDL_T0 * exp2f(8.0f * e->ddl_time / 99.0f);      /* measured: 70 -> 216 ms, 99 -> 1.08 s */
    /* mod depth: the time swings 1:5 at 31 (manual, measured); below that the
     * swing falls by about 10 dB per 10 steps (measured at 21 and 11); 0 = none */
    e->ddl_g = (exp2f((e->ddl_mi - 31.0f) * 0.33219f) - 0.000794f) * (1.0f / 0.999206f);
    if (!e->ddl_glide) e->ddl_mute = 0.0f; /* "delay sound output is momentarily interrupted" */
}

/* soft clipping (rational, as in cz4~), exact +-1 from |x| = 3 */
static inline float softclip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/* overdrive before or after the VCF: gain 1 .. 256 (+48 dB, quadratic), the makeup
 * keeps a signal at half the reference level at its level */
#define P8_DRV_REF 3.0f                                      /* one saw note, all feet: peak about 3 */
void p8_set_drive(p8_engine_t *e, float d)
{
    e->drive = clampf(d, 0.0f, 1.0f);
    e->drv_g = 1.0f + 255.0f * e->drive * e->drive;
    e->drv_c = 0.5f / softclip(0.5f * e->drv_g);
}

void p8_init(p8_engine_t *e, float fs)
{
    blep_init();
    memset(e, 0, sizeof *e);
    e->a4 = 440.0f;
    e->mode = P8_DOUBLE;
    e->wav[0] = e->wav[1] = 1.0f;
    e->har[0] = e->har[1] = 64.0f;
    e->lev[0] = 1.0f;
    e->hsnap = 1.0f;
    e->rm_mask = 0x8;
    e->env[0] = e->env[1] = e->env[2] = 0.0f;
    e->trig3 = 2; e->vcf_c = 99.0f; e->kbd_note = 60.0f;
    e->ddl_ms = e->ddl_ms_s = 4.0f;
    e->fm_ratio = 1.0f;
    e->pw[0] = e->pw[1] = e->pw_t[0] = e->pw_t[1] = 0.5f;
    e->pfade = 1;
    e->smooth_ms = 25.0f;
    e->spread = 0.5f; e->level = 0.5f;
    e->tone = 1;
    e->mg_hz = 5.0f;
    e->rng = 0x2545F491u;
    p8_set_drive(e, 0.0f);
    p8_set_fs(e, fs);
    for (int t = 0; t < P8_TG; t++) {
        e->tg[t].cnt = (uint32_t)(t * 37u);                  /* chip counters are not in step */
        e->tg[t].pan_l = e->tg[t].pan_r = e->tg[t].pan_lt = e->tg[t].pan_rt = 0.70710678f;
        e->tg[t].vg = 1.0f; e->tg[t].dfac = 1.0f; e->tg[t].rs = 1.0f;
    }
    e->vel_last = 1.0f;
    e->deg3.rs = 1.0f;
    e->drng = 0x9E3779B9u;
    p8_update(e);
    p8_settle(e);
}

/* ------------------------------------------------------- DEG (envelopes) */
/* Measured on a Poly-800 (one DCO, one key, line out; see README):
 * - level 0..31 -> amplitude: the square root of the amplitude is close to
 *   linear in the level (amplitude about (L/31)^2, a little lower below 12);
 * - attack: the level rises linearly; time for 0 -> 31 per attack value;
 * - decay, slope (down) and release: exponential, a constant dB per second;
 *   time for -40 dB per value; decay and release share one table;
 * - a released voice does not close completely: it stays about -55 dB
 *   (the 'leak' setting). */
static const float DEG_SQ[9] = {0.0f, 0.095f, 0.2356f, 0.3764f, 0.5133f, 0.6477f, 0.778f, 0.9056f, 1.0f};
static const float DEG_TA[9] = {0.010f, 0.031f, 0.078f, 0.20f, 0.465f, 1.06f, 2.30f, 4.92f, 8.98f};
static const float DEG_T40[9] = {0.06f, 0.085f, 0.135f, 0.29f, 0.625f, 1.39f, 3.3f, 6.9f, 18.5f};
#define DEG_LEAK 0.00178f                                    /* -55 dB */

static float deg_pos(float v, int *i)                        /* table points at 0,4,...,28,31 */
{
    v = clampf(v, 0.0f, 31.0f);
    if (v >= 28.0f) { *i = 7; return (v - 28.0f) / 3.0f; }
    *i = (int)(v / 4.0f);
    return (v - 4.0f * (float)*i) / 4.0f;
}
static float deg_sqrt(float L) { int i; float t = deg_pos(L, &i); return DEG_SQ[i] + t * (DEG_SQ[i + 1] - DEG_SQ[i]); }
static float deg_amp(float L) { float q = deg_sqrt(L); return q * q; }
static float deg_level(float a)                              /* inverse of deg_amp */
{
    float q = sqrtf(a > 0.0f ? a : 0.0f);
    if (q >= 1.0f) return 31.0f;
    for (int i = 0; i < 8; i++)
        if (q <= DEG_SQ[i + 1]) {
            float x0 = (float)(4 * i), x1 = i == 7 ? 31.0f : (float)(4 * i + 4);
            return x0 + (q - DEG_SQ[i]) / (DEG_SQ[i + 1] - DEG_SQ[i]) * (x1 - x0);
        }
    return 31.0f;
}
static float deg_time(const float *tab, float v)             /* log interpolation */
{
    int i; float t = deg_pos(v, &i);
    return tab[i] * powf(tab[i + 1] / tab[i], t);
}

/* One-knob morph: 0 is a plain gate; from 1 on a line through Poly-800 DEG
 * settings (attack, decay, break point, slope, sustain, release, 0..31),
 * interpolated parameter by parameter: very short pluck -> pluck -> piano ->
 * long release -> swell -> pad -> long pad. No fade between gate and the
 * short pluck. Pluck, piano and long release get the widest stretches; from
 * swell on attack and release only grow. */
#define DEG_KNOTS 8
static const float DEG_KPOS[DEG_KNOTS] = {0, 1, 24, 48, 72, 92, 108, 127};
static const float DEG_KNOT[DEG_KNOTS][6] = {
    { 0,  0, 31,  0, 31,  0},                                /* 0   gate (alone) */
    { 0,  3,  0,  0,  0,  3},                                /* 1   very short pluck */
    { 0, 14,  0,  0,  0, 12},                                /* 24  pluck */
    { 0, 10, 20, 24,  0, 14},                                /* 48  piano: fast to 20, slow slope */
    { 2, 16, 24,  0, 24, 26},                                /* 72  long release */
    {16,  0, 31,  0, 31, 18},                                /* 92  swell */
    {20, 22, 26,  0, 26, 24},                                /* 108 pad */
    {30, 26, 26,  0, 26, 31},                                /* 127 long pad */
};

void p8_env_morph(float pos, float hsnap, float deg[6])
{
    pos = clampf(pos, 0.0f, 127.0f);
    if (pos < 0.5f) { for (int k = 0; k < 6; k++) deg[k] = DEG_KNOT[0][k]; return; }   /* gate, no fade */
    if (pos < 1.0f) pos = 1.0f;
    int a = 1;
    while (a < DEG_KNOTS - 2 && pos > DEG_KPOS[a + 1]) a++;
    float x0 = DEG_KPOS[a], x1 = DEG_KPOS[a + 1];
    float sn = clampf(hsnap, 0.0f, 0.45f * (x1 - x0));
    float lo = a == 1 ? x0 : x0 + sn, hi = a + 1 == DEG_KNOTS - 1 ? x1 : x1 - sn;   /* plateau around each knot */
    float t = pos <= lo ? 0.0f : pos >= hi ? 1.0f : (pos - lo) / (hi - lo);
    for (int k = 0; k < 6; k++) deg[k] = DEG_KNOT[a][k] + t * (DEG_KNOT[a + 1][k] - DEG_KNOT[a][k]);
}

/* per-sample coefficients of a group from its six DEG values */
static void deg_coeffs(p8_engine_t *e, int g)
{
    float d[6];
    for (int k = 0; k < 6; k++) d[k] = e->deg[g][k];
    if (g < 2) d[0] = clampf(d[0] + e->perf_att, 0.0f, 31.0f);            /* attack all */
    if (g < 3) d[5] = clampf(d[5] + e->perf_rel, 0.0f, 31.0f);            /* release all */
    p8_deg_t *c = &e->dc[g];
    float fs = e->fs;
    c->att = 31.0f / (deg_time(DEG_TA, d[0]) * fs);
    c->dec = powf(10.0f, -2.0f / (deg_time(DEG_T40, d[1]) * fs));      /* -40 dB in T40 */
    c->sl_dn = powf(10.0f, -2.0f / (deg_time(DEG_T40, d[3]) * fs));
    c->sl_up = 31.0f / (deg_time(DEG_TA, d[3]) * fs);
    c->rel = powf(10.0f, -2.0f / (deg_time(DEG_T40, d[5]) * fs));
    c->bp = deg_amp(d[2]); c->su = deg_amp(d[4]);
    c->bp_off = d[2] >= 30.999f;                             /* break point 31: plain ADSR via slope */
}

/* one sample of a tone generator's envelope; returns the amplitude */
static float deg_tick(p8_engine_t *e, p8_tg_t *g, int grp)
{
    const p8_deg_t *c = &e->dc[grp];
    const float rs = g->rs;                                  /* drift: c^rs ~ 1 - rs (1 - c) */
    int gate = g->amp_target > 0.0f;
    if (g->trig || (gate && !g->gate)) { g->stage = 1; g->elv = deg_level(g->amp); g->trig = 0; }
    else if (!gate && g->gate) g->stage = 5;
    g->gate = gate;
    float a = g->amp;
    switch (g->stage) {
    case 1:                                                  /* attack: level rises linearly */
        g->elv += c->att * rs;
        if (g->elv >= 31.0f) { g->elv = 31.0f; g->stage = c->bp_off ? 3 : 2; }
        a = deg_amp(g->elv);
        break;
    case 2:                                                  /* decay to the break point */
        a *= 1.0f - rs * (1.0f - c->dec);
        if (a <= c->bp) { a = c->bp; g->stage = 3; }
        break;
    case 3:                                                  /* slope to the sustain level */
        if (c->su < a) { a *= 1.0f - rs * (1.0f - c->sl_dn); if (a <= c->su) { a = c->su; g->stage = 4; } }
        else if (c->su > a) {
            float L = deg_level(a) + c->sl_up * rs, Ls = deg_level(c->su);
            if (L >= Ls) { a = c->su; g->stage = 4; } else a = deg_amp(L);
        } else g->stage = 4;
        break;
    case 4:                                                  /* sustain: follows changes smoothly */
        a += 0.002f * (c->su - a);
        break;
    case 5: {                                                /* release, exponential */
        float floor_ = e->leak ? DEG_LEAK : 0.0f;
        a *= 1.0f - rs * (1.0f - c->rel);
        if (a <= floor_ || a < 1e-6f) { a = floor_; g->stage = floor_ > 0.0f ? 6 : 0; }
        break;
    }
    default: break;                                          /* 0 idle, 6 leaking */
    }
    g->amp = a;
    return a;
}

/* per-foot target gains of both groups: wave weights x foot gains x level */
/* Walsh morph targets of DCO g at loop position pos (wraps) */
static void walsh_targets(p8_engine_t *e, int g, float pos)
{
    for (int S = 0; S < 16; S++) e->Wct[g][S] = 0.0f;
    if (e->wlev[g] <= 0.0f) return;
    pos = fmodf(pos, 127.0f); if (pos < 0.0f) pos += 127.0f;
    int wa, wb; float wt;
    knot_pos(pos, e->hsnap, 15, 1, &wa, &wb, &wt);
    e->Wct[g][P8_WALSH[wa]] += e->wlev[g] * sqrtf(1.0f - wt);
    e->Wct[g][P8_WALSH[wb]] += e->wlev[g] * sqrtf(wt);
    e->wal[g] = 1;
}

/* foot weight targets of DCO g at timbre position h (and its Walsh level) */
static void foot_targets(p8_engine_t *e, int g, float h, float dmix, float dlev)
{
    static const float SQ[P8_FEET]  = {1.0f, 1.057f, 1.083f, 1.098f};
    static const float SAW[P8_FEET] = {2.47f, 2.47f * 0.523f, 2.47f * 0.268f, 2.47f * 0.135f};
    float gain[P8_FEET], net, wv = clampf(e->wav[g], 0.0f, 1.0f);
    p8_path_gains(e->path, h, e->hsnap, e->pfade, gain, &net);
    if (net >= 0.0f) wv = net;
    float lev = (g == 1 && e->mode != P8_DOUBLE) ? 0.0f : clampf(e->lev[g] + dlev, 0.0f, 1.0f);
    float w[P8_FEET], pw = 0.0f;
    for (int f = 0; f < P8_FEET; f++) { w[f] = (SQ[f] + wv * (SAW[f] - SQ[f])) * gain[f]; pw += w[f] * w[f]; }
    /* loudness: the feet are orthogonal (Rademacher functions), so the
     * RMS is exactly sqrt(sum w^2) / 2; norm 1 holds it at the level of
     * the square net with all feet, which also bounds the peak there */
    float k = 1.0f;
    if (e->norm > 0.0f && pw > 1e-9f) {
        float wd = clampf(e->pw_t[g], 0.03f, 0.97f), duty = 4.0f * wd * (1.0f - wd);   /* pulse power / square power */
        k = powf(P8_NORM_REF / sqrtf(pw * duty), clampf(e->norm, 0.0f, 1.0f));
    }
    float mix = clampf(e->wmix[g] + dmix, 0.0f, 1.0f);
    for (int f = 0; f < P8_FEET; f++) e->Wt[g][f] = w[f] * k * lev * (1.0f - mix);
    /* Walsh morph: two neighbours on the sequency loop, equal power
     * (Walsh functions are orthonormal), at the RMS of the square net
     * with all feet */
    e->wlev[g] = 0.5f * P8_NORM_REF * mix * lev;
}

void p8_update(p8_engine_t *e)
{
    /* Foot weights measured on a Poly-800 (square / saw, one key over five
     * octaves, response of the output removed): square net (BR5) a little
     * stronger on the higher feet; saw net (BR6) binary within 1 dB and its
     * 16' 2.47 x (+7.9 dB) the square net's, so both nets peak alike. */
    for (int g = 0; g < 2; g++) {
        foot_targets(e, g, e->har[g], 0.0f, 0.0f);
        walsh_targets(e, g, e->wpos[g]);
    }
    if (e->mode != P8_DOUBLE) for (int t = 4; t < P8_TG; t++) e->tg[t].amp_target = 0.0f;
    for (int g = 0; g < 3; g++) {
        if (e->env[g] >= 0.0f) p8_env_morph(e->env[g], e->hsnap, e->deg[g]);
        deg_coeffs(e, g);
    }
    /* stereo position of the 4 voices (both DCOs of a voice together) */
    static const float POS[P8_VOICES] = {-1.0f, -0.3333333f, 0.3333333f, 1.0f};
    for (int k = 0; k < P8_VOICES; k++) {
        float a = (clampf(e->spread, 0.0f, 1.0f) * POS[k] + 1.0f) * (P8_PI * 0.25f);
        for (int t = k; t < P8_TG; t += 4) { e->tg[t].pan_lt = cosf(a); e->tg[t].pan_rt = sinf(a); }
    }
}

/* jump the gain smoothing to its targets (preset recall, before notes) */
void p8_settle(p8_engine_t *e)
{
    for (int g = 0; g < 2; g++) for (int f = 0; f < P8_FEET; f++) e->W[g][f] = e->Wt[g][f];
    for (int g = 0; g < 2; g++) for (int S = 0; S < 16; S++) e->Wc[g][S] = e->Wct[g][S];
    for (int g = 0; g < 2; g++) e->pw[g] = e->pw_t[g];
    e->sm.cut = vcf_cut_oct(e->vcf_c); e->sm.res = e->vcf_r;   /* key tracking follows within mrph */
    e->sm.fm[0] = e->fm; e->sm.fm[1] = e->fm2; e->sm.ring = e->rmod; e->sm.nois = e->noise_level;
    e->sm.det = e->detune_cents; e->sm.eg = e->egint; e->sm.swp = e->swp2; e->sm.mgc = e->mg_cents;
    e->sm.mgv = e->mg_vcf; e->sm.fmhz = e->fm_hz; e->sm.fmr = log2f(e->fm_ratio > 0.0f ? e->fm_ratio : 1.0f);
    e->sm.drv = e->drive; e->sm.dfb = e->ddl_fb; e->sm.dlev = e->ddl_lev;
    e->sm.swe = e->sw_e; e->sm.swm = e->sw_m; e->sm.pwm[0] = e->pwm_d[0]; e->sm.pwm[1] = e->pwm_d[1];
    e->sm.hm = e->h_m; e->sm.wsm = e->ws_m; e->sm.wse = e->ws_e; e->sm.fmig = e->fm_ig;
    e->sm.tr = e->eq_tr; e->sm.bs = e->eq_bs; eq_coeffs(e);
    for (int t = 0; t < P8_TG; t++) { e->tg[t].pan_l = e->tg[t].pan_lt; e->tg[t].pan_r = e->tg[t].pan_rt; }
    e->ddl_ms_s = e->ddl_ms;
}

static void tg_pitch(p8_engine_t *e, int t)
{
    p8_tg_t *g = &e->tg[t];
    int grp = t >= 4;
    /* octaves -1..1 are the chip's LOW / MID / HIGH; above that the feet are
     * taken one counter bit higher per octave, exactly what leaving out the
     * 16' does on the original (8' 4' 2' = 16' 8' 4' an octave up), with the
     * chip's clamp at bit 0 in the top octaves */
    int oc = e->octave[grp], ex = oc > 1 ? oc - 1 : 0;
    if (oc > 1) oc = 1;
    int code = (int)lroundf(g->note) - 24 + 12 * oc + (grp ? e->interval : 0);
    while (code < 0) code += 12;
    while (code > P8_CODES - 1) code -= 12;
    g->code = code;
    int n;
    rom(code, &g->counter, &n);
    for (int f = 0; f < P8_FEET; f++) g->bit[f] = n - f - ex < 0 ? 0 : n - f - ex;
}

void p8_voice_on(p8_engine_t *e, int k, float note)
{
    p8_voice_glide(e, k, note, -1.0f);
}

/* drift random numbers, uniform -1..1 */
static float drand(p8_engine_t *e)
{
    e->drng = e->drng * 1664525u + 1013904223u;
    return (float)(e->drng >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

/* Portamento (not on the Poly-800): the counters jump to the new divider at
 * once, as on the chip, and a pitch offset in cents from the old note to
 * the new one fades out exponentially on top of the clock, so the glide
 * ends exactly on the chip's own pitch. from < 0: no glide. */
void p8_voice_glide(p8_engine_t *e, int k, float note, float from)
{
    if (k < 0 || k >= P8_VOICES) return;
    p8_voice_pitch(e, k, note, from);
    if (e->drift > 0.0f) {                                   /* drift: each note's envelopes a little faster or slower */
        for (int t = k; t < P8_TG; t += 4) e->tg[t].rs = 1.0f + 0.2f * e->drift * drand(e);
        if (e->trig3 == 2 || e->deg3.stage == 0) e->deg3.rs = 1.0f + 0.2f * e->drift * drand(e);
    } else { e->tg[k].rs = e->tg[k + 4].rs = 1.0f; e->deg3.rs = 1.0f; }
    e->tg[k].amp_target = 1.0f; e->tg[k].trig = 1;
    e->tg[k + 4].amp_target = e->mode == P8_DOUBLE ? 1.0f : 0.0f;
    e->tg[k + 4].trig = e->mode == P8_DOUBLE;
    if (e->trig3 == 2) e->deg3.trig = 1;                    /* multi: every key restarts DEG3 */
}

/* new pitch for a sounding voice without retriggering its envelopes
 * (mono legato); glides from 'from' if portamento is on */
void p8_voice_pitch(p8_engine_t *e, int k, float note, float from)
{
    if (k < 0 || k >= P8_VOICES) return;
    for (int t = k; t < P8_TG; t += 4) {
        p8_tg_t *g = &e->tg[t];
        float c_old = 0.0f;
        int glide = from >= 0.0f && e->port_k < 1.0f;
        if (glide) { g->note = from; tg_pitch(e, t); c_old = (float)g->bit[0] + log2f(g->counter); }
        g->note = note; tg_pitch(e, t);
        /* 16' frequency ~ 1 / (counter * 2^n): the offset in cents from the old pitch */
        g->glide = glide ? 1200.0f * (((float)g->bit[0] + log2f(g->counter)) - c_old) : 0.0f;
    }
}

/* velocity of voice k (0..1): its level (vel_amp) and, for the one VCF,
 * the cutoff (vel_cut); full velocity leaves both as set */
void p8_voice_vel(p8_engine_t *e, int k, float vel)
{
    if (k < 0 || k >= P8_VOICES) return;
    vel = clampf(vel, 0.0f, 1.0f);
    float g = 1.0f - clampf(e->vel_amp, 0.0f, 1.0f) * (1.0f - vel);
    e->tg[k].vg = e->tg[k + 4].vg = g;
    e->vel_last = vel;
}

void p8_set_port(p8_engine_t *e, float sec)
{
    e->port_s = sec < 0.0f ? 0.0f : sec;
    /* exponential, about 95 % of the way after sec */
    e->port_k = e->port_s <= 0.0f ? 1.0f : expf(-3.0f / (e->port_s * e->fs));
}

void p8_voice_off(p8_engine_t *e, int k)
{
    if (k < 0 || k >= P8_VOICES) return;
    e->tg[k].amp_target = 0.0f; e->tg[k + 4].amp_target = 0.0f;
}

int p8_voice_sounding(const p8_engine_t *e, int k)
{
    return e->tg[k].amp_target > 0.0f || e->tg[k + 4].amp_target > 0.0f;
}

void p8_retune(p8_engine_t *e)
{
    for (int t = 0; t < P8_TG; t++) tg_pitch(e, t);
    for (int k = 0; k < P8_VOICES; k++)                      /* mode change: DCO2 follows DCO1's gate */
        e->tg[k + 4].amp_target = (e->mode == P8_DOUBLE && e->tg[k].amp_target > 0.0f) ? 1.0f : 0.0f;
}

/* ------------------------------------------------------------ render */
/* How a tone generator's foot states become its output: the foot sum
 * (square or pulse levels, weights W) plus, with the Walsh morph, a sum of
 * Walsh functions (products of the feet, coefficients Wc). */
typedef struct {
    const float *W;              /* foot weights */
    const float *Wc;             /* Walsh coefficients by foot subset, 16 */
    int wal;                     /* Walsh part active */
    float w;                     /* pulse width, 0.5 = square */
    float rm;                    /* ring modulation factor (DCO2), 1 = none */
} shape_t;

/* h: foot states, bit (3 - f) set = foot f high (16' = 0x8 ... 2' = 0x1) */
static float tg_eval(const p8_tg_t *g, const shape_t *sh, unsigned h)
{
    const float hi = 1.0f - sh->w, lo = -sh->w;
    float s = 0.0f;
    for (int f = 0; f < P8_FEET; f++) s += sh->W[f] * (((h >> (3 - f)) & 1u) ? hi : lo);
    if (sh->wal) {
        /* product over subset S of +-1 = (-1)^(number of low feet in S) */
        unsigned low = ~h & 0xFu;
        for (unsigned S = 1; S < 16; S++) {
            float c = sh->Wc[S];
            if (c != 0.0f) s += (__builtin_popcount(S & low) & 1) ? -c : c;
        }
    }
    return s * g->amp * g->vg * sh->rm;
}

/* foot states from the counter: square feet are its bits; pulse feet are
 * high in the last w of their period (the falling edge stays on the
 * counter step: the AND of the foot with a shifted copy) */
static unsigned tg_states(const p8_tg_t *g, float w)
{
    unsigned h = 0;
    for (int f = 0; f < P8_FEET; f++) {
        uint32_t P = 2u << g->bit[f];
        float local = (float)(g->cnt & (P - 1u)) + g->ph;
        h |= (unsigned)(local >= (1.0f - w) * (float)P) << (3 - f);
    }
    return h;
}

static unsigned tg_bits(const p8_tg_t *g)
{
    unsigned h = 0;
    for (int f = 0; f < P8_FEET; f++) h |= ((g->cnt >> g->bit[f]) & 1u) << (3 - f);
    return h;
}

/* Advance a tone generator by span samples, ending tend samples before the
 * end of the current sample, and place every change as a band-limited step.
 * generic 0: the plain counter (inc > 0, square feet); generic 1: every foot
 * as a pulse of width w, either direction (through-zero FM); foot edges are
 * sorted in time, so products of feet (Walsh) change correctly. Returns the
 * time (tau) of the first forward wrap of the 16' period, or -1. */
static float tg_run(p8_engine_t *e, p8_tg_t *g, const shape_t *sh, int generic,
                    float inc, float span, float tend, float *t16, int *n16)
{
    float wrap = -1.0f;
    const float d = inc * span;
    if (span <= 0.0f) return wrap;
    if (!generic) {
        const uint32_t P16 = 2u << g->bit[0];
        g->ph += d;
        while (g->ph >= 1.0f) {
            g->ph -= 1.0f;
            g->cnt++;
            float tau = tend + g->ph / inc;
            if (tau > tend + span) tau = tend + span;
            if (wrap < 0.0f && (g->cnt & (P16 - 1u)) == 0u) wrap = tau;
            if (t16 && *n16 < P8_RMAX) {                     /* ring: the product of the masked feet flips */
                uint32_t ch = g->cnt ^ (g->cnt - 1u); int par = 0;
                for (int f = 0; f < P8_FEET; f++) if ((e->rm_mask >> (3 - f)) & 1u) par ^= (int)((ch >> g->bit[f]) & 1u);
                if (par) t16[(*n16)++] = tau;
            }
            float vn = tg_eval(g, sh, tg_bits(g));
            if (vn != g->v) { float dv = vn - g->v; step_sharp(e, dv * g->pan_l, dv * g->pan_r, tau); g->v = vn; }
        }
        return wrap;
    }
    if (d == 0.0f) return wrap;
    /* collect the foot edges of this span */
    struct { float tau; int f, up; } ev[24];
    int nev = 0;
    unsigned h = tg_states(g, sh->w);
    for (int f = 0; f < P8_FEET; f++) {
        const uint32_t P = 2u << g->bit[f];
        const float Pf = (float)P, x0 = (float)(g->cnt & (P - 1u)) + g->ph, x1 = x0 + d;
        const float r = (1.0f - sh->w) * Pf;
        const float lo = d > 0.0f ? x0 : x1, hi = d > 0.0f ? x1 : x0;
        for (int kk = (int)floorf(lo / Pf) - 1; (float)kk * Pf <= hi + Pf && nev < 22; kk++) {
            float pr = (float)kk * Pf + r, pf = (float)kk * Pf;
            if (pr > lo && pr <= hi) { ev[nev].tau = tend + span * (1.0f - (pr - x0) / d); ev[nev].f = f; ev[nev].up = d > 0.0f; nev++; }
            if (pf > lo && pf <= hi) {
                float tau = tend + span * (1.0f - (pf - x0) / d);
                ev[nev].tau = tau; ev[nev].f = f; ev[nev].up = d < 0.0f; nev++;
                if (f == 0 && d > 0.0f && tau > wrap) wrap = tau;
            }
        }
    }
    /* in time order (largest tau first), each edge sets its foot */
    for (int i = 1; i < nev; i++)
        for (int j = i; j > 0 && ev[j].tau > ev[j - 1].tau; j--) { __typeof__(ev[0]) tmp = ev[j]; ev[j] = ev[j - 1]; ev[j - 1] = tmp; }
    for (int i = 0; i < nev; i++) {
        unsigned bit = 1u << (3 - ev[i].f);
        unsigned h0 = h;
        h = ev[i].up ? (h | bit) : (h & ~bit);
        if (t16 && ((e->rm_mask >> (3 - ev[i].f)) & 1u) && h != h0 && *n16 < P8_RMAX) t16[(*n16)++] = ev[i].tau;
        float vn = tg_eval(g, sh, h);
        if (vn != g->v) { float dv = vn - g->v; step_sharp(e, dv * g->pan_l, dv * g->pan_r, ev[i].tau); g->v = vn; }
    }
    float total = g->ph + d, fl = floorf(total);
    g->cnt += (uint32_t)(int32_t)fl;
    g->ph = total - fl;
    if (g->ph >= 1.0f) { g->ph -= 1.0f; g->cnt++; }
    return wrap;
}

/* VCF, measured on a Poly-800 (noise through the filter, see
 * docs/vcf-calibration.md): corner of the 4-pole low-pass per cutoff value
 * 0, 10, ... 90, 99, and the ladder feedback per resonance 0, 4, 8, 12, 15.
 * The top value (99) is set by the first recording (open filter), where
 * a softer top fits: 4 poles at about 20 kHz plus the output pole at 5 kHz.
 * DEG3 at EG INT 15 and full level moves the cutoff up 7.9 octaves. */
static const float VCF_FC[11] = {66.0f, 128.0f, 218.0f, 364.0f, 617.0f, 1083.0f, 1916.0f, 3492.0f, 6384.0f, 11774.0f, P8_FC99};
static const float VCF_K[5] = {0.0f, 0.53f, 1.84f, 3.07f, 3.90f};
#ifndef P8_EG_OCT
#define P8_EG_OCT 7.9f
#endif

static float vcf_cut_oct(float c)
{
    c = clampf(c, 0.0f, 99.0f);
    int i = c >= 90.0f ? 9 : (int)(c / 10.0f);
    float t = i == 9 ? (c - 90.0f) / 9.0f : (c - 10.0f * (float)i) / 10.0f;
    return log2f(VCF_FC[i]) + t * (log2f(VCF_FC[i + 1]) - log2f(VCF_FC[i]));
}

static float vcf_k(float r)
{
    r = clampf(r, 0.0f, 15.0f);
    int i = r >= 12.0f ? 3 : (int)(r / 4.0f);
    float t = i == 3 ? (r - 12.0f) / 3.0f : (r - 4.0f * (float)i) / 4.0f;
    float k = VCF_K[i] + t * (VCF_K[i + 1] - VCF_K[i]);
    return k > 3.95f ? 3.95f : k;
}

void p8_render(p8_engine_t *e, float *out_l, float *out_r, unsigned n)
{
    const float clk0 = P8_TG_CLK * e->a4 / 440.0f;
    /* smoothing of continuous settings: per block here, per sample below */
    const float ksm = e->ksm, kb = 1.0f - powf(1.0f - ksm, (float)n);
    {   p8_engine_t *q = e;
#define P8_SMB(c, t) q->sm.c += kb * ((t) - q->sm.c)
        P8_SMB(det, q->detune_cents); P8_SMB(eg, q->egint); P8_SMB(swp, q->swp2); P8_SMB(mgc, q->mg_cents);
        P8_SMB(mgv, q->mg_vcf); P8_SMB(fmhz, q->fm_hz); P8_SMB(fmr, log2f(fmaxf(q->fm_ratio, 0.01f))); P8_SMB(drv, q->drive);
        P8_SMB(dfb, q->ddl_fb); P8_SMB(dlev, q->ddl_lev);
        P8_SMB(swe, q->sw_e); P8_SMB(swm, q->sw_m); P8_SMB(pwm[0], q->pwm_d[0]); P8_SMB(pwm[1], q->pwm_d[1]);
        P8_SMB(hm, q->h_m); P8_SMB(wsm, q->ws_m); P8_SMB(wse, q->ws_e); P8_SMB(fmig, q->fm_ig);
        if (fabsf(q->sm.tr - q->eq_tr) > 1e-4f || fabsf(q->sm.bs - q->eq_bs) > 1e-4f) {
            P8_SMB(tr, q->eq_tr); P8_SMB(bs, q->eq_bs); eq_coeffs(q);
        } else if (q->sm.tr != q->eq_tr || q->sm.bs != q->eq_bs) { q->sm.tr = q->eq_tr; q->sm.bs = q->eq_bs; eq_coeffs(q); }
#undef P8_SMB
    }
    for (int t = 0; t < P8_TG; t++) {                        /* pan (spread): move the gains, step the held value */
        p8_tg_t *g = &e->tg[t];
        if (g->pan_l != g->pan_lt || g->pan_r != g->pan_rt) {
            float nl = g->pan_l + kb * (g->pan_lt - g->pan_l), nr = g->pan_r + kb * (g->pan_rt - g->pan_r);
            if (fabsf(nl - g->pan_lt) < 1e-5f && fabsf(nr - g->pan_rt) < 1e-5f) { nl = g->pan_lt; nr = g->pan_rt; }
            step_smooth(e, g->v * (nl - g->pan_l), g->v * (nr - g->pan_r));
            g->pan_l = nl; g->pan_r = nr;
        }
    }
    float det = exp2f(-clampf(e->sm.det, 0.0f, 100.0f) / 1200.0f);
    const float inv_fs = 1.0f / e->fs;
    int any = 0;
    for (int t = 0; t < P8_TG; t++) if (e->tg[t].amp_target > 0.0f) any = 1;
    /* DEG3 gate: any key down; keyboard tracking follows the highest key */
    e->deg3.amp_target = any ? 1.0f : 0.0f;
    {
        float hi = -1.0f;
        for (int t = 0; t < P8_VOICES; t++) if (e->tg[t].amp_target > 0.0f && e->tg[t].note > hi) hi = e->tg[t].note;
        if (hi >= 0.0f) e->kbd_note = hi;
    }
    /* measured on a Poly-800 II: full = 1 oct/oct with the nominal cutoff at key C4 (note 60),
     * half = 0.5 oct/oct with the nominal cutoff at the top key C6 (note 84) */
    const float kbd_oct = e->kbd == 2 ? (e->kbd_note - 60.0f) / 12.0f
                        : e->kbd == 1 ? 0.5f * (e->kbd_note - 84.0f) / 12.0f : 0.0f;
    const float cut_rel = 0.0f;                              /* key tracking and velocity are in the smoothed cutoff */
    const float tgt_cut = vcf_cut_oct(e->vcf_c) + kbd_oct - e->vel_cut * (1.0f - e->vel_last);
    int sm_run = fabsf(e->sm.cut - tgt_cut) > 1e-5f || fabsf(e->sm.res - e->vcf_r) > 1e-5f
              || fabsf(e->sm.fm[0] - e->fm) > 1e-6f || fabsf(e->sm.fm[1] - e->fm2) > 1e-6f
              || fabsf(e->sm.ring - e->rmod) > 1e-6f || fabsf(e->sm.nois - e->noise_level) > 1e-6f;

    float vk = vcf_k(e->sm.res);
    const float eg_oct = P8_EG_OCT * clampf(e->sm.eg, -15.0f, 15.0f) / 15.0f;
    const float fc_max = 0.42f * e->fs;
    float cf = exp2f((e->tune_cents + e->bend_semi * 100.0f) / 1200.0f);
    const float cf_static = cf;
    const int sw_mod = e->sm.swe > 1e-4f || e->sm.swm > 1e-4f || e->sw_e > 0.0f || e->sw_m > 0.0f;
    float swp = exp2f(clampf(e->sm.swp, 0.0f, 96.0f) / 12.0f);            /* DCO2 offset, semitones */
    const int sync_on = e->sync && e->mode == P8_DOUBLE;
    float fm_i[2] = {clampf(e->sm.fm[0], 0.0f, 16.0f), clampf(e->sm.fm[1], 0.0f, 16.0f)};
    float ring = clampf(e->sm.ring, 0.0f, 1.0f);
    float fmr = exp2f(e->sm.fmr);
    float drv = e->sm.drv, drv_g = e->drv_g, drv_c = e->drv_c;
    if (fabsf(drv - e->drive) > 1e-6f) {
        drv_g = 1.0f + 255.0f * drv * drv; drv_c = 0.5f / softclip(0.5f * drv_g);
    }
    const int drv_on = drv > 0.0f;

    /* drift (per block): each tone generator's pitch, the MG rate and the
     * cutoff wander slowly and independently (low-passed noise at about
     * 0.25 Hz, about +-1); at drft 127: pitch about +-3 cents (peaks about 8), MG rate
     * +-10 %, cutoff +-0.2 octaves, envelope times +-20 % per note */
    float mg_hz = e->mg_hz, dr_oct = 0.0f;
    if (e->drift > 0.0f) {
        const float a = expf(-2.0f * P8_PI * 0.25f * (float)n / e->fs);
        const float k = (1.0f - a) * sqrtf((1.0f + a) / (1.0f - a)) * 1.732f;   /* unit variance */
        for (int t = 0; t < P8_TG; t++) {
            p8_tg_t *g = &e->tg[t];
            g->dr = a * g->dr + k * drand(e);
            g->dfac = exp2f(3.0f * e->drift * g->dr / 1200.0f);
        }
        e->mg_dr = a * e->mg_dr + k * drand(e);
        e->f_dr = a * e->f_dr + k * drand(e);
        mg_hz *= 1.0f + 0.1f * e->drift * clampf(e->mg_dr, -2.0f, 2.0f);
        dr_oct = 0.2f * e->drift * e->f_dr;
    } else for (int t = 0; t < P8_TG; t++) e->tg[t].dfac = 1.0f;
    /* timbre position moved by the MG (per block) */
    const int fmod = e->sm.hm > 1e-3f || e->h_m > 0.0f;
    if (fmod)
        for (int g = 0; g < 2; g++)
            foot_targets(e, g, clampf(e->har[g] + e->sm.hm * e->mg_val, 0.0f, 127.0f), 0.0f, 0.0f);
    /* Walsh loop position moved by the MG and DEG3 (per block) */
    if (fmod || e->sm.wsm > 1e-3f || fabsf(e->sm.wse) > 1e-3f || e->ws_m > 0.0f || e->ws_e != 0.0f)
        for (int g = 0; g < 2; g++)
            walsh_targets(e, g, e->wpos[g] + e->sm.wsm * e->mg_val + e->sm.wse * e->e3n);

    /* MG delay: counts from the first key of a phrase; then the MG comes in
     * at full depth (measured: no fade-in, a 10 ms ramp avoids a click) */
    if (any && !e->mg_any) e->mg_t = 0.0f;
    e->mg_any = any;
    const int mg_on = e->sm.mgc > 0.01f || e->sm.mgv > 1e-4f || e->mg_cents > 0.0f || e->mg_vcf > 0.0f || e->pwm_d[0] > 0.0f || e->pwm_d[1] > 0.0f
                    || e->sw_m > 0.0f || e->ws_m > 0.0f || e->h_m > 0.0f;
    float mg_tri = 0.0f;

    for (unsigned i = 0; i < n; i++) {
        if (sm_run) {                                        /* per-sample smoothing (CC steps -> ramps) */
            e->sm.cut += ksm * (tgt_cut - e->sm.cut);
            e->sm.res += ksm * (e->vcf_r - e->sm.res); vk = vcf_k(e->sm.res);
            e->sm.fm[0] += ksm * (e->fm - e->sm.fm[0]); e->sm.fm[1] += ksm * (e->fm2 - e->sm.fm[1]);
            e->sm.ring += ksm * (e->rmod - e->sm.ring); e->sm.nois += ksm * (e->noise_level - e->sm.nois);
            fm_i[0] = clampf(e->sm.fm[0], 0.0f, 16.0f); fm_i[1] = clampf(e->sm.fm[1], 0.0f, 16.0f);
            ring = clampf(e->sm.ring, 0.0f, 1.0f);
        }
        if (mg_on) {                                         /* MG: one triangle for the instrument */
            e->mg_ph += mg_hz * inv_fs;
            if (e->mg_ph >= 1.0f) e->mg_ph -= 1.0f;
            float fade = 1.0f;
            if (e->mg_delay > 0.0f) {
                if (e->mg_t < e->mg_delay + 0.02f) e->mg_t += inv_fs;
                fade = clampf((e->mg_t - e->mg_delay) / 0.01f, 0.0f, 1.0f);
            }
            mg_tri = (4.0f * fabsf(e->mg_ph - 0.5f) - 1.0f) * fade;
            e->mg_val = mg_tri;
            e->mg_lp += (mg_tri - e->mg_lp) * (1.0f - expf(-2.0f * P8_PI * P8_MG_LP * inv_fs));
            const float mgc = e->sm.mgc;
            if (mgc > 0.0f) cf = cf_static * exp2f(mgc * e->mg_lp / 1200.0f);
        }
        for (int g = 0; g < 2; g++) {
            for (int f = 0; f < P8_FEET; f++) e->W[g][f] += e->ksm * (e->Wt[g][f] - e->W[g][f]);
            e->pw[g] += e->ksm * (e->pw_t[g] - e->pw[g]);
            if (e->wal[g]) {
                float mx = 0.0f;
                for (int S = 1; S < 16; S++) {
                    e->Wc[g][S] += e->ksm * (e->Wct[g][S] - e->Wc[g][S]);
                    float m = fabsf(e->Wc[g][S]) + fabsf(e->Wct[g][S]);
                    if (m > mx) mx = m;
                }
                if (mx < 1e-7f) { for (int S = 0; S < 16; S++) e->Wc[g][S] = 0.0f; e->wal[g] = 0; }
            }
            if (fabsf(e->pw[g] - e->pw_t[g]) < 1e-6f) e->pw[g] = e->pw_t[g];
        }
        float wcur[2];                                       /* pulse width with MG (PWM) */
        for (int g = 0; g < 2; g++) {
            wcur[g] = e->sm.pwm[g] > 1e-5f ? clampf(e->pw[g] + e->sm.pwm[g] * mg_tri, 0.03f, 0.97f) : e->pw[g];
        }
        if (sw_mod)                                          /* DCO2 sweep: DEG3, MG */
            swp = exp2f(clampf(e->sm.swp + e->sm.swe * e->e3n + e->sm.swm * mg_tri, -48.0f, 96.0f) / 12.0f);
        float t16[P8_VOICES][P8_RMAX]; int n16[P8_VOICES] = {0, 0, 0, 0};
        float rs0[P8_VOICES] = {1.0f, 1.0f, 1.0f, 1.0f};    /* ring: 16' of DCO1 at the start of the sample */
        if (ring > 0.0f) for (int k = 0; k < P8_VOICES; k++)
            rs0[k] = (__builtin_popcount(~tg_states(&e->tg[k], wcur[0]) & e->rm_mask) & 1) ? -1.0f : 1.0f;

        float sync_tau[P8_VOICES] = {-1.0f, -1.0f, -1.0f, -1.0f};
        for (int t = 0; t < P8_TG; t++) {
            p8_tg_t *g = &e->tg[t];
            if (g->amp_target <= 0.0f && g->amp <= 0.0f && g->v == 0.0f && !g->trig) continue;
            const float *W = e->W[t >= 4];
            /* gate ramp */
            deg_tick(e, g, t >= 4);                          /* DEG1 / DEG2 */
            const int k = t & 3, slave = t >= 4;
            const float w = wcur[slave];
            const int pwm = fabsf(w - 0.5f) > 1e-5f;
            float rsg = rs0[k];                              /* ring: DCO1's 16' at the start of the sample */
            shape_t sh = {W, e->Wc[slave], e->wal[slave], w, slave && ring > 0.0f ? 1.0f - ring + ring * rsg : 1.0f};
            /* smooth changes of gains, gate and width */
            float vn = tg_eval(g, &sh, pwm ? tg_states(g, w) : tg_bits(g));
            if (vn != g->v) { float d = vn - g->v; step_smooth(e, d * g->pan_l, d * g->pan_r); g->v = vn; }
            if (g->amp <= 0.0f && g->amp_target <= 0.0f && g->stage == 0) {   /* silent: park at 0 */
                if (g->v != 0.0f) { step_smooth(e, -g->v * g->pan_l, -g->v * g->pan_r); g->v = 0.0f; }
                continue;
            }
            /* counter steps: band-limited edges */
            float clk = clk0 * cf * (slave ? det * swp : 1.0f) * g->dfac;
            float inc = 2.0f * clk / g->counter * inv_fs;
            if (g->glide != 0.0f) {                          /* portamento */
                inc *= exp2f(g->glide / 1200.0f);
                g->glide *= e->port_k;
                if (fabsf(g->glide) < 0.01f) g->glide = 0.0f;
            }
            int generic = pwm;
            if (fm_i[slave] > 0.0f) {                        /* linear FM, through zero */
                float m = 0.0f;
                if (e->fm_src == 1) {                        /* the other DCO of the voice */
                    const int o = slave ? 0 : 1;
                    const p8_tg_t *s2 = &e->tg[k + 4 * o];
                    float full = 0.0f;
                    for (int f = 0; f < P8_FEET; f++) full += 0.5f * e->W[o][f];
                    for (int S2 = 1; S2 < 16; S2++) full += fabsf(e->Wc[o][S2]);
                    m = (full > 1e-6f && s2->amp > 0.0f) ? s2->v / (full * s2->amp) : 0.0f;
                } else if (e->fm_src == 2) {                 /* external signal */
                    m = e->fm_in ? clampf(e->fm_in[i] * e->sm.fmig, -2.0f, 2.0f) : 0.0f;
                } else {                                     /* sine at fmr x the 16' frequency + fmhz */
                    g->mph += fmr * inc / (float)(2u << g->bit[0]) + e->sm.fmhz * inv_fs;
                    g->mph -= floorf(g->mph);
                    m = sinf(2.0f * P8_PI * g->mph);
                }
                inc *= 1.0f + fm_i[slave] * m;
                generic = 1;
            }
            /* keep the edges per sample bounded (sweeps, FM): at most 3 counts */
            if (inc > 3.0f) inc = 3.0f; else if (inc < -3.0f) inc = -3.0f;
            /* split points of DCO2 in this sample: sync (reset) and the 16' edges of DCO1 (ring) */
            float pt[P8_RMAX + 2]; int pty[P8_RMAX + 2], np = 0;
            if (slave && sync_tau[k] >= 0.0f) { pt[np] = sync_tau[k]; pty[np++] = 0; }
            if (slave && ring > 0.0f) for (int j = 0; j < n16[k] && np < P8_RMAX + 1; j++) { pt[np] = t16[k][j]; pty[np++] = 1; }
            if (np) {
                for (int a = 1; a < np; a++)                 /* in time order: largest tau first */
                    for (int b = a; b > 0 && pt[b] > pt[b - 1]; b--) {
                        float tf = pt[b]; pt[b] = pt[b - 1]; pt[b - 1] = tf;
                        int ti = pty[b]; pty[b] = pty[b - 1]; pty[b - 1] = ti;
                    }
                float cur = 1.0f;
                for (int a = 0; a < np; a++) {
                    float tau = pt[a] > cur ? cur : pt[a];
                    tg_run(e, g, &sh, 1, inc, cur - tau, tau, 0, 0);
                    if (pty[a] == 0) { g->cnt = 0; g->ph = 0.0f; }          /* hard sync */
                    else { rsg = -rsg; sh.rm = 1.0f - ring + ring * rsg; }    /* ring: DCO1's 16' flips */
                    vn = tg_eval(g, &sh, tg_states(g, w));
                    if (vn != g->v) { float d = vn - g->v; step_sharp(e, d * g->pan_l, d * g->pan_r, tau); g->v = vn; }
                    cur = tau;
                }
                tg_run(e, g, &sh, 1, inc, cur, 0.0f, 0, 0);
            } else {
                float wrap = tg_run(e, g, &sh, generic, inc, 1.0f, 0.0f, slave ? 0 : t16[k], &n16[k]);
                if (!slave && sync_on) sync_tau[k] = wrap;
            }
        }

        /* DEG3: its level (0..31, linear) moves the cutoff in octaves and
         * sets the noise VCA, both measured as linear in the DEG level */
        deg_tick(e, &e->deg3, 2);
        const float e3 = deg_level(e->deg3.amp);
        e->e3n = e3 / 31.0f;
        float oct = e->sm.cut + cut_rel + eg_oct * e3 / 31.0f + e->sm.mgv * mg_tri + dr_oct;
        float fc = exp2f(oct) ;
        fc = fc < 20.0f ? 20.0f : fc > fc_max ? fc_max : fc;
        const float vg = tanf(P8_PI * fc * inv_fs), VG = vg / (1.0f + vg), VB = 1.0f / (1.0f + vg);
        const float G4 = VG * VG * VG * VG;
        /* integrate, noise, VCF, output stage */
        e->rng = e->rng * 1664525u + 1013904223u;
        float nz = ((float)(e->rng >> 8) * (1.0f / 8388608.0f) - 1.0f) * clampf(e->sm.nois, 0.0f, 1.0f) * (e3 / 31.0f) * 0.5f;
        float o[2];
        /* digital delay: mono input (sum), band-limited, feedback inside the loop;
         * the echo goes to L in phase and to R inverted (measured on a Poly-800 II) */
        float wet = 0.0f;
        const float dlev = e->sm.dlev / 15.0f;
        const int ddl_on = dlev > 0.0f;
        for (int c = 0; c < 2; c++) {
            e->acc[c] += e->ring[c][e->rpos];
            e->ring[c][e->rpos] = 0.0f;
            float x = e->acc[c] + nz * 0.70710678f;
            float y;
            if (e->tone) {
                /* VCF: zero-delay-feedback 4-pole ladder, feedback k solved
                 * exactly; no level compensation (bass falls 1/(1+k)) */
                if (drv_on && e->drv_pre) x = P8_DRV_REF * drv_c * softclip(drv_g * x * (1.0f / P8_DRV_REF));
                float *st = e->vcf_s[c];
                float S = VB * (VG * VG * VG * st[0] + VG * VG * st[1] + VG * st[2] + st[3]);
                float y4 = (G4 * x + S) / (1.0f + vk * G4);
                float u = x - vk * y4;
                for (int p = 0; p < 4; p++) {
                    float v = (u - st[p]) * VG;
                    float yp = v + st[p]; st[p] = yp + v; u = yp;
                }
                if (drv_on && !e->drv_pre) u = P8_DRV_REF * drv_c * softclip(drv_g * u * (1.0f / P8_DRV_REF));
                /* output stage: 54 Hz high-pass, one pole at about 10 kHz */
                y = e->hp_k * (e->dc_y[c] + u - e->dc_x[c]);
                e->dc_x[c] = u; e->dc_y[c] = y;
                { float v = (y - e->op_s[c]) * e->op_g; y = v + e->op_s[c]; e->op_s[c] = y + v; }
            } else {
                if (drv_on) x = P8_DRV_REF * drv_c * softclip(drv_g * x * (1.0f / P8_DRV_REF));
                y = x - e->dc_x[c] + e->dc_k * e->dc_y[c];
                e->dc_x[c] = x; e->dc_y[c] = y;
            }
            o[c] = y;
        }
        if (ddl_on || e->ddl_fb > 0.0f) {
            if (e->ddl_glide) e->ddl_ms_s += 0.0003f * (e->ddl_ms - e->ddl_ms_s);   /* glide, about 70 ms at 48 kHz */
            else if (e->ddl_mute < 0.5f) e->ddl_ms_s = e->ddl_ms;  /* muted while the time jumps */
            if (e->ddl_mute < 1.0f) {   /* fade in: after a muted change, and at the start (in both modes;
                                           the ramp used to run only with dgld 0, so dgld 1 stayed silent) */
                e->ddl_mute += inv_fs / 0.04f; if (e->ddl_mute > 1.0f) e->ddl_mute = 1.0f;
            }
            float ms = e->ddl_ms_s;
            if (e->ddl_mi > 0.0f) {
                e->ddl_ph += P8_DDL_MF0 * exp2f(6.644f * e->ddl_mf / 31.0f) * inv_fs;   /* x100 over 0..31 */
                if (e->ddl_ph >= 1.0f) e->ddl_ph -= 1.0f;
                float tri = 4.0f * fabsf(e->ddl_ph - 0.5f) - 1.0f;
                ms *= exp2f(1.161f * e->ddl_g * tri);                /* see p8_set_ddl */
            }
            float d = ms * 0.001f * e->fs;
            if (d < 1.0f) d = 1.0f;
            if (d > (float)(P8_DDL - 4)) d = (float)(P8_DDL - 4);
            float rp = (float)e->ddl_w - d;
            int i0 = (int)floorf(rp); float fr = rp - (float)i0;
            float ym1 = e->ddl_buf[(unsigned)(i0 - 1) & (P8_DDL - 1)], y0 = e->ddl_buf[(unsigned)i0 & (P8_DDL - 1)];
            float y1 = e->ddl_buf[(unsigned)(i0 + 1) & (P8_DDL - 1)], y2 = e->ddl_buf[(unsigned)(i0 + 2) & (P8_DDL - 1)];
            float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            wet = ((c3 * fr + c2) * fr + c1) * fr + y0;
            float in = 0.5f * (o[0] + o[1]) + wet * (e->sm.dfb / 15.0f) * P8_DDL_FB15;
            for (int p = 0; p < 2; p++) {                         /* band limit of the DDL */
                float v = (in - e->ddl_lp[p]) * e->ddl_lpg; float yp = v + e->ddl_lp[p]; e->ddl_lp[p] = yp + v; in = yp;
            }
            e->ddl_buf[e->ddl_w & (P8_DDL - 1)] = in;
            e->ddl_w = (e->ddl_w + 1) & (P8_DDL - 1);              /* keep it small: (float)ddl_w must stay exact */
            float mu = e->ddl_mute < 0.5f ? 0.0f : 2.0f * e->ddl_mute - 1.0f;
            /* gate: the echo stops when it has fallen below a threshold (measured: an
             * echo train ends abruptly about 37 dB under the note) */
            {   float a = fabsf(wet);
                e->ddl_env = a > e->ddl_env ? a : e->ddl_env * e->ddl_rel;
                float tgt = e->ddl_env > P8_DDL_GATE ? 1.0f : 0.0f;
                e->ddl_gate += (tgt - e->ddl_gate) * 0.002f;
                mu *= e->ddl_gate; }
            wet *= dlev * mu;
        }
        for (int c = 0; c < 2; c++) {
            float y = c == 0 ? o[c] + wet : o[c] - wet;     /* measured: L = dry + echo, R = dry - echo */
            /* EQ: treble and bass shelves */
            if (e->sm.tr != 0.0f) { const float *q = e->eq_c[0]; float t = y; float out = q[0] * t + e->eq_s[c][0]; e->eq_s[c][0] = q[1] * t - q[2] * out; y = out; }
            if (e->sm.bs != 0.0f) { const float *q = e->eq_c[1]; float t = y; float out = q[0] * t + e->eq_s[c][1]; e->eq_s[c][1] = q[1] * t - q[2] * out; y = out; }
            y *= P8_OUT * e->level;
            /* safety: soft knee above 0.8, never beyond 1 */
            float a = fabsf(y);
            if (a > 0.8f) { float k = 0.8f + 0.2f * tanhf((a - 0.8f) * 5.0f); y = y < 0.0f ? -k : k; }
            o[c] = y;
        }
        e->rpos = (e->rpos + 1u) & (P8_RING - 1);
        out_l[i] = o[0]; out_r[i] = o[1];
    }
}
