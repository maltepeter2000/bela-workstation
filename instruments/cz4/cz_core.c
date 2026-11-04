/* SPDX-License-Identifier: BSD-3-Clause
 * cz_core: paraphonic Casio CZ (uPD933) phase-distortion core + vintage layer.
 *
 * Derived from melodee firmware/src/cz_native.c
 *   Copyright (c) Devin Acker (uPD933 model, MAME)
 *   Modifications Copyright (c) 2026 Kerem Kilic (Ellic Studio)
 * See LICENSE-BSD-3-uPD933.txt.
 *
 * The clean wave function is melodee's continuous float evaluation of the
 * uPD933 phase functions. The chip path re-quantizes it the way the chip
 * works: 11-bit phase, integer phase-distortion slopes, a 12-bit cosine table
 * and a 12-bit window product, without interpolation. It is modelled on the
 * chip's structure, not bit-exact against hardware. */
#include "cz_core.h"
#include <math.h>
#include <string.h>

#define TWO_PI 6.28318530717958647692f

/* ------------------------------------------------------------------ tables */
static float    SIN_T[1025];      /* one sine cycle, as melodee's FM6_SIN   */
static uint16_t CHIP_COS[2048];   /* 4095 * (1 - cos) / 2, 12 bit           */
static uint8_t  COS8[256];        /* patent ROM: one cosine cycle, 8 bit    */
static int      tables_ready;

static void tables_init(void)
{
    if (tables_ready) return;
    for (int i = 0; i <= 1024; i++) SIN_T[i] = sinf(TWO_PI * (float)i / 1024.0f);
    for (int i = 0; i < 2048; i++)
        CHIP_COS[i] = (uint16_t)lrintf(4095.0f * (1.0f - cosf(TWO_PI * (float)i / 2048.0f)) * 0.5f);
    for (int i = 0; i < 256; i++)
        COS8[i] = (uint8_t)lrintf(255.0f * (1.0f - cosf(TWO_PI * (float)i / 256.0f)) * 0.5f);
    tables_ready = 1;
}

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline uint32_t xorshift(uint32_t *s)
{
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x;
}

/* ------------------------------------------------- phase-distortion setup */
/* DCW is shared by every voice (paraphonic), so the slopes are computed once
 * per evaluated sample, not per voice. */
typedef struct {
    float dcw, pivot, k0, k1;   /* clean path                         */
    int32_t idcw, ipiv;         /* chip path: integer DCW and pivot   */
    uint32_t r0, r1;            /* 2^30/pivot, 2^30/(2048-pivot)      */
    float wg;                   /* window gain: 1, or rising with DCW (sine mode) */
    float g;                    /* sine mode: 0 at DCW 0 .. 1 from DCW 25 %      */
    int sine;                   /* sine mode on                                  */
    int p8;                     /* patent 8-bit resonance on                     */
    uint32_t mx8;               /* its frequency factor, 8.8 fixed point          */
} pd_t;

static void pd_make(pd_t *p, float dcw)
{
    p->dcw = clampf(dcw, 0.0f, 1023.0f);
    p->pivot = 1024.0f - p->dcw;
    p->k0 = 1024.0f / p->pivot;
    p->k1 = 1024.0f / (2048.0f - p->pivot);
    p->idcw = (int32_t)p->dcw;
    p->ipiv = 1024 - p->idcw;                       /* 1..1024 */
    /* Float reciprocals: the Cortex-A8 has no integer divider. */
    p->r0 = (uint32_t)(1073741824.0f / (float)p->ipiv);
    p->r1 = (uint32_t)(1073741824.0f / (float)(2048 - p->ipiv));
    p->wg = p->g = 1.0f; p->sine = 0; p->p8 = 0; p->mx8 = 256u;
}

/* Sine mode: DCW 0 gives the fundamental sine at every wave position. The
 * windows (resonance) and the doubling of the double waves fade in over the
 * first quarter of the DCW range. Off = the chip's own behaviour. */
static void pd_sine(pd_t *p, int on)
{
    if (!on) return;
    float t = clampf(p->dcw * (1.0f / (0.25f * 1023.0f)), 0.0f, 1.0f);
    p->g = p->wg = t * t * (3.0f - 2.0f * t);
    p->sine = 1;
}

