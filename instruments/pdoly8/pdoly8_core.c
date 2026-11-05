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
#ifndef P8_LP_HZ
#define P8_LP_HZ    7500.0f       /* measured: VCF fully open + output stage, two poles */
#endif
#ifndef P8_LP_POLES
#define P8_LP_POLES 2
#endif
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
    const uint8_t *k = path == 1 ? P8_LOOP8 : path == 2 ? P8_FIG8 : P8_KNOT;
    int n = path == 1 ? 8 : path == 2 ? 16 : P8_KNOTS, a, b;
    float t;
    knot_pos(h, hsnap, n, path != 0, &a, &b, &t);
    for (int f = 0; f < P8_FEET; f++) {
        float fa = (float)((k[a] >> (3 - f)) & 1), fb = (float)((k[b] >> (3 - f)) & 1);
        float x = fa + t * (fb - fa);
        g[f] = pfade ? sqrtf(x) : x;                         /* sqrt: the fading foot's power is linear */
    }
    if (net) {                                               /* path 2: the net is part of the path */
        float na = (float)(a >= 8), nb = (float)(b >= 8);
        *net = path == 2 ? na + t * (nb - na) : -1.0f;
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
    {   float lp = P8_LP_HZ < 0.45f * e->fs ? P8_LP_HZ : 0.45f * e->fs, g = tanf(P8_PI * lp / e->fs);
        e->lp_g = g / (1.0f + g); }
    p8_set_smooth(e, e->smooth_ms);
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
    e->fm_ratio = 1.0f;
    e->pw[0] = e->pw[1] = e->pw_t[0] = e->pw_t[1] = 0.5f;
    e->pfade = 1;
    e->smooth_ms = 8.0f;
    e->spread = 0.5f; e->level = 0.5f;
    e->tone = 1;
    e->mg_hz = 5.0f;
    e->rng = 0x2545F491u;
    p8_set_fs(e, fs);
    for (int t = 0; t < P8_TG; t++) {
        e->tg[t].cnt = (uint32_t)(t * 37u);                  /* chip counters are not in step */
        e->tg[t].pan_l = e->tg[t].pan_r = 0.70710678f;
    }
    p8_update(e);
    p8_settle(e);
}

/* per-foot target gains of both groups: wave weights x foot gains x level */
void p8_update(p8_engine_t *e)
{
    /* Foot weights measured on a Poly-800 (square / saw, one key over five
     * octaves, response of the output removed): square net (BR5) a little
     * stronger on the higher feet; saw net (BR6) binary within 1 dB and its
     * 16' 2.47 x (+7.9 dB) the square net's, so both nets peak alike. */
    static const float SQ[P8_FEET]  = {1.0f, 1.057f, 1.083f, 1.098f};
    static const float SAW[P8_FEET] = {2.47f, 2.47f * 0.523f, 2.47f * 0.268f, 2.47f * 0.135f};
    for (int g = 0; g < 2; g++) {
        float gain[P8_FEET], net, wv = clampf(e->wav[g], 0.0f, 1.0f);
        p8_path_gains(e->path, e->har[g], e->hsnap, e->pfade, gain, &net);
        if (net >= 0.0f) wv = net;
        float lev = (g == 1 && e->mode != P8_DOUBLE) ? 0.0f : clampf(e->lev[g], 0.0f, 1.0f);
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
        float mix = clampf(e->wmix[g], 0.0f, 1.0f);
        for (int f = 0; f < P8_FEET; f++) e->Wt[g][f] = w[f] * k * lev * (1.0f - mix);
        /* Walsh morph: two neighbours on the sequency loop, equal power
         * (Walsh functions are orthonormal), at the RMS of the square net
         * with all feet */
        for (int S = 0; S < 16; S++) e->Wct[g][S] = 0.0f;
        if (mix > 0.0f) {
            int wa, wb; float wt;
            knot_pos(e->wpos[g], e->hsnap, 15, 1, &wa, &wb, &wt);
            float A = 0.5f * P8_NORM_REF * mix * lev;
            e->Wct[g][P8_WALSH[wa]] += A * sqrtf(1.0f - wt);
            e->Wct[g][P8_WALSH[wb]] += A * sqrtf(wt);
            e->wal[g] = 1;
        }
    }
    if (e->mode != P8_DOUBLE) for (int t = 4; t < P8_TG; t++) e->tg[t].amp_target = 0.0f;
    /* stereo position of the 4 voices (both DCOs of a voice together) */
    static const float POS[P8_VOICES] = {-1.0f, -0.3333333f, 0.3333333f, 1.0f};
    for (int k = 0; k < P8_VOICES; k++) {
        float a = (clampf(e->spread, 0.0f, 1.0f) * POS[k] + 1.0f) * (P8_PI * 0.25f);
        for (int t = k; t < P8_TG; t += 4) { e->tg[t].pan_l = cosf(a); e->tg[t].pan_r = sinf(a); }
    }
}

