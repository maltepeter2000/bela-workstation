// eps_loader.cpp — see eps_loader.h for scope and licensing notes.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "eps_loader.h"

#include <cstdio>
#include <cstring>
#include <fstream>

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

// Offset fields in the instrument header (nybble-packed 24-bit values).
uint32_t encoded24(const uint8_t *p) {
    uint32_t b0 = p[0], b2 = p[2], b3 = p[3];
    return (b3 >> 4 & 0x0F) << 20 | b0 << 12 | b2 << 4 | (b3 & 0x0F);
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

// Bounds-checked sequential reader mirroring ConvertWithMoss' StreamUtils.
class Cursor {
public:
    Cursor(const uint8_t *d, size_t n) : d_(d), n_(n) {}
    bool ok() const { return ok_; }
    size_t pos() const { return pos_; }
    void skip(size_t k) { need(k) ? pos_ += k : 0; }
    int u8() { return need(1) ? d_[pos_++] : 0; }
    int s8() { return need(1) ? int(int8_t(d_[pos_++])) : 0; }
    // Parameter stored in the high byte of a 16-bit word.
    int u8w() { int v = u8(); skip(1); return v; }
    int s8w() { int v = s8(); skip(1); return v; }
    int u16() { if (!need(2)) return 0; int v = be16(d_ + pos_); pos_ += 2; return v; }
    // ASCII text with one character per 16-bit word (high byte).
    std::string asciiWords(size_t chars) {
        std::string s;
        for (size_t i = 0; i < chars; ++i) { s += char(u8()); skip(1); }
        return s;
    }
    // 32-bit value assembled from the high bytes of four words.
    uint32_t highBytes32() {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) { v = v << 8 | uint32_t(u8()); skip(1); }
        return v;
    }

private:
    bool need(size_t k) {
        if (pos_ + k > n_) { ok_ = false; pos_ = n_; return false; }
        return true;
    }
    const uint8_t *d_;
    size_t n_, pos_ = 0;
    bool ok_ = true;
};

void readEnvelope(Cursor &c, Envelope &e) {
    e.type = c.u8w();
    e.softLevel[0] = c.u8w();
    e.hardLevel[0] = c.u8w();
    for (int i = 1; i <= 4; ++i) {
        e.time[i] = c.u8w();
        e.softLevel[i] = c.u8w();
        e.hardLevel[i] = c.u8w();
    }
    e.time[5] = c.u8w();
    e.velocitySwitch = c.u8w();
    e.level5 = c.s8w();
    e.time[6] = c.u8w();
    e.time1VelSens = c.u8w();
    e.kbTimeScaling = c.u8w();
    e.mode = c.u8w();
}

int sampleRateFromIndex(int idx) {
    if (idx == 0) return 48000;
    if (idx == 1) return 44100;
    return int(625000.0 / idx + 0.5);
}

