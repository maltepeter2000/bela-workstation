// eps_sysex.h — talking to a real EPS-16 PLUS over MIDI System Exclusive, after
// the "EPS-16 PLUS External Command Specification" 1.30 (Ensoniq), checked against
// the original OS 1.30 in an emulator.
//
// Packets: F0 0F 03 <base channel-1> <command> <data> F7. Command parameters are
// 12-bit words in two bytes (6 bits each), data blocks 16-bit words in three
// bytes (4, 6, 6 bits). A transfer from the EPS: GET -> RESPONSE ACK (or WAIT:
// answer ACK, it loads an overlay) -> PUT header -> ACK -> data block -> ACK.
//
// Measured: the parameter blocks are the bytes of the instrument file. GET
// INSTRUMENT = file[10, 656), GET LAYER = layer block [10, 224), GET WAVESAMPLE
// PARAMETERS = wavesample block [10, 288), GET WAVESAMPLE DATA = the samples at
// block + 0x120 (end offset exclusive; one past the data = status 0A). The six
// bytes after each block length are rebuilt by the OS when it loads a file
// (zeros give the same register writes), so a file can be put together from
// the blocks alone.
//
// The EPS must have MIDI SYS-EX = ON (EDIT / System•MIDI; the default is OFF).
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eps {

class SysexLink {
public:
    using Sender = std::function<void(const std::vector<uint8_t> &)>;
    explicit SysexLink(Sender send) : send_(std::move(send)) {}

    void setChannel(int ch) { channel_ = ch & 15; }   // EPS base channel - 1
    // Every byte of a System Exclusive message from the EPS (F0 ... F7).
    void receive(uint8_t b, double now);
    // Call often (every 10-50 ms): timeouts and the next step.
    void tick(double now);

    bool busy() const { return state_ != State::Idle; }
    void cancel();

    // Jobs; false if one runs already. inst 0-7 = Instrument•Track 1-8.
    bool fetchInstrument(int inst, double now);
    bool fetchWaveSample(int inst, int layer, int ws, double now);
    // Into the EPS: an instrument file (as encodeInstrument gives it) into an empty
    // instrument slot; samples into an existing wavesample (its length follows).
    bool sendInstrument(int inst, const std::vector<uint8_t> &file, double now);
    bool sendWaveSample(int inst, int layer, int ws, const std::vector<int16_t> &samples, double now);

    enum class Done { None, Instrument, WaveSample, Sent, Error };
    Done takeDone() { Done d = done_; done_ = Done::None; return d; }
    // Results: an instrument file (without EFE header) and its name, or the
    // samples of one wavesample with its rate; error text.
    std::vector<uint8_t> file;
    std::string name, error;
    std::vector<int16_t> pcm;
    int sampleRate = 0;
    // Progress for a display: what runs and 0-100.
    std::string step;
    int percent = 0;

    // The encodings, public for tests.
    static std::vector<uint8_t> packet(int channel, int cmd, const std::vector<int> &words12);
    static std::vector<uint16_t> data16(const std::vector<uint8_t> &pk);

private:
    enum class State { Idle, Sent, Data, Quiet, ProbeWait, PutData };
    struct Op {
        int cmd = 0;
        std::vector<int> words;          // 12-bit parameter words
        bool probe = false;              // only: is the range valid?
        bool simple = false;             // a command answered by a RESPONSE only
        bool param = false;              // PUT PARAMETER: answered only on an error
        std::vector<uint16_t> payload;   // PUT: the data block after the EPS's ACK
        std::function<void()> before;    // runs just before the command goes out
        Op();
        ~Op();
        Op(const Op &);
        Op &operator=(const Op &);
        std::function<void(const std::vector<uint16_t> &)> data;   // the block
        std::function<void(bool valid)> probed;
    };

    void start(double now);
    void finish(double now);
    void fail(const std::string &why);
    void sendOp();
    void ack();
    void packetIn(const std::vector<uint8_t> &pk, double now);
    void queueWaveData(int inst, int layer, int ws, uint32_t n, uint32_t blockOff);
    void findLength(int inst, int layer, int ws, uint32_t lo, uint32_t hi, uint32_t blockOff);
    void assemble();
    void queuePutData(int inst, int layer, int ws, const std::vector<int16_t> &pcm);
    void sendData(const std::vector<uint16_t> &w);
    Op param(int inst, int layer, int ws, int page, int item, uint32_t value);

    Sender send_;
    int channel_ = 0;
    State state_ = State::Idle;
    std::deque<Op> ops_;
    Op cur_;
    double deadline_ = 0, nextAt_ = 0;
    int tries_ = 0;
    std::vector<uint8_t> in_;
    bool inSx_ = false;
    int job_ = 0;                        // 1 instrument, 2 wavesample
    int inst_ = 0;
    std::vector<uint8_t> img_;           // the file being built
    std::vector<uint32_t> layerOff_, wsOff_, allOff_;   // its block offsets
    uint32_t end_ = 0;
    int total_ = 0;                      // ops of the job, for percent
    Done done_ = Done::None;
    std::shared_ptr<std::function<void()>> search_;
public:
    bool debug_ = false;              // packets to stderr
};

} // namespace eps