/* jump the gain smoothing to its targets (preset recall, before notes) */
void p8_settle(p8_engine_t *e)
{
    for (int g = 0; g < 2; g++) for (int f = 0; f < P8_FEET; f++) e->W[g][f] = e->Wt[g][f];
    for (int g = 0; g < 2; g++) for (int S = 0; S < 16; S++) e->Wc[g][S] = e->Wct[g][S];
    for (int g = 0; g < 2; g++) e->pw[g] = e->pw_t[g];
}

static void tg_pitch(p8_engine_t *e, int t)
{
    p8_tg_t *g = &e->tg[t];
    int grp = t >= 4;
    int code = (int)lroundf(g->note) - 24 + 12 * e->octave[grp] + (grp ? e->interval : 0);
    while (code < 0) code += 12;
    while (code > P8_CODES - 1) code -= 12;
    g->code = code;
    int n;
    rom(code, &g->counter, &n);
    for (int f = 0; f < P8_FEET; f++) g->bit[f] = n - f < 0 ? 0 : n - f;
}

void p8_voice_on(p8_engine_t *e, int k, float note)
{
    if (k < 0 || k >= P8_VOICES) return;
    e->tg[k].note = note; tg_pitch(e, k); e->tg[k].amp_target = 1.0f;
    e->tg[k + 4].note = note; tg_pitch(e, k + 4);
    e->tg[k + 4].amp_target = e->mode == P8_DOUBLE ? 1.0f : 0.0f;
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
    return s * g->amp;
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
                    float inc, float span, float tend)
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
        h = ev[i].up ? (h | bit) : (h & ~bit);
        float vn = tg_eval(g, sh, h);
        if (vn != g->v) { float dv = vn - g->v; step_sharp(e, dv * g->pan_l, dv * g->pan_r, ev[i].tau); g->v = vn; }
    }
    float total = g->ph + d, fl = floorf(total);
    g->cnt += (uint32_t)(int32_t)fl;
    g->ph = total - fl;
    if (g->ph >= 1.0f) { g->ph -= 1.0f; g->cnt++; }
    return wrap;
}

