#include "eps_bank.h"

#include <algorithm>

namespace eps {

namespace {
constexpr size_t kName = 0x08, kMask = 0x20, kFib = 0x22, kFibSize = 16, kVol = 0xB2, kPan = 0xC2,
                 kPresets = 0xD2;

uint32_t effectLength(const uint8_t *d) {   // two words of 12 bits each, as in effect files
    const uint32_t w0 = uint32_t(d[0] << 8 | d[1]), w1 = uint32_t(d[2] << 8 | d[3]);
    return (w1 >> 4) << 12 | (w0 >> 4);
}

void putEffectLength(uint8_t *d, uint32_t v) {
    const uint32_t lo = (v & 0xFFF) << 4, hi = (v >> 12 & 0xFFF) << 4;
    d[0] = uint8_t(lo >> 8); d[1] = uint8_t(lo); d[2] = uint8_t(hi >> 8); d[3] = uint8_t(hi);
}
}  // namespace

bool parseBank(const std::vector<uint8_t> &data, Bank &bank, std::string &err) {
    bank = Bank();
    if (data.size() < kBankHeader) { err = "too short for a bank"; return false; }
    const uint8_t *d = data.data();
    if (effectLength(d) != kBankHeader) { err = "not a bank (header length)"; return false; }
    bank.header.assign(data.begin(), data.begin() + kBankHeader);
    for (size_t i = 0; i < 12; ++i) {
        const char c = char(d[kName + 2 * i]);
        bank.name += c >= 32 && c < 127 ? c : ' ';
    }
    while (!bank.name.empty() && bank.name.back() == ' ') bank.name.pop_back();
    const uint8_t mask = d[kMask];
    for (int t = 0; t < 8; ++t) {
        BankTrack &k = bank.track[t];
        const uint8_t *f = d + kFib + size_t(t) * kFibSize;
        k.valid = (mask >> t & 1) != 0;
        k.copy = (f[0] & 0x80) != 0;
        k.copyOf = f[0] & 0x0F;
        k.device = f[2];
        if (f[3] >= 32 && f[3] < 127) k.disk += char(f[3]);
        for (int i = 0; i < 6; ++i) {
            k.path.push_back(f[4 + 2 * i]);
            const uint8_t c = f[5 + 2 * i];
            if (c >= 32 && c < 127) k.disk += char(c);
        }
        while (!k.disk.empty() && k.disk.back() == ' ') k.disk.pop_back();
        k.volume = d[kVol + 2 * size_t(t)];
        k.output = d[kVol + 2 * size_t(t) + 1];
        k.pan = d[kPan + 2 * size_t(t)];
        k.fxControl = d[kPan + 2 * size_t(t) + 1];
    }
    if (data.size() >= kBankHeader + 0x100) {
        const uint32_t len = effectLength(d + kBankHeader);
        if (len >= 0x100 && kBankHeader + len <= data.size())
            bank.effect.assign(data.begin() + kBankHeader, data.begin() + kBankHeader + len);
    }
    return true;
}

std::vector<uint8_t> buildBank(const Bank &bank) {
    std::vector<uint8_t> h = bank.header;
    if (h.size() != kBankHeader) {           // a new bank, as the OS writes one
        h.assign(kBankHeader, 0);
        h[4] = 0xA8; h[5] = 0x20;
        h[kMask + 1] = 0xFF;
        for (int f = 0; f < 9; ++f) {
            uint8_t *b = h.data() + kFib + size_t(f) * kFibSize;
            b[0] = 0x00; b[1] = 0xFF; b[2] = 0xFF; b[3] = 0xFF;
        }
        for (int p = 0; p < 8; ++p) h[kPresets + size_t(p) * 134 + 1] = 0x04;
    }
    putEffectLength(h.data(), uint32_t(kBankHeader));
    for (size_t i = 0; i < 12; ++i) {
        h[kName + 2 * i] = uint8_t(i < bank.name.size() ? bank.name[i] : ' ');
        h[kName + 2 * i + 1] = 0x20;
    }
    uint8_t mask = 0;
    for (int t = 0; t < 8; ++t) {
        const BankTrack &k = bank.track[t];
        if (!k.valid) continue;
        mask |= uint8_t(1 << t);
        uint8_t *f = h.data() + kFib + size_t(t) * kFibSize;
        f[0] = uint8_t(k.copy ? 0x80 | (k.copyOf & 0x0F) : 0x00);
        f[1] = 0xFF;
        if (k.copy && bank.header.size() == kBankHeader) {   // a copy: the rest as it was
            h[kVol + 2 * size_t(t)] = uint8_t(std::clamp(k.volume, 0, 0x7F));
            h[kVol + 2 * size_t(t) + 1] = uint8_t(k.output);
            h[kPan + 2 * size_t(t)] = uint8_t(k.pan);
            h[kPan + 2 * size_t(t) + 1] = uint8_t(k.fxControl);
            continue;
        }
        f[2] = uint8_t(k.device);
        f[3] = uint8_t(k.disk.size() > 0 ? k.disk[0] : ' ');
        for (int i = 0; i < 6; ++i) {
            f[4 + 2 * i] = uint8_t(size_t(i) < k.path.size() ? k.path[size_t(i)] : 0);
            f[5 + 2 * i] = uint8_t(size_t(i + 1) < k.disk.size() ? k.disk[size_t(i + 1)] : ' ');
        }
        h[kVol + 2 * size_t(t)] = uint8_t(std::clamp(k.volume, 0, 0x7F));
        h[kVol + 2 * size_t(t) + 1] = uint8_t(k.output);
        h[kPan + 2 * size_t(t)] = uint8_t(k.pan);
        h[kPan + 2 * size_t(t) + 1] = uint8_t(k.fxControl);
    }
    h[kMask] = mask;
    std::vector<uint8_t> out = h;
    if (bank.effect.size() >= 0x100) {
        std::vector<uint8_t> e = bank.effect;
        e.resize((e.size() + 1) & ~size_t(1), 0);
        putEffectLength(e.data(), uint32_t(e.size()));
        out.insert(out.end(), e.begin(), e.end());
    }
    return out;
}

}  // namespace eps