/* Clean: melodee's cz_native_wave, unchanged in substance. */
static inline float wave_clean_pd(unsigned wave, unsigned window, uint32_t ph, const pd_t *b)
{
    float pos = (float)ph * (2048.0f / 4294967296.0f), half = pos >= 1024.0f ? pos - 1024.0f : pos;
    float pivot = b->pivot, phase = 0.0f, win = 0.0f;
    switch (wave) {
    case 0: phase = pos < pivot ? pos * b->k0 : 1024.0f + (pos - pivot) * b->k1; break;
    case 1: phase = (half < pivot ? half * b->k0 : 1024.0f) + (pos >= 1024.0f ? 1024.0f : 0.0f); break;
    case 2: phase = pos < pivot * 2.0f ? pos * b->k0 : 2048.0f; break;
    case 3: return 0.0f;
    case 4: phase = pos < pivot ? pos * b->k0 * 2.0f : (pos - pivot) * b->k1 * 2.0f; break;
    case 5: phase = pos < 1024.0f ? pos : pos < pivot + 1024.0f ? 1024.0f + half * b->k0 : 2048.0f; break;
    case 6: phase = pos * (1.0f + b->dcw * (1.0f / 64.0f)); break;
    default: phase = half < pivot ? half * b->k0 : 2048.0f; break;
    }
    if (phase >= 2048.0f) phase -= (float)(uint32_t)(phase * (1.0f / 2048.0f)) * 2048.0f;
    switch (window) {
    case 0: break;
    case 1: win = pos; break;
    case 2: win = pos < 1024.0f ? 2048.0f - half * 2.0f : half * 2.0f; break;
    case 3: if (pos >= 1024.0f) win = half * 2.0f; break;
    case 4: win = pos < 1024.0f ? pos * 2.0f : 2048.0f; break;
    case 6: { float x = pos * 0.5f + 256.0f; uint32_t k = (uint32_t)x; float fr = x - (float)k; k &= 1023u;
              /* raised cosine (patent US4658691, HROM): 0 -> 1 -> 0 over the cycle */
              win = 1024.0f * (1.0f + SIN_T[k] + (SIN_T[k + 1u] - SIN_T[k]) * fr); } break;
    default: win = (1024.0f - half) * 2.0f; break;
    }
    float x = phase * 0.5f + 256.0f; uint32_t i = (uint32_t)x; float f = x - (float)i;
    i &= 1023u;
    float carrier = 1.0f - (SIN_T[i] + (SIN_T[i + 1u] - SIN_T[i]) * f);
    return carrier * (1.0f - win * b->wg * (1.0f / 2048.0f)) - 1.0f;
}

/* Chip: the same functions on the chip's integer grid. */
static inline float wave_chip_pd(unsigned wave, unsigned window, uint32_t ph, const pd_t *b)
{
    int32_t pos = (int32_t)(ph >> 21), half = pos & 1023, piv = b->ipiv, phase, win = 0;
    switch (wave) {
    case 0: phase = pos < piv ? (int32_t)(((uint64_t)pos * b->r0) >> 20)
                              : 1024 + (int32_t)(((uint64_t)(pos - piv) * b->r1) >> 20); break;
    case 1: phase = (half < piv ? (int32_t)(((uint64_t)half * b->r0) >> 20) : 1024) + (pos >= 1024 ? 1024 : 0); break;
    case 2: phase = pos < piv * 2 ? (int32_t)(((uint64_t)pos * b->r0) >> 20) : 2048; break;
    case 3: return 0.0f;
    case 4: phase = pos < piv ? (int32_t)(((uint64_t)pos * b->r0) >> 19)
                              : (int32_t)(((uint64_t)(pos - piv) * b->r1) >> 19); break;
    case 5: phase = pos < 1024 ? pos : pos < piv + 1024 ? 1024 + (int32_t)(((uint64_t)half * b->r0) >> 20) : 2048; break;
    case 6: phase = (pos * (64 + b->idcw)) >> 6; break;
    default: phase = half < piv ? (int32_t)(((uint64_t)half * b->r0) >> 20) : 2048; break;
    }
    phase &= 2047;
    switch (window) {
    case 0: break;
    case 1: win = pos; break;
    case 2: win = pos < 1024 ? 2048 - half * 2 : half * 2; break;
    case 3: if (pos >= 1024) win = half * 2; break;
    case 4: win = pos < 1024 ? pos * 2 : 2048; break;
    case 6: win = 2048 - (int32_t)((CHIP_COS[pos & 2047] * 2049u) >> 12); break;   /* raised cosine */
    default: win = (1024 - half) * 2; break;
    }
    if (b->wg < 1.0f) win = (int32_t)((float)win * b->wg);
    int32_t s = ((int32_t)CHIP_COS[phase] * (2048 - win)) >> 11;   /* 12-bit product */
    return (float)s * (2.0f / 4095.0f) - 1.0f;
}

/* ---- phase-domain helpers for the morph: the bent phase is returned
 * UNWRAPPED (double waves run to 4096) so two phases can be interpolated. */
static inline void phw_clean(unsigned wave, unsigned window, float pos, const pd_t *b, float *ph, float *wn)
{
    float half = pos >= 1024.0f ? pos - 1024.0f : pos, pivot = b->pivot, phase = 0.0f, win = 0.0f;
    switch (wave) {
    case 0: phase = pos < pivot ? pos * b->k0 : 1024.0f + (pos - pivot) * b->k1; break;
    case 1: phase = (half < pivot ? half * b->k0 : 1024.0f) + (pos >= 1024.0f ? 1024.0f : 0.0f); break;
    case 2: phase = pos < pivot * 2.0f ? pos * b->k0 : 2048.0f; break;
    case 3: phase = 0.0f; break;
    case 4: phase = pos < pivot ? pos * b->k0 * 2.0f : 2048.0f + (pos - pivot) * b->k1 * 2.0f; break;
    case 5: phase = pos < 1024.0f ? pos : pos < pivot + 1024.0f ? 1024.0f + half * b->k0 : 2048.0f; break;
    case 6: phase = pos * (1.0f + b->dcw * (1.0f / 64.0f)); break;
    default: phase = (half < pivot ? half * b->k0 : 2048.0f) + (pos >= 1024.0f ? 2048.0f : 0.0f); break;
    }
    switch (window) {
    case 0: break;
    case 1: win = pos; break;
    case 2: win = pos < 1024.0f ? 2048.0f - half * 2.0f : half * 2.0f; break;
    case 3: if (pos >= 1024.0f) win = half * 2.0f; break;
    case 4: win = pos < 1024.0f ? pos * 2.0f : 2048.0f; break;
    case 6: { float x = pos * 0.5f + 256.0f; uint32_t k = (uint32_t)x; float fr = x - (float)k; k &= 1023u;
              /* raised cosine (patent US4658691, HROM): 0 -> 1 -> 0 over the cycle */
              win = 1024.0f * (1.0f + SIN_T[k] + (SIN_T[k + 1u] - SIN_T[k]) * fr); } break;
    default: win = (1024.0f - half) * 2.0f; break;
    }
    *ph = phase; *wn = win;
}

