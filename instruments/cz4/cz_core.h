/* SPDX-License-Identifier: BSD-3-Clause
 * cz_core: paraphonic Casio CZ (uPD933) phase-distortion core with a
 * switchable "vintage" layer, for the Bela Pepper (Eurorack).
 *
 * Derived from melodee firmware/src/cz_native.c
 *   Copyright (c) Devin Acker (uPD933 model, MAME)
 *   Modifications Copyright (c) 2026 Kerem Kilic (Ellic Studio)
 * See LICENSE-BSD-3-uPD933.txt.
 *
 * Scope (spec v0.2): no envelopes. DCW comes from an external envelope via CV,
 * DCA is an external VCA. 4 voices share one DCW value (paraphonic). Pitches
 * arrive per voice (USB-MIDI in the Bela layer); one gate input resets the
 * phase of all voices.
 *
 * Plain C99, no allocation, no platform dependencies. Not thread-safe: call
 * every function from the audio thread (or guard parameter writes yourself). */
#ifndef CZ_CORE_H
#define CZ_CORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CZ_VOICES 4

/* Chip waveforms (uPD933 codes), as used inside a CZ tone:
 * 0 saw, 1 square, 2 pulse, 3 silent, 4 double sine, 5 saw-pulse,
 * 6 resonance (frequency follows DCW), 7 double pulse.
 * Windows: 0 none, 1 saw, 2 triangle, 3 trapezoid, 4 pulse, 5 double saw,
 * 6 raised cosine (patent only). */
enum { CZ_WAVE_SAW, CZ_WAVE_SQUARE, CZ_WAVE_PULSE, CZ_WAVE_SILENT,
       CZ_WAVE_DSINE, CZ_WAVE_SAWPULSE, CZ_WAVE_RESO, CZ_WAVE_DPULSE };
enum { CZ_WIN_NONE, CZ_WIN_SAW, CZ_WIN_TRI, CZ_WIN_TRAP, CZ_WIN_PULSE, CZ_WIN_DSAW,
       CZ_WIN_COS /* raised cosine, from the Casio patent; not on the CZ chip */ };

/* Continuous wave position. Integer positions are the chip combinations
 * below, ordered so that neighbours are harmonically close:
 *   0 saw            1 saw-pulse       2 square          3 pulse
 *   4 reso, saw win  5 reso, tri win   6 reso, cosine    7 reso, trapezoid
 *   8 reso, pulse    9 reso, dbl saw  10 double sine    11 double pulse
 * The raised-cosine window (6) is described in Casio's patent US4658691
 * (Fig. 24) but is not one of the CZ chip's windows. 
 * In between, with phase_morph on, the two neighbours' bent phases (and
 * windows) are interpolated: one single wave at every point. Where the
 * neighbours differ in cycle count (3->4, 9->10) or phase_morph is off,
 * their outputs are crossfaded at a shared phase. */
#define CZ_MORPH_MAX 11.0f

/* Line modes as on the CZ: 1, 2, 1+1' (line 2 = detuned copy of line 1),
 * 1+2' (line 2 with its own waves, detuned). */
enum { CZ_LINE_1, CZ_LINE_2, CZ_LINE_1_1, CZ_LINE_1_2 };

/* Modulation flags, may be combined. Ring replaces line 2 by line1*line2;
 * noise randomly jumps line 2's pitch (as the chip's noise modulation). */
enum { CZ_MOD_RING = 1, CZ_MOD_NOISE = 2 };

typedef struct {
    float pos;         /* wave position 0..CZ_MORPH_MAX, continuous        */
    float pair;        /* second wave of a CZ wave pair (every other cycle) */
    int   pair_on;     /* 0: every cycle plays pos                          */
} cz_line_t;

/* Vintage layer. All stages are bypassed at their neutral value; the ERA
 * macro (cz_set_era) sets them together, or write the fields directly and
 * call cz_vintage_update(). */
typedef struct {
    float quant;     /* 0..1 crossfade clean float -> chip-quantized wave     */
    float chip_hz;   /* hold rate of the oscillators + DAC, 0 = off (= fs)     */
    int   dac_bits;  /* 16 = transparent .. 8                                  */
    float dac_err;   /* 0..1 R-2R bit-weight mismatch                          */
    float noise_db;  /* DAC hiss level in dBFS, <= -120 = off                   */
    float lpf_hz;    /* output / reconstruction low-pass, 0 = off              */
    float drive;     /* 0..1 soft saturation of the output stage               */
    float drift;     /* 0..1 slow random pitch wander per voice (not in ERA)   */
} cz_vintage_t;

typedef struct {
    uint32_t ph[2], tog[2];   /* phase accumulators, wave-pair toggles  */
    float    dc[2];           /* DC blockers per line                   */
    uint32_t noise;           /* xorshift state for noise modulation    */
    float    note;            /* MIDI note number (float)               */
    float    gain, gain_target;
    float    note2;           /* line 2 when split (own MIDI channel)   */
    float    gain2, gain2_target;
    float    drift, drift_goal; uint32_t drift_rng; uint32_t drift_count;
    float    ring_ms, ring_pk, ring_dc;  /* ring mod: line 1 mean square and peak, DC of the product */
} cz_voice_t;

