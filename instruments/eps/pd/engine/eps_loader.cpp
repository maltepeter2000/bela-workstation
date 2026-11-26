// eps_loader.cpp — see eps_loader.h for scope and licensing notes.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "eps_loader.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>

namespace eps {

namespace {

constexpr uint32_t kBlock = 512;
constexpr uint32_t kDirEntrySize = 26;
constexpr uint32_t kWaveSampleHeader = 0x120;   // incl. 4-byte length word

uint16_t be16(const uint8_t *p) { return uint16_t(p[0] << 8 | p[1]); }
uint32_t be24(const uint8_t *p) { return uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]; }
uint32_t be32(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
uint32_t le32(const uint8_t *p) {
    return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0];
}
uint16_t le16(const uint8_t *p) { return uint16_t(p[1] << 8 | p[0]); }

// Ensoniq "long": two 16-bit words, LSW first, each carrying 12 bits in its
// upper bits.
uint32_t epsLong(const uint8_t *p) {
    uint32_t lsw = be16(p) >> 4;
    uint32_t msw = be16(p + 2) >> 4;
    return msw << 12 | lsw;
}

void putEpsLong(uint8_t *p, uint32_t v) {
    const uint32_t lo = (v & 0xFFF) << 4, hi = (v >> 12 & 0xFFF) << 4;
    p[0] = uint8_t(lo >> 8); p[1] = uint8_t(lo); p[2] = uint8_t(hi >> 8); p[3] = uint8_t(hi);
}

// Offset fields in the instrument header (nybble-packed 24-bit values).
uint32_t encoded24(const uint8_t *p) {
    uint32_t b0 = p[0], b2 = p[2], b3 = p[3];
    return (b3 >> 4 & 0x0F) << 20 | b0 << 12 | b2 << 4 | (b3 & 0x0F);
}
void putEncoded24(uint8_t *p, uint32_t v) {
    p[0] = uint8_t(v >> 12); p[1] = 0; p[2] = uint8_t(v >> 4); p[3] = uint8_t((v >> 20 & 0xF) << 4 | (v & 0xF));
}

std::string trimRight(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

std::string printable(const std::string &s) {
    std::string r;
    for (unsigned char c : s) r += (c >= 32 && c < 127) ? char(c) : '?';
    return r;
}

// Bounds-checked sequential reader/writer mirroring ConvertWithMoss' StreamUtils.
// The same field list reads a file (const data) or writes the fields back into a
// copy of it (mutable data), so saving changes only the fields eps~ knows.
class Codec {
public:
    Codec(const uint8_t *d, size_t n) : r_(d), n_(n) {}
    Codec(uint8_t *d, size_t n) : r_(d), w_(d), n_(n) {}
    bool ok() const { return ok_; }
    bool writing() const { return w_ != nullptr; }
    size_t pos() const { return pos_; }
    void skip(size_t k) { need(k) ? pos_ += k : 0; }
    void u8(int &v) {
        if (!need(1)) { if (!w_) v = 0; return; }
        if (w_) w_[pos_] = uint8_t(v); else v = r_[pos_];
        ++pos_;
    }
    void s8(int &v) { u8(v); if (!w_) v = int(int8_t(uint8_t(v))); }
    // Parameter stored in the high byte of a 16-bit word.
    void u8w(int &v) { u8(v); skip(1); }
    void s8w(int &v) { s8(v); skip(1); }
    void u16(int &v) {
        if (!need(2)) { if (!w_) v = 0; return; }
        if (w_) { w_[pos_] = uint8_t(v >> 8); w_[pos_ + 1] = uint8_t(v); } else v = be16(r_ + pos_);
        pos_ += 2;
    }
    // ASCII text with one character per 16-bit word (high byte); read trims the
    // padding, write pads with spaces.
    void ascii(std::string &s, size_t chars) {
        std::string r;
        for (size_t i = 0; i < chars; ++i) {
            int ch = w_ ? (i < s.size() ? (unsigned char)s[i] : ' ') : 0;
            u8(ch);
            skip(1);
            r += char(ch);
        }
        if (!w_) s = trimRight(r);
    }
    // 32-bit value assembled from the high bytes of four words.
    void highBytes32(uint32_t &v) {
        uint32_t r = 0;
        for (int i = 0; i < 4; ++i) {
            int b = int(v >> (24 - 8 * i) & 0xFF);
            u8(b);
            skip(1);
            r = r << 8 | uint32_t(b);
        }
        if (!w_) v = r;
    }

private:
    bool need(size_t k) {
        if (pos_ + k > n_) { ok_ = false; pos_ = n_; return false; }
        return true;
    }
    const uint8_t *r_;
    uint8_t *w_ = nullptr;
    size_t n_, pos_ = 0;
    bool ok_ = true;
};

void codeEnvelope(Codec &c, Envelope &e) {
    c.u8w(e.type);
    c.u8w(e.softLevel[0]);
    c.u8w(e.hardLevel[0]);
    for (int i = 1; i <= 4; ++i) {
        c.u8w(e.time[i]);
        c.u8w(e.softLevel[i]);
        c.u8w(e.hardLevel[i]);
    }
    c.u8w(e.time[5]);
    c.u8w(e.velocitySwitch);
    c.s8w(e.level5);
    c.u8w(e.time[6]);
    c.u8w(e.time1VelSens);
    c.u8w(e.kbTimeScaling);
    c.u8w(e.mode);
}

int sampleRateFromIndex(int idx) {
    if (idx == 0) return 48000;
    if (idx == 1) return 44100;
    return int(625000.0 / idx + 0.5);
}

bool codeWaveSample(Codec &c, WaveSample &ws) {
    c.skip(6);
    c.ascii(ws.name, 12);
    c.u8w(ws.copyOfWaveSample);
    c.u8w(ws.copyOfLayer);
    codeEnvelope(c, ws.pitchEnv);
    codeEnvelope(c, ws.filterEnv);
    codeEnvelope(c, ws.ampEnv);
    c.u8(ws.rootNote);
    c.u8(ws.volumeModCrossfadeCurve);
    c.s8w(ws.pitchEnvAmount);
    c.s8w(ws.lfoPitchAmount);
    c.s8w(ws.randomModAmount);
    c.u8w(ws.pitchBendRange);
    c.u8w(ws.pitchModSource);
    c.s8w(ws.fineTune);
    c.s8w(ws.pitchModAmount);
    c.u8w(ws.filterMode);
    c.u8w(ws.filterCutoff[0]);
    c.u8w(ws.filterCutoff[1]);
    c.s8w(ws.filterKeyAmount[0]);
    c.s8w(ws.filterKeyAmount[1]);
    c.s8w(ws.filterEnvAmount[0]);
    c.s8w(ws.filterEnvAmount[1]);
    c.u8w(ws.filterModSource[0]);
    c.u8w(ws.filterModSource[1]);
    c.s8w(ws.filterModAmount[0]);
    c.s8w(ws.filterModAmount[1]);
    c.u8(ws.volume);
    c.u8(ws.outputBus);
    c.u8(ws.ampModSource);
    c.u8(ws.panModSource);
    for (int i = 0; i < 4; ++i) c.s8w(ws.crossfade[i]);
    c.u8(ws.panEps);
    c.s8(ws.panSigned);
    c.u8(ws.ampModAmount);
    c.u8(ws.panModAmount);
    c.u8(ws.lfoWaveform);
    c.u8(ws.boost);
    c.u8(ws.lfoSpeed);
    c.s8(ws.lfoRateModAmount);
    c.u8w(ws.lfoDepth);
    c.u8(ws.lfoDelay);
    c.s8(ws.lfoDepthModAmount);
    c.u8(ws.lfoDepthModSource);
    c.u8(ws.lfoRateModSource);
    c.u8w(ws.lfoMode);
    c.u8w(ws.randomFreq);
    c.u8w(ws.loopMode);
    c.highBytes32(ws.rawStart);
    c.highBytes32(ws.rawEnd);
    c.highBytes32(ws.rawLoopStart);
    c.highBytes32(ws.rawLoopEnd);
    c.u8w(ws.sampleRateIndex);
    c.u8w(ws.keyLow);
    c.u8w(ws.keyHigh);
    for (int i = 0; i < 5; ++i) c.u8w(ws.waveModRaw[i]);
    if (c.writing()) return c.ok();
    ws.sampleStart = ws.rawStart >> 9;
    ws.sampleEnd = ws.rawEnd >> 9;
    ws.loopStart = ws.rawLoopStart >> 9;
    // As in ConvertWithMoss: loop end carries a 4-bit fine part.
    ws.loopEnd = (ws.rawLoopEnd >> 9) + ((ws.rawLoopEnd >> 5) & 0xF);
    ws.sampleRate = sampleRateFromIndex(ws.sampleRateIndex);
    return c.ok();
}

// `d`/`n` cover the wavesample block *after* its 4-byte length word.
bool parseWaveSample(int index, const uint8_t *d, size_t n, WaveSample &ws, std::string &err) {
    Codec c(d, n);
    ws.index = index;
    if (!codeWaveSample(c, ws)) { err = "wavesample " + std::to_string(index) + ": header truncated"; return false; }

    const size_t headerAfterLen = kWaveSampleHeader - 4;
    if (n > headerAfterLen) {
        size_t bytes = n - headerAfterLen;
        const uint8_t *p = d + headerAfterLen;
        ws.pcm.resize(bytes / 2);
        for (size_t i = 0; i < ws.pcm.size(); ++i) ws.pcm[i] = int16_t(be16(p + 2 * i));
    }
    return true;
}

bool codeLayer(Codec &c, Layer &l) {
    c.skip(2);
    c.s8w(l.index);
    c.skip(2);
    c.ascii(l.name, 12);
    c.u8(l.glideMode);
    c.u8(l.delayModByVelocity);
    c.u8(l.glideTime);
    c.u8(l.restrikeDecay);
    c.u8(l.legatoLayer);
    c.u8(l.stereoLink);
    c.u8w(l.velocityLow);
    c.u8w(l.velocityHigh);
    c.u8w(l.pitchTable);
    c.u8w(l.delayTime);
    for (int i = 0; i < 88; ++i) {
        int k = l.keyMap[i];
        c.u8w(k);
        l.keyMap[i] = uint8_t(k);
    }
    return c.ok();
}

bool parseLayer(const uint8_t *d, size_t n, Layer &l, std::string &err) {
    Codec c(d, n);
    if (!codeLayer(c, l)) { err = "layer truncated"; return false; }
    return true;
}

// The instrument header up to the offset tables (100 bytes).
bool codeHeader(Codec &c, Instrument &in, std::string &name) {
    c.skip(4 + 4 + 2);    // total size, hardware slot address, unknown
    c.ascii(name, 12);
    c.s8w(in.midiOutChannel);
    c.s8w(in.midiProgram);
    c.s8w(in.pressureMode);
    c.u16(in.sizeBlocks);
    c.s8w(in.keyDestination);
    for (int i = 0; i < 4; ++i) c.s8w(in.patches[i]);
    c.s8w(in.keyDownLayers);
    c.s8w(in.keyUpLayers);
    c.s8w(in.patchSelectMode);
    c.skip(2);
    c.u16(in.instrumentId);
    c.s8w(in.keyLow);
    c.s8w(in.keyHigh);
    c.s8w(in.transpose);
    c.skip(32);                                   // pitch table offsets
    return c.ok() && c.pos() == 100;
}

bool readWholeFile(const std::string &path, std::vector<uint8_t> &out, std::string &err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open " + path; return false; }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

std::string lower(std::string s) {
    for (auto &ch : s) ch = char(tolower((unsigned char)ch));
    return s;
}

} // namespace

const char *fileTypeName(int t) {
    switch (t) {
        case kUnused: return "UNUSED";      case kOs: return "EPS OS";
        case kSubdir: return "SUBDIR";      case kInstrument: return "INST";
        case kEpsBank: return "EPS BANK";   case kEpsSeq: return "EPS SEQ";
        case kEpsSong: return "EPS SONG";   case kEpsSysex: return "SYSEX";
        case kParentPtr: return "PARENT";   case kEpsMacro: return "MACRO";
        case 17: case 18: case 19: return "SD SEQ";
        case kPlusBank: return "16+ BANK";  case kPlusFx: return "16+ FX";
        case kPlusSeq: return "16+ SEQ";    case kPlusSong: return "16+ SONG";
        case kPlusOs: return "16+ OS";      case kAsrSeq: return "ASR SEQ";
        case kAsrSong: return "ASR SONG";   case kAsrBank: return "ASR BANK";
        case kAsrTrack: return "ASR TRACK"; case kAsrOs: return "ASR OS";
        case kAsrFx: return "ASR FX";       case kAsrMacro: return "ASR MACRO";
        default: return "UNKNOWN";
    }
}

const char *imageFormatName(ImageFormat f) {
    switch (f) {
        case ImageFormat::Raw: return "RAW/IMG";
        case ImageFormat::Gkh: return "GKH";
        case ImageFormat::Efe: return "EFE";
        case ImageFormat::Ede: return "EDE";
        case ImageFormat::Eda: return "EDA";
    }
    return "?";
}

const WaveSample *Instrument::pcmOwner(const WaveSample &ws) const {
    const WaveSample *cur = &ws;
    for (int guard = 0; guard < 128 && cur->copyOfWaveSample != 0; ++guard) {
        auto it = waveSamples.find(cur->copyOfWaveSample);
        if (it == waveSamples.end()) return nullptr;
        cur = &it->second;
    }
    return cur->copyOfWaveSample == 0 ? cur : nullptr;
}

const char *Instrument::modelName() const {
    if (instrumentId == kIdEps16Plus) return "EPS-16 PLUS";
    if (instrumentId == kIdAsr10) return "ASR-10";
    return "EPS";
}

bool parseInstrument(const uint8_t *data, size_t size, const std::string &fallbackName,
                     Instrument &out, std::string &err) {
    out = Instrument();
    const size_t kHeader = 644;
    if (size < kHeader) { err = "instrument file too small"; return false; }
    Codec c(data, size);
    std::string name;
    if (!codeHeader(c, out, name)) { err = "instrument header layout mismatch"; return false; }
    out.name = printable(name.empty() ? fallbackName : name);
    for (int i = 0; i < 4; ++i) out.patches[i] &= 0xFF;
    out.keyDownLayers &= 0xFF;
    out.keyUpLayers &= 0xFF;

    uint32_t layerOffsets[8], wsOffsets[128];
    for (int i = 0; i < 8; ++i) layerOffsets[i] = encoded24(data + 100 + 4 * i);
    for (int i = 0; i < 128; ++i) wsOffsets[i] = encoded24(data + 132 + 4 * i);

    for (int i = 0; i < 8; ++i) {
        uint32_t off = layerOffsets[i];
        if (off == 0) break;
        if (off + 4 > size) { err = "layer offset out of range"; return false; }
        uint32_t len = epsLong(data + off);
        if (len == 0) break;
        if (len < 4 || off + len > size) { err = "layer length out of range"; return false; }
        Layer l;
        if (!parseLayer(data + off + 4, len - 4, l, err)) return false;
        l.fileOffset = off;
        out.layers.push_back(l);
    }
    for (int i = 0; i < 128; ++i) {
        uint32_t off = wsOffsets[i];
        if (off == 0) continue;
        if (off + 4 > size) { err = "wavesample offset out of range"; return false; }
        uint32_t len = epsLong(data + off);
        if (len == 0) break;
        if (len < 4 || off + len > size) {
            err = "wavesample " + std::to_string(i) + " length out of range";
            return false;
        }
        WaveSample ws;
        if (!parseWaveSample(i, data + off + 4, len - 4, ws, err)) return false;
        ws.fileOffset = off;
        if (!ws.pcm.empty() && (off % 2) == 0) ws.imageWord = (off + kWaveSampleHeader) / 2;
        out.waveSamples[i] = std::move(ws);
    }
    // The whole instrument as the EPS holds it in sample memory: headers, layers
    // and PCM in file order, 16-bit words (measured on the original OS).
    out.image.resize(size / 2);
    for (size_t i = 0; i < out.image.size(); ++i) out.image[i] = int16_t(be16(data + 2 * i));
    return true;
}

bool encodeInstrument(const Instrument &inst, std::vector<uint8_t> &out, std::string &err) {
    if (inst.image.size() * 2 < 644) { err = "no file data to write"; return false; }
    out.resize(inst.image.size() * 2);
    for (size_t i = 0; i < inst.image.size(); ++i) {
        out[2 * i] = uint8_t(uint16_t(inst.image[i]) >> 8);
        out[2 * i + 1] = uint8_t(inst.image[i]);
    }
    // The codec takes references; writing reads them and changes nothing, so the
    // instrument (with its PCM) is not copied.
    Instrument &in = const_cast<Instrument &>(inst);
    std::string name = in.name;
    Codec h(out.data(), out.size());
    if (!codeHeader(h, in, name)) { err = "header"; return false; }
    for (Layer &l : in.layers) {
        if (!l.fileOffset || l.fileOffset + 4 > out.size()) { err = "layer offset"; return false; }
        const uint32_t len = epsLong(out.data() + l.fileOffset);
        Codec c(out.data() + l.fileOffset + 4, len - 4);
        if (len < 4 || !codeLayer(c, l)) { err = "layer " + std::to_string(l.index); return false; }
    }
    for (auto &kv : in.waveSamples) {
        WaveSample &w = kv.second;
        if (!w.fileOffset || w.fileOffset + 4 > out.size()) { err = "wavesample offset"; return false; }
        const uint32_t len = epsLong(out.data() + w.fileOffset);
        Codec c(out.data() + w.fileOffset + 4, len - 4);
        if (len < 4 || !codeWaveSample(c, w)) { err = "wavesample " + std::to_string(kv.first); return false; }
    }
    return true;
}

// ---------------------------------------------------------------- Disk

Disk::~Disk() { closeFile(); }

void Disk::closeFile() {
    if (file_) fclose(file_);
    file_ = nullptr;
}

bool Disk::open(const std::string &path, std::string &err) {
    closeFile();
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) { err = "cannot open " + path; return false; }
    // Peek at the first blocks: a raw Ensoniq image/device has its device-ID
    // block at 512 ('ID' at byte 550) and the root directory end ('DR') at 2558.
    uint8_t head[2560];
    size_t got = fread(head, 1, sizeof head, f);
    if (got == sizeof head && head[550] == 'I' && head[551] == 'D' && head[2558] == 'D' &&
        head[2559] == 'R') {
        data_.clear();
        skipTable_.clear();
        files_.clear();
        label_.clear();
        osVersion_.clear();
        blockSize_ = kBlock;
        format_ = ImageFormat::Raw;
        file_ = f;
        if (parseRaw(err)) return true;
        closeFile();
        return false;
    }
    fclose(f);
    std::vector<uint8_t> d;
    if (!readWholeFile(path, d, err)) return false;
    return openMemory(std::move(d), path, err);
}

bool Disk::openMemory(std::vector<uint8_t> d, const std::string &nameHint, std::string &err) {
    closeFile();
    data_ = std::move(d);
    skipTable_.clear();
    files_.clear();
    label_.clear();
    osVersion_.clear();
    blockSize_ = kBlock;

    if (lower(nameHint).size() >= 4 && lower(nameHint).substr(lower(nameHint).size() - 4) == ".hfe") {
        err = "HFE is not supported yet; convert to IMG first (e.g. HxC tool or EpsLin)";
        return false;
    }
    const uint8_t *p = data_.data();
    const size_t n = data_.size();

    if (n >= 8 && memcmp(p, "TDDF", 4) == 0) {
        format_ = ImageFormat::Gkh;
        if (p[4] != 'I' || p[5] != 1) { err = "unsupported GKH variant"; return false; }
        unsigned tags = le16(p + 6);
        for (unsigned i = 0; i < tags; ++i) {
            size_t base = 8 + 10 * i;
            if (base + 10 > n) break;
            if (p[base] == 0x0B) {
                uint32_t len = le32(p + base + 2), off = le32(p + base + 6);
                if (size_t(off) + len > n) { err = "GKH image range invalid"; return false; }
                data_ = std::vector<uint8_t>(p + off, p + off + len);
                return parseRaw(err);
            }
        }
        err = "GKH file has no image tag";
        return false;
    }

    if (n >= 512 && p[0] == 0x0D && p[1] == 0x0A) {
        if (p[47] == 0x0D && p[48] == 0x0A && p[49] == 0x1A) {
            format_ = ImageFormat::Efe;
            // Header text: "Eps File:" + pad, name at 0x12, type at 0x22.
            std::string instName = trimRight(std::string(reinterpret_cast<const char *>(p + 0x12), 12));
            std::string typeText = trimRight(std::string(reinterpret_cast<const char *>(p + 0x22), 13));
            unsigned blocks = be16(p + 0x34);
            if (512 + size_t(blocks) * 512 > n) { err = "EFE truncated"; return false; }
            DirEntry e;
            e.path = "00";
            e.name = printable(instName);
            e.type = typeText.rfind("Instr", 0) == 0 ? kInstrument : kUnused;
            e.sizeBlocks = e.contiguousBlocks = blocks;
            e.firstBlock = 0;
            data_ = std::vector<uint8_t>(p + 512, p + 512 + size_t(blocks) * 512);
            if (e.type != kInstrument) {
                err = "EFE contains '" + printable(typeText) + "', not an instrument";
                return false;
            }
            files_.push_back(e);
            return true;
        }
        bool ede = p[78] == 0x0D && p[79] == 0x0A && p[157] == 0x0D && p[158] == 0x0A && p[159] == 0x1A;
        bool eda = p[78] == 0x0D && p[79] == 0x0A && p[93] == 0x0D && p[94] == 0x0A && p[95] == 0x1A;
        if (ede || eda) {
            format_ = ede ? ImageFormat::Ede : ImageFormat::Eda;
            size_t tableOff = ede ? 160 : 96, tableLen = ede ? 200 : 400;
            if (p[509] != 0) { err = "compressed EDE/EDA is not supported"; return false; }
            unsigned diskType = be16(p + 510);
            if ((ede && diskType != 3) || (eda && diskType != 203)) {
                err = "unexpected EDE/EDA disk type " + std::to_string(diskType);
                return false;
            }
            skipTable_.assign(p + tableOff, p + tableOff + tableLen);
            data_ = std::vector<uint8_t>(p + 512, p + n);
            return parseRaw(err);
        }
    }

    if (n >= 2560 && p[550] == 'I' && p[551] == 'D' && p[2558] == 'D' && p[2559] == 'R') {
        format_ = ImageFormat::Raw;
        return parseRaw(err);
    }
    err = "unknown image format";
    return false;
}

bool Disk::isSkipped(uint32_t b) const {
    if (skipTable_.empty() || b / 8 >= skipTable_.size()) return false;
    return (skipTable_[b / 8] >> (7 - b % 8)) & 1;
}

uint32_t Disk::skippedBefore(uint32_t b) const {
    uint32_t k = 0;
    for (uint32_t i = 0; i < b; ++i) k += isSkipped(i);
    return k;
}

bool Disk::readBlocks(uint32_t index, uint32_t count, std::vector<uint8_t> &out) const {
    for (uint32_t i = index; i < index + count; ++i) {
        if (isSkipped(i)) {
            // Empty block filler as written by Ensoniq formatting.
            for (uint32_t j = 0; j < blockSize_; j += 2) { out.push_back(0x6D); out.push_back(0xB6); }
            continue;
        }
        if (file_) {
            size_t old = out.size();
            out.resize(old + blockSize_);
            if (fseeko(file_, off_t(i) * blockSize_, SEEK_SET) != 0 ||
                fread(out.data() + old, 1, blockSize_, file_) != blockSize_) {
                out.resize(old);
                return false;
            }
            continue;
        }
        size_t start = size_t(i - skippedBefore(i)) * blockSize_;
        if (start + blockSize_ > data_.size()) return false;
        out.insert(out.end(), data_.begin() + start, data_.begin() + start + blockSize_);
    }
    return true;
}

uint32_t Disk::fatNext(uint32_t block) const {
    std::vector<uint8_t> fat;
    if (!readBlocks(5 + block / 170, 1, fat)) return 2;
    return be24(fat.data() + (block % 170) * 3);
}

bool Disk::parseRaw(std::string &err) {
    std::vector<uint8_t> b;
    if (!readBlocks(1, 2, b)) { err = "image too small"; return false; }
    const uint8_t *id = b.data();
    if (id[38] != 'I' || id[39] != 'D') { err = "missing device ID block"; return false; }
    uint32_t bpb = be32(id + 10);
    if (bpb != 512) {
        // CD-ROM images may use other block sizes; the rest of the parser
        // assumes the block size given here.
        if (bpb == 0 || bpb > 4096) { err = "implausible block size"; return false; }
        blockSize_ = bpb;
    }
    std::string raw(reinterpret_cast<const char *>(id + 30), 8);
    for (unsigned char ch : raw) if (ch >= 32 && ch < 128) label_ += char(ch);
    const uint8_t *os = b.data() + blockSize_;
    if (os[28] != 'O' || os[29] != 'S') {
        err = "missing OS block";
        return false;
    }
    if (os[4] || os[5]) osVersion_ = std::to_string(os[4]) + "." + std::to_string(os[5]);
    return parseDir(3, 2, 2, "", 0, err);
}

bool Disk::readFile(const DirEntry &e, std::vector<uint8_t> &out, std::string &err) const {
    out.clear();
    if (format_ == ImageFormat::Efe) {
        out = data_;
        return true;
    }
    uint32_t contiguous = e.contiguousBlocks ? e.contiguousBlocks : 1;
    if (contiguous > e.sizeBlocks) contiguous = e.sizeBlocks;
    if (!readBlocks(e.firstBlock, contiguous, out)) { err = "file blocks outside image"; return false; }
    uint32_t last = e.firstBlock + contiguous - 1;
    uint32_t read = contiguous;
    while (read < e.sizeBlocks) {
        uint32_t next = fatNext(last);
        if (next == 0 || next == 1) break;
        if (next == 2) { err = "bad block in FAT chain"; return false; }
        if (!readBlocks(next, 1, out)) { err = "FAT points outside image"; return false; }
        last = next;
        ++read;
    }
    if (read < e.sizeBlocks) { err = "FAT chain shorter than file size"; return false; }
    return true;
}

bool Disk::parseDir(uint32_t first, uint32_t blocks, uint32_t contiguous, const std::string &prefix,
                    int depth, std::string &err) {
    if (depth > 16) { err = "directory nesting too deep"; return false; }
    DirEntry self;
    self.firstBlock = first;
    self.sizeBlocks = blocks;
    self.contiguousBlocks = contiguous;
    std::vector<uint8_t> d;
    if (!readFile(self, d, err)) return false;
    if (d.size() < 2 || d[d.size() - 2] != 'D' || d[d.size() - 1] != 'R') {
        err = "directory at block " + std::to_string(first) + " lacks DR marker";
        return false;
    }
    for (size_t i = 0; i < d.size() / kDirEntrySize; ++i) {
        const uint8_t *e = d.data() + i * kDirEntrySize;
        int type = e[1];
        if (type == kUnused) continue;
        DirEntry de;
        char num[24];
        snprintf(num, sizeof num, "%02zu", i);
        de.path = prefix.empty() ? num : prefix + " " + num;
        de.name = printable(trimRight(std::string(reinterpret_cast<const char *>(e + 2), 12)));
        de.type = type;
        de.sizeBlocks = be16(e + 14);
        de.contiguousBlocks = be16(e + 16);
        de.firstBlock = be32(e + 18);
        files_.push_back(de);
        // A directory is always 2 contiguous blocks; the size field of its entry
        // counts the files in it (the original OS writes 0 for a new directory).
        if (type == kSubdir && !parseDir(de.firstBlock, 2, 2, de.path, depth + 1, err))
            return false;
    }
    return true;
}

std::vector<const DirEntry *> Disk::instruments() const {
    std::vector<const DirEntry *> r;
    for (auto &f : files_) if (f.type == kInstrument) r.push_back(&f);
    return r;
}

bool Disk::loadInstrument(const DirEntry &e, Instrument &out, std::string &err) const {
    if (e.type != kInstrument) { err = e.name + " is not an instrument"; return false; }
    std::vector<uint8_t> raw;
    if (!readFile(e, raw, err)) return false;
    return parseInstrument(raw.data(), raw.size(), e.name, out, err);
}

// ---------------------------------------------------------------- WAV

bool readWav(const std::string &path, std::vector<int16_t> &pcm, int &rate, std::string &err) {
    std::vector<uint8_t> d;
    if (!readWholeFile(path, d, err)) return false;
    auto le16 = [&d](size_t o) { return uint32_t(d[o] | d[o + 1] << 8); };
    auto le32 = [&](size_t o) { return le16(o) | le16(o + 2) << 16; };
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) {
        err = path + ": not a WAV file";
        return false;
    }
    int channels = 0, bits = 0;
    for (size_t o = 12; o + 8 <= d.size();) {
        const uint32_t len = le32(o + 4);
        const size_t body = o + 8;
        if (!std::memcmp(d.data() + o, "fmt ", 4) && body + 16 <= d.size()) {
            if (le16(body) != 1 && le16(body) != 0xFFFE) { err = path + ": not PCM"; return false; }
            channels = int(le16(body + 2));
            rate = int(le32(body + 4));
            bits = int(le16(body + 14));
        } else if (!std::memcmp(d.data() + o, "data", 4)) {
            if (channels < 1 || (bits != 16 && bits != 24)) { err = path + ": 16 or 24 bit PCM only"; return false; }
            const size_t frame = size_t(channels) * size_t(bits / 8);
            const size_t n = std::min<size_t>(len, d.size() - body) / frame;
            pcm.resize(n);
            for (size_t i = 0; i < n; ++i) {
                const uint8_t *p = d.data() + body + i * frame;
                pcm[i] = bits == 16 ? int16_t(p[0] | p[1] << 8) : int16_t(p[1] | p[2] << 8);
            }
            return true;
        }
        o = body + len + (len & 1);
    }
    err = path + ": no data";
    return false;
}

