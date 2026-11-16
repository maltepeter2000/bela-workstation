/* SPDX-License-Identifier: BSD-3-Clause
 * cz_tone: Casio CZ tone data, see cz_tone.h. */
#include "cz_tone.h"
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int line_base(int line) { return line ? CZB_LINE2 : CZB_LINE1; }
static int env_base(int line, int env)
{
    static const int off[3] = { CZL_DCO, CZL_DCW, CZL_DCA };
    return line_base(line) + off[clampi(env, 0, 2)];
}

/* Key-follow machine values (second byte), by panel value 0..9. */
static const uint8_t KFA_MACHINE[10] = { 0, 8, 17, 26, 36, 47, 58, 69, 82, 95 };
static const uint8_t KFW_MACHINE[10] = { 0, 31, 44, 57, 70, 83, 96, 111, 146, 255 };
static const uint8_t VWAVE_BYTE[4] = { 8, 4, 32, 2 };

/* Rate and level of an envelope step, panel 0..99 <-> byte (without bit 7). */
static int rate_byte(int env, int a)
{
    return env == CZ_ENV_DCO ? a * 127 / 99 : env == CZ_ENV_DCW ? 8 + a * 119 / 99 : a * 119 / 99;
}
static int level_byte(int env, int a)
{
    return env == CZ_ENV_DCO ? a + (a > 63 ? 4 : 0) : env == CZ_ENV_DCW ? a * 127 / 99 : (a ? a + 28 : 0);
}
static int rate_panel(int env, int b)
{
    b &= 127;
    if (env == CZ_ENV_DCW) b = b > 8 ? b - 8 : 0;
    if (!b) return 0;
    int a = b * 99 / (env == CZ_ENV_DCO ? 127 : 119) + 1;
    return a > 99 ? 99 : a;
}
static int level_panel(int env, int b)
{
    b &= 127;
    int a = env == CZ_ENV_DCO ? (b > 63 ? b - 4 : b)
          : env == CZ_ENV_DCW ? (b == 127 ? 99 : b ? b * 99 / 127 + 1 : 0)
          : (b > 28 ? b - 28 : 0);
    return a > 99 ? 99 : a;
}

unsigned cz_vib_machine(int kind, int n)
{
    n = clampi(n, 0, 99);
    int g = n < 32 ? 0 : (n - 16) / 16;       /* groups of 16 above 31 */
    unsigned step = 1u << g;
    if (kind == 1) {
        if (n == 31) return 0x420;                /* as in Casio's own tones */
        static const unsigned start[6] = { 0x20, 0x460, 0x8e0, 0x11e0, 0x23e0, 0x47e0 };
        return start[g] + 32u * step * (unsigned)(n - (g ? 16 + 16 * g : 0));
    }
    unsigned v = g ? (unsigned)(n - 16 - 16 * g) * step + ((17u << g) - 1u) : (unsigned)n;
    if (kind == 2) return n == 99 ? 0x300 : v + step;
    return v;
}

void cz_param_range(int p, int *lo, int *hi)
{
    *lo = 0;
    switch (p) {
    case CZP_LINESEL: *hi = 3; break;
    case CZP_OCTAVE: *lo = -1; *hi = 1; break;
    case CZP_MOD: *hi = 2; break;
    case CZP_DSIGN: *hi = 1; break;
    case CZP_DOCT: *hi = 3; break;
    case CZP_DNOTE: *hi = 11; break;
    case CZP_DFINE: *hi = 60; break;
    case CZP_VWAVE: *hi = 3; break;
    case CZP_WAVE: *hi = 7; break;
    case CZP_WAVE2: *hi = 8; break;
    case CZP_WINDOW: *hi = 7; break;
    case CZP_KFW: case CZP_KFA: *hi = 9; break;
    case CZP_LEVEL: *lo = 1; *hi = 15; break;
    case CZP_VELP: case CZP_VELW: case CZP_VELA: *hi = 15; break;
    case CZP_SUS: *hi = 8; break;
    case CZP_END: *lo = 1; *hi = 8; break;
    default: *hi = 99; break;   /* vibrato, rates, levels */
    }
}

