/* cz_tone test: read a .syx, decode every tone to panel values, encode them
 * again into a copy and compare byte for byte. usage: tone_test file.syx */
#include "cz_tone.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ntones, nbad, diffs[CZ_TONE_BYTES];

static void roundtrip(void *ctx, const uint8_t *t, int prog, int was144)
{
    (void)ctx; (void)prog; (void)was144;
    uint8_t c[CZ_TONE_BYTES];
    memcpy(c, t, CZ_TONE_BYTES);
    memset(c, 0, 128);              /* rebuild the 128 synthesis bytes from panel values */
    for (int p = 0; p < CZP_COUNT; p++) {
        int lines = (p >= CZP_WAVE) ? 2 : 1, envs = (p >= CZP_RATE1) ? 3 : 1;
        for (int l = 0; l < lines; l++)
            for (int e = 0; e < envs; e++) {
                if (p == CZP_SUS || (p >= CZP_LEVEL1 && p < CZP_LEVEL1 + 8)) continue;
                int lo, hi, v = cz_get(t, p, l, e);
                cz_param_range(p, &lo, &hi);
                cz_set(c, p, l, e, v == lo ? hi : lo);   /* force a write */
                cz_set(c, p, l, e, v);
            }
    }
    for (int l = 0; l < 2; l++) for (int e = 0; e < 3; e++) {
        cz_set(c, CZP_END, l, e, cz_get(t, CZP_END, l, e));
        int end = cz_get(t, CZP_END, l, e) - 1;
        for (int j = 0; j < 8; j++) {
            int ev = (j == end) ? 0 : 1;   /* END's level is not a panel value: keep its byte */
            if (!ev) { int at = (l ? 71 : 14) + (e == 0 ? 40 : e == 1 ? 23 : 6) + 2 + 2 * j; c[at] = t[at]; continue; }
            int v = cz_get(t, CZP_LEVEL1 + j, l, e);
            cz_set(c, CZP_LEVEL1 + j, l, e, v ? 0 : 99); cz_set(c, CZP_LEVEL1 + j, l, e, v);
        }
        cz_set(c, CZP_SUS, l, e, cz_get(t, CZP_SUS, l, e));
    }
    /* rebuilt from panel values: equal except direction bits of rate bytes */
    int bad = 0;
    static const int eo[3] = { 40, 23, 6 };
    uint8_t dirmask[CZ_TONE_BYTES]; memset(dirmask, 0xFF, sizeof dirmask);
    for (int l = 0; l < 2; l++) for (int e = 0; e < 3; e++)
        for (int j = 0; j < 8; j++) dirmask[(l ? 71 : 14) + eo[e] + 1 + 2 * j] = 0x7F;
    /* hidden bits of the wave word (unused modulation codes): only the
     * window bits are panel values there */
    dirmask[15] = dirmask[72] = 0xC0;
    if (cz_get(c, CZP_MOD, 0, 0) != cz_get(t, CZP_MOD, 0, 0)) bad = 1;
    for (int i = 0; i < CZ_TONE_BYTES; i++) if ((c[i] ^ t[i]) & dirmask[i]) { diffs[i]++; bad = 1; }
    /* setting every value to itself changes nothing */
    uint8_t d[CZ_TONE_BYTES]; memcpy(d, t, CZ_TONE_BYTES);
    for (int p = 0; p < CZP_COUNT; p++) for (int l = 0; l < 2; l++) for (int e = 0; e < 3; e++)
        cz_set(d, p, l, e, cz_get(d, p, l, e));
    if (memcmp(d, t, CZ_TONE_BYTES)) { printf("tone %d: set to itself changed bytes\n", ntones); bad = 1; }
    /* SysEx out and in again */
    uint8_t msg[400]; int n = cz_syx_tone(t, 0, 0x60, 1, msg);
    uint8_t back[CZ_TONE_BYTES];
    if (n != 296 || !cz_nibbles_in(msg + 7, CZ_TONE_BYTES, back) || memcmp(back, t, CZ_TONE_BYTES)) {
        printf("tone %d: sysex roundtrip failed\n", ntones); bad = 1;
    }
    char name[17]; cz_name_get(t, name);
    if (bad && nbad < 6) {
        printf("tone %2d %-16s differs at:", ntones, name);
        for (int i = 0; i < 128; i++) if ((c[i] ^ t[i]) & dirmask[i]) printf(" %d(%02x>%02x)", i, t[i], c[i]);
        printf("\n");
    }
    nbad += bad; ntones++;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: tone_test file.syx\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
    static uint8_t buf[1 << 20]; long n = (long)fread(buf, 1, sizeof buf, f); fclose(f);
    int found = cz_syx_scan(buf, n, roundtrip, NULL);
    printf("%d tones, %d with differences\n", found, nbad);
    for (int i = 0; i < 128; i++) if (diffs[i]) printf("  byte %d: %d tones\n", i, diffs[i]);
    /* encode one tone back to a librarian message and scan it again */
    return nbad ? 1 : 0;
}