static inline float out_clean(float phase, float win, const pd_t *b)
{
    if (phase >= 2048.0f) phase -= (float)(uint32_t)(phase * (1.0f / 2048.0f)) * 2048.0f;
    float x = phase * 0.5f + 256.0f; uint32_t i = (uint32_t)x; float f = x - (float)i;
    i &= 1023u;
    float carrier = 1.0f - (SIN_T[i] + (SIN_T[i + 1u] - SIN_T[i]) * f);
    return carrier * (1.0f - win * b->wg * (1.0f / 2048.0f)) - 1.0f;
}

static inline void phw_chip(unsigned wave, unsigned window, int32_t pos, const pd_t *b, int32_t *ph, int32_t *wn)
{
    int32_t half = pos & 1023, piv = b->ipiv, phase = 0, win = 0;
    switch (wave) {
    case 0: phase = pos < piv ? (int32_t)(((uint64_t)pos * b->r0) >> 20)
                              : 1024 + (int32_t)(((uint64_t)(pos - piv) * b->r1) >> 20); break;
    case 1: phase = (half < piv ? (int32_t)(((uint64_t)half * b->r0) >> 20) : 1024) + (pos >= 1024 ? 1024 : 0); break;
    case 2: phase = pos < piv * 2 ? (int32_t)(((uint64_t)pos * b->r0) >> 20) : 2048; break;
    case 3: phase = 0; break;
    case 4: phase = pos < piv ? (int32_t)(((uint64_t)pos * b->r0) >> 19)
                              : 2048 + (int32_t)(((uint64_t)(pos - piv) * b->r1) >> 19); break;
    case 5: phase = pos < 1024 ? pos : pos < piv + 1024 ? 1024 + (int32_t)(((uint64_t)half * b->r0) >> 20) : 2048; break;
    case 6: phase = (pos * (64 + b->idcw)) >> 6; break;
    default: phase = (half < piv ? (int32_t)(((uint64_t)half * b->r0) >> 20) : 2048) + (pos >= 1024 ? 2048 : 0); break;
    }
    switch (window) {
    case 0: break;
    case 1: win = pos; break;
    case 2: win = pos < 1024 ? 2048 - half * 2 : half * 2; break;
    case 3: if (pos >= 1024) win = half * 2; break;
    case 4: win = pos < 1024 ? pos * 2 : 2048; break;
    case 6: win = 2048 - (int32_t)((CHIP_COS[pos & 2047] * 2049u) >> 12); break;   /* raised cosine */
    default: win = (1024 - half) * 2; break;
    }
    *ph = phase; *wn = win;
}

static inline float out_chip(int32_t phase, int32_t win, const pd_t *b)
{
    phase &= 2047;
    if (b->wg < 1.0f) win = (int32_t)((float)win * b->wg);
    int32_t s = ((int32_t)CHIP_COS[phase] * (2048 - win)) >> 11;
    return (float)s * (2.0f / 4095.0f) - 1.0f;
}

float cz_wave_clean(unsigned wave, unsigned window, uint32_t ph, float dcw)
{
    pd_t p; tables_init(); pd_make(&p, dcw);
    return wave_clean_pd(wave & 7u, window, ph, &p);
}

float cz_wave_chip(unsigned wave, unsigned window, uint32_t ph, float dcw)
{
    pd_t p; tables_init(); pd_make(&p, dcw);
    return wave_chip_pd(wave & 7u, window, ph, &p);
}

/* ------------------------------------------------------------ wave morph */
/* Positions ordered so that neighbours are harmonically close: the four
 * classic waves, the six resonance windows (the raised cosine from the
 * Casio patent next to the triangle), then the two double waves. */
#define NPOS 12u
static const uint8_t MORPH_WAVE[NPOS] = {0, 5, 1, 2, 6, 6, 6, 6, 6, 6, 4, 7};
static const uint8_t MORPH_WIN[NPOS]  = {0, 0, 0, 0, 1, 2, 6, 3, 4, 5, 0, 0};

typedef struct { uint8_t wa, na, wb, nb, phase_ok; float f, ga, gb; } morph_t;

static int is_double(unsigned w) { return w == 4u || w == 7u; }

static void morph_make(morph_t *m, float pos, int phase_morph, int p8)
{
    pos = clampf(pos, 0.0f, CZ_MORPH_MAX);
    unsigned i = (unsigned)pos;
    if (i >= NPOS - 1u) i = NPOS - 2u;
    m->f = pos - (float)i;
    if (m->f < 1e-4f) m->f = 0.0f;
    if (m->f > 1.0f - 1e-4f) { i++; m->f = 0.0f; }
    m->wa = MORPH_WAVE[i]; m->na = MORPH_WIN[i];
    unsigned j = i < NPOS - 1u ? i + 1u : i;
    m->wb = MORPH_WAVE[j]; m->nb = MORPH_WIN[j];
    /* Interpolating the bent phase gives one single wave at every point, but
     * only where both phases end a cycle on the same multiple of 2048:
     * two resonance windows (same phase), or two non-resonance waves with
     * the same number of sine cycles. Elsewhere the outputs are crossfaded. */
    int both_reso = m->wa == 6u && m->wb == 6u;
    int no_reso = m->wa != 6u && m->wb != 6u && is_double(m->wa) == is_double(m->wb);
    m->phase_ok = (uint8_t)(phase_morph && ((both_reso && !p8) || no_reso));
    /* Crossfade gains. Neighbours with a different cycle count (a single
     * and a double wave) share almost no harmonics, so a linear fade dips
     * about 4 dB at the midpoint; with phase_morph on they get an
     * equal-power fade. Correlated neighbours keep the linear fade (equal
     * power would overshoot them by 2 dB). */
    if (phase_morph && is_double(m->wa) != is_double(m->wb)) {
        m->ga = cosf(m->f * 1.5707963f); m->gb = sinf(m->f * 1.5707963f);
    } else { m->ga = 1.0f - m->f; m->gb = m->f; }
}

