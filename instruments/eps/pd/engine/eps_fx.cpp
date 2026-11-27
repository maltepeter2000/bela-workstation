// The EPS-16 PLUS effect machine: see eps_fx.h.
#include "eps_fx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "m68k.h"
}

namespace eps {
namespace {

// the effect block's place in the 68000 address space (sample RAM on the EPS)
constexpr uint32_t kBlock = 0x600000, kBlockMax = 0x40000;
// small helpers below $2000 (the OS file starts at $2000; $0000-$1FFF are OS variables)
constexpr uint32_t kAline = 0x1F00, kRte = 0x1F08, kStop = 0x1F10, kSentinel = 0x1F20;
constexpr uint32_t kStack = 0xFFCA00;     // the OS stack area
constexpr uint32_t kModTable = 0xDF4C;    // mod sources, 2 bytes each
// OS / ROM entry points (OS 1.30, ROM 1.00F)
constexpr uint32_t kRomLoader = 0xC0F52E;   // A3 = program section: ESP instructions and GPRs
constexpr uint32_t kOsParams = 0xFFB0A0;    // every parameter of the effect at $C88
constexpr uint32_t kOsParam = 0x7EA2;       // one descriptor: A3, A6 = its value, A1 = effect
constexpr uint32_t kOsRealtime = 0x7EC6;    // the effect's real-time routine

FxMachine *g_current = nullptr;
size_t g_contextSize = 0;
bool g_coreReady = false;

void hook(unsigned int pc) { if (g_current) g_current->onInstruction(pc & 0xffffff); }

}  // namespace
}  // namespace eps