int cz_get(const uint8_t *b, int p, int line, int env)
{
    int o = line_base(line), e = env_base(line, env);
    switch (p) {
    case CZP_LINESEL: return b[CZB_PFLAG] & 3;
    case CZP_OCTAVE: { int k = (b[CZB_PFLAG] >> 2) & 3; return k == 1 ? 1 : k == 2 ? -1 : 0; }
    case CZP_MOD: { int m = (b[CZB_LINE1 + 1] >> 3) & 7; return m == 4 ? 1 : m == 3 ? 2 : 0; }
    case CZP_DSIGN: return b[CZB_PDS] & 1;
    case CZP_DOCT: return clampi(b[CZB_PDETH] / 12, 0, 3);
    case CZP_DNOTE: return b[CZB_PDETH] % 12;
    case CZP_DFINE: { int f = b[CZB_PDETL] >> 2; return clampi(f - (f >> 4), 0, 60); }
    case CZP_VWAVE: { int w = b[CZB_PVK]; return w == 4 ? 1 : w == 32 ? 2 : w == 2 ? 3 : 0; }
    case CZP_VDELAY: return clampi(b[CZB_PVD], 0, 99);
    case CZP_VRATE: return clampi(b[CZB_PVS], 0, 99);
    case CZP_VDEPTH: return clampi(b[CZB_PVV], 0, 99);
    case CZP_WAVE: return b[o] >> 5;
    case CZP_WAVE2: return (b[o] & 2) ? 1 + ((b[o] >> 2) & 7) : 0;
    case CZP_WINDOW: return ((b[o] & 1) << 2) | (b[o + 1] >> 6);
    case CZP_KFA: return clampi(b[o + CZL_KFA] & 15, 0, 9);
    case CZP_KFW: return clampi(b[o + CZL_KFW] & 15, 0, 9);
    case CZP_LEVEL: return clampi(15 - (b[o + CZL_KFA] >> 4), 1, 15);
    case CZP_VELP: return 15 - (b[o + CZL_DCO] >> 4);
    case CZP_VELW: return 15 - (b[o + CZL_DCW] >> 4);
    case CZP_VELA: return 15 - (b[o + CZL_DCA] >> 4);
    case CZP_SUS: {
        int end = b[e] & 7;
        for (int j = 0; j < end; j++) if (b[e + 2 + 2 * j] & 128) return j + 1;
        return 0;
    }
    case CZP_END: return (b[e] & 7) + 1;
    default:
        if (p >= CZP_RATE1 && p < CZP_RATE1 + 8) return rate_panel(env, b[e + 1 + 2 * (p - CZP_RATE1)]);
        if (p >= CZP_LEVEL1 && p < CZP_LEVEL1 + 8) {
            int j = p - CZP_LEVEL1;
            if (j == (b[e] & 7)) return 0;        /* the CZ ignores END's level */
            return level_panel(env, b[e + 2 + 2 * j]);
        }
    }
    return 0;
}

/* Direction bit (bit 7 of a rate byte): set when the step goes down. Casio
 * tones are not fully consistent about it on END and later steps, so only
 * the steps next to an edit are recomputed; the player itself does not use
 * the bit (it compares the current value with the target). */
static void fix_dir(uint8_t *b, int line, int env, int j)
{
    int e = env_base(line, env), end = b[e] & 7;
    if (j < 0 || j > 7) return;
    int lv = j == end ? 0 : (b[e + 2 + 2 * j] & 127);
    int prev = j ? (b[e + 2 * j] & 127) : 0;
    int down = j <= end && lv < prev;
    b[e + 1 + 2 * j] = (uint8_t)((b[e + 1 + 2 * j] & 127) | (down ? 128 : 0));
}

