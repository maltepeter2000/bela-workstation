// eps_loader — reads Ensoniq EPS / EPS-16 PLUS / ASR-10 disk images and
// instrument files (EFE, IMG/raw, GKH, EDE, EDA) into plain C++ structs.
//
// Format knowledge follows ConvertWithMoss (Jürgen Moßgraber, LGPLv3),
// src/main/java/de/mossgrabers/convertwithmoss/format/ensoniq/epsasr/*.
// This is an independent C++ re-implementation, but because it mirrors that
// code's structure closely, treat it as LGPLv3-derived.
//
// Designed for loading outside the audio thread (e.g. Bela AuxiliaryTask or a
// Pd "open" message). No exceptions; every call reports errors via a string.

#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace eps {

enum FileType : int {
    kUnused = 0, kOs = 1, kSubdir = 2, kInstrument = 3, kEpsBank = 4,
    kEpsSeq = 5, kEpsSong = 6, kEpsSysex = 7, kParentPtr = 8, kEpsMacro = 9,
    kPlusBank = 23, kPlusFx = 24, kPlusSeq = 25, kPlusSong = 26, kPlusOs = 27,
    kAsrSeq = 28, kAsrSong = 29, kAsrBank = 30, kAsrTrack = 31, kAsrOs = 32,
    kAsrFx = 33, kAsrMacro = 34
};

const char *fileTypeName(int type);

enum class ImageFormat { Raw, Gkh, Efe, Ede, Eda };
const char *imageFormatName(ImageFormat f);

// Instrument ID stored in every instrument file.
enum : int { kIdEps = 0x0000, kIdEps16Plus = 0xFFFF, kIdAsr10 = 0xFFFE };

struct Envelope {
    // Levels 0..4 (0..127) exist in a soft (low velocity) and hard (high
    // velocity) version; times 1..6 are 0..99 panel values.
    int type = 0;
    int softLevel[5] = {};
    int hardLevel[5] = {};
    int time[7] = {};          // index 1..6 used, [0] unused
    int velocitySwitch = 0;
    int level5 = 0;            // release level (signed)
    int time1VelSens = 0;
    int kbTimeScaling = 0;
    int mode = 0;
};

struct WaveSample {
    int index = 0;             // 1..127 slot within the instrument
    std::string name;
    int copyOfWaveSample = 0;  // != 0: PCM data lives in that wavesample
    int copyOfLayer = 0;

    Envelope pitchEnv, filterEnv, ampEnv;

    int rootNote = 60;         // MIDI note
    int volumeModCrossfadeCurve = 0;
    int pitchEnvAmount = 0;    // 1/8 semitones (signed)
    int lfoPitchAmount = 0;    // 1/8 semitones (signed)
    int randomModAmount = 0;
    int pitchBendRange = 0;    // semitones; >12 = use global
    int pitchModSource = 0;
    int fineTune = 0;          // signed, 1/128 semitone (verified on CLASSIC PAD)
    int pitchModAmount = 0;

    int filterMode = 0;        // 0..3 (ES5505 filter configuration)
    int filterCutoff[2] = {};  // 0..150
    int filterKeyAmount[2] = {};
    int filterEnvAmount[2] = {};
    int filterModSource[2] = {};
    int filterModAmount[2] = {};

    int volume = 0;
    int outputBus = 0;
    int ampModSource = 0;
    int panModSource = 0;
    int crossfade[4] = {};     // A, B, C, D
    int panEps = 0;            // EPS: 1..8 (0 = unset)
    int panSigned = 0;         // EPS-16+/ASR: -127..127
    int ampModAmount = 0;
    int panModAmount = 0;

    int lfoWaveform = 0;       // 0 tri, 1 sine/tri, 2 sine, 3 +tri, 4 +sine, 5 saw, 6 square
    int boost = 0;
    int lfoSpeed = 0;
    int lfoRateModAmount = 0;
    int lfoDepth = 0;
    int lfoDelay = 0;
    int lfoDepthModAmount = 0;
    int lfoDepthModSource = 0;
    int lfoRateModSource = 0;
    int lfoMode = 0;           // 0 normal, 1 reset, 2 humanize (per manual)
    int randomFreq = 0;