void p8_render(p8_engine_t *e, float *out_l, float *out_r, unsigned n)
{
    const float clk0 = P8_TG_CLK * e->a4 / 440.0f;
    const float det = exp2f(-clampf(e->detune_cents, 0.0f, 100.0f) / 1200.0f);
    const float gstep = 1.0f / (P8_GATE_MS * 0.001f * e->fs);
    const float inv_fs = 1.0f / e->fs;
    int any = 0;
    for (int t = 0; t < P8_TG; t++) if (e->tg[t].amp_target > 0.0f) any = 1;
    const float noise_t = any ? clampf(e->noise_level, 0.0f, 1.0f) : 0.0f;
    float cf = exp2f((e->tune_cents + e->bend_semi * 100.0f) / 1200.0f);
    const float cf_static = cf;
    const float swp = exp2f(clampf(e->swp2, 0.0f, 48.0f) / 12.0f);        /* DCO2 offset, semitones */
    const int sync_on = e->sync && e->mode == P8_DOUBLE;
    const float fm_i = clampf(e->fm, 0.0f, 8.0f);

    for (unsigned i = 0; i < n; i++) {
        if (e->mg_cents > 0.0f) {                            /* MG: triangle on the master clock */
            e->mg_ph += e->mg_hz * inv_fs;
            if (e->mg_ph >= 1.0f) e->mg_ph -= 1.0f;
            float tri = 4.0f * fabsf(e->mg_ph - 0.5f) - 1.0f;
            cf = cf_static * exp2f(e->mg_cents * tri / 1200.0f);
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

        float sync_tau[P8_VOICES] = {-1.0f, -1.0f, -1.0f, -1.0f};
        for (int t = 0; t < P8_TG; t++) {
            p8_tg_t *g = &e->tg[t];
            if (g->amp_target <= 0.0f && g->amp <= 0.0f && g->v == 0.0f) continue;
            const float *W = e->W[t >= 4];
            /* gate ramp */
            if (g->amp < g->amp_target) { g->amp += gstep; if (g->amp > g->amp_target) g->amp = g->amp_target; }
            else if (g->amp > g->amp_target) { g->amp -= gstep; if (g->amp < g->amp_target) g->amp = g->amp_target; }
            const float w = e->pw[t >= 4];
            const int pwm = fabsf(w - 0.5f) > 1e-5f;
            const shape_t sh = {W, e->Wc[t >= 4], e->wal[t >= 4], w};
            /* smooth changes of gains, gate and width */
            float vn = tg_eval(g, &sh, pwm ? tg_states(g, w) : tg_bits(g));
            if (vn != g->v) { float d = vn - g->v; step_smooth(e, d * g->pan_l, d * g->pan_r); g->v = vn; }
            if (g->amp <= 0.0f && g->amp_target <= 0.0f) {   /* silent: park exactly at 0 */
                if (g->v != 0.0f) { step_smooth(e, -g->v * g->pan_l, -g->v * g->pan_r); g->v = 0.0f; }
                continue;
            }
            /* counter steps: band-limited edges */
            const int k = t & 3, slave = t >= 4;
            float clk = clk0 * cf * (slave ? det * swp : 1.0f);
            float inc = 2.0f * clk / g->counter * inv_fs;
            int generic = pwm;
            if (!slave && fm_i > 0.0f) {                     /* linear FM of DCO1, through zero */
                float m;
                if (e->fm_src == 1) {                        /* DCO2 as modulator (last sample) */
                    const p8_tg_t *s2 = &e->tg[k + 4];
                    float full = 0.0f;
                    for (int f = 0; f < P8_FEET; f++) full += 0.5f * e->W[1][f];
                    for (int S2 = 1; S2 < 16; S2++) full += fabsf(e->Wc[1][S2]);
                    m = (full > 1e-6f && s2->amp > 0.0f) ? s2->v / (full * s2->amp) : 0.0f;
                } else {                                     /* sine at fmr x the 16' frequency */
                    g->mph += e->fm_ratio * inc / (float)(2u << g->bit[0]);
                    g->mph -= floorf(g->mph);
                    m = sinf(2.0f * P8_PI * g->mph);
                }
                inc *= 1.0f + fm_i * m;
                generic = 1;
            }
            if (slave && sync_tau[k] >= 0.0f) {              /* hard sync: reset at DCO1's wrap */
                float tau = sync_tau[k];
                tg_run(e, g, &sh, 1, inc, 1.0f - tau, tau);
                g->cnt = 0; g->ph = 0.0f;
                vn = tg_eval(g, &sh, tg_states(g, w));
                if (vn != g->v) { float d = vn - g->v; step_sharp(e, d * g->pan_l, d * g->pan_r, tau); g->v = vn; }
                tg_run(e, g, &sh, 1, inc, tau, 0.0f);
            } else {
                float wrap = tg_run(e, g, &sh, generic, inc, 1.0f, 0.0f);
                if (!slave && sync_on) sync_tau[k] = wrap;
            }
        }

        /* integrate, coupling capacitor, noise, output */
        e->noise_amp += (noise_t - e->noise_amp) * (gstep * 0.5f);
        e->rng = e->rng * 1664525u + 1013904223u;
        float nz = ((float)(e->rng >> 8) * (1.0f / 8388608.0f) - 1.0f) * e->noise_amp * 0.5f;
        float o[2];
        for (int c = 0; c < 2; c++) {
            e->acc[c] += e->ring[c][e->rpos];
            e->ring[c][e->rpos] = 0.0f;
            float x = e->acc[c] + nz * 0.70710678f;
            float y;
            if (e->tone) {                                   /* measured output: 54 Hz HP, 2 x 7.5 kHz LP */
                y = e->hp_k * (e->dc_y[c] + x - e->dc_x[c]);
                e->dc_x[c] = x; e->dc_y[c] = y;
                for (int p = 0; p < P8_LP_POLES; p++) {
                    float v = (y - e->lp_s[c][p]) * e->lp_g;
                    y = v + e->lp_s[c][p]; e->lp_s[c][p] = y + v;
                }
            } else {
                y = x - e->dc_x[c] + e->dc_k * e->dc_y[c];
                e->dc_x[c] = x; e->dc_y[c] = y;
            }
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
