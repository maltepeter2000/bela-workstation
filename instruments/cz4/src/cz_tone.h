/* SPDX-License-Identifier: BSD-3-Clause
 * cz_tone: Casio CZ tone data -- the 128 synthesis bytes of the CZ-101,
 * CZ-1000, CZ-3000 and CZ-5000, and the 144-byte tone of the CZ-1 (the same
 * 128 bytes with line levels and velocity amounts in spare nibbles, then a
 * 16-character name). Panel values <-> bytes, and the SysEx transport.
 *
 * Written from the Casio MIDI specifications as documented by melodee
 * (docs/CZ1_SYSEX.md) and the CZ-101 MIDI specification. Plain C99, no
 * allocation. Not Casio code or data. */
#ifndef CZ_TONE_H
#define CZ_TONE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CZ_TONE_BYTES   144
#define CZ_TONE_OLD     128
#define CZ_NAME_LEN     16

/* Byte offsets. A line block starts at 14 (line 1) or 71 (line 2). */
enum {
    CZB_PFLAG = 0,      /* bits 0-1 line select, bits 2-3 octave           */
    CZB_PDS = 1,        /* detune sign: 0 +, 1 -                           */
    CZB_PDETL = 2,      /* detune fine, six bits in positions 7..2         */
    CZB_PDETH = 3,      /* detune octave * 12 + note                       */
    CZB_PVK = 4,        /* vibrato wave: 8 tri, 4 saw up, 32 saw down, 2 sq */
    CZB_PVD = 5,        /* vibrato delay: panel value, then 16 bits (lo hi) */
    CZB_PVS = 8,        /* vibrato rate                                    */
    CZB_PVV = 11,       /* vibrato depth                                   */
    CZB_LINE1 = 14, CZB_LINE2 = 71,
    CZB_NAME = 128
};
/* Inside a line block: wave word (2), DCA key follow (2), DCW key follow (2),
 * then DCA, DCW, DCO envelopes: end byte + 8 x (rate, level). */
enum { CZL_WAVE = 0, CZL_KFA = 2, CZL_KFW = 4, CZL_DCA = 6, CZL_DCW = 23, CZL_DCO = 40 };
enum { CZ_ENV_DCO = 0, CZ_ENV_DCW = 1, CZ_ENV_DCA = 2 };

/* Panel values of a tone (as the CZ shows them):
 *   tone:  linesel 0..3 (1, 2, 1+1', 1+2'), octave -1..1, mod 0 off 1 ring 2 noise,
 *          dsign 0 + 1 -, doct 0..3, dnote 0..11, dfine 0..60,
 *          vwave 0 tri 1 saw up 2 saw down 3 square, vdelay vrate vdepth 0..99
 *   line:  wave 0..7 (chip codes: saw square pulse null dsine sawpulse reso pulse2),
 *          wave2 0 off / 1..8 = code + 1, window 0..7, kfw kfa 0..9, level 1..15,
 *          velp velw vela 0..15
 *   env:   rate1..8, level1..8 0..99, sus 0 off / 1..8, end 1..8 */
enum {
    CZP_LINESEL, CZP_OCTAVE, CZP_MOD, CZP_DSIGN, CZP_DOCT, CZP_DNOTE, CZP_DFINE,
    CZP_VWAVE, CZP_VDELAY, CZP_VRATE, CZP_VDEPTH,
    CZP_WAVE, CZP_WAVE2, CZP_WINDOW, CZP_KFW, CZP_KFA, CZP_LEVEL, CZP_VELP, CZP_VELW, CZP_VELA,
    CZP_RATE1, CZP_LEVEL1 = CZP_RATE1 + 8, CZP_SUS = CZP_LEVEL1 + 8, CZP_END,
    CZP_COUNT
};
int  cz_get(const uint8_t *b, int param, int line, int env);
/* Writes only the bytes of that value (and the direction bits of its
 * envelope), so an imported tone keeps everything else bit for bit. */
void cz_set(uint8_t *b, int param, int line, int env, int value);
void cz_param_range(int param, int *lo, int *hi);

/* A plain tone: sawtooth, organ-like envelope, name "INIT TONE". */
void cz_tone_init(uint8_t *b);
/* A 128-byte tone into 144 bytes: line levels 15, no velocity, a name. */
void cz_tone_from128(uint8_t *b144, const uint8_t *b128, const char *name);
/* Name: 16 characters, padded with spaces; get gives a trimmed C string. */
void cz_name_get(const uint8_t *b, char *out17);
void cz_name_set(uint8_t *b, const char *name);
/* Light sanity check, as melodee does (rejects data that is not a tone). */
int  cz_tone_valid(const uint8_t *b);
/* Line 2 copies line 1, or the other way: line block and its own fields. */
void cz_copy_line(uint8_t *b, int from, int to);

/* ---- the envelope of a line, decoded for playback -------------------- */
typedef struct {
    uint8_t rate[8];    /* chip rate code 0..127 (the rate byte without bit 7) */
    uint8_t level[8];   /* chip level byte without bit 7                       */
    int8_t  sus;        /* step with the sustain mark before end, -1 = none    */
    uint8_t end;        /* 0..7                                                */
} cz_env_t;
void cz_env_decode(const uint8_t *b, int line, int env, cz_env_t *out);

/* ---- SysEx --------------------------------------------------------------
 * A CZ sends every byte as two nibbles, low first. A tone message is
 *   F0 44 00 00 7n <cmd> [prog] <nibbles> F7
 * where the data are 256 nibbles (128 bytes) or 288 (144 bytes). Files of
 * librarians hold the "receive request" form (cmd 20 / 21, then prog) with the
 * data in the same message, as a computer sends it to a CZ. */
void cz_nibbles_out(const uint8_t *bytes, int n, uint8_t *out2n);
int  cz_nibbles_in(const uint8_t *in2n, int n, uint8_t *bytes);   /* 0 if a byte >= 16 */
/* Scans a buffer (a .syx file or a capture) for tone messages. For each tone
 * found, calls got(ctx, tone144, prog, was144). Returns the number found. */
typedef void (*cz_tone_cb)(void *ctx, const uint8_t *tone144, int prog, int was144);
int  cz_syx_scan(const uint8_t *buf, long len, cz_tone_cb got, void *ctx);
/* One tone as a librarian message: F0 44 00 00 7n 21 prog <288 nibbles> F7
 * (len144 = 1) or 20 prog <256 nibbles> (0). Returns the length (296 or 264). */
int  cz_syx_tone(const uint8_t *tone144, int channel, int prog, int len144, uint8_t *out);

/* ---- vibrato data: the machine value Casio stores after a panel value ---- */
unsigned cz_vib_machine(int kind /* 0 delay 1 rate 2 depth */, int panel);

#ifdef __cplusplus
}
#endif
#endif
