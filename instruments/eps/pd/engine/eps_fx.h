// The EPS-16 PLUS effect machine: the effect block of an instrument runs as on the EPS. A
// 68000 (Musashi, MIT) executes the effect's driver code together with the helper routines of
// the EPS ROM and the OS (the user's files, loaded at run time, never part of eps~); the
// driver writes the ESP (ES5510) program and registers through the host interface.
//
// The OS side that eps~ does itself: load (ROM program loader, the effect's init, all
// parameters), the real-time routine (about 77 Hz), the table of mod sources, the fade-in of
// the effect output (GPR 127).
//
// Audio: the ESP runs at the effect's rate (byte $66 of the effect: 0 = 29 762 Hz, 1 =
// 44 643 Hz, 2 = 78 125 Hz). Inputs SER0 / SER2 / SER3 = ES5505 channels 0 / 2 / 3, output SER1.
#pragma once
#include "es5510.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eps {

class FxMachine {
public:
    FxMachine();
    ~FxMachine();
    FxMachine(const FxMachine &) = delete;
    FxMachine &operator=(const FxMachine &) = delete;

    // The EPS-16 PLUS ROM (128 KB, both halves interleaved as one file) and the OS file (EFE
    // or the bare file, OS 1.30). Without both, no effect can be loaded.
    bool setSystem(const std::vector<uint8_t> &rom, const std::vector<uint8_t> &os, std::string &err);
    bool hasSystem() const { return system_; }

    // Load an effect block (from the instrument file, at the effect offset). False with a
    // reason; then the machine is empty (dry).
    bool load(const uint8_t *block, size_t size, std::string &err);
    void unload();
    bool loaded() const { return loaded_; }
    std::string name() const { return name_; }
    int rateMode() const { return rateMode_; }
    double rate() const;

    struct Param {
        std::string name;
        int lo = 0, hi = 127, value = 0;
        int index = 0;           // the descriptor slot
        int type = 0;            // descriptor type: 0-2 unsigned, 3 signed, $0E list, $17 16 bit
        bool variation = false;  // this one chooses the variation
        std::vector<std::string> items;   // a list: the names of its values
    };
    const std::vector<Param> &params() const { return params_; }
    bool setParam(int k, int value);   // k = index into params(); runs its descriptor

    // mod sources for the driver: 4 bend (64 = centre), 5 wheel, 7 xctrl, 8 pedal, 9 volume,
    // 10 sustain, 11 pressure (0..127)
    void setMod(int slot, int value);

    // the real-time routine of the effect, every 1/77 s
    void tick();

    // one ESP sample period: in[0..5] = channel 0 L R, channel 2 L R, channel 3 L R (16 bit)
    void frame(const int16_t in[6], int16_t &outL, int16_t &outR);

    ES5510 &esp() { return esp_; }
    // the effect block as the driver left it (parameter values live in it): for saving
    const std::vector<uint8_t> &block() const { return block_; }

    // the memory of the 68000 side; public for the C callbacks of the core
    uint8_t read8(uint32_t a);
    void write8(uint32_t a, uint8_t v);
    void onInstruction(uint32_t pc);

private:
    bool call(uint32_t pc, uint32_t a0, uint32_t a1, uint32_t a3, uint32_t a6, uint32_t d1, int budget, uint32_t a2 = 0);
    void writeGpr(int reg, int32_t v);
    void readParams();
    uint16_t blk16(uint32_t off) const;
    void put16(uint32_t a, uint16_t v);
    void put32(uint32_t a, uint32_t v);

    std::vector<uint8_t> mem_;    // the OS RAM, 64 KB: $0000-$7FFF and $FF8000-$FFFFFF are one
    std::vector<uint8_t> rom_;    // 128 KB at $C00000
    std::vector<uint8_t> block_;  // the effect block, at kBlock
    std::vector<uint8_t> ctx_;    // the 68000 context (Musashi keeps one global CPU)
    ES5510 esp_;
    bool system_ = false, loaded_ = false, stopped_ = false;
    int rateMode_ = 0, fade_ = 0;
    std::string name_;
    std::vector<Param> params_;
};

}  // namespace eps