void cz_set(uint8_t *b, int p, int line, int env, int v)
{
    int lo, hi, o = line_base(line), e = env_base(line, env);
    cz_param_range(p, &lo, &hi);
    v = clampi(v, lo, hi);
    if (cz_get(b, p, line, env) == v) return;     /* unchanged: keep the bytes exactly */
    switch (p) {
    case CZP_LINESEL: b[CZB_PFLAG] = (uint8_t)((b[CZB_PFLAG] & ~3) | v); return;
    case CZP_OCTAVE: b[CZB_PFLAG] = (uint8_t)((b[CZB_PFLAG] & 3) | ((v == 1 ? 1 : v == -1 ? 2 : 0) << 2)); return;
    case CZP_MOD:
        b[CZB_LINE1 + 1] = (uint8_t)((b[CZB_LINE1 + 1] & ~0x38) | ((v == 1 ? 4 : v == 2 ? 3 : 0) << 3));
        return;
    case CZP_DSIGN: b[CZB_PDS] = (uint8_t)v; return;
    case CZP_DOCT: b[CZB_PDETH] = (uint8_t)(v * 12 + b[CZB_PDETH] % 12); return;
    case CZP_DNOTE: b[CZB_PDETH] = (uint8_t)(clampi(b[CZB_PDETH] / 12, 0, 3) * 12 + v); return;
    case CZP_DFINE: b[CZB_PDETL] = (uint8_t)((v + (v ? (v - 1) / 15 : 0)) << 2); return;
    case CZP_VWAVE: b[CZB_PVK] = VWAVE_BYTE[v]; return;
    case CZP_VDELAY: case CZP_VRATE: case CZP_VDEPTH: {
        int kind = p == CZP_VDELAY ? 0 : p == CZP_VRATE ? 1 : 2, at = CZB_PVD + 3 * kind;
        unsigned m = cz_vib_machine(kind, v);
        b[at] = (uint8_t)v; b[at + 1] = (uint8_t)(m & 255); b[at + 2] = (uint8_t)(m >> 8);
        return;
    }
    case CZP_WAVE: b[o] = (uint8_t)((b[o] & 0x1F) | (v << 5)); return;
    case CZP_WAVE2:
        b[o] = (uint8_t)((b[o] & 0xE1) | (v ? (((v - 1) << 2) | 2) : 0));
        return;
    case CZP_WINDOW:
        b[o] = (uint8_t)((b[o] & ~1) | (v >> 2));
        b[o + 1] = (uint8_t)((b[o + 1] & 0x3F) | ((v & 3) << 6));
        return;
    case CZP_KFA:
        b[o + CZL_KFA] = (uint8_t)((b[o + CZL_KFA] & 0xF0) | v); b[o + CZL_KFA + 1] = KFA_MACHINE[v]; return;
    case CZP_KFW:
        b[o + CZL_KFW] = (uint8_t)((b[o + CZL_KFW] & 0xF0) | v); b[o + CZL_KFW + 1] = KFW_MACHINE[v]; return;
    case CZP_LEVEL: b[o + CZL_KFA] = (uint8_t)((b[o + CZL_KFA] & 15) | ((15 - v) << 4)); return;
    case CZP_VELP: b[o + CZL_DCO] = (uint8_t)((b[o + CZL_DCO] & 15) | ((15 - v) << 4)); return;
    case CZP_VELW: b[o + CZL_DCW] = (uint8_t)((b[o + CZL_DCW] & 15) | ((15 - v) << 4)); return;
    case CZP_VELA: b[o + CZL_DCA] = (uint8_t)((b[o + CZL_DCA] & 15) | ((15 - v) << 4)); return;
    case CZP_SUS:
        for (int j = 0; j < 8; j++) b[e + 2 + 2 * j] &= 127;
        if (v && v - 1 < (b[e] & 7)) b[e + 2 + 2 * (v - 1)] |= 128;
        return;
    case CZP_END: {
        int sus = cz_get(b, CZP_SUS, line, env);
        int old = b[e] & 7;
        b[e] = (uint8_t)((b[e] & 0xF0) | (v - 1));
        if (sus && sus - 1 >= v - 1) for (int j = 0; j < 8; j++) b[e + 2 + 2 * j] &= 127;
        fix_dir(b, line, env, old); fix_dir(b, line, env, old + 1);
        fix_dir(b, line, env, v - 1); fix_dir(b, line, env, v);
        return;
    }
    default:
        if (p >= CZP_RATE1 && p < CZP_RATE1 + 8) {
            int at = e + 1 + 2 * (p - CZP_RATE1);
            b[at] = (uint8_t)((b[at] & 128) | rate_byte(env, v));
        } else if (p >= CZP_LEVEL1 && p < CZP_LEVEL1 + 8) {
            int at = e + 2 + 2 * (p - CZP_LEVEL1);
            b[at] = (uint8_t)((b[at] & 128) | level_byte(env, v));
            fix_dir(b, line, env, p - CZP_LEVEL1); fix_dir(b, line, env, p - CZP_LEVEL1 + 1);
        }
    }
}