bool writeWav(const std::string &path, const WaveSample &ws, const std::vector<int16_t> &pcm,
              std::string &err) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) { err = "cannot write " + path; return false; }
    auto w32 = [&](uint32_t v) { uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)}; fwrite(b, 1, 4, f); };
    auto w16 = [&](uint16_t v) { uint8_t b[2] = {uint8_t(v), uint8_t(v >> 8)}; fwrite(b, 1, 2, f); };
    const bool loop = ws.loopMode != 0 && ws.loopEnd > ws.loopStart;
    const uint32_t dataBytes = uint32_t(pcm.size() * 2);
    const uint32_t smplBytes = loop ? 36 + 24 : 0;
    fwrite("RIFF", 1, 4, f);
    w32(4 + 24 + 8 + dataBytes + (loop ? 8 + smplBytes : 0));
    fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(1); w32(uint32_t(ws.sampleRate)); w32(uint32_t(ws.sampleRate) * 2); w16(2); w16(16);
    fwrite("data", 1, 4, f);
    w32(dataBytes);
    for (int16_t s : pcm) w16(uint16_t(s));
    if (loop) {
        fwrite("smpl", 1, 4, f);
        w32(smplBytes);
        w32(0); w32(0); w32(uint32_t(1e9 / ws.sampleRate)); w32(uint32_t(ws.rootNote)); w32(0);
        w32(0); w32(0); w32(1); w32(0);
        w32(0); w32(ws.loopMode == 3 ? 1 : ws.loopMode == 1 ? 2 : 0);
        w32(ws.loopStart); w32(ws.loopEnd); w32(0); w32(0);
    }
    bool ok = !ferror(f);
    fclose(f);
    if (!ok) err = "write error on " + path;
    return ok;
}

