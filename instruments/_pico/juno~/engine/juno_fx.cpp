// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71

/*
  juno_fx.cpp -- the chorus
*/

#include "juno/juno_fx.h"

#include <math.h>
#include <string.h>

void JunoChorus::init(float sampleRate)
{
    sr_ = sampleRate;
    hissL_.seed(0x5bd1e995u);
    hissR_.seed(0x27d4eb2du);
    /* juno~ change: the chorus board's filters. Each Sallen-Key stage has
     * 22k + 22k with C1 in the feedback and C2 to ground, so
     *   f0 = 1 / (2 pi 22k sqrt(C1 C2)),  Q = sqrt(C1 / C2) / 2:
     *   820 pF / 680 pF  ->  9.69 kHz, Q 0.549
     *   1.8 nF / 270 pF  -> 10.38 kHz, Q 1.291
     * the same pair ahead of the lines and after each of them. R15/C7 ahead
     * of the line is 10k and 2.2 nF, 7.23 kHz. */
    inRc_.setCutoff(7234.0f, sampleRate);
    inSk_[0].set(9689.0f, 0.549f, sampleRate);
    inSk_[1].set(10377.0f, 1.291f, sampleRate);
    for (int c = 0; c < 2; ++c) {
        outSk_[c][0].set(9689.0f, 0.549f, sampleRate);
        outSk_[c][1].set(10377.0f, 1.291f, sampleRate);
    }
    reset();
    setMode(JUNO_CH_OFF);
}

void JunoChorus::reset()
{
    memset(buf_, 0, sizeof(buf_));
    write_ = 0;
    phase_ = 0.0f;
    inRc_.reset();
    for (int c = 0; c < 2; ++c) { inSk_[c].reset(); outSk_[c][0].reset(); outSk_[c][1].reset(); }
}

void JunoChorus::setMode(int mode)
{
    mode_ = (mode < 0) ? 0 : (mode >= JUNO_CH_COUNT ? JUNO_CH_COUNT - 1 : mode);

    float hz;
    switch (mode_) {
        case JUNO_CH_I:
            hz = JUNO_CHORUS_I_HZ;
            minMs_ = JUNO_CHORUS_MIN_MS; maxMs_ = JUNO_CHORUS_MAX_MS;
            mono_ = false;
            break;
        case JUNO_CH_II:
            hz = JUNO_CHORUS_II_HZ;
            minMs_ = JUNO_CHORUS_MIN_MS; maxMs_ = JUNO_CHORUS_MAX_MS;
            mono_ = false;
            break;
        case JUNO_CH_III:
            /* Both lines modulated together, and over a much narrower range --
             * measured 3.30 to 3.70 ms. The result is a vibrato. */
            hz = JUNO_CHORUS_III_HZ;
            minMs_ = 3.30f; maxMs_ = 3.70f;
            mono_ = true;
            break;
        default:
            hz = 0.0f;
            mono_ = false;
            break;
    }
    /* juno~ addition: the Juno-106's chorus, measured on Cherry Audio's
     * DCO-106 -- about twice the rate over half the sweep, so the lines move
     * at the same speed but drift apart less: a narrower image. */
    if (model106_ && (mode_ == JUNO_CH_I || mode_ == JUNO_CH_II)) {
        hz = (mode_ == JUNO_CH_I) ? JUNO106_CHORUS_I_HZ : JUNO106_CHORUS_II_HZ;
        maxMs_ = minMs_ + ((mode_ == JUNO_CH_I) ? JUNO106_CHORUS_I_SPAN_MS : JUNO106_CHORUS_II_SPAN_MS);
    }
    inc_ = hz / sr_;
}

void JunoChorus::process(float* l, float* r, int n)
{
    if (mode_ == JUNO_CH_OFF) return;

    const float minS = minMs_ * 0.001f * sr_;
    const float spanS = (maxMs_ - minMs_) * 0.001f * sr_;

    for (int i = 0; i < n; ++i) {
        /* The voices arrive summed and mono, as they do at the chorus board's
         * single signal input. */
        const float in = 0.5f * (l[i] + r[i]);

        phase_ += inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        /* Triangle, running 0 .. 1 .. 0 over one cycle. */
        const float modL = 1.0f - 2.0f * fabsf(phase_ - 0.5f);

        /* Right-hand modulation inverted, so the two channels sit half a cycle
         * apart -- except in I+II, where both lines share it and the output is
         * mono. */
        const float modR = mono_ ? modL : (1.0f - modL);

        float dL = minS + spanS * modL;
        float dR = minS + spanS * modR;
        dL = junoClamp(dL, 1.0f, (float) (kLen - 2));
        dR = junoClamp(dR, 1.0f, (float) (kLen - 2));

        const int   iL = (int) dL, iR = (int) dR;
        const float fL = dL - (float) iL, fR = dR - (float) iR;

        int aL = write_ - iL; while (aL < 0) aL += kLen;
        int bL = aL - 1;      if (bL < 0)    bL += kLen;
        int aR = write_ - iR; while (aR < 0) aR += kLen;
        int bR = aR - 1;      if (bR < 0)    bR += kLen;

        float wetL = buf_[0][aL] + (buf_[0][bL] - buf_[0][aL]) * fL;
        float wetR = buf_[1][aR] + (buf_[1][bR] - buf_[1][aR]) * fR;

        if (noise_ > 0.0f) {
            wetL += noise_ * hissL_.white();
            wetR += noise_ * hissR_.white();
        }
        wetL = outSk_[0][1].process(outSk_[0][0].process(wetL));
        wetR = outSk_[1][1].process(outSk_[1][0].process(wetR));

        /* Written after reading, so the shortest delay is one sample rather
         * than zero. The low-pass ahead of the line is what rounds the
         * sawtooth. */
        buf_[0][write_] = inSk_[1].process(inSk_[0].process(inRc_.process(in)));
        buf_[1][write_] = buf_[0][write_];
        if (++write_ >= kLen) write_ = 0;

        /* juno~ change: the mix of the board. Each half of IC8 sums the
         * direct signal through 47k (R86, R87) and its own line through 39k
         * (R89, R88) into 100k, so the line comes in 47/39 = 1.205 times the
         * direct signal. The chorus switch only opens the FETs in the line
         * paths (TR8, TR16): the direct signal stays as loud as with the
         * chorus off, and the chorus adds to it. Upstream mixed 0.7 : 0.7. */
        l[i] = in + wetL * 1.205f;
        r[i] = in + wetR * 1.205f;
    }
}
