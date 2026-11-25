// license:BSD-3-Clause
// copyright-holders:Aaron Giles (MAME es5506.cpp), standalone extraction for Bela
//
// Logic follows MAME's es550x_device / es5505_device:
//   compute_tables(8, 4, 4), get_accum_mask(20, 9), generate_pcm(),
//   apply_filters(), es5505 check_for_end_forward/reverse(), reg_write_low().
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "es5505.h"

#include <cmath>

namespace otis {

namespace {
constexpr uint32_t kFineFilterBit = 16;
constexpr uint32_t kFilterBit = 12;
constexpr uint32_t kFilterShift = kFineFilterBit - kFilterBit;
constexpr int kVolumeAccShift = 11;          // (16 + 15) - 20
constexpr uint32_t kAddrShift = 2;           // ADDRESS_FRAC_BIT(11) - 9

inline int32_t lowpass(int32_t out, int32_t cutoff, int32_t in) {
    return (int32_t(cutoff >> kFilterShift) * (out - in) / (1 << kFilterBit)) + in;
}
inline int32_t highpass(int32_t out, int32_t cutoff, int32_t in, int32_t prev) {
    return out - prev + (int32_t(cutoff >> kFilterShift) * in) / (1 << (kFilterBit + 1)) + in / 2;
}
} // namespace

uint32_t ES5505::volumeLookup_[256];
bool ES5505::tablesReady_ = false;

ES5505::ES5505() {
    if (!tablesReady_) {
        // compute_tables(VOLUME_BIT_ES5505 = 8, exponent 4, mantissa 4)
        for (int i = 0; i < 256; ++i) {
            uint32_t exponent = (i >> 4) & 15;
            uint32_t mantissa = (i & 15) | 16;
            volumeLookup_[i] = (mantissa << 11) >> (16 - exponent);
        }
        tablesReady_ = true;
    }
    reset();
}

void ES5505::reset() {
    for (auto &v : voices_) v = Voice();
    activeVoices_ = 31;
}

void ES5505::setMemory(int bank, const int16_t *words, uint32_t count) {
    if (bank < 0 || bank > 1) return;
    mem_[bank] = words;
    memSize_[bank] = count > (1u << 20) ? (1u << 20) : count;
}

float ES5505::volumeGain(uint8_t code) {
    // Full-scale sample (32767) * lookup >> 11 relative to the 2^19 output range.
    return float(volumeLookup_[code]) / float(1 << 15);
}

uint8_t ES5505::volumeCode(float gain) {
    if (gain <= 0.f) return 0;
    uint8_t best = 0;
    float bestErr = 1e9f;
    for (int c = 0; c < 256; ++c) {
        float e = std::fabs(volumeGain(uint8_t(c)) - gain);
        if (e < bestErr) { bestErr = e; best = uint8_t(c); }
    }
    return best;
}

void ES5505::writeReg(int vi, Reg reg, uint16_t data) {
    if (vi < 0 || vi >= kVoices) return;
    Voice &v = voices_[vi];
    switch (reg) {
        case CR:     v.control = uint16_t(0xf000 | (data & 0x0fff)); break;
        case FC:     v.freqcount = uint32_t(data & 0xfffe) << 1; break;
        case STRT_H: v.start = (v.start & ~(0x1fff0000u << kAddrShift)) | (uint32_t(data & 0x1fff) << 16 << kAddrShift); break;
        case STRT_L: v.start = (v.start & ~(0xffe0u << kAddrShift)) | (uint32_t(data & 0xffe0) << kAddrShift); break;
        case END_H:  v.end = (v.end & ~(0x1fff0000u << kAddrShift)) | (uint32_t(data & 0x1fff) << 16 << kAddrShift); break;
        case END_L:  v.end = (v.end & ~(0xffe0u << kAddrShift)) | (uint32_t(data & 0xffe0) << kAddrShift); break;
        case K2:     v.k2 = data & 0xfff0; break;
        case K1:     v.k1 = data & 0xfff0; break;
        case LVOL:   v.lvol = data >> 8; break;
        case RVOL:   v.rvol = data >> 8; break;
        case ACC_H:  v.accum = (v.accum & ~(0x1fff0000u << kAddrShift)) | (uint32_t(data & 0x1fff) << 16 << kAddrShift); break;
        case ACC_L:  v.accum = (v.accum & ~(0xffffu << kAddrShift)) | (uint32_t(data) << kAddrShift); break;
        case ACT:    activeVoices_ = data & 0x1f; break;
    }
    v.start &= kAccMask;
    v.end &= kAccMask;
    v.accum &= kAccMask;
}

void ES5505::setAddresses(int vi, uint32_t start29, uint32_t end29, uint32_t accum29) {
    // Same truncation as the register path (STRT/END low 5 fraction bits unused).
    writeReg(vi, STRT_H, uint16_t(start29 >> 16));
    writeReg(vi, STRT_L, uint16_t(start29));
    writeReg(vi, END_H, uint16_t(end29 >> 16));
    writeReg(vi, END_L, uint16_t(end29));
    writeReg(vi, ACC_H, uint16_t(accum29 >> 16));
    writeReg(vi, ACC_L, uint16_t(accum29));
}

void ES5505::setFrequency(int vi, uint16_t fc) { writeReg(vi, FC, fc); }

void ES5505::applyFilters(Voice &v, int32_t &s) {
    s = lowpass(s, int32_t(v.k1), v.o1n1);
    v.o1n1 = s;
    s = lowpass(s, int32_t(v.k1), v.o2n1);
    v.o2n2 = v.o2n1;
    v.o2n1 = s;
    switch ((v.control >> 10) & 3) {
        case 0:
            s = highpass(s, int32_t(v.k2), v.o3n1, v.o2n2);
            v.o3n2 = v.o3n1; v.o3n1 = s;
            s = highpass(s, int32_t(v.k2), v.o4n1, v.o3n2);
            v.o4n1 = s;
            break;
        case 1:
            s = lowpass(s, int32_t(v.k1), v.o3n1);
            v.o3n2 = v.o3n1; v.o3n1 = s;
            s = highpass(s, int32_t(v.k2), v.o4n1, v.o3n2);
            v.o4n1 = s;
            break;
        case 2:
            s = lowpass(s, int32_t(v.k2), v.o3n1);
            v.o3n2 = v.o3n1; v.o3n1 = s;
            s = lowpass(s, int32_t(v.k2), v.o4n1);
            v.o4n1 = s;
            break;
        default:
            s = lowpass(s, int32_t(v.k1), v.o3n1);
            v.o3n2 = v.o3n1; v.o3n1 = s;
            s = lowpass(s, int32_t(v.k2), v.o4n1);
            v.o4n1 = s;
            break;
    }
}

void ES5505::checkEndForward(Voice &v, uint32_t &accum) {
    if (accum <= v.end) return;
    if (v.control & kIrqe) v.control |= kIrq;
    switch (v.control & (kLpe | kBle)) {
        case 0:
        case kBle: v.control |= kStop0; break;                       // BLE ignored without LPE
        case kLpe: accum = (v.start + (accum - v.end)) & kAccMask; break;
        default:   accum = (v.end - (accum - v.end)) & kAccMask; v.control ^= kDir; break;
    }
}

void ES5505::checkEndReverse(Voice &v, uint32_t &accum) {
    if (accum >= v.start) return;
    if (v.control & kIrqe) v.control |= kIrq;
    switch (v.control & (kLpe | kBle)) {
        case 0:
        case kBle: v.control |= kStop0; break;
        case kLpe: accum = (v.end - (v.start - accum)) & kAccMask; break;
        default:   accum = (v.start + (v.start - accum)) & kAccMask; v.control ^= kDir; break;
    }
}

void ES5505::generate(Voice &v, int32_t *dest) {
    if (v.control & kStopMask) return;
    uint32_t accum = v.accum & kAccMask;
    const uint32_t addr = accum >> kFracBits;
    int32_t s1 = readSample(v, addr);
    int32_t s2 = readSample(v, (addr + 1) & 0xfffff);
    const int32_t frac = int32_t(accum & ((1u << kFracBits) - 1));
    int32_t s = (s1 * ((1 << kFracBits) - frac) + s2 * frac) >> kFracBits;
    if (v.control & kDir) accum = (accum - v.freqcount) & kAccMask;
    else accum = (accum + v.freqcount) & kAccMask;
    applyFilters(v, s);
    dest[0] += int32_t((int64_t(s) * volumeLookup_[v.lvol & 0xff]) >> kVolumeAccShift);
    dest[1] += int32_t((int64_t(s) * volumeLookup_[v.rvol & 0xff]) >> kVolumeAccShift);
    if (v.control & kDir) checkEndReverse(v, accum);
    else checkEndForward(v, accum);
    v.accum = accum;
}

void ES5505::renderInt(int32_t *out[kChannels * 2], size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        int32_t cur[kChannels * 2] = {};
        for (int vi = 0; vi <= activeVoices_; ++vi) {
            Voice &v = voices_[vi];
            const int ch = (v.control >> 8) & 3;
            generate(v, &cur[ch * 2]);
            // IRQ vectoring is not modelled; host code polls stopped().
            v.control &= uint16_t(~kIrq);
        }
        for (int c = 0; c < kChannels * 2; ++c) {
            if (!out[c]) continue;
            int32_t x = cur[c];
            if (x > (1 << 19) - 1) x = (1 << 19) - 1;
            if (x < -(1 << 19)) x = -(1 << 19);
            out[c][i] = x;
        }
    }
}

void ES5505::renderAdd(float *left, float *right, size_t frames) {
    constexpr float kScale = 1.f / float(1 << 19);
    for (size_t i = 0; i < frames; ++i) {
        int32_t l = 0, r = 0;
        for (int vi = 0; vi <= activeVoices_; ++vi) {
            Voice &v = voices_[vi];
            if (v.control & kStopMask) continue;
            int32_t cur[2] = {0, 0};
            generate(v, cur);
            v.control &= uint16_t(~kIrq);
            l += cur[0];
            r += cur[1];
        }
        left[i] += float(l) * kScale;
        right[i] += float(r) * kScale;
    }
}

} // namespace otis
