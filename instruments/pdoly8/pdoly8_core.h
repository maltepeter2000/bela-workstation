/* SPDX-License-Identifier: BSD-3-Clause
 * pdoly8_core : Korg Poly-800 tone generation (MSM5232 + waveform networks),
 * C99, single precision, no allocation, no platform dependencies.
 *
 * Model (Poly-800 service manual, circuits 19, 22, 31, 32, 27):
 *   - one master clock for all 8 tone generators (tune, bend, vibrato act
 *     on it, so all voices move together);
 *   - MSM5232: group 1 = TG0..3 on CLK1, group 2 = TG4..7 on CLK2; a key
 *     becomes an integer divider from the chip's 88 x 12 bit ROM, followed by
 *     a binary counter whose bits are the 16', 8', 4', 2' squares;
 *   - detune thins out CLK2 pulses: group 2 a little lower (0..-20 cents);
 *   - per group the four foot buses are summed by the square net (BR5) or
 *     the saw net (BR6, binary weights); weights measured on a Poly-800,
 *     continuous between the nets, plus continuous foot gains;
 *   - LEVEL1 / LEVEL2 (VCAs gm-1 / gm-2 of the NJM2069), white noise.
 * Output: measured response of a Poly-800 with the VCF fully open (54 Hz
 * high-pass, two poles at 7.5 kHz). Envelopes, a moving VCF and chorus
 * are left out in this version.
 *
 * Every level step is band-limited (windowed-sinc BLEP), so the output has
 * the analog square edges of the original without digital aliasing.
 */
#ifndef PDOLY8_CORE_H
#define PDOLY8_CORE_H

#include <stdint.h>

#define P8_VOICES   4
#define P8_TG       8
#define P8_FEET     4            /* 16', 8', 4', 2' */
#define P8_KNOTS    15           /* foot combinations on the har path */
#define P8_TAPS     32           /* band-limited impulse length (delay 16 samples) */
#define P8_PHASES   64           /* sub-sample resolution of the impulse */
#define P8_RING     64           /* ring buffer, power of two > P8_TAPS */

enum { P8_WHOLE = 1, P8_DOUBLE = 2 };

typedef struct {
    float   note;                /* MIDI note of the key (before octave / interval) */
    int     code;                /* MSM5232 pitch code 0..84 */
    float   counter;             /* programmable counter value from the ROM */
    int     bit[P8_FEET];        /* counter bit per foot (16', 8', 4', 2') */
    uint32_t cnt;                /* binary counter (free running, never reset) */
    float   ph;                  /* fraction of the next count, 0..1 */
    float   mph;                 /* FM sine phase (DCO1 TGs) */
    float   amp, amp_target;     /* envelope output; gate (1 key down, 0 released) */
    float   elv;                 /* envelope level 0..31 during attack / rising slope */
    int     stage, gate, trig;   /* DEG stage 0 idle 1 A 2 D 3 slope 4 S 5 R 6 leak */
    float   v;                   /* last output value, before panning */
    float   pan_l, pan_r;
} p8_tg_t;

typedef struct {                 /* DEG per-sample coefficients of a group */
    float   att, dec, sl_dn, sl_up, rel, bp, su;
    int     bp_off;
} p8_deg_t;