    int loopMode = 0;          // 0 forward-no-loop, 1 backward, 2 loop, 3 bidirectional, 4 loop-until-release, ...
    // Positions in samples, relative to the start of this wavesample's PCM
    // (or of the referenced wavesample when copyOfWaveSample != 0).
    uint32_t sampleStart = 0, sampleEnd = 0, loopStart = 0, loopEnd = 0;
    // Raw 32-bit address words (ES5505-style, 9 fractional bits), kept for
    // exact register programming later.
    uint32_t rawStart = 0, rawEnd = 0, rawLoopStart = 0, rawLoopEnd = 0;

    // Wave modulation (EDIT/Wave page), the 5 words after the key range:
    // [0] source, [1] amount (raw byte, signed), [2] range, [3] type, [4] no effect
    // seen (measured on the original OS, see below).
    int waveModRaw[5] = {};
    int waveModSource() const { return waveModRaw[0]; }
    int waveModAmount() const { return int(int8_t(waveModRaw[1])); }
    int waveModAmountDisplay() const { return (waveModAmount() * 99 + (waveModAmount() >= 0 ? 63 : -63)) / 127; }
    // Measured on the original OS: word 3 is the type, word 2 the range
    // (2^(21 - n) samples: 6 = 32K). Types as stored: 0 off, 1 loop position,
    // 2 sample start, 3 start + loop position, 4 loop start, 5 loop end,
    // 6 loop start-X, 7 transwave. waveModType() returns the engine's numbering
    // (0 loop position .. 6 transwave, -1 off).
    int waveModTypeRaw() const { return waveModRaw[3]; }
    int waveModType() const {
        static const int kMap[8] = {-1, 0, 1, 2, 3, 4, 5, 6};
        return (waveModRaw[3] >= 0 && waveModRaw[3] < 8) ? kMap[waveModRaw[3]] : -1;
    }
    uint32_t waveModRangeSamples() const { return 1u << (21 - (waveModRaw[2] > 21 ? 21 : waveModRaw[2])); }

    int sampleRateIndex = 0;
    int sampleRate = 0;        // Hz, derived from the index
    int keyLow = 0, keyHigh = 127;

    std::vector<int16_t> pcm;  // mono 16-bit
    uint32_t imageWord = 0;    // where the PCM starts in Instrument::image (0 = not there)
    uint32_t fileOffset = 0;   // of its block in the instrument file (for writing)
};

struct Layer {
    int index = 0;             // 1-based
    uint32_t fileOffset = 0;   // of its block in the instrument file (for writing)
    std::string name;
    int glideMode = 0, delayModByVelocity = 0, glideTime = 0;
    int restrikeDecay = 0, legatoLayer = 0, stereoLink = 0;
    int velocityLow = 0, velocityHigh = 127;
    int pitchTable = 0, delayTime = 0;
    // Wavesample number (1-based, 0 = none) for each of 88 keys starting at
    // MIDI note 21 (A0).
    uint8_t keyMap[88] = {};
    static constexpr int kFirstNote = 21;
    int waveSampleForNote(int midiNote) const {
        int k = midiNote - kFirstNote;
        return (k >= 0 && k < 88) ? keyMap[k] : 0;
    }
};

struct Instrument {
    std::string name;
    int instrumentId = 0;      // see kIdEps*
    int midiOutChannel = 0, midiProgram = 0, pressureMode = 0;
    int sizeBlocks = 0, keyDestination = 0;
    int patches[4] = {};       // layer bitmasks for patch select 00, 0*, *0, **
    int keyDownLayers = 0, keyUpLayers = 0;
    int patchSelectMode = 0;
    int keyLow = 0, keyHigh = 127, transpose = 0;
    std::vector<Layer> layers;
    std::map<int, WaveSample> waveSamples;   // by slot number
    // The instrument data as it sits in the EPS sample memory (16-bit words):
    // the PCM of every wavesample is preceded by its header and the instrument's
    // other data. Loop start-X reads into it, as on the original.
    std::vector<int16_t> image;

    // Resolve copy references: returns the wavesample that owns the PCM.
    const WaveSample *pcmOwner(const WaveSample &ws) const;
    const char *modelName() const;
};