int sampleRateOfIndex(int index) { return sampleRateFromIndex(index); }

// ---- building instrument files ----------------------------------------------------

namespace {

constexpr uint32_t kHeaderBytes = 656, kLayerBlock = 224;

void setEnvelope(Envelope &e, std::initializer_list<int> soft, std::initializer_list<int> hard,
                 std::initializer_list<int> times, int level5) {
    int i = 0;
    for (int v : soft) e.softLevel[i++] = v;
    i = 0;
    for (int v : hard) e.hardLevel[i++] = v;
    i = 1;
    for (int v : times) e.time[i++] = v;
    e.level5 = level5;
}

} // namespace

void newInstrument(Instrument &in) {
    in = Instrument();
    in.name = "UNNAMED INST";
    in.instrumentId = kIdEps16Plus;
    in.midiOutChannel = 0;
    in.midiProgram = 1;
    in.pressureMode = 1;
    in.sizeBlocks = 0x100;
    in.keyDestination = 0;
    in.patches[0] = 7; in.patches[1] = 1; in.patches[2] = 2; in.patches[3] = 3;
    in.keyDownLayers = 0xFF;
    in.keyUpLayers = 0;
    in.patchSelectMode = 0;
    in.keyLow = 36;
    in.keyHigh = 96;
    in.transpose = 0;
}

