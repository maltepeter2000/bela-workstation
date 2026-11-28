// Ensoniq ES5510 "ESP": see es5510.h (ported from MAME, BSD-3-Clause, Christian Brunschen).
#include "es5510.h"
#include <algorithm>

namespace eps {
namespace {

constexpr uint8_t kN = 1 << 7, kC = 1 << 6, kV = 1 << 5, kLT = 1 << 4, kZ = 1 << 3, kNot = 1 << 2;
constexpr uint8_t kFlagMask = kN | kC | kV | kLT | kZ;

inline uint8_t flagTo(uint8_t ccr, uint8_t f, bool s) { return s ? uint8_t(ccr | f) : uint8_t(ccr & ~f); }
inline int32_t sext24(int32_t v) { return (v & 0x00800000) ? (v | ~0x00ffffff) : (v & 0x00ffffff); }

inline int32_t add(int32_t a, int32_t b, uint8_t &flags) {
    const int32_t aSign = a & 0x00800000, bSign = b & 0x00800000;
    const int32_t result = a + b;
    const int32_t rSign = result & 0x00800000;
    const bool overflow = aSign == bSign && aSign != rSign;
    const bool negative = rSign != 0;
    flags = flagTo(flags, kC, (result & 0x01000000) != 0);
    flags = flagTo(flags, kN, negative);
    flags = flagTo(flags, kZ, (result & 0x00ffffff) == 0);
    flags = flagTo(flags, kV, overflow);
    flags = flagTo(flags, kLT, overflow != negative);
    return result & 0x00ffffff;
}
inline int32_t saturate(int32_t value, uint8_t &flags, bool negative) {
    if (!(flags & kV)) return value;
    flags = flagTo(flags, kN, negative);
    flags &= ~kZ;
    return negative ? 0x00800000 : 0x007fffff;
}
inline int32_t negate(int32_t v) { return ((v ^ 0x00ffffff) + 1) & 0x00ffffff; }
// ASL2 / ASL8: the carry stays as it was
inline int32_t asl(int32_t value, int shift, uint8_t &flags) {
    const int64_t shifted = int64_t(sext24(value)) * (int64_t(1) << shift);
    const bool overflow = shifted > 0x007fffffLL || shifted < -0x00800000LL;
    const int32_t r = overflow ? (shifted < 0 ? 0x00800000 : 0x007fffff) : int32_t(shifted) & 0x00ffffff;
    flags = flagTo(flags, kV, overflow);
    flags = flagTo(flags, kN, (r & 0x00800000) != 0);
    flags = flagTo(flags, kZ, r == 0);
    return r;
}

struct OpSel { uint8_t aluSrc, aluDst, macSrc, macDst; };
constexpr uint8_t R = 1, D = 2, B = 3;
constexpr OpSel kOpSel[16] = {
    {R, R, R, R}, {R, R, R, D}, {R, R, R, B}, {R, R, D, R}, {R, R, D, B}, {R, D, R, R}, {R, D, D, R}, {R, B, R, R},
    {R, B, D, R}, {D, R, R, R}, {D, R, R, D}, {D, R, R, B}, {D, R, D, R}, {D, R, D, B}, {D, B, R, R}, {D, B, D, R},
};
constexpr int kOperands[16] = {2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};
constexpr int kCmp = 4, kEnd = 15;

}  // namespace

ES5510::ES5510() : dram_(kDramWords, 0) { reset(); }

void ES5510::reset() {
    pc_ = 0;
    std::fill(std::begin(gpr_), std::end(gpr_), 0);
    std::fill(std::begin(instr_), std::end(instr_), 0);
    std::fill(dram_.begin(), dram_.end(), int16_t(0));
    state_ = Running;
    dilLatch_ = dolLatch_ = gprLatch_ = 0;
    dadrLatch_ = 0;
    instrLatch_ = 0;
    ramSel_ = 0;
    hostControl_ = 0x04;
    hostSerial_ = 0;
    sigreg_ = 0;
    mulshift_ = 2;
    ram_ = ramP_ = ramPP_ = Ram{};
    alu_ = Alu{};
    mac_ = Mac{};
    dolCount_ = 0;
}

uint8_t ES5510::hostRead(int reg) {
    switch (reg) {
    case 0x00: return uint8_t(gprLatch_ >> 16);
    case 0x01: return uint8_t(gprLatch_ >> 8);
    case 0x02: return uint8_t(gprLatch_);
    case 0x03: return uint8_t(instrLatch_ >> 40);
    case 0x04: return uint8_t(instrLatch_ >> 32);
    case 0x05: return uint8_t(instrLatch_ >> 24);
    case 0x06: return uint8_t(instrLatch_ >> 16);
    case 0x07: return uint8_t(instrLatch_ >> 8);
    case 0x08: return uint8_t(instrLatch_);
    case 0x09: return uint8_t(dilLatch_ >> 16);
    case 0x0a: return uint8_t(dilLatch_ >> 8);
    case 0x0b: return 0;
    case 0x0c: return uint8_t(dolLatch_ >> 16);
    case 0x0d: return uint8_t(dolLatch_ >> 8);
    case 0x0e: return 0xff;
    case 0x0f: return uint8_t(dadrLatch_ >> 16);
    case 0x10: return uint8_t(dadrLatch_ >> 8);
    case 0x11: return uint8_t(dadrLatch_);
    case 0x12: return hostControl_ & ~0x04;   // the host may always access (no transfer pending)
    case 0x16: return 0x27;
    }
    return 0;
}

void ES5510::hostWrite(int reg, uint8_t data) {
    switch (reg) {
    case 0x00: gprLatch_ = (gprLatch_ & 0x00ffff) | data << 16; break;
    case 0x01: gprLatch_ = (gprLatch_ & 0xff00ff) | data << 8; break;
    case 0x02: gprLatch_ = (gprLatch_ & 0xffff00) | data; break;
    case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: {
        const int shift = 8 * (8 - reg);
        instrLatch_ = (instrLatch_ & ~(uint64_t(0xff) << shift)) | uint64_t(data) << shift;
        break;
    }
    case 0x0c: dolLatch_ = (dolLatch_ & 0x00ffff) | data << 16; break;
    case 0x0d: dolLatch_ = (dolLatch_ & 0xff00ff) | data << 8; break;
    case 0x0e: dolLatch_ = (dolLatch_ & 0xffff00) | data; break;
    case 0x0f: {
        dadrLatch_ = (dadrLatch_ & 0x00ffff) | uint32_t(data) << 16;
        // DADR is left-justified like the GPR offsets: word = (DADR & mask) >> MEMSIZ
        const int32_t a = (int32_t(dadrLatch_) & memmask_) >> memshift_;
        if (ramSel_) dilLatch_ = int32_t(dram_[a & dramMask_]) * 256;
        else dram_[a & dramMask_] = int16_t(dolLatch_ >> 8);
        break;
    }
    case 0x10: dadrLatch_ = (dadrLatch_ & 0xff00ff) | uint32_t(data) << 8; break;
    case 0x11: dadrLatch_ = (dadrLatch_ & 0xffff00) | data; break;
    case 0x12:
        hostControl_ = (hostControl_ & 0x4) | (data & 0x3);
        if (hostControl_ & 2) {
            if (state_ == Halted) std::fill(dram_.begin(), dram_.end(), int16_t(0));
            hostControl_ &= ~0x2;
        }
        break;
    case 0x14: ramSel_ = data & 0x80; break;
    case 0x18: hostSerial_ = data; break;
    case 0x1f:
        if (haltLine_) state_ = Halted;
        break;
    case 0x80:   // read select: GPR + instruction
        if (data < 0xa0) instrLatch_ = instr_[data];
        if (data < 0xc0) gprLatch_ = gpr_[data] & 0xffffff;
        else if (data >= 0xea) gprLatch_ = readReg(data);
        break;
    case 0xa0: writeReg(data, gprLatch_); break;
    case 0xc0: if (data < 0xa0) instr_[data] = instrLatch_ & 0xffffffffffffULL; break;
    case 0xe0:
        if (data < 0xa0) instr_[data] = instrLatch_ & 0xffffffffffffULL;
        writeReg(data, gprLatch_);
        break;
    }
}

int32_t ES5510::readReg(uint8_t reg) {
    if (reg < 0xc0) return gpr_[reg];
    switch (reg) {
    case 234: return int32_t(ser_[1]) * 256;   // SER0R
    case 235: return int32_t(ser_[0]) * 256;   // SER0L
    case 236: return int32_t(ser_[3]) * 256;
    case 237: return int32_t(ser_[2]) * 256;
    case 238: return int32_t(ser_[5]) * 256;
    case 239: return int32_t(ser_[4]) * 256;
    case 240: return int32_t(ser_[7]) * 256;
    case 241: return int32_t(ser_[6]) * 256;
    case 242: return macOverflow_ ? (machl_ < 0 ? 0 : 0x00ffffff) : int32_t(machl_ & 0x00ffffff);
    case 243: return macOverflow_ ? (machl_ < 0 ? 0x00800000 : 0x007fffff) : int32_t((machl_ >> 24) & 0x00ffffff);
    case 244: return int32_t(dil_) * 256;
    case 245: return dlength_;
    case 246: return abase_;
    case 247: return bbase_;
    case 248: return dbase_;
    case 249: return sigreg_;
    case 250: return ccr_ << 16;
    case 251: return cmr_ << 16;
    case 252: return 0x00ffffff;
    case 253: return 0x00800000;
    case 254: return 0x007fffff;
    }
    return 0;
}

void ES5510::writeReg(uint8_t reg, int32_t value) {
    value &= 0x00ffffff;
    if (reg < 0xc0) { gpr_[reg] = value; return; }
    switch (reg) {
    case 234: ser_[1] = int16_t(value >> 8); break;
    case 235: ser_[0] = int16_t(value >> 8); break;
    case 236: ser_[3] = int16_t(value >> 8); break;
    case 237: ser_[2] = int16_t(value >> 8); break;
    case 238: ser_[5] = int16_t(value >> 8); break;
    case 239: ser_[4] = int16_t(value >> 8); break;
    case 240: ser_[7] = int16_t(value >> 8); break;
    case 241: ser_[6] = int16_t(value >> 8); break;
    case 242: {   // MACL
        int64_t v = (machl_ & (int64_t(0xffffff) << 24)) | value;
        machl_ = ((v & 0xffffffffffffLL) ^ 0x800000000000LL) - 0x800000000000LL;
        break;
    }
    case 243: {   // MACH
        int64_t v = (machl_ & 0xffffff) | int64_t(value) << 24;
        machl_ = ((v & 0xffffffffffffLL) ^ 0x800000000000LL) - 0x800000000000LL;
        macOverflow_ = false;
        break;
    }
    case 244: {   // MEMSIZ when written
        int n = 0;
        for (int32_t x = value; x & 1; x >>= 1) ++n;
        memshift_ = n;
        memsiz_ = 0x00ffffff >> (24 - n);
        memmask_ = 0x00ffffff & ~memsiz_;
        memincrement_ = 1 << n;
        dbase_ &= memmask_;
        break;
    }
    case 245: dlength_ = value; break;
    case 246: abase_ = value; break;
    case 247: bbase_ = value; break;
    case 248: dbase_ = value; break;
    case 249: sigreg_ = value; mulshift_ = (sigreg_ >> 22 & 1) ? 1 : 2; break;
    case 250: ccr_ = uint8_t(value >> 16) & kFlagMask; break;
    case 251: cmr_ = uint8_t(value >> 16) & (kFlagMask | kNot); break;
    }
}

void ES5510::writeDol(int32_t value) {
    const int16_t v = int16_t(value >> 8);
    if (dolCount_ >= 2) { dol_[0] = dol_[1]; dol_[1] = v; }
    else dol_[dolCount_++] = v;
}

int32_t ES5510::aluOp(uint8_t op, int32_t a, int32_t b, uint8_t &flags) const {
    switch (op) {
    case 0x0: { int32_t t = add(a, b, flags); return saturate(t, flags, (a & 0x00800000) != 0); }
    case 0x1: { int32_t t = add(a, negate(b), flags); return saturate(t, flags, (a & 0x00800000) != 0); }
    case 0x2: return add(a, b, flags);
    case 0x3: return add(a, negate(b), flags);
    case 0x4: add(a, negate(b), flags); return a;
    case 0x5: a &= b; flags = flagTo(flagTo(flags, kN, (a & 0x00800000) != 0), kZ, a == 0); return a;
    case 0x6: a |= b; flags = flagTo(flagTo(flags, kN, (a & 0x00800000) != 0), kZ, a == 0); return a;
    case 0x7: a ^= b; flags = flagTo(flagTo(flags, kN, (a & 0x00800000) != 0), kZ, a == 0); return a;
    case 0x8: {   // ABS (ones' complement), Z stays
        const bool neg = (b & 0x00800000) != 0;
        flags &= ~kN;
        flags = flagTo(flags, kC, neg);
        return neg ? (0x00ffffff ^ b) : b;
    }
    case 0x9: return b;
    case 0xa: return asl(b, 2, flags);
    case 0xb: return asl(b, 8, flags);
    case 0xc:   // LS15: C = bit 8 of the operand
        flags &= ~kN;
        flags = flagTo(flags, kC, (b & 0x100) != 0);
        return (b << 15) & 0x007fffff;
    case 0xd: return add(0x007fffff, negate(b), flags);
    case 0xe:
        flags = flagTo(flags, kN, (b & 0x00800000) != 0);
        flags = flagTo(flags, kC, (b & 1) != 0);
        return (b >> 1) | (b & 0x00800000);
    }
    return 0;
}

int32_t ES5510::wordAddress(int32_t base, int32_t offset) const {
    const int32_t words = 0x00ffffff >> memshift_;   // address bits above MEMSIZ
    return (((base & memmask_) >> memshift_) + ((offset & memmask_) >> memshift_)) & words;
}

void ES5510::endOfProgram() {
    if (haltLine_) {
        state_ = Halted;
        hostControl_ &= ~0x04;
    }
    dbase_ -= memincrement_;
    if (dbase_ < 0) dbase_ = dlength_;
    if (state_ == Running) pc_ = 0;
}

void ES5510::step() {
    const uint8_t ipc = pc_;
    ramPP_ = ramP_;
    ramP_ = ram_;
    const uint64_t in = instr_[ipc % 160];

    // RAM cycle N-2 (a read): the data lands in DIL
    if (ramPP_.cycle != Write) dil_ = ramPP_.io ? 0 : dram_[ramPP_.address & dramMask_];

    // start of RAM cycle N
    static const RamCycle kCycle[8] = {Read, Write, Read, Write, Read, Dump, Read, Write};
    static const RamAccess kAccess[8] = {Delay, Delay, TableA, TableA, TableB, Delay, Io, Io};
    const int rc = int(in >> 3) & 7;
    ram_.cycle = kCycle[rc];
    ram_.io = kAccess[rc] == Io;
    const int32_t offset = gpr_[ipc % 0xc0];
    switch (kAccess[rc]) {
    case Delay: {   // (base + offset) modulo the delay length; mostly one subtraction
        const uint32_t mod = uint32_t(dlength_ >> memshift_) + 1;
        uint32_t a = uint32_t((dbase_ & memmask_) >> memshift_) + uint32_t((offset & memmask_) >> memshift_);
        if (a >= mod) { a -= mod; if (a >= mod) a %= mod; }
        ram_.address = int32_t(a);
        break;
    }
    case TableA: ram_.address = wordAddress(abase_, offset); break;
    case TableB: ram_.address = wordAddress(bbase_, offset); break;
    case Io: ram_.address = offset & 0x00fffff0; break;
    }

    // the ALU result of N-1 and its flags: the flags count for the skip test of N
    int32_t aluResult = 0;
    if (alu_.write) {
        uint8_t flags = ccr_;
        aluResult = aluOp(alu_.op, alu_.a, alu_.b, flags);
        if (alu_.updateCcr) ccr_ = flags;
    }

    const OpSel &sel = kOpSel[(in >> 8) & 0x0f];
    const bool skippable = (in & 0x80) != 0;
    bool skip = false;
    if (skippable) {
        skip = (ccr_ & cmr_ & kFlagMask) != 0;
        if (cmr_ & kNot) skip = !skip;
    }

    // write the multiplier result of N-1
    if (mac_.write) {
        const int64_t product = int64_t(sext24(mac_.c)) * int64_t(sext24(mac_.d)) * (int64_t(1) << mulshift_);
        const int64_t result = mac_.accumulate ? product + machl_ : product;
        macOverflow_ = result < -(int64_t(1) << 47) || result >= (int64_t(1) << 47);
        machl_ = result;
        const int32_t t = macOverflow_ ? (machl_ < 0 ? 0x00800000 : 0x007fffff) : int32_t((result >> 24) & 0xffffff);
        if (mac_.dst & SrcReg) writeReg(mac_.cReg, t);
        if (mac_.dst & SrcDelay) writeDol(t);
    }

    // start the multiplier cycle N
    mac_.cReg = uint8_t(in >> 32);
    mac_.dReg = uint8_t(in >> 40);
    mac_.src = sel.macSrc;
    mac_.dst = sel.macDst;
    mac_.accumulate = ((in >> 6) & 1) != 0;
    mac_.write = !skip;
    mac_.c = mac_.src == SrcReg ? readReg(mac_.cReg) : int32_t(dil_) * 256;
    mac_.d = readReg(mac_.dReg);

    // write the ALU result of N-1
    if (alu_.write && alu_.op != kCmp) {
        if (alu_.dst & SrcReg) writeReg(alu_.aReg, aluResult);
        if (alu_.dst & SrcDelay) writeDol(aluResult);
    }

    // start the ALU cycle N
    alu_.aReg = uint8_t(in >> 16);
    alu_.bReg = uint8_t(in >> 24);
    alu_.op = uint8_t(in >> 12) & 0x0f;
    alu_.src = sel.aluSrc;
    alu_.dst = sel.aluDst;
    alu_.write = alu_.op != kEnd && (!skip || alu_.op == kCmp);
    alu_.updateCcr = !skippable;   // a skippable instruction never changes the CCR
    if (alu_.op == kEnd) {
        endOfProgram();
    } else if (kOperands[alu_.op] == 1) {
        alu_.b = alu_.src == SrcReg ? readReg(alu_.bReg) : int32_t(dil_) * 256;
    } else {
        alu_.a = alu_.src == SrcReg ? readReg(alu_.aReg) : int32_t(dil_) * 256;
        alu_.b = readReg(alu_.bReg);
    }

    // RAM cycle N-1: a write puts the front of the DOL FIFO out, a write or dump ejects it
    if (ramP_.cycle != Read) {
        if (ramP_.cycle == Write && !ramP_.io) dram_[ramP_.address & dramMask_] = dol_[0];
        dol_[0] = dol_[1];
        if (dolCount_ > 0) --dolCount_;
    }
    if (alu_.op != kEnd) ++pc_;   // END: endOfProgram() set it
}

void ES5510::runFrame() {
    if (state_ == Halted) {
        if (haltLine_) { hostControl_ &= ~0x04; return; }
        state_ = Running;
        hostControl_ |= 0x04;
        pc_ = 0;
    }
    for (int n = 0; n < 160 && state_ == Running; ++n) {
        const bool end = ((instr_[pc_ % 160] >> 12) & 0x0f) == kEnd;
        step();
        if (end) {
            if (state_ == Halted) {   // HALT: finish END (the pending results), empty the DOL FIFO
                if (mac_.write) {
                    const int64_t p = int64_t(sext24(mac_.c)) * int64_t(sext24(mac_.d)) * (int64_t(1) << mulshift_);
                    machl_ = mac_.accumulate ? p + machl_ : p;
                    mac_.write = false;
                }
                if (ramP_.cycle == Write && !ramP_.io) dram_[ramP_.address & dramMask_] = dol_[0];
                dolCount_ = 0;
                ram_ = ramP_ = ramPP_ = Ram{};
            }
            return;
        }
    }
}

}  // namespace eps