typedef struct {
    /* ---- parameters (write freely between render calls) ---- */
    float     fs;            /* audio rate, set by cz_init           */
    float     a4;            /* concert pitch, default 440           */
    cz_line_t line[2];
    int       line_mode;     /* CZ_LINE_*                            */
    int       mod;           /* CZ_MOD_* flags                       */
    int       octave;        /* -1, 0, +1 (whole tone)               */
    float     detune;        /* line 2, signed semitones (cz_detune) */
    float     line_amp[2];   /* level of line 1 and line 2 (two-line modes),
                                0..1 each, default 0.5; with ring modulation
                                line_amp[1] is the level of the ring product */
    float     spread;        /* 0 mono .. 1 wide                     */
    float     level;         /* master gain, default 0.4             */
    cz_vintage_t vint;
    float     morph_ms;      /* smoothing of wave positions, default 8 ms */
    int       phase_morph;   /* 1: morph the bent phase (default); 0: crossfade */
    int       sine0;         /* 1: DCW 0 = fundamental sine everywhere (default);
                                0: the chip's own behaviour (octave, windows) */
    int       split;         /* 0: line 2 follows the voice; 1: line 2 has its own
                                notes and gain (cz_line2_*), like a second osc */
    /* ---- state ---- */
    float pos_s[2][2];                  /* smoothed wave positions [line][pair] */
    float amp_s[2];                     /* line levels at the end of the last block */
    cz_voice_t v[CZ_VOICES];
    float era_target, era;              /* ERA macro and its smoothed value  */
    int   era_active;                   /* 1 once cz_set_era has been called  */
    float chip_acc, hold_l, hold_r;     /* sample-and-hold                    */
    float dcw_last;
    float lp_b0, lp_b1, lp_b2, lp_a1, lp_a2, lp_z[2][2], lp_hz_cur;
    uint32_t hiss_rng;
    float dac_w[16], dac_mid;           /* R-2R weights for dac_bits          */
    int   dac_w_bits; float dac_w_err;
} cz_engine_t;

void  cz_init(cz_engine_t *e, float fs);

/* Voices. cz_voice_on fades the voice in at the given pitch (no phase reset:
 * the global gate does that); cz_voice_off fades it out over ~4 ms. */
void  cz_voice_note(cz_engine_t *e, int voice, float midi_note);
void  cz_voice_on(cz_engine_t *e, int voice, float midi_note);
void  cz_voice_off(cz_engine_t *e, int voice);
void  cz_gate_reset(cz_engine_t *e);          /* rising gate edge: all phases to 0 */
void  cz_voice_reset(cz_engine_t *e, int voice); /* one voice's phases to 0 (CZ note-on) */
/* With split on, line 2 of each voice is played on its own: */
void  cz_line2_on(cz_engine_t *e, int voice, float midi_note);   /* also resets its phase */
void  cz_line2_off(cz_engine_t *e, int voice);
/* Note on the voice functions with split on: they then drive line 1 only. */

/* Render n samples. dcw0..dcw1 (0..1) is ramped across the block; it is
 * the sum of offset knob and external envelope CV, clamped by the caller.
 * cz_render_dcw takes DCW per sample instead (dcw[i], 0..1). */
void  cz_render(cz_engine_t *e, float *out_l, float *out_r, unsigned n, float dcw0, float dcw1);
void  cz_render_dcw(cz_engine_t *e, float *out_l, float *out_r, unsigned n, const float *dcw);
/* Separate DCW ramps for line 1 and line 2 (0..1 each). */
void  cz_render2(cz_engine_t *e, float *out_l, float *out_r, unsigned n,
                 float dcw1_0, float dcw1_1, float dcw2_0, float dcw2_1);

/* ERA macro 0 (clean) .. 1 (crunchy). Smoothed internally. */
void  cz_set_era(cz_engine_t *e, float era);
void  cz_vintage_update(cz_engine_t *e);      /* after writing e->vint directly */

/* Helpers */
float cz_detune(int sign, int octave, int note, int fine); /* CZ detune: oct 0-3, note 0-11, fine 0-60 */
/* Integer position for a chip wave/window combination (for tone import). */
float cz_pos_from_chip(unsigned wave, unsigned window);
/* Take wave, window, line, modulation, octave and detune from a native CZ-1
 * tone (128 or 144 bytes, decoded, Casio MIDI spec); envelopes are ignored.
 * Returns 0 on success. */
int   cz_import_tone(cz_engine_t *e, const uint8_t *tone, unsigned len);

/* The chip-quantized and the clean wave function, exposed for tests.
 * wave 0..7, window 0..5, ph: 32-bit phase, dcw: chip units 0..1023. */
float cz_wave_clean(unsigned wave, unsigned window, uint32_t ph, float dcw);
float cz_wave_chip(unsigned wave, unsigned window, uint32_t ph, float dcw);

#ifdef __cplusplus
}
#endif
#endif