Layer newLayer(int index) {
    Layer l;
    l.index = index;
    l.name = "UNNAMEDLAYER";
    l.velocityLow = 0;
    l.velocityHigh = 127;
    return l;
}

WaveSample newWaveSample(int index) {
    WaveSample w;
    w.index = index;
    w.name = "UNNAMED WS";
    w.rootNote = 60;
    w.lfoPitchAmount = 8;
    w.pitchBendRange = 13;
    w.pitchModSource = 3;
    w.filterMode = 3;
    w.filterCutoff[0] = w.filterCutoff[1] = 127;
    w.filterKeyAmount[0] = w.filterKeyAmount[1] = 10;
    w.filterModSource[0] = w.filterModSource[1] = 13;
    w.volume = 127;
    w.ampModSource = 13;
    w.crossfade[0] = 0; w.crossfade[1] = w.crossfade[2] = w.crossfade[3] = 127;
    w.panEps = 3;
    w.lfoSpeed = 40;
    w.lfoDelay = 40;
    w.lfoDepthModAmount = 127;
    w.lfoDepthModSource = 10;
    w.lfoRateModSource = 13;
    w.randomFreq = 20;
    w.sampleRateIndex = 14;
    w.sampleRate = sampleRateFromIndex(14);
    w.keyLow = 21;
    w.keyHigh = 108;
    w.waveModRaw[0] = 13;
    w.waveModRaw[2] = 6;
    setEnvelope(w.pitchEnv, {0, 100, 90, 80, 64}, {0, 127, 100, 85, 80}, {10, 64, 52, 40, 30, 0}, -128);
    setEnvelope(w.filterEnv, {64, 64, 60, 55, 50}, {127, 127, 100, 85, 80}, {10, 64, 52, 40, 30, 0}, -127);
    setEnvelope(w.ampEnv, {64, 64, 64, 64, 64}, {127, 127, 127, 127, 127}, {0, 64, 52, 40, 20, 0}, -127);
    return w;
}

