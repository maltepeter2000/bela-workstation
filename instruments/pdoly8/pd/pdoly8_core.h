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
 * VCF (NJM2069): 4-pole ladder measured on a Poly-800 (cutoff 66 Hz ..
 * 12.8 kHz, resonance as uncompensated feedback up to k 3.9), DEG3 on cutoff
 * and noise, keyboard tracking. Output: 54 Hz high-pass, one pole at 10 kHz.
 * Chorus is left out in this version.
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
#define P8_DDL      (1u << 17)   /* delay line: > 1.3 s at 96 kHz */

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
    float   glide;               /* portamento: pitch offset in cents, fading to 0 */
    float   v;                   /* last output value, before panning */
    float   vg;                  /* velocity gain, 1 = full */
    float   pan_lt, pan_rt;      /* pan targets (spread), the gains follow smoothly */
    float   dr, dfac;            /* drift: slow random walk (about +-1), pitch factor */
    float   rs;                  /* drift: envelope rate scale of this note, 1 = exact */
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
    float   mg_hz, mg_cents;     /* MG rate; depth on the master clock (vibrato), cents */
    float   mg_ph;
    int     mg_wave;             /* 0 triangle (the Poly-800), 1 sine, 2 square, 3 noise (random step per cycle) */
    int     mg_retrig;           /* 1: every key restarts the MG cycle */
    float   mg_sh, mg_sl;        /* noise: the value of this cycle; square / noise: slewed output */
    uint32_t mg_rng;
    float   mg_vcf;              /* MG -> VCF cutoff, octaves (Poly-800 II: up to 10 2/3) */
    float   mg_delay;            /* MG delay after the first key of a phrase, seconds */
    float   mg_t;                /* time since the first key (MG delay), seconds */
    int     mg_any;              /* a key was down in the last block */
    float   mg_lp;               /* MG -> pitch low-pass state (measured: one pole, 14.5 Hz) */
    float   detune_cents;        /* group 2, 0..-20 cents (thinned CLK2) */
    int     interval;            /* group 2, semitones 0..12 */
    int     octave[2];           /* per DCO, -1 / 0 / +1 (chip), +2..+4: feet one bit higher per octave */
    int     mode;                /* P8_WHOLE (4 voices, group 1) / P8_DOUBLE */

    float   wav[2];              /* 0 = square (1:1:1:1) .. 1 = saw (1:1/2:1/4:1/8) */
    float   har[2];              /* 0..127 on the foot path, see p8_foot_gains */
    float   lev[2];              /* group levels 0..1 */
    float   hsnap;               /* plateau around each knot, control steps */
    int     path;                /* 0 original (15 knots), 1 ring (8, root kept), 2 figure eight (16, both nets), 3 one-knob timbre (15, dark -> bright) */
    int     pfade;               /* 1: fading foot follows sqrt (linear power) */
    float   norm;                /* 0..1: hold the loudness constant over the morph */
    float   pw[2], pw_t[2];      /* pulse width per DCO, 0.5 = square (current, target) */
    int     sync;                /* 1: DCO2 restarts at every 16' period of DCO1 (DOUBLE) */
    float   swp2;                /* DCO2 pitch offset 0..48 semitones, continuous (sync sweep) */
    float   fm;                  /* cross mod (xmod): linear FM index of DCO1 by DCO2, 0..16, through zero above 1 */
    float   fm2;                 /* external FM (extfm): index of both DCOs by fm_in, 0..16 */
    const float *fm_in;          /* external modulator for this block, or 0 */
    float   xm_e, xm_m;          /* DEG3 / MG -> cross mod index, +-16 / 0..16 */
    float   Wm[2][P8_FEET];      /* foot shape per DCO without level (the cross mod modulator) */
    float   pwm_d[2];            /* MG -> pulse width, 0..0.47 */
    float   sw_e, sw_m;          /* DEG3 / MG -> DCO2 pitch, semitones */
    unsigned rm_mask;            /* ring modulator: product of these feet of DCO1 (0x8 = 16') */
    float   h_m;                 /* MG -> timbre position har, +- steps */
    float   rmod;                /* 0..1: DCO2 times the 16' of DCO1 (XOR ring modulation) */
    float   ws_m, ws_e;          /* MG / DEG3 -> Walsh loop position, in loop steps 0..127 */
    float   wlev[2];             /* Walsh amplitude per DCO (mix x level) */
    float   drive, drv_g, drv_c; /* overdrive 0..1: gain, makeup */
    /* smoothed copies of continuous settings (time: smooth_ms), so that 7-bit
     * MIDI CC steps become ramps: cutoff (octaves), resonance, FM, ring and
     * noise per sample, the others per block */
    struct { float cut, res, fm[2], ring, nois, det, eg, swp, mgc, mgv, drv, dfb, dlev,
             swe, swm, pwm[2], hm, wsm, wse, tr, bs; } sm;
    int     ddl_glide;           /* 1: delay time changes glide instead of muting for 40 ms (original) */
    int     drv_pre;             /* 0: after the VCF, 1: before it */
    float   e3n;                 /* DEG3 level 0..1 (last sample) */
    float   vel_amp, vel_cut;    /* velocity -> level 0..1, -> cutoff in octaves */
    float   vel_last;            /* velocity of the last key, 0..1 (one VCF for the instrument) */
    float   drift;               /* 0..1: analogue drift of pitch, envelopes, MG rate, cutoff */
    float   mg_dr, f_dr;         /* drift states of MG rate and cutoff */
    uint32_t drng;
    float   mg_val;              /* MG triangle (last sample, after the delay) */
    float   wpos[2], wmix[2];    /* Walsh morph per DCO: position 0..127 on the loop, mix 0..1 */
    float   Wc[2][16], Wct[2][16];   /* Walsh coefficients by foot subset (current, target) */
    int     wal[2];              /* Walsh part active */
    float   env[3];              /* DEG morph position 0..127 (DEG1, DEG2, DEG3), < 0: raw values in deg */
    float   deg[3][6];           /* DEG1 .. DEG3: attack, decay, break point, slope, sustain, release (0..31) */
    p8_deg_t dc[3];
    p8_tg_t deg3;                /* DEG3 state (one for the instrument: VCF and noise) */
    int     trig3;               /* DEG3 trigger: 1 single (first key only), 2 multi (every key) */
    float   vcf_c, vcf_r;        /* VCF cutoff 0..99, resonance 0..15 */
    int     kbd;                 /* keyboard tracking: 0 off, 1 half, 2 full (highest key, C4 = nominal) */
    float   egint;               /* DEG3 -> cutoff, 0..15, sign = polarity (negative: inverted) */
    float   kbd_note;            /* highest key held (kept while released) */
    float   vcf_s[2][4];         /* ladder states L / R */
    float   op_s[2], op_g;       /* output pole (about 10 kHz) */
    int     leak;                /* 1: released voices stay at -55 dB, as measured */
    float   port_s, port_k;      /* portamento time (s) and per-sample factor */
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

    int     tone;                /* 1: measured Poly-800 output response */

    /* equalizer (Poly-800 II, 76/77): first-order shelves, -6..6 steps = -10..+10 dB */
    float   eq_tr, eq_bs;        /* steps -6..6 */
    float   eq_c[2][3];          /* treble / bass: b0 b1 a1 */
    float   eq_s[2][2];          /* states per channel: treble, bass */

    /* digital delay (Poly-800 II, 71-75), mono, added to both outputs */
    float   ddl_time, ddl_fb, ddl_mf, ddl_mi, ddl_lev;   /* raw: 0..99, 0..15, 0..31, 0..31, 0..15 */
    float   ddl_ms, ddl_ms_s;    /* delay time in ms (target, in use) */
    float   ddl_mute;            /* 0..1: the original mutes the delay output briefly on changes */
    float   ddl_env, ddl_rel, ddl_gate;   /* echo gate */
    float   ddl_g;               /* mod depth factor 0..1 */
    float   ddl_ph;              /* mod LFO phase */
    float   ddl_lp[2], ddl_lpg;  /* 2-pole low-pass in the delay path (band limit of the DDL) */
    unsigned ddl_w;
    /* performance offsets (not part of a sound), in DEG steps: attack of DEG1/DEG2, release
     * of DEG1/DEG2/DEG3, laid over the DEG values (own or morphed) */
    float   perf_att, perf_rel;
    float   ddl_buf[P8_DDL];
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
void  p8_mg_key(p8_engine_t *e);         /* a key pressed: MG retrigger */
void  p8_voice_glide(p8_engine_t *e, int k, float note, float from);   /* portamento from a note */
void  p8_voice_off(p8_engine_t *e, int k);
void  p8_voice_pitch(p8_engine_t *e, int k, float note, float from);   /* legato: no retrigger */
void  p8_set_port(p8_engine_t *e, float sec);
int   p8_voice_sounding(const p8_engine_t *e, int k);
void  p8_retune(p8_engine_t *e);             /* after octave / interval change */
void  p8_set_eq(p8_engine_t *e, float treble, float bass);   /* steps -6..6 */
void  p8_set_ddl(p8_engine_t *e, float time, float fb, float mf, float mi, float lev);   /* Poly-800 II units */
void  p8_voice_vel(p8_engine_t *e, int k, float vel);   /* 0..1, before the note */
void  p8_set_drive(p8_engine_t *e, float d);   /* overdrive 0..1 after the VCF */

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