// `d`/`n` cover the wavesample block *after* its 4-byte length word.
bool parseWaveSample(int index, const uint8_t *d, size_t n, WaveSample &ws, std::string &err) {
    Cursor c(d, n);
    ws.index = index;
    c.skip(6);
    ws.name = trimRight(c.asciiWords(12));
    ws.copyOfWaveSample = c.u8w();
    ws.copyOfLayer = c.u8w();
    readEnvelope(c, ws.pitchEnv);
    readEnvelope(c, ws.filterEnv);
    readEnvelope(c, ws.ampEnv);
    ws.rootNote = c.u8();
    ws.volumeModCrossfadeCurve = c.u8();
    ws.pitchEnvAmount = c.s8w();
    ws.lfoPitchAmount = c.s8w();
    ws.randomModAmount = c.s8w();
    ws.pitchBendRange = c.u8w();
    ws.pitchModSource = c.u8w();
    ws.fineTune = c.s8w();
    ws.pitchModAmount = c.s8w();
    ws.filterMode = c.u8w();
    ws.filterCutoff[0] = c.u8w();
    ws.filterCutoff[1] = c.u8w();
    ws.filterKeyAmount[0] = c.s8w();
    ws.filterKeyAmount[1] = c.s8w();
    ws.filterEnvAmount[0] = c.s8w();
    ws.filterEnvAmount[1] = c.s8w();
    ws.filterModSource[0] = c.u8w();
    ws.filterModSource[1] = c.u8w();
    ws.filterModAmount[0] = c.s8w();
    ws.filterModAmount[1] = c.s8w();
    ws.volume = c.u8();
    ws.outputBus = c.u8();
    ws.ampModSource = c.u8();
    ws.panModSource = c.u8();
    for (int i = 0; i < 4; ++i) ws.crossfade[i] = c.s8w();
    ws.panEps = c.u8();
    ws.panSigned = c.s8();
    ws.ampModAmount = c.u8();
    ws.panModAmount = c.u8();
    ws.lfoWaveform = c.u8();
    ws.boost = c.u8();
    ws.lfoSpeed = c.u8();
    ws.lfoRateModAmount = c.s8();
    ws.lfoDepth = c.u8w();
    ws.lfoDelay = c.u8();
    ws.lfoDepthModAmount = c.s8();
    ws.lfoDepthModSource = c.u8();
    ws.lfoRateModSource = c.u8();
    ws.lfoMode = c.u8w();
    ws.randomFreq = c.u8w();
    ws.loopMode = c.u8w();
    ws.rawStart = c.highBytes32();
    ws.rawEnd = c.highBytes32();
    ws.rawLoopStart = c.highBytes32();
    ws.rawLoopEnd = c.highBytes32();
    ws.sampleStart = ws.rawStart >> 9;
    ws.sampleEnd = ws.rawEnd >> 9;
    ws.loopStart = ws.rawLoopStart >> 9;
    // As in ConvertWithMoss: loop end carries a 4-bit fine part.
    ws.loopEnd = (ws.rawLoopEnd >> 9) + ((ws.rawLoopEnd >> 5) & 0xF);
    ws.sampleRateIndex = c.u8w();
    ws.sampleRate = sampleRateFromIndex(ws.sampleRateIndex);
    ws.keyLow = c.u8w();
    ws.keyHigh = c.u8w();
    for (int i = 0; i < 5; ++i) ws.waveModRaw[i] = c.u8w();
    if (!c.ok()) { err = "wavesample " + std::to_string(index) + ": header truncated"; return false; }

    const size_t headerAfterLen = kWaveSampleHeader - 4;
    if (n > headerAfterLen) {
        size_t bytes = n - headerAfterLen;
        const uint8_t *p = d + headerAfterLen;
        ws.pcm.resize(bytes / 2);
        for (size_t i = 0; i < ws.pcm.size(); ++i) ws.pcm[i] = int16_t(be16(p + 2 * i));
    }
    return true;
}

bool parseLayer(const uint8_t *d, size_t n, Layer &l, std::string &err) {
    Cursor c(d, n);
    c.skip(2);
    l.index = c.s8w();
    c.skip(2);
    l.name = trimRight(c.asciiWords(12));
    l.glideMode = c.u8();
    l.delayModByVelocity = c.u8();
    l.glideTime = c.u8();
    l.restrikeDecay = c.u8();
    l.legatoLayer = c.u8();
    l.stereoLink = c.u8();
    l.velocityLow = c.u8w();
    l.velocityHigh = c.u8w();
    l.pitchTable = c.u8w();
    l.delayTime = c.u8w();
    for (int i = 0; i < 88; ++i) l.keyMap[i] = uint8_t(c.u8w());
    if (!c.ok()) { err = "layer truncated"; return false; }
    return true;
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
    Cursor c(data, size);
    c.skip(4 + 4 + 2);    // total size, hardware slot address, unknown
    std::string name = trimRight(c.asciiWords(12));
    out.name = printable(name.empty() ? fallbackName : name);
    out.midiOutChannel = c.s8w();
    out.midiProgram = c.s8w();
    out.pressureMode = c.s8w();
    out.sizeBlocks = c.u16();
    out.keyDestination = c.s8w();
    for (int i = 0; i < 4; ++i) out.patches[i] = c.s8w() & 0xFF;
    out.keyDownLayers = c.s8w() & 0xFF;
    out.keyUpLayers = c.s8w() & 0xFF;
    out.patchSelectMode = c.s8w();
    c.skip(2);
    out.instrumentId = c.u16();
    out.keyLow = c.s8w();
    out.keyHigh = c.s8w();
    out.transpose = c.s8w();
    c.skip(32);                                   // pitch table offsets
    if (!c.ok() || c.pos() != 100) { err = "instrument header layout mismatch"; return false; }

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
        if (!ws.pcm.empty() && (off % 2) == 0) ws.imageWord = (off + kWaveSampleHeader) / 2;
        out.waveSamples[i] = std::move(ws);
    }
    // The whole instrument as the EPS holds it in sample memory: headers, layers
    // and PCM in file order, 16-bit words (measured on the original OS).
    out.image.resize(size / 2);
    for (size_t i = 0; i < out.image.size(); ++i) out.image[i] = int16_t(be16(data + 2 * i));
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
        if (type == kSubdir && !parseDir(de.firstBlock, de.sizeBlocks, de.contiguousBlocks, de.path,
                                         depth + 1, err))
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

} // namespace eps