bool buildInstrument(const Instrument &inst, std::vector<uint8_t> &out, std::string &err) {
    Instrument &in = const_cast<Instrument &>(inst);   // the codec reads the fields only
    // The file the instrument came from, as bytes (empty for a new instrument).
    std::vector<uint8_t> old(in.image.size() * 2);
    for (size_t i = 0; i < in.image.size(); ++i) {
        old[2 * i] = uint8_t(uint16_t(in.image[i]) >> 8);
        old[2 * i + 1] = uint8_t(in.image[i]);
    }
    const bool haveOld = old.size() >= kHeaderBytes;
    // A block of the old file: offset (from the header), its length; 0 = none.
    auto oldBlock = [&](uint32_t off, uint32_t &len) {
        len = 0;
        if (!haveOld || off < kHeaderBytes || off + 4 > old.size()) return false;
        len = epsLong(old.data() + off);
        if (len < 4 || off + len > old.size()) { len = 0; return false; }
        return true;
    };

    out.assign(kHeaderBytes, 0);
    if (haveOld) {
        std::memcpy(out.data(), old.data(), kHeaderBytes);
    } else {
        // As the OS writes a new instrument (the hardware address is rebuilt on load).
        static const uint8_t addr[4] = {0xC9, 0x00, 0xFF, 0xD0};
        std::memcpy(out.data() + 4, addr, 4);
        out[650] = 0x10;
    }
    std::memset(out.data() + 100, 0, 548);              // offset tables and effect
    std::string name = in.name;
    {
        Codec h(out.data(), out.size());
        if (!codeHeader(h, in, name)) { err = "header"; return false; }
    }
    // Every block starts on 16 bytes: the OS ignores the low four bits of the offsets
    // (measured: a layer at an odd offset stays silent), as its own files show.
    auto append = [&out](const uint8_t *p, size_t n) {
        const size_t at = (out.size() + 15) & ~size_t(15);
        out.resize(at + n, 0);
        if (p) std::memcpy(out.data() + at, p, n);
        return uint32_t(at);
    };

    // The effect: copied as it is.
    uint32_t len = 0;
    if (haveOld && oldBlock(encoded24(old.data() + 644), len))
        putEncoded24(out.data() + 644, append(old.data() + encoded24(old.data() + 644), len));

    std::vector<bool> written(128, false);
    auto writeWs = [&](int idx) {
        auto it = in.waveSamples.find(idx);
        if (it == in.waveSamples.end() || written[size_t(idx)]) return true;
        written[size_t(idx)] = true;
        WaveSample &w = it->second;
        const size_t n = w.copyOfWaveSample ? 0 : w.pcm.size();
        uint32_t ol = 0;
        const bool had = w.fileOffset && oldBlock(w.fileOffset, ol) && ol >= kWaveSampleHeader;
        const uint32_t at = append(had ? old.data() + w.fileOffset : nullptr, kWaveSampleHeader);
        out.resize(at + kWaveSampleHeader + 2 * n, 0);
        putEpsLong(out.data() + at, uint32_t(kWaveSampleHeader + 2 * n));
        Codec c(out.data() + at + 4, kWaveSampleHeader - 4);
        if (!codeWaveSample(c, w)) { err = "wavesample " + std::to_string(idx); return false; }
        uint8_t *p = out.data() + at + kWaveSampleHeader;
        for (size_t i = 0; i < n; ++i) { p[2 * i] = uint8_t(uint16_t(w.pcm[i]) >> 8); p[2 * i + 1] = uint8_t(w.pcm[i]); }
        putEncoded24(out.data() + 132 + 4 * idx, at);
        return true;
    };
    // Each layer, then its wavesamples (key order), as the OS lays them out.
    for (size_t li = 0; li < in.layers.size() && li < 8; ++li) {
        Layer &l = in.layers[li];
        uint32_t ol = 0;
        const bool had = l.fileOffset && oldBlock(l.fileOffset, ol) && ol == kLayerBlock;
        const uint32_t at = append(had ? old.data() + l.fileOffset : nullptr, kLayerBlock);
        putEpsLong(out.data() + at, kLayerBlock);
        Codec c(out.data() + at + 4, kLayerBlock - 4);
        if (!codeLayer(c, l)) { err = "layer " + std::to_string(li + 1); return false; }
        putEncoded24(out.data() + 100 + 4 * li, at);
        for (uint8_t k : l.keyMap)
            if (k && !writeWs(k)) return false;
    }
    for (auto &kv : in.waveSamples)
        if (!writeWs(kv.first)) return false;
    // Pitch tables: copied as they are.
    for (int i = 0; i < 8 && haveOld; ++i) {
        const uint32_t o = encoded24(old.data() + 612 + 4 * i);
        if (o && oldBlock(o, len)) putEncoded24(out.data() + 612 + 4 * i, append(old.data() + o, len));
    }
    out.resize((out.size() + kBlock - 1) / kBlock * kBlock, 0);
    putEpsLong(out.data(), uint32_t(out.size()));
    return true;
}

} // namespace eps