typedef struct {
    float   fs;
    float   a4;                  /* tuning reference, Hz */
    float   tune_cents;          /* front panel tune, +-50 */
    float   bend_semi;           /* current pitch bend in semitones */
    float   mg_hz, mg_cents;     /* vibrato (MG on the master clock) */
    float   mg_ph;
    float   detune_cents;        /* group 2, 0..-20 cents (thinned CLK2) */
    int     interval;            /* group 2, semitones 0..12 */
    int     octave[2];           /* per DCO, -1 / 0 / +1 */
    int     mode;                /* P8_WHOLE (4 voices, group 1) / P8_DOUBLE */

    float   wav[2];              /* 0 = square (1:1:1:1) .. 1 = saw (1:1/2:1/4:1/8) */
    float   har[2];              /* 0..127 on the foot path, see p8_foot_gains */
    float   lev[2];              /* group levels 0..1 */
    float   hsnap;               /* plateau around each knot, control steps */
    int     path;                /* 0 original (15 knots), 1 ring (8, root kept), 2 figure eight (16, both nets) */
    int     pfade;               /* 1: fading foot follows sqrt (linear power) */
    float   norm;                /* 0..1: hold the loudness constant over the morph */
    float   pw[2], pw_t[2];      /* pulse width per DCO, 0.5 = square (current, target) */
    int     sync;                /* 1: DCO2 restarts at every 16' period of DCO1 (DOUBLE) */
    float   swp2;                /* DCO2 pitch offset 0..48 semitones, continuous (sync sweep) */
    float   fm;                  /* linear FM index of DCO1, 0..8, through zero above 1 */
    float   fm_ratio;            /* sine modulator: ratio to DCO1's 16' frequency */
    int     fm_src;              /* 0 internal sine, 1 DCO2 */
    float   wpos[2], wmix[2];    /* Walsh morph per DCO: position 0..127 on the loop, mix 0..1 */
    float   Wc[2][16], Wct[2][16];   /* Walsh coefficients by foot subset (current, target) */
    int     wal[2];              /* Walsh part active */
    float   env[2];              /* DEG morph position 0..127 per DCO, < 0: raw values in deg */
    float   deg[2][6];           /* DEG1 / DEG2: attack, decay, break point, slope, sustain, release (0..31) */
    p8_deg_t dc[2];
    int     leak;                /* 1: released voices stay at -55 dB, as measured */
    float   W[2][P8_FEET];       /* current per foot gain (wav x har x lev) */
    float   Wt[2][P8_FEET];      /* target */
    float   smooth_ms;
    float   ksm;                 /* one-pole coefficient from smooth_ms */

    float   noise_level;         /* 0..1 */
    float   noise_amp;
    uint32_t rng;

    float   spread, level;
    p8_tg_t tg[P8_TG];

    float   ring[2][P8_RING];    /* band-limited impulses L, R */
    unsigned rpos;
    float   acc[2];              /* integrator: the waveform itself */
    float   dc_x[2], dc_y[2];    /* output high-pass state */
    float   dc_k, hp_k;          /* 5 Hz (tone 0) / measured 54 Hz (tone 1) */
    float   lp_s[2][4], lp_g;    /* open VCF + output: two poles at 7.5 kHz */
    int     tone;                /* 1: measured Poly-800 output response */
} p8_engine_t;

/* knot table: foot combination of each knot, bits 16' 8' 4' 2' (MSB first) */
extern const uint8_t P8_KNOT[P8_KNOTS];
extern const uint8_t P8_WALSH[15];       /* Walsh loop by sequency, foot subsets */

void  p8_init(p8_engine_t *e, float fs);
void  p8_set_fs(p8_engine_t *e, float fs);
void  p8_update(p8_engine_t *e);             /* after changing wav/har/lev/mode/... */
void  p8_set_smooth(p8_engine_t *e, float ms);
void  p8_settle(p8_engine_t *e);             /* jump gain smoothing to the targets */

/* voice k (0..3): DCO1 on TG k, DCO2 on TG k+4 (DOUBLE) */
void  p8_voice_on(p8_engine_t *e, int k, float note);
void  p8_voice_off(p8_engine_t *e, int k);
int   p8_voice_sounding(const p8_engine_t *e, int k);
void  p8_retune(p8_engine_t *e);             /* after octave / interval change */

/* foot gains of har position h (0..127) with a plateau of hsnap steps */
void  p8_foot_gains(float h, float hsnap, float g[P8_FEET]);              /* path 0, linear */
/* DEG morph: the six values at loop position pos (0..127) */
void  p8_env_morph(float pos, float hsnap, float deg[6]);
/* any path: foot gains, and for path 2 the net (0 square .. 1 saw), else -1 */
void  p8_path_gains(int path, float h, float hsnap, int pfade, float g[P8_FEET], float *net);
/* MSM5232 pitch: frequency of the 16' foot for a code at the given clock
 * (the Poly-800 runs the chip at about 1.06 MHz; A4 on 8' = code 0x2D) */
float p8_code_hz16(int code, float clock_hz);

void  p8_render(p8_engine_t *e, float *out_l, float *out_r, unsigned n);

#endif