/* Resonance as in the multiplier arrangement of Casio's patent US4658691
 * (Figs. 16-24): a 12-bit phase N is multiplied by MX (8.8 fixed point,
 * MX = 256 is one cycle per note); the low 8 bits of N*MX/4096 address a
 * 256-entry cosine ROM with 8-bit amplitude, read without interpolation;
 * the result is multiplied 8 x 8 bit by an envelope over the note cycle,
 * taken from the top 8 phase bits (here: the chosen window, at 8 bit). */
static inline float wave_p8(unsigned window, uint32_t ph, const pd_t *b)
{
    uint32_t n = ph >> 20;                                    /* 12-bit phase N   */
    uint32_t addr = ((n * b->mx8) >> 12) & 255u;              /* MPY1, low 8 bits */
    uint32_t top = n >> 4;                                    /* N4..N11          */
    int32_t pos = (int32_t)(top << 3), half = pos & 1023, win = 0;   /* 11-bit grid of 256 steps */
    switch (window) {
    case 0: break;
    case 1: win = pos; break;
    case 2: win = pos < 1024 ? 2048 - half * 2 : half * 2; break;
    case 3: if (pos >= 1024) win = half * 2; break;
    case 4: win = pos < 1024 ? pos * 2 : 2048; break;
    case 6: win = 2048 - (int32_t)((CHIP_COS[pos & 2047] * 2049u) >> 12); break;
    default: win = (1024 - half) * 2; break;
    }
    if (b->wg < 1.0f) win = (int32_t)((float)win * b->wg);
    uint32_t env = (uint32_t)(((2048 - win) * 255) >> 11);       /* 8-bit envelope    */
    uint32_t s = ((uint32_t)COS8[addr] * env) >> 8;              /* MPY2, 8 x 8 bit   */
    return (float)s * (2.0f / 255.0f) - 1.0f;
}

float cz_wave_p8_test(unsigned window, uint32_t ph, float dcw)
{
    pd_t p; tables_init(); pd_make(&p, dcw); p.p8 = 1; p.mx8 = (uint32_t)(256.0f + p.dcw * 4.0f);
    return wave_p8(window, ph, &p);
}

static inline float wave_q(unsigned w, unsigned win, uint32_t ph, const pd_t *pd, float quant)
{
    if (w == 6u && pd->p8) return wave_p8(win, ph, pd);
    if (quant <= 0.0f) return wave_clean_pd(w, win, ph, pd);
    if (quant >= 1.0f) return wave_chip_pd(w, win, ph, pd);
    return lerpf(wave_clean_pd(w, win, ph, pd), wave_chip_pd(w, win, ph, pd), quant);
}

/* fundamental sine at this phase (sine mode, double waves) */
static inline float fund_q(uint32_t ph, const pd_t *pd, float quant)
{
    float c = 0.0f, q = 0.0f;
    if (quant < 1.0f) c = out_clean((float)ph * (2048.0f / 4294967296.0f), 0.0f, pd);
    if (quant > 0.0f) { int32_t p = (int32_t)(ph >> 21); q = (float)(CHIP_COS[p & 2047] * 2) * (1.0f / 4095.0f) - 1.0f; }
    return quant <= 0.0f ? c : quant >= 1.0f ? q : lerpf(c, q, quant);
}

/* sine mode: the double waves grow out of the fundamental sine */
static inline float sine_fix(float v, int dbl, uint32_t ph, const pd_t *pd, float quant)
{
    if (!pd->sine || !dbl || pd->g >= 1.0f) return v;
    float s = fund_q(ph, pd, quant);
    return s + (v - s) * pd->g;
}

static inline float pos_eval(unsigned w, unsigned n, uint32_t ph, const pd_t *pd, float quant)
{
    return sine_fix(wave_q(w, n, ph, pd, quant), is_double(w), ph, pd, quant);
}

static inline float wave_morph(const morph_t *m, uint32_t ph, const pd_t *pd, float quant)
{
    if (m->f == 0.0f) return pos_eval(m->wa, m->na, ph, pd, quant);
    if (!m->phase_ok) {
        float a = pos_eval(m->wa, m->na, ph, pd, quant), b = pos_eval(m->wb, m->nb, ph, pd, quant);
        return a * m->ga + b * m->gb;
    }
    float c = 0.0f, q = 0.0f;
    if (quant < 1.0f) {
        float pos = (float)ph * (2048.0f / 4294967296.0f), pa, wa, pb, wb;
        phw_clean(m->wa, m->na, pos, pd, &pa, &wa);
        phw_clean(m->wb, m->nb, pos, pd, &pb, &wb);
        c = out_clean(pa + (pb - pa) * m->f, wa + (wb - wa) * m->f, pd);
    }
    if (quant > 0.0f) {
        int32_t pos = (int32_t)(ph >> 21), pa, wa, pb, wb;
        phw_chip(m->wa, m->na, pos, pd, &pa, &wa);
        phw_chip(m->wb, m->nb, pos, pd, &pb, &wb);
        q = out_chip((int32_t)((float)pa + (float)(pb - pa) * m->f),
                     (int32_t)((float)wa + (float)(wb - wa) * m->f), pd);
    }
    float v = quant <= 0.0f ? c : quant >= 1.0f ? q : lerpf(c, q, quant);
    return sine_fix(v, is_double(m->wa), ph, pd, quant);
}

