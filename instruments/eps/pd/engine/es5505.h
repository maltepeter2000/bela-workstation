// license:BSD-3-Clause
// copyright-holders:Aaron Giles (MAME es5506.cpp), standalone extraction for Bela
//
// Ensoniq ES5505 "OTIS" voice chip, as used in the EPS-16 PLUS.
// Extracted from MAME src/devices/sound/es5506.cpp (BSD-3-Clause) and reduced
// to the ES5505 PCM path: 32 voices, 20.9 fixed-point addresses, linear
// interpolation, 4-pole filter (K1/K2), 8-bit log volume, loop modes.
// No MAME framework: sample memory is a plain int16 array, registers are
// written with writeReg(), audio comes out of render().
//
// Real-time safe: no allocation or locking in render()/writeReg().

#pragma once

#include <cstddef>
#include <cstdint>

namespace otis {

enum : uint16_t {
    kStop0 = 0x0001, kStop1 = 0x0002, kStopMask = 0x0003,
    kBank = 0x0004,          // ES5505: bank select (LEI position on ES5506)
    kLpe = 0x0008,           // loop enable
    kBle = 0x0010,           // bidirectional loop (with LPE)
    kIrqe = 0x0020, kDir = 0x0040, kIrq = 0x0080,
    kCa0 = 0x0100, kCa1 = 0x0200,   // output channel (0..3)
    kLp3 = 0x0400, kLp4 = 0x0800    // filter configuration
};

// Filter configurations (control bits LP3/LP4):
//   0       : 2-pole LP (K1) + 2-pole HP (K2)
//   kLp3    : 3-pole LP (K1) + 1-pole HP (K2)
//   kLp4    : 2-pole LP (K1) + 2-pole LP (K2)
//   kLp3|4  : 3-pole LP (K1) + 1-pole LP (K2)

// Register numbers on the voice pages (page 0x00..0x1f).
enum Reg { CR = 0, FC = 1, STRT_H = 2, STRT_L = 3, END_H = 4, END_L = 5,
           K2 = 6, K1 = 7, LVOL = 8, RVOL = 9, ACC_H = 10, ACC_L = 11, ACT = 13 };

struct Voice {
    uint16_t control = 0xf000 | kStopMask;
    uint32_t freqcount = 0;            // 11 fractional bits (internal)
    uint32_t start = 0, end = 0, accum = 0;   // 11 fractional bits (internal)
    uint32_t k1 = 0, k2 = 0;           // 16-bit, low nibble zero
    uint32_t lvol = 0x80, rvol = 0x80; // 8-bit: 4-bit exponent, 4-bit mantissa
    int32_t o1n1 = 0, o2n1 = 0, o2n2 = 0, o3n1 = 0, o3n2 = 0, o4n1 = 0;
};

class ES5505 {
public:
    static constexpr int kVoices = 32;
    static constexpr int kChannels = 4;      // stereo output pairs
    static constexpr int kFracBits = 11;     // internal accumulator fraction
    static constexpr uint32_t kAccMask = 0x7fffffff;   // 20 int + 9 frac bits, shifted by 2

    ES5505();
    void reset();

    // Sample memory per bank, 16-bit signed words, max 2^20 words.
    void setMemory(int bank, const int16_t *words, uint32_t count);

    // Chip-level register write (16-bit value), as the 68000 would do it.
    void writeReg(int voice, Reg reg, uint16_t value);

    // Convenience for host code: addresses in chip format (20.9 fixed point,
    // i.e. sample index << 9), frequency in FC format (ratio * 1024).
    void setAddresses(int voice, uint32_t start29, uint32_t end29, uint32_t accum29);
    void setFrequency(int voice, uint16_t fc);

    int activeVoices() const { return activeVoices_ + 1; }
    const Voice &voice(int v) const { return voices_[v]; }
    bool stopped(int v) const { return voices_[v].control & kStopMask; }

    // Renders `frames` samples. out[2*c] / out[2*c+1] receive channel c (L/R),
    // as 20-bit integers (range ±2^19). Pass nullptr for unused channels.
    void renderInt(int32_t *out[kChannels * 2], size_t frames);
    // Sums all channels to stereo float (-1..1, no clipping), adds into L/R.
    void renderAdd(float *left, float *right, size_t frames);
    // The same per output channel (float, -1..1): out[2*c] / out[2*c+1] = channel c L / R.
    void renderAddChannels(float *out[kChannels * 2], size_t frames);

    // Volume code -> linear gain (code 255 = ~0.97).
    static float volumeGain(uint8_t code);
    // Nearest volume code for a linear gain 0..1.
    static uint8_t volumeCode(float gain);

private:
    void generate(Voice &v, int32_t *dest);
    void applyFilters(Voice &v, int32_t &sample);
    void checkEndForward(Voice &v, uint32_t &accum);
    void checkEndReverse(Voice &v, uint32_t &accum);
    int32_t readSample(const Voice &v, uint32_t addr) const {
        int b = (v.control & kBank) ? 1 : 0;
        return addr < memSize_[b] ? mem_[b][addr] : 0;
    }

    Voice voices_[kVoices];
    int activeVoices_ = 31;
    const int16_t *mem_[2] = {nullptr, nullptr};
    uint32_t memSize_[2] = {0, 0};
    static uint32_t volumeLookup_[256];
    static bool tablesReady_;
};

} // namespace otis
