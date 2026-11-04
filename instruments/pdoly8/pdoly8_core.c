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
#define P8_CODES    85            /* pitch codes 0x00..0x54 */
#define P8_OUT      0.65f         /* output scale before 'level' */
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

void p8_foot_gains(float h, float hsnap, float g[P8_FEET])
{
    const float step = 127.0f / (float)(P8_KNOTS - 1);
    h = clampf(h, 0.0f, 127.0f);
    int seg = (int)(h / step);
    if (seg > P8_KNOTS - 2) seg = P8_KNOTS - 2;
    float x0 = (float)seg * step, x1 = x0 + step;
    float s = clampf(hsnap, 0.0f, step * 0.45f);
    float lo = x0 + s, hi = x1 - s, t;
    if (h <= lo + 1e-3f) t = 0.0f; else if (h >= hi - 1e-3f) t = 1.0f; else t = (h - lo) / (hi - lo);
    for (int f = 0; f < P8_FEET; f++) {
        float a = (float)((P8_KNOT[seg] >> (3 - f)) & 1), b = (float)((P8_KNOT[seg + 1] >> (3 - f)) & 1);
        g[f] = a + t * (b - a);
    }
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
    e->smooth_ms = 8.0f;
    e->spread = 0.5f; e->level = 0.5f;
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
    static const float SAW[P8_FEET] = {1.0f, 0.5f, 0.25f, 0.125f};
    for (int g = 0; g < 2; g++) {
        float gain[P8_FEET], wv = clampf(e->wav[g], 0.0f, 1.0f);
        p8_foot_gains(e->har[g], e->hsnap, gain);
        float lev = (g == 1 && e->mode != P8_DOUBLE) ? 0.0f : clampf(e->lev[g], 0.0f, 1.0f);
        for (int f = 0; f < P8_FEET; f++) e->Wt[g][f] = (1.0f + wv * (SAW[f] - 1.0f)) * gain[f] * lev;
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
}

static void tg_pitch(p8_engine_t *e, int t)
{
    p8_tg_t *g = &e->tg[t];
    int grp = t >= 4;
    int code = (int)lroundf(g->note) - 36 + 12 * e->octave[grp] + (grp ? e->interval : 0);
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
static float tg_value(const p8_tg_t *g, const float W[P8_FEET])
{
    float s = 0.0f;
    for (int f = 0; f < P8_FEET; f++) s += ((g->cnt >> g->bit[f]) & 1u) ? 0.5f * W[f] : -0.5f * W[f];
    return s * g->amp;
}

void p8_render(p8_engine_t *e, float *out_l, float *out_r, unsigned n)
{
    const float clk0 = P8_REF_CLK * e->a4 / 440.0f;
    const float det = exp2f(-clampf(e->detune_cents, 0.0f, 100.0f) / 1200.0f);
    const float gstep = 1.0f / (P8_GATE_MS * 0.001f * e->fs);
    const float inv_fs = 1.0f / e->fs;
    int any = 0;
    for (int t = 0; t < P8_TG; t++) if (e->tg[t].amp_target > 0.0f) any = 1;
    const float noise_t = any ? clampf(e->noise_level, 0.0f, 1.0f) : 0.0f;
    float cf = exp2f((e->tune_cents + e->bend_semi * 100.0f) / 1200.0f);
    const float cf_static = cf;

    for (unsigned i = 0; i < n; i++) {
        if (e->mg_cents > 0.0f) {                            /* MG: triangle on the master clock */
            e->mg_ph += e->mg_hz * inv_fs;
            if (e->mg_ph >= 1.0f) e->mg_ph -= 1.0f;
            float tri = 4.0f * fabsf(e->mg_ph - 0.5f) - 1.0f;
            cf = cf_static * exp2f(e->mg_cents * tri / 1200.0f);
        }
        for (int g = 0; g < 2; g++)
            for (int f = 0; f < P8_FEET; f++) e->W[g][f] += e->ksm * (e->Wt[g][f] - e->W[g][f]);

        for (int t = 0; t < P8_TG; t++) {
            p8_tg_t *g = &e->tg[t];
            if (g->amp_target <= 0.0f && g->amp <= 0.0f && g->v == 0.0f) continue;
            const float *W = e->W[t >= 4];
            /* gate ramp */
            if (g->amp < g->amp_target) { g->amp += gstep; if (g->amp > g->amp_target) g->amp = g->amp_target; }
            else if (g->amp > g->amp_target) { g->amp -= gstep; if (g->amp < g->amp_target) g->amp = g->amp_target; }
            /* smooth changes of gains and gate */
            float vn = tg_value(g, W);
            if (vn != g->v) { float d = vn - g->v; step_smooth(e, d * g->pan_l, d * g->pan_r); g->v = vn; }
            if (g->amp <= 0.0f && g->amp_target <= 0.0f) {   /* silent: park exactly at 0 */
                if (g->v != 0.0f) { step_smooth(e, -g->v * g->pan_l, -g->v * g->pan_r); g->v = 0.0f; }
                continue;
            }
            /* counter steps: band-limited edges */
            float clk = clk0 * cf * (t >= 4 ? det : 1.0f);
            float inc = 2.0f * clk / g->counter * inv_fs;
            g->ph += inc;
            while (g->ph >= 1.0f) {
                g->ph -= 1.0f;
                g->cnt++;
                vn = tg_value(g, W);
                if (vn != g->v) {
                    float d = vn - g->v, tau = g->ph / inc;
                    step_sharp(e, d * g->pan_l, d * g->pan_r, tau > 1.0f ? 1.0f : tau);
                    g->v = vn;
                }
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
            float y = x - e->dc_x[c] + e->dc_k * e->dc_y[c];
            e->dc_x[c] = x; e->dc_y[c] = y;
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