float cz_pos_from_chip(unsigned wave, unsigned window)
{
    static const float RESO[6] = {4.0f, 4.0f, 5.0f, 7.0f, 8.0f, 9.0f};   /* window 0..5 */
    switch (wave & 7u) {
    case 0: return 0.0f;
    case 5: return 1.0f;
    case 1: return 2.0f;
    case 2: return 3.0f;
    case 3: return 0.0f;                     /* silent: no morph position */
    case 6: return RESO[window > 5u ? 5u : window];
    case 4: return 10.0f;
    default: return 11.0f;
    }
}

/* ------------------------------------------------------------ vintage layer */
static void era_apply(cz_engine_t *e, float x)
{
    cz_vintage_t *v = &e->vint;
    float t;
    /* 1. chip quantization fades in over the first fifth of the knob */
    t = clampf(x / 0.2f, 0.0f, 1.0f);
    v->quant = t * t * (3.0f - 2.0f * t);
    /* 2. hold rate: fs -> 16 kHz, logarithmic, from 0.2 on */
    if (x <= 0.2f) v->chip_hz = 0.0f;
    else v->chip_hz = e->fs * powf(16000.0f / e->fs, (x - 0.2f) / 0.8f);
    /* 3. DAC: 16 -> 8 bit and bit-weight mismatch from 0.3 on */
    t = clampf((x - 0.3f) / 0.7f, 0.0f, 1.0f);
    v->dac_bits = (int)lrintf(16.0f - 8.0f * t);
    v->dac_err = t;
    /* 3b. hiss, 4. reconstruction filter */
    if (x < 0.05f) { v->noise_db = -200.0f; v->lpf_hz = 0.0f; }
    else {
        t = (x - 0.05f) / 0.95f;
        v->noise_db = -96.0f + 30.0f * t;
        v->lpf_hz = 18000.0f * powf(5000.0f / 18000.0f, t);
    }
    /* 4b. output stage saturation */
    v->drive = 0.6f * clampf((x - 0.3f) / 0.7f, 0.0f, 1.0f);
}

/* R-2R ladder with mismatched bit weights. Fixed per-bit errors (seeded), so
 * the nonlinearity is static like a real converter. */