struct DirEntry {
    std::string path;          // e.g. "03 07" (file numbers)
    std::string name;
    int type = 0;
    uint32_t sizeBlocks = 0, contiguousBlocks = 0, firstBlock = 0;
};

class Disk {
public:
    Disk() = default;
    ~Disk();
    Disk(const Disk &) = delete;
    Disk &operator=(const Disk &) = delete;

    // Opens an image file or a raw device (e.g. /dev/sdb, /dev/sr0, a
    // ZuluSCSI/BlueSCSI image on an SD card). Raw Ensoniq images and devices
    // are read block by block on demand, so multi-GB hard disks work without
    // loading them into RAM. Container formats (EFE/EDE/EDA/GKH) are loaded
    // into memory.
    bool open(const std::string &path, std::string &err);
    bool isStreaming() const { return file_ != nullptr; }
    bool openMemory(std::vector<uint8_t> data, const std::string &nameHint, std::string &err);

    ImageFormat format() const { return format_; }
    const std::string &label() const { return label_; }
    const std::string &osVersion() const { return osVersion_; }
    const std::vector<DirEntry> &files() const { return files_; }
    std::vector<const DirEntry *> instruments() const;

    bool readFile(const DirEntry &e, std::vector<uint8_t> &out, std::string &err) const;
    bool loadInstrument(const DirEntry &e, Instrument &out, std::string &err) const;

private:
    bool parseRaw(std::string &err);
    bool parseDir(uint32_t firstBlock, uint32_t blocks, uint32_t contiguous,
                  const std::string &prefix, int depth, std::string &err);
    bool readBlocks(uint32_t index, uint32_t count, std::vector<uint8_t> &out) const;
    uint32_t fatNext(uint32_t block) const;
    bool isSkipped(uint32_t block) const;
    uint32_t skippedBefore(uint32_t block) const;

    void closeFile();
    std::vector<uint8_t> data_;
    FILE *file_ = nullptr;             // streaming mode for raw images/devices
    std::vector<uint8_t> skipTable_;   // EDE/EDA only
    ImageFormat format_ = ImageFormat::Raw;
    uint32_t blockSize_ = 512;
    std::string label_, osVersion_;
    std::vector<DirEntry> files_;
};

// Parse an instrument file's raw bytes (as stored on disk, without EFE header).
bool parseInstrument(const uint8_t *data, size_t size, const std::string &fallbackName,
                     Instrument &out, std::string &err);

// The instrument file again (without EFE header): the bytes it was loaded from
// (Instrument::image) with every parsed field written back, so changes made to the
// structs land in the file and everything else stays as it was.
bool encodeInstrument(const Instrument &in, std::vector<uint8_t> &out, std::string &err);

// ---- Building instrument files from the structs (the wave recorder) ----------
// What the original OS creates when it samples into an empty instrument slot
// (measured on OS 1.30: UNNAMED INST with layer UNNAMEDLAYER and wavesample
// UNNAMED WS, 44.6 kHz). The new layer maps no key, the new wavesample has no
// samples and no key range.
void newInstrument(Instrument &in);
Layer newLayer(int index);                     // 1-based
WaveSample newWaveSample(int index);           // 1..127
// The whole instrument file from the structs: header, effect, layers, wavesamples
// with their samples (WaveSample::pcm of every wavesample that is not a copy) and
// pitch tables. Blocks the instrument was loaded with start from their old bytes
// (what eps~ does not know stays), new ones from zero. For new or resampled
// wavesamples; parseInstrument() the result to play it.
bool buildInstrument(const Instrument &in, std::vector<uint8_t> &out, std::string &err);
// The EPS sample rates of a recording (Hz, index as stored): 625 kHz / index.
constexpr int kRecordRateIndex[7] = {14, 21, 28, 35, 42, 49, 56};
int sampleRateOfIndex(int index);

// Helpers for checking results.
// A WAV file (PCM 16 or 24 bit, the first channel) as 16-bit samples.
bool readWav(const std::string &path, std::vector<int16_t> &pcm, int &rate, std::string &err);
bool writeWav(const std::string &path, const WaveSample &ws, const std::vector<int16_t> &pcm,
              std::string &err);

} // namespace eps