void cz_env_decode(const uint8_t *b, int line, int env, cz_env_t *d)
{
    int e = env_base(line, env);
    d->end = (uint8_t)(b[e] & 7);
    d->sus = -1;
    for (int j = 0; j < 8; j++) {
        d->rate[j] = (uint8_t)(b[e + 1 + 2 * j] & 127);
        d->level[j] = (uint8_t)(b[e + 2 + 2 * j] & 127);
        if ((b[e + 2 + 2 * j] & 128) && j < d->end && d->sus < 0) d->sus = (int8_t)j;
    }
}

/* The CZ-1 centres its names on the LCD: leading and trailing spaces go. */
void cz_name_get(const uint8_t *b, char *out)
{
    int n = 0, start = -1;
    for (int i = 0; i < CZ_NAME_LEN; i++) {
        uint8_t c = b[CZB_NAME + i];
        char ch = (c >= 32 && c < 127) ? (char)c : ' ';
        if (ch == ' ' && start < 0) continue;
        if (start < 0) start = i;
        out[n++] = ch;
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

void cz_name_set(uint8_t *b, const char *name)
{
    int i = 0;
    for (; i < CZ_NAME_LEN && name && name[i]; i++) {
        char c = name[i] == '_' ? ' ' : name[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);      /* the CZ-1 LCD is upper case */
        b[CZB_NAME + i] = (uint8_t)((c >= 32 && c < 127) ? c : ' ');
    }
    for (; i < CZ_NAME_LEN; i++) b[CZB_NAME + i] = ' ';
}

int cz_tone_valid(const uint8_t *b)
{
    if ((b[0] & 0xF0) || ((b[0] >> 2) & 3) > 2 || b[1] > 1 || (b[2] & 3) || b[3] > 47) return 0;
    for (int l = 0; l < 2; l++) {
        int o = line_base(l);
        if ((b[o + CZL_KFA] & 15) > 9 || (b[o + CZL_KFW] & 15) > 9) return 0;
        for (int e = 0; e < 3; e++) if (b[env_base(l, e)] & 8) return 0;
    }
    return 1;
}

void cz_copy_line(uint8_t *b, int from, int to)
{
    if (from == to) return;
    int fo = line_base(from), to_ = line_base(to);
    uint8_t keepmod = b[CZB_LINE1 + 1] & 0x38;      /* MOD lives in line 1's wave word */
    memcpy(b + to_, b + fo, 57);
    b[CZB_LINE1 + 1] = (uint8_t)((b[CZB_LINE1 + 1] & ~0x38) | keepmod);
    b[CZB_LINE2 + 1] &= (uint8_t)~0x38;
}

/* Organ-like: saw, DCW a bit open, DCA fast up, sustain, fast release. */
void cz_tone_init(uint8_t *b)
{
    memset(b, 0, CZ_TONE_BYTES);
    cz_set(b, CZP_VWAVE, 0, 0, 0);
    for (int k = 0; k < 3; k++) cz_set(b, CZP_VDELAY + k, 0, 0, 0);
    for (int l = 0; l < 2; l++) {
        cz_set(b, CZP_LEVEL, l, 0, 15);
        for (int e = 0; e < 3; e++) {
            int o = env_base(l, e);
            b[o] = 0xF0;                                  /* velocity 0 */
            cz_set(b, CZP_END, l, e, 2);
            cz_set(b, CZP_RATE1, l, e, 99);
            cz_set(b, CZP_RATE1 + 1, l, e, e == CZ_ENV_DCA ? 70 : 50);
            cz_set(b, CZP_LEVEL1, l, e, e == CZ_ENV_DCO ? 0 : e == CZ_ENV_DCW ? 30 : 99);
            cz_set(b, CZP_SUS, l, e, 1);
        }
    }
    cz_set(b, CZP_WAVE, 0, 0, 0); cz_set(b, CZP_WAVE, 1, 0, 0);
    cz_name_set(b, "INIT TONE");
}

void cz_tone_from128(uint8_t *b, const uint8_t *src, const char *name)
{
    memcpy(b, src, CZ_TONE_OLD);
    for (int l = 0; l < 2; l++) {
        int o = line_base(l);
        b[o + CZL_KFA] &= 15;                             /* level 15 */
        for (int e = 0; e < 3; e++) b[env_base(l, e)] = (uint8_t)((b[env_base(l, e)] & 7) | 0xF0);
    }
    cz_name_set(b, name ? name : "CZ TONE");
}

/* ------------------------------------------------------------- SysEx */
void cz_nibbles_out(const uint8_t *bytes, int n, uint8_t *out)
{
    for (int i = 0; i < n; i++) { out[2 * i] = bytes[i] & 15; out[2 * i + 1] = bytes[i] >> 4; }
}

int cz_nibbles_in(const uint8_t *in, int n, uint8_t *bytes)
{
    for (int i = 0; i < n; i++) {
        if (in[2 * i] > 15 || in[2 * i + 1] > 15) return 0;
        bytes[i] = (uint8_t)(in[2 * i] | (in[2 * i + 1] << 4));
    }
    return 1;
}

int cz_syx_scan(const uint8_t *buf, long len, cz_tone_cb got, void *ctx)
{
    int found = 0;
    for (long i = 0; i + 6 < len; i++) {
        if (buf[i] != 0xF0 || buf[i + 1] != 0x44 || buf[i + 2] || buf[i + 3] || (buf[i + 4] & 0xF0) != 0x70)
            continue;
        long j = i + 1;
        while (j < len && buf[j] != 0xF7) j++;
        if (j >= len) break;
        /* data start: after "cmd prog" (20/21 pp) or after "cmd" (30) */
        int prog = -1;
        long starts[2] = { i + 7, i + 6 };
        if (buf[i + 5] == 0x20 || buf[i + 5] == 0x21) prog = buf[i + 6];
        for (int s = 0; s < 2; s++) {
            long n = j - starts[s];
            uint8_t raw[CZ_TONE_BYTES];
            if (n != 2 * CZ_TONE_BYTES && n != 2 * CZ_TONE_OLD) continue;
            if (!cz_nibbles_in(buf + starts[s], (int)(n / 2), raw)) continue;
            uint8_t t[CZ_TONE_BYTES];
            if (n == 2 * CZ_TONE_BYTES) memcpy(t, raw, CZ_TONE_BYTES);
            else cz_tone_from128(t, raw, "CZ-101 TONE");
            if (!cz_tone_valid(t)) continue;
            got(ctx, t, prog, n == 2 * CZ_TONE_BYTES);
            found++;
            break;
        }
        i = j;
    }
    return found;
}

int cz_syx_tone(const uint8_t *t, int channel, int prog, int len144, uint8_t *out)
{
    int nb = len144 ? CZ_TONE_BYTES : CZ_TONE_OLD;
    out[0] = 0xF0; out[1] = 0x44; out[2] = 0; out[3] = 0;
    out[4] = (uint8_t)(0x70 | (channel & 15));
    out[5] = len144 ? 0x21 : 0x20;
    out[6] = (uint8_t)(prog & 127);
    cz_nibbles_out(t, nb, out + 7);
    out[7 + 2 * nb] = 0xF7;
    return 8 + 2 * nb;
}