extern "C" {
unsigned int m68k_read_memory_8(unsigned int a) { return eps::g_current ? eps::g_current->read8(a) : 0; }
unsigned int m68k_read_memory_16(unsigned int a) { return m68k_read_memory_8(a) << 8 | m68k_read_memory_8(a + 1); }
unsigned int m68k_read_memory_32(unsigned int a) { return m68k_read_memory_16(a) << 16 | m68k_read_memory_16(a + 2); }
void m68k_write_memory_8(unsigned int a, unsigned int v) { if (eps::g_current) eps::g_current->write8(a, uint8_t(v)); }
void m68k_write_memory_16(unsigned int a, unsigned int v) { m68k_write_memory_8(a, v >> 8); m68k_write_memory_8(a + 1, v); }
void m68k_write_memory_32(unsigned int a, unsigned int v) { m68k_write_memory_16(a, v >> 16); m68k_write_memory_16(a + 2, v); }
unsigned int m68k_read_disassembler_8(unsigned int a) { return m68k_read_memory_8(a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_memory_16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_memory_32(a); }
}

namespace eps {

FxMachine::FxMachine() : mem_(0x10000, 0), rom_(0x20000, 0) {
    if (!g_coreReady) {
        g_current = nullptr;
        m68k_init();
        m68k_set_cpu_type(M68K_CPU_TYPE_68000);
        m68k_set_instr_hook_callback(hook);
        g_contextSize = m68k_context_size();
        g_coreReady = true;
    }
    ctx_.assign(g_contextSize, 0);
    m68k_get_context(ctx_.data());
}

FxMachine::~FxMachine() { if (g_current == this) g_current = nullptr; }

double FxMachine::rate() const {
    static const double kRate[3] = {625000.0 / 21, 625000.0 / 14, 625000.0 / 8};
    return kRate[rateMode_ < 0 || rateMode_ > 2 ? 2 : rateMode_];
}

uint8_t FxMachine::read8(uint32_t a) {
    a &= 0xffffff;
    if (a < 0x10000) return mem_[a];
    if (a >= 0xFF0000) return mem_[a - 0xFF0000];
    if (a >= 0xC00000 && a < 0xC20000) return rom_[a - 0xC00000];
    if (a >= 0x260000 && a < 0x260200) return (a & 1) ? esp_.hostRead(int(a >> 1) & 0xff) : 0xff;
    if (a >= kBlock && a < kBlock + block_.size()) return block_[a - kBlock];
    return 0;
}

void FxMachine::write8(uint32_t a, uint8_t v) {
    a &= 0xffffff;
    if (a < 0x10000) { mem_[a] = v; return; }
    if (a >= 0xFF0000) { mem_[a - 0xFF0000] = v; return; }
    if (a >= 0x260000 && a < 0x260200) { if (a & 1) esp_.hostWrite(int(a >> 1) & 0xff, v); return; }
    if (a >= kBlock && a < kBlock + block_.size()) block_[a - kBlock] = v;
}

static unsigned long g_steps = 0;
void FxMachine::onInstruction(uint32_t pc) {
    ++g_steps;
    if (pc != kStop) { lastPc_[0] = lastPc_[1]; lastPc_[1] = lastPc_[2]; lastPc_[2] = pc; }
    if (pc == kSentinel || pc == kStop) {
        if (pc == kStop && std::getenv("FXDEBUG")) {
            const uint32_t sp = m68k_get_reg(nullptr, M68K_REG_A7);
            std::fprintf(stderr, "fx exception: from pc %06x (sr %04x), last %06x %06x %06x\n",
                         unsigned(read8(sp + 2) << 24 | read8(sp + 3) << 16 | read8(sp + 4) << 8 | read8(sp + 5)) & 0xffffff,
                         unsigned(read8(sp) << 8 | read8(sp + 1)), unsigned(lastPc_[0]), unsigned(lastPc_[1]), unsigned(lastPc_[2]));
        }
        stopped_ = true;
        m68k_end_timeslice();
    }
}

void FxMachine::put16(uint32_t a, uint16_t v) { write8(a, uint8_t(v >> 8)); write8(a + 1, uint8_t(v)); }
void FxMachine::put32(uint32_t a, uint32_t v) { put16(a, uint16_t(v >> 16)); put16(a + 2, uint16_t(v)); }
uint16_t FxMachine::blk16(uint32_t off) const {
    return off + 1 < block_.size() ? uint16_t(block_[off] << 8 | block_[off + 1]) : 0;
}

bool FxMachine::setSystem(const std::vector<uint8_t> &rom, const std::vector<uint8_t> &os, std::string &err) {
    system_ = false;
    if (rom.size() < 0x20000) { err = "the ROM file is not 128 KB"; return false; }
    size_t skip = os.size() > 512 && os[0] == 0x0d && os[1] == 0x0a ? 512 : 0;   // EFE header
    if (os.size() < skip + 0xE000) { err = "the OS file is too short"; return false; }
    std::memcpy(rom_.data(), rom.data(), 0x20000);
    std::fill(mem_.begin(), mem_.end(), uint8_t(0));
    std::memcpy(mem_.data() + 0x2000, os.data() + skip, 0xE000);   // the OS from $2000 (to $FFFF)
    // the jump table entry the drivers use most: JMP $C0F758 at $8030 (OS 1.30)
    if (!(mem_[0x8030] == 0x4E && mem_[0x8031] == 0xF9 && mem_[0x8034] == 0xF7 && mem_[0x8035] == 0x58)) {
        err = "the OS file is not OS 1.30 for the EPS-16 PLUS";
        return false;
    }
    // vectors: A-line = "SR = D0" (move.w D0,(SP); addq.l #2,2(SP); rte), trap #7 / #8
    // (yield, wait) return at once, every other exception stops the run
    for (int v = 2; v < 64; ++v) put32(uint32_t(4 * v), kStop);
    put32(0x28, kAline);
    put32(0x9C, kRte);
    put32(0xA0, kRte);
    const uint8_t aline[] = {0x3E, 0x80, 0x54, 0xAF, 0x00, 0x02, 0x4E, 0x73};
    std::memcpy(mem_.data() + kAline, aline, sizeof aline);
    put16(kRte, 0x4E73);
    put16(kStop, 0x4E71);
    put16(kSentinel, 0x4E71);
    // OS variables the parameter routines read, as the OS has them after booting (measured):
    // $228 / $229 effect paths on (0 mutes some outputs), $216 = $7F
    mem_[0x228] = 1;
    mem_[0x229] = 1;
    mem_[0x216] = 0x7f;
    put16(kModTable + 8, 0x4000);    // bend in the centre
    put16(kModTable + 18, 0x7F00);   // volume up
    system_ = true;
    return true;
}

bool FxMachine::call(uint32_t pc, uint32_t a0, uint32_t a1, uint32_t a3, uint32_t a6, uint32_t d1, int budget, uint32_t a2) {
    FxMachine *before = g_current;
    g_current = this;
    m68k_set_context(ctx_.data());
    put32(kStack - 4, kSentinel);
    m68k_set_reg(M68K_REG_SR, 0x2700);
    m68k_set_reg(M68K_REG_A7, kStack - 4);
    m68k_set_reg(M68K_REG_A0, a0);
    m68k_set_reg(M68K_REG_A1, a1);
    m68k_set_reg(M68K_REG_A2, a2);
    m68k_set_reg(M68K_REG_A3, a3);
    m68k_set_reg(M68K_REG_A4, kModTable | 0xFF0000);
    m68k_set_reg(M68K_REG_A6, a6);
    m68k_set_reg(M68K_REG_A5, block_.size() > 0x70 ? kBlock + blk16(0x2e + 2u * block_[0x63]) : 0);
    m68k_set_reg(M68K_REG_D1, d1);
    m68k_set_reg(M68K_REG_PC, pc);
    stopped_ = false;
    for (int i = 0; i < budget && !stopped_; ++i) m68k_execute(10000);
    const bool ok = stopped_ && (m68k_get_reg(nullptr, M68K_REG_PC) & 0xffffff) != kStop + 2 &&
                    (m68k_get_reg(nullptr, M68K_REG_PC) & 0xffffff) != kStop;
    m68k_get_context(ctx_.data());
    g_current = before;
    if (std::getenv("FXDEBUG")) std::fprintf(stderr, "fx call %06x: %s, %lu instructions\n", unsigned(pc), ok ? "ok" : "STOPPED", g_steps);
    g_steps = 0;
    return ok;
}

void FxMachine::writeGpr(int reg, int32_t v) {
    for (int k = 0; k < 3; ++k) esp_.hostWrite(k, uint8_t(v >> (16 - 8 * k)));
    esp_.hostWrite(0xa0, uint8_t(reg));
}

void FxMachine::unload() {
    loaded_ = false;
    block_.clear();
    params_.clear();
    name_.clear();
    esp_.reset();
}

bool FxMachine::load(const uint8_t *block, size_t size, std::string &err) {
    unload();
    if (!system_) { err = "no ROM / OS for the effects"; return false; }
    if (size < 0x100 || size > kBlockMax) { err = "no effect"; return false; }
    block_.assign(block, block + size);
    const uint16_t section = blk16(0x26), init = blk16(0x2a), tables = blk16(0x28);
    if (section == 0 || section >= size) { err = "the effect has no ESP program"; block_.clear(); return false; }
    rateMode_ = block_[0x66];
    name_.assign(reinterpret_cast<const char *>(&block_[0x3c]), 12);
    while (!name_.empty() && (name_.back() == ' ' || name_.back() == 0)) name_.pop_back();

    esp_.reset();
    writeGpr(249, 0xffffff);   // SIGREG as the OS leaves it after booting (multiplier shift 1)
    put32(0xC84, kBlock);
    put32(0xC88, kBlock);
    mem_[0xC90] = 0;
    mem_[0xC81] = 0;
    put16(0xCBBA, 0);
    put32(0x8000, 0);
    if (!call(kRomLoader, kBlock, 0, kBlock + section, 0, 0, 4000)) { err = "the ESP program did not load"; unload(); return false; }
    if (tables) {
        const uint32_t t = kBlock + tables;
        const uint32_t a1 = t + 4, a2 = a1 + blk16(tables);
        const uint32_t d1 = blk16(tables + 2);
        if (!call(0xC0F894, 0, a1, 0, 0, d1, 2000, a2)) { err = "the effect tables did not load"; unload(); return false; }
    }
    if (init && !call(kBlock + init, kBlock, 0, 0, 0, 0, 2000)) { err = "the effect init stopped"; unload(); return false; }
    mem_[0xC90] = 0;   // the loader leaves the "ESP halted" flags set: the OS clears them
    mem_[0xC81] = 0;
    put16(0xCBBA, 0);
    if (!call(kOsParams, 0, kBlock, 0, 0, 0, 400)) { err = "the effect parameters stopped"; unload(); return false; }
    writeGpr(127, 0);   // faded up in tick(), as the OS does
    fade_ = 0;
    readParams();
    loaded_ = true;
    return true;
}

void FxMachine::readParams() {
    params_.clear();
    std::string group;
    int repeat = 1;
    const uint16_t from = blk16(0x36), to = blk16(0x38);
    for (uint32_t d1 = from; d1 && d1 <= to && d1 < block_.size(); d1 += 2) {
        const uint16_t d = blk16(d1);
        if (!d || d + 10u > block_.size()) continue;
        Param p;
        p.index = int(d1);
        p.type = block_[d + 1u];
        const uint16_t name = blk16(d + 6u);
        for (uint32_t i = name; i < block_.size() && i < name + 24u && block_[i]; ++i) p.name += char(block_[i]);
        while (!p.name.empty() && p.name.back() == ' ') p.name.pop_back();
        // flags: $80 starts a group, $C0 continues it (" R" after "ECHOTIME L"), $E0 repeats it
        // without a name (the 2nd, 3rd chorus voice): named after the group as the EPS shows it
        const uint8_t flags = block_[d];
        if ((flags & 0xc0) == 0xc0 && !params_.empty()) {
            std::string head = group;
            const size_t sp = head.find(' ');
            if (p.name.find_first_not_of(' ') == std::string::npos) p.name = head + " " + std::to_string(++repeat);
            else p.name = (sp == std::string::npos ? head : head.substr(0, sp)) + " " + p.name.substr(p.name.find_first_not_of(' '));
        } else {
            while (!p.name.empty() && p.name.front() == ' ') p.name.erase(0, 1);
            group = p.name;
            repeat = 1;
        }
        const uint16_t at = blk16(d + 4u);
        p.variation = block_[d + 8u] == 4;
        bool word = false, sign = false;
        if (p.type == 0x0e) {   // a list: [lo] [names] [width] [last], the names at <names>
            const uint16_t l = blk16(d + 2u);
            if (l + 6u <= block_.size()) {
                const uint32_t at = blk16(l + 2u);
                const int width = block_[l + 4u];
                p.hi = block_[l + 5u];
                for (int k = 0; k <= p.hi; ++k) {
                    std::string s;
                    for (int i = 0; i < width && at + k * width + i < block_.size(); ++i) {
                        const char c = char(block_[at + uint32_t(k * width + i)]);
                        if (c) s += c;
                    }
                    while (!s.empty() && s.back() == ' ') s.pop_back();
                    p.items.push_back(s);
                }
            }
        } else if (p.type == 0x17) {
            word = true;
            p.hi = blk16(d + 2u);
        } else {
            sign = p.type == 3;
            p.lo = sign ? int8_t(block_[d + 2u]) : block_[d + 2u];
            p.hi = sign ? int8_t(block_[d + 3u]) : block_[d + 3u];
        }
        const uint32_t v = p.variation ? 0x63u : at + blk16(0x2e + 2u * block_[0x63]);
        if (v + 1 < block_.size())
            p.value = word ? int16_t(blk16(v)) : (sign ? int8_t(block_[v]) : block_[v]);
        params_.push_back(p);
    }
}

bool FxMachine::setParam(int k, int value) {
    if (!loaded_ || k < 0 || k >= int(params_.size())) return false;
    Param &p = params_[size_t(k)];
    if (value < p.lo) value = p.lo;
    if (value > p.hi) value = p.hi;
    const uint16_t d = blk16(uint32_t(p.index));
    if (p.variation) {   // another variation: every parameter again, with its values
        block_[0x63] = uint8_t(value);
        call(kOsParams, 0, kBlock, 0, 0, 0, 400);
        readParams();
        return true;
    }
    const uint32_t v = blk16(d + 4u) + blk16(0x2e + 2u * block_[0x63]);
    if (v + 1 >= block_.size()) return false;
    if (p.type == 0x17) { block_[v] = uint8_t(value >> 8); block_[v + 1] = uint8_t(value); }
    else block_[v] = uint8_t(value);
    p.value = value;
    return call(kOsParam, 0, kBlock, kBlock + d, kBlock + v, uint32_t(p.index), 100);
}

void FxMachine::setMod(int slot, int value) {
    if (slot < 0 || slot > 15) return;
    put16(kModTable + 2u * uint32_t(slot), uint16_t((value & 0x7f) << 8));
}

void FxMachine::tick() {
    if (!loaded_) return;
    if (fade_ < 0x7fff) {   // the OS fades the effect output up after a load
        fade_ = fade_ + 0x0c00 > 0x7fff ? 0x7fff : fade_ + 0x0c00;
        writeGpr(127, fade_ << 8);
    }
    mem_[0xC81] = 0;
    mem_[0xC90] = 0;
    put16(0xCBBA, 0);
    call(kOsRealtime, 0, 0, 0, 0, 0, 20);
}

void FxMachine::frame(const int16_t in[6], int16_t &outL, int16_t &outR) {
    if (!loaded_) { outL = outR = 0; return; }
    esp_.setSerial(0, in[0]);
    esp_.setSerial(1, in[1]);
    esp_.setSerial(4, in[2]);
    esp_.setSerial(5, in[3]);
    esp_.setSerial(6, in[4]);
    esp_.setSerial(7, in[5]);
    esp_.runFrame();
    outL = esp_.serial(2);
    outR = esp_.serial(3);
}

namespace {
// An effect block at o in the ROM: its size (0 = none). The ROM keeps the length as a plain long.
uint32_t romBlock(const std::vector<uint8_t> &rom, size_t o) {
    auto w = [&](size_t i) { return uint32_t(rom[i] << 8 | rom[i + 1]); };
    if (o + 0x80 >= rom.size()) return 0;
    const uint32_t len = w(o) << 16 | w(o + 2);
    if (len < 0x100 || len > 0x8000 || o + len > rom.size()) return 0;
    const uint32_t from = w(o + 0x36), to = w(o + 0x38);
    if (from < 0x60 || to <= from || to >= len || rom[o + 0x66] > 2) return 0;
    for (size_t i = 0; i < 12; ++i)
        if (rom[o + 0x3c + i] < 32 || rom[o + 0x3c + i] > 126) return 0;
    return (len + 1) & ~1u;
}
}  // namespace

std::vector<RomEffect> findRomEffects(const std::vector<uint8_t> &rom) {
    // The names the manual gives the 13 ROM effects (the blocks' own names repeat).
    static const char *const kNames[13] = {
        "HALL REVERB", "44KHZ REVERB", "ROOM REVERB", "DUAL DELAYS", "44KHZ DELAYS",
        "CHORUS+REVERB", "PHASER+REVERB", "FLANGER+REV", "ROT. SPKR+REV", "CHOR+REV+DDL",
        "CMP+DIST+REV", "DIST+CHO+REV", "WAH+DIST+REV"};
    std::vector<RomEffect> out;
    // The OS's table: consecutive pointers ($C0xxxx) to the effect blocks, ROM 1 to 13.
    for (size_t t = 0; t + 4 <= rom.size() && out.empty(); t += 2) {
        std::vector<RomEffect> run;
        for (size_t i = t; i + 4 <= rom.size(); i += 4) {
            const uint32_t v = uint32_t(rom[i]) << 24 | uint32_t(rom[i + 1]) << 16 | uint32_t(rom[i + 2]) << 8 | rom[i + 3];
            if ((v & 0xFFF00000u) != 0x00C00000u) break;
            const uint32_t o = v - 0xC00000u, size = romBlock(rom, o);
            if (!size) break;
            RomEffect e;
            e.offset = o;
            e.size = size;
            e.rateMode = rom[o + 0x66];
            for (size_t k = 0; k < 12; ++k) e.name += char(rom[o + 0x3c + k]);
            while (!e.name.empty() && e.name.back() == ' ') e.name.pop_back();
            while (!e.name.empty() && e.name.front() == ' ') e.name.erase(0, 1);
            run.push_back(e);
        }
        if (run.size() >= 8) out = run;
    }
    if (out.size() == 13)
        for (size_t k = 0; k < 13; ++k) out[k].name = kNames[k];
    return out;
}

} // namespace eps
