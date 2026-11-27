// Ensoniq ES5510 "ESP" effects DSP, standalone.
// Ported from MAME src/devices/cpu/es5510/es5510.cpp (license: BSD-3-Clause, copyright holder
// Christian Brunschen), without the MAME device framework. Changes against MAME (behaviour the
// EPS-16 PLUS / ASR-10 effects need, after the ES5510 data sheet as summarised in the ASR-10
// emulator notes by dudumil): the skip test sees the flags of the instruction before; ABS keeps
// Z, ASL2/ASL8 keep C, LS15 puts bit 8 of its operand into C; skippable instructions never
// change the CCR; delay and table addresses are formed in whole words (the base bits below
// MEMSIZ and the fraction of an offset do not count); the delay modulus is (DLENGTH >> MEMSIZ)
// + 1 words; with HALT the END instruction is finished and the DOL FIFO emptied.
#pragma once
#include <cstdint>
#include <vector>

namespace eps {

class ES5510 {
public:
    static constexpr int kDramWords = 1 << 20;
    ES5510();
    void reset();

    // the host interface, register n = (address - base) / 2 on the 68000 side
    uint8_t hostRead(int reg);
    void hostWrite(int reg, uint8_t data);

    // serial ports 0..7 = SER0L SER0R SER1L SER1R SER2L SER2R SER3L SER3R
    int16_t serial(int i) const { return ser_[i & 7]; }
    void setSerial(int i, int16_t v) { ser_[i & 7] = v; }

    void setHalt(bool h) { haltLine_ = h; }
    bool halted() const { return state_ == Halted; }

    // one sample period: run from the program counter to END (at most 160 instructions)
    void runFrame();

    uint64_t instruction(int i) const { return instr_[i]; }
    int32_t gpr(int i) const { return gpr_[i]; }
    void setDramWords(int words) { dramMask_ = words - 1; }   // a power of two

private:
    enum State { Running, Halted };
    enum : uint8_t { SrcReg = 1, SrcDelay = 2, SrcBoth = 3 };
    enum RamCycle : uint8_t { Read, Write, Dump };
    enum RamAccess : uint8_t { Delay, TableA, TableB, Io };
    struct Alu { uint8_t aReg, bReg, src, dst, op; int32_t a, b; bool updateCcr, write; };
    struct Mac { uint8_t cReg, dReg, src, dst; bool accumulate, write; int32_t c, d; };
    struct Ram { int32_t address = 0; bool io = false; RamCycle cycle = Read; };

    int32_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, int32_t value);
    void writeDol(int32_t value);
    int32_t aluOp(uint8_t op, int32_t a, int32_t b, uint8_t &flags) const;
    void endOfProgram();
    void step();
    int32_t wordAddress(int32_t base, int32_t offset) const;

    State state_ = Halted;
    bool haltLine_ = false;
    uint8_t pc_ = 0;
    int32_t gpr_[0xc0] = {};
    uint64_t instr_[160] = {};
    std::vector<int16_t> dram_;
    int dramMask_ = kDramWords - 1;
    int16_t ser_[8] = {};
    int64_t machl_ = 0;
    bool macOverflow_ = false;
    int16_t dil_ = 0;
    int32_t memsiz_ = 0xffffff, memmask_ = 0, memincrement_ = 0x1000000;
    int memshift_ = 24;
    int32_t dlength_ = 0, abase_ = 0, bbase_ = 0, dbase_ = 0, sigreg_ = 0;
    int mulshift_ = 2;
    uint8_t ccr_ = 0, cmr_ = 0;
    int16_t dol_[2] = {};
    int dolCount_ = 0;
    int32_t dolLatch_ = 0, dilLatch_ = 0, gprLatch_ = 0;
    uint32_t dadrLatch_ = 0;
    uint64_t instrLatch_ = 0;
    uint8_t ramSel_ = 0, hostControl_ = 0x04, hostSerial_ = 0;
    Alu alu_{};
    Mac mac_{};
    Ram ram_, ramP_, ramPP_;
};

}  // namespace eps