static void dac_weights(cz_engine_t *e)
{
    int n = e->vint.dac_bits < 4 ? 4 : e->vint.dac_bits > 16 ? 16 : e->vint.dac_bits;
    uint32_t s = 0x2545F491u;
    for (int b = 0; b < n; b++) {
        float r = (float)(xorshift(&s) & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;   /* -1..1 */
        /* MSBs carry the larger absolute error, as in real ladders */
        e->dac_w[b] = ldexpf(1.0f, b) * (1.0f + e->vint.dac_err * 0.012f * r);
    }
    e->dac_mid = e->dac_w[n - 1];   /* code of 0.0 in offset binary: MSB only */
    e->dac_w_bits = n; e->dac_w_err = e->vint.dac_err;
}

static inline float dac(const cz_engine_t *e, float x)
{
    int n = e->dac_w_bits;
    if (n >= 16 && e->dac_w_err <= 0.0f) return x;
    float full = ldexpf(1.0f, n - 1);
    float q = floorf(clampf(x, -1.0f, 1.0f) * (full - 1.0f) + 0.5f);
    if (e->dac_w_err <= 0.0f) return q / full;
    uint32_t code = (uint32_t)(int32_t)(q + full), m = (1u << n) - 1u;
    if (code > m) code = m;
    float val = 0.0f;
    for (int b = 0; b < n; b++) if ((code >> b) & 1u) val += e->dac_w[b];
    return (val - e->dac_mid) / full;
}

static void lpf_coeffs(cz_engine_t *e, float hz)
{
    float w = TWO_PI * clampf(hz, 20.0f, 0.45f * e->fs) / e->fs;
    float c = cosf(w), al = sinf(w) * 0.70710678f;    /* Q = 1/sqrt(2) */
    float a0 = 1.0f + al;
    e->lp_b0 = (1.0f - c) * 0.5f / a0; e->lp_b1 = (1.0f - c) / a0; e->lp_b2 = e->lp_b0;
    e->lp_a1 = -2.0f * c / a0; e->lp_a2 = (1.0f - al) / a0;
    e->lp_hz_cur = hz;
}

static inline float lpf(cz_engine_t *e, int ch, float x)
{   /* transposed direct form II */
    float *z = e->lp_z[ch];
    float y = e->lp_b0 * x + z[0];
    z[0] = e->lp_b1 * x - e->lp_a1 * y + z[1];
    z[1] = e->lp_b2 * x - e->lp_a2 * y;
    return y;
}

static inline float softclip(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void cz_vintage_update(cz_engine_t *e)
{
    e->era_active = 0;
    if (e->vint.dac_bits != e->dac_w_bits || e->vint.dac_err != e->dac_w_err) dac_weights(e);
    if (e->vint.lpf_hz > 0.0f) lpf_coeffs(e, e->vint.lpf_hz);
}

void cz_set_era(cz_engine_t *e, float era)
{
    e->era_target = clampf(era, 0.0f, 1.0f);
    if (!e->era_active) { e->era = e->era_target; e->era_active = 1; }
}

/* -------------------------------------------------------------------- API */
void cz_init(cz_engine_t *e, float fs)
{
    tables_init();
    memset(e, 0, sizeof *e);
    e->fs = fs; e->a4 = 440.0f; e->level = 0.5f; e->spread = 0.5f;
    e->line_amp[0] = e->line_amp[1] = e->amp_s[0] = e->amp_s[1] = 0.5f;
    e->line_mode = CZ_LINE_1;            /* lines start on saw, no pair */
    e->morph_ms = 8.0f; e->fade_ms = 4.0f;
    e->phase_morph = 1; e->sine0 = 1;
    e->vint.dac_bits = 16; e->vint.noise_db = -200.0f;
    e->dcw_last = -1.0f;
    e->hiss_rng = 0x9E3779B9u;
    for (int v = 0; v < CZ_VOICES; v++) {
        e->v[v].note = 60.0f;
        e->v[v].noise = 0x6D2B79F5u ^ (uint32_t)v * 0x9E3779B9u;
        e->v[v].drift_rng = 0x1234567u + (uint32_t)v * 0x51ED270Bu;
    }
    dac_weights(e);
}

void cz_voice_note(cz_engine_t *e, int v, float n) { if (v >= 0 && v < CZ_VOICES) e->v[v].note = n; }
void cz_voice_on(cz_engine_t *e, int v, float n)
{
    if (v < 0 || v >= CZ_VOICES) return;
    e->v[v].note = n; e->v[v].gain_target = 1.0f;
}
void cz_voice_off(cz_engine_t *e, int v) { if (v >= 0 && v < CZ_VOICES) e->v[v].gain_target = 0.0f; }

void cz_gate_reset(cz_engine_t *e)
{
    for (int v = 0; v < CZ_VOICES; v++)
        e->v[v].ph[0] = e->v[v].ph[1] = e->v[v].tog[0] = e->v[v].tog[1] = 0;
}

void cz_line2_on(cz_engine_t *e, int v, float n)
{
    if (v < 0 || v >= CZ_VOICES) return;
    e->v[v].note2 = n; e->v[v].gain2_target = 1.0f;
    e->v[v].ph[1] = e->v[v].tog[1] = 0;
}
void cz_line2_off(cz_engine_t *e, int v) { if (v >= 0 && v < CZ_VOICES) e->v[v].gain2_target = 0.0f; }

void cz_voice_reset(cz_engine_t *e, int v)
{
    if (v < 0 || v >= CZ_VOICES) return;
    e->v[v].ph[0] = e->v[v].ph[1] = e->v[v].tog[0] = e->v[v].tog[1] = 0;
}

float cz_detune(int sign, int octave, int note, int fine)
{
    /* CZ: octave 0-3, note 0-11, fine 0-60. Fine in 1/64 semitone follows
     * melodee's reading of the Casio encoding. */
    float d = (float)(octave * 12 + note) + (float)fine * (1.0f / 64.0f);
    return sign < 0 ? -d : d;
}

int cz_import_tone(cz_engine_t *e, const uint8_t *b, unsigned len)
{
    if (len < 128u || (b[0] & 0xF0u) || (b[0] >> 2) > 2u || b[3] > 47u) return -1;
    e->line_mode = b[0] & 3u;
    e->octave = (b[0] >> 2) == 1u ? 1 : (b[0] >> 2) == 2u ? -1 : 0;
    e->detune = ((float)b[3] + (float)(b[2] >> 2) * (1.0f / 64.0f)) * (b[1] ? -1.0f : 1.0f);
    for (int l = 0; l < 2; l++) {
        unsigned off = l ? 57u : 0u, w = (unsigned)b[14u + off] << 8 | b[15u + off];
        unsigned win = (w >> 6) & 7u;                          /* 5..7 render identically */
        e->line[l].pos = cz_pos_from_chip((w >> 13) & 7u, win);
        e->line[l].pair_on = (w & 512u) != 0u;
        e->line[l].pair = e->line[l].pair_on ? cz_pos_from_chip((w >> 10) & 7u, win) : e->line[l].pos;
    }
    unsigned m = (b[15] >> 3) & 7u;
    e->mod = m == 4u ? CZ_MOD_RING : m == 3u ? CZ_MOD_NOISE : 0;
    return 0;
}

static void render(cz_engine_t *e, float *out_l, float *out_r, unsigned n,
                   const float dcw0[2], const float dcw1[2], const float *dcw_buf)
{
    if (!n) return;
    const float fs = e->fs;
    cz_vintage_t *vi = &e->vint;

    /* ERA macro, smoothed per block (~30 ms) */
    if (e->era_active) {
        e->era += (e->era_target - e->era) * (1.0f - expf(-(float)n / (0.03f * fs)));
        era_apply(e, e->era);
    }
    if (vi->dac_bits != e->dac_w_bits || vi->dac_err != e->dac_w_err) dac_weights(e);
    int use_lpf = vi->lpf_hz > 0.0f;
    if (use_lpf && fabsf(vi->lpf_hz - e->lp_hz_cur) > 0.002f * e->lp_hz_cur + 1.0f) lpf_coeffs(e, vi->lpf_hz);

    /* hold (chip) rate */
    int hold = vi->chip_hz > 0.0f && vi->chip_hz < 0.995f * fs;
    float rate = hold ? vi->chip_hz : fs, tick = rate / fs;
    float quant = clampf(vi->quant, 0.0f, 1.0f);
    float dc_k = 1.0f - expf(-TWO_PI * 6.9f / rate);          /* = melodee's 1/1024 at 44.1k */
    float ring_k = 1.0f - expf(-1.0f / (0.03f * rate));       /* line 1 level follower, 30 ms */
    float fade = 1.0f / (0.001f * clampf(e->fade_ms, 0.1f, 1000.0f) * rate);   /* per evaluated sample */
    float hiss = vi->noise_db > -120.0f ? 1.7320508f * powf(10.0f, vi->noise_db / 20.0f) : 0.0f;
    float g_drive = 1.0f + 2.0f * clampf(vi->drive, 0.0f, 1.0f);
    int use_drive = vi->drive > 0.001f;

    /* line routing as on the CZ */
    int mode = e->line_mode & 3;
    unsigned first = mode == CZ_LINE_2 ? 1u : 0u, last = mode >= CZ_LINE_1_1 ? 1u : first;
    unsigned srcl[2] = { 0u, mode == CZ_LINE_1_1 ? 0u : 1u };
    int ring = (e->mod & CZ_MOD_RING) && first != last, noise = (e->mod & CZ_MOD_NOISE) != 0;
    /* line levels (two-line modes): ramped across the block, no zipper */
    float amp0[2], ampd[2];
    for (int l = 0; l < 2; l++) {
        float t = clampf(e->line_amp[l], 0.0f, 1.0f);
        amp0[l] = e->amp_s[l]; ampd[l] = (t - amp0[l]) / (float)n; e->amp_s[l] = t;
    }
    /* line 2, silent before and after the ramp, is not computed (ring needs it).
     * Line 1 always runs: it feeds the ring level followers. */
    if (first != last && !ring && amp0[1] <= 0.0f && e->amp_s[1] <= 0.0f) last = first;

    /* wave positions: smoothed per block, then split into neighbour + blend */
    morph_t mo[2][2];
    float pk = e->morph_ms > 0.0f ? 1.0f - expf(-(float)n / (0.001f * e->morph_ms * fs)) : 1.0f;
    for (int l = 0; l < 2; l++) {
        float tgt[2] = { e->line[l].pos, e->line[l].pair_on ? e->line[l].pair : e->line[l].pos };
        for (int t = 0; t < 2; t++) {
            e->pos_s[l][t] += (clampf(tgt[t], 0.0f, CZ_MORPH_MAX) - e->pos_s[l][t]) * pk;
            morph_make(&mo[l][t], e->pos_s[l][t], e->phase_morph, e->patent8);
        }
    }

    /* per voice, per block: increments (drift included) and pan gains */
    uint32_t inc[CZ_VOICES][2];
    float pan_l[CZ_VOICES], pan_r[CZ_VOICES];
    static const float POS[CZ_VOICES] = {-0.75f, -0.25f, 0.25f, 0.75f};
    float drift_k = 1.0f - expf(-(float)n / (1.5f * fs));
    for (int v = 0; v < CZ_VOICES; v++) {
        cz_voice_t *c = &e->v[v];
        c->drift_count += n;
        if (c->drift_count >= (uint32_t)(0.7f * fs)) {
            c->drift_count = 0;
            c->drift_goal = (float)(xorshift(&c->drift_rng) & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
        }
        c->drift += (c->drift_goal - c->drift) * drift_k;
        float cents = c->drift * clampf(vi->drift, 0.0f, 1.0f) * 8.0f;
        for (int l = 0; l < 2; l++) {
            float base = (l && e->split) ? c->note2 : c->note;
            float note = base + 12.0f * (float)e->octave + (l ? e->detune : 0.0f) + cents * 0.01f;
            float hz = e->a4 * exp2f((note - 69.0f) * (1.0f / 12.0f));
            inc[v][l] = (uint32_t)clampf(hz * (4294967296.0f / rate), 0.0f, 2147483520.0f);
        }
        float a = (clampf(POS[v] * e->spread, -1.0f, 1.0f) + 1.0f) * 0.78539816f;
        pan_l[v] = cosf(a); pan_r[v] = sinf(a);
    }

    /* DCW per line: line 2 may have its own (a 1+1' copy uses line 2's too) */
    pd_t pd[2]; memset(pd, 0, sizeof pd);
    float pd_dcw[2] = {-1.0f, -1.0f};
    float dstep[2] = {(dcw1[0] - dcw0[0]) / (float)n, (dcw1[1] - dcw0[1]) / (float)n};

    for (unsigned i = 0; i < n; i++) {
        float yl, yr;
        int evaluate = 1;
        if (hold) {
            e->chip_acc += tick;
            if (e->chip_acc < 1.0f) evaluate = 0; else e->chip_acc -= 1.0f;
        }
        if (evaluate) {
            for (unsigned l = first; l <= last; l++) {
                float d = dcw_buf ? dcw_buf[i] : dcw0[l] + dstep[l] * (float)(i + 1u);
                float dcw = clampf(d, 0.0f, 1.0f) * 1023.0f;
                if (dcw != pd_dcw[l]) {
                    pd_make(&pd[l], dcw); pd_sine(&pd[l], e->sine0);
                    pd[l].p8 = e->patent8;
                    pd[l].mx8 = (uint32_t)(256.0f + dcw * 4.0f);   /* 1 + DCW/64, in 8.8 */
                    pd_dcw[l] = dcw;
                }
            }
            float sl = 0.0f, sr = 0.0f;
            for (int v = 0; v < CZ_VOICES; v++) {
                cz_voice_t *c = &e->v[v];
                int split = e->split;
                int on1 = c->gain != 0.0f || c->gain_target != 0.0f;
                int on2 = split ? (c->gain2 != 0.0f || c->gain2_target != 0.0f) : on1;
                if (!on1 && !on2) continue;
                /* gains fade per sample (about 4 ms) */
                if (c->gain < c->gain_target) { c->gain += fade; if (c->gain > c->gain_target) c->gain = c->gain_target; }
                else if (c->gain > c->gain_target) { c->gain -= fade; if (c->gain < c->gain_target) c->gain = c->gain_target; }
                if (split) {
                    if (c->gain2 < c->gain2_target) { c->gain2 += fade; if (c->gain2 > c->gain2_target) c->gain2 = c->gain2_target; }
                    else if (c->gain2 > c->gain2_target) { c->gain2 -= fade; if (c->gain2 < c->gain2_target) c->gain2 = c->gain2_target; }
                }
                float g[2] = { c->gain, split ? c->gain2 : c->gain };
                float line[2] = {0.0f, 0.0f};
                for (unsigned l = first; l <= last; l++) {
                    if (!(l ? on2 : on1)) continue;
                    uint32_t adv = inc[v][l], old = c->ph[l];
                    if (l && noise && (xorshift(&c->noise) & 1u))
                        adv = (uint32_t)fminf((float)adv * (1625.0f / 256.0f), 2147483520.0f);
                    float raw = wave_morph(&mo[srcl[l]][c->tog[l] & 1u], c->ph[l], &pd[l], quant);
                    c->ph[l] += adv;
                    if (c->ph[l] < old) c->tog[l] ^= 1u;
                    c->dc[l] += (raw - c->dc[l]) * dc_k;
                    line[l] = (raw - c->dc[l]) * 0.5f * g[l];
                }
                float s;
                if (mode < CZ_LINE_1_1) s = line[first];       /* single-line CZ modes: no amps */
                else {
                    float a1 = amp0[0] + ampd[0] * (float)(i + 1u), a2 = amp0[1] + ampd[1] * (float)(i + 1u);
                    float l2 = line[1];
                    /* line 1 level followers run all the time, so switching to
                     * ring modulation starts at the right level, without a swell */
                    float m = line[0] / (g[0] > 1e-4f ? g[0] : 1e-4f);       /* line 1 before its fade */
                    float am = fabsf(m);
                    c->ring_ms += (m * m - c->ring_ms) * ring_k;
                    c->ring_pk = am > c->ring_pk ? am : c->ring_pk + (am - c->ring_pk) * ring_k;
                    if (ring) {
                        /* The product's level follows line 1's level, which changes
                         * with wave and DCW (-3 to -7 dB against line 2 alone).
                         * Divide by line 1's running RMS so the ring product sits at
                         * line 2's level, and block the DC a same-pitch product has. */
                        /* normalise by the RMS, but never let the normalised line 1
                         * peak above 3 (a sine is 1.41): peaky resonance waves
                         * would otherwise blow the product up */
                        float den = sqrtf(c->ring_ms), lim = c->ring_pk * (1.0f / 3.0f);
                        den = den > lim ? den : lim;
                        float r = m * line[1] / (den > 0.05f ? den : 0.05f);
                        /* +1 dB, then a soft saturation that keeps the product's
                         * peaks at those of a plain line: same loudness as line 2
                         * within about 2 dB, no extra headroom needed */
                        r = softclip(r * 1.12f);
                        c->ring_dc += (r - c->ring_dc) * dc_k;
                        l2 = r - c->ring_dc;
                    }
                    s = line[0] * a1 + l2 * a2;
                }
                sl += s * pan_l[v]; sr += s * pan_r[v];
            }
            /* the DAC runs at the hold rate, like the chip's converter */
            e->hold_l = dac(e, sl * e->level);
            e->hold_r = dac(e, sr * e->level);
        }
        yl = e->hold_l; yr = e->hold_r;
        if (hiss > 0.0f) {
            yl += hiss * ((float)(xorshift(&e->hiss_rng) & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f);
            yr += hiss * ((float)(xorshift(&e->hiss_rng) & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f);
        }
        if (use_lpf) { yl = lpf(e, 0, yl); yr = lpf(e, 1, yr); }
        if (use_drive) { yl = softclip(yl * g_drive) / g_drive; yr = softclip(yr * g_drive) / g_drive; }
        out_l[i] = yl; out_r[i] = yr;
    }
}

void cz_render(cz_engine_t *e, float *out_l, float *out_r, unsigned n, float dcw0, float dcw1)
{
    const float a[2] = {dcw0, dcw0}, b[2] = {dcw1, dcw1};
    render(e, out_l, out_r, n, a, b, 0);
}

void cz_render2(cz_engine_t *e, float *out_l, float *out_r, unsigned n,
                float dcw1_0, float dcw1_1, float dcw2_0, float dcw2_1)
{
    const float a[2] = {dcw1_0, dcw2_0}, b[2] = {dcw1_1, dcw2_1};
    render(e, out_l, out_r, n, a, b, 0);
}

void cz_render_dcw(cz_engine_t *e, float *out_l, float *out_r, unsigned n, const float *dcw)
{
    const float z[2] = {0.0f, 0.0f};
    render(e, out_l, out_r, n, z, z, dcw);
}
