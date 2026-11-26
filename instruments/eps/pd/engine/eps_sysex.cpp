// eps_sysex.cpp — see eps_sysex.h.
#include "eps_sysex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace eps {

namespace {

constexpr int kResponse = 0x01;
constexpr int kAck = 0x00, kWait = 0x01, kBadRange = 0x0A, kNak = 0x17;
constexpr int kPutInstrument = 0x0C, kPutLayer = 0x0D, kPutWsParams = 0x0E, kPutWsData = 0x0F,
              kPutParameter = 0x11, kCreateInstrument = 0x15, kCreateLayer = 0x16, kCreateWs = 0x19,
              kCopyWs = 0x1B, kDeleteWs = 0x1A, kTruncateWs = 0x1E;
constexpr int kGetInstrument = 0x03, kGetLayer = 0x04, kGetWsParams = 0x05, kGetWsData = 0x06,
              kGetPitchTable = 0x07;
constexpr uint32_t kLayerLen = 224, kWsHeader = 0x120;                     // in the file
constexpr uint32_t kChunk = 4096;                                          // samples per GET
constexpr double kReplyTimeout = 8.0, kWaitTimeout = 30.0, kDataGap = 10.0, kQuiet = 1.5,
                 kProbeWait = 2.6;
// In a wavesample block (file offsets): copy-of number, sample end (high bytes
// of four words), sample rate index.
constexpr uint32_t kWsCopyOf = 34, kWsRawEnd = 248, kWsRateIdx = 272;

uint32_t enc24(const uint8_t *p) {
    return uint32_t(p[3] >> 4 & 0xF) << 20 | uint32_t(p[0]) << 12 | uint32_t(p[2]) << 4 | (p[3] & 0xFu);
}
// Ensoniq long: two words, 12 bits each in their upper bits, low word first.
void putLong(uint8_t *p, uint32_t v) {
    const uint32_t lo = (v & 0xFFF) << 4, hi = (v >> 12 & 0xFFF) << 4;
    p[0] = uint8_t(lo >> 8); p[1] = uint8_t(lo); p[2] = uint8_t(hi >> 8); p[3] = uint8_t(hi);
}
std::vector<int> addr(int inst, int layer, int ws) {
    return {inst >> 6 & 0x3F, inst & 0x3F, layer >> 6 & 0x3F, layer & 0x3F, ws >> 6 & 0x3F, ws & 0x3F};
}
void off20(std::vector<int> &w, uint32_t v) {
    for (uint32_t x : {v >> 12, v & 0xFFF}) { w.push_back(int(x >> 6 & 0x3F)); w.push_back(int(x & 0x3F)); }
}
int rateOf(int idx) { return idx == 0 ? 48000 : idx == 1 ? 44100 : int(625000.0 / idx + 0.5); }

} // namespace

std::vector<uint8_t> SysexLink::packet(int channel, int cmd, const std::vector<int> &words) {
    std::vector<uint8_t> p = {0xF0, 0x0F, 0x03, uint8_t(channel & 15), uint8_t(cmd)};
    for (int w : words) p.push_back(uint8_t(w & 0x7F));
    p.push_back(0xF7);
    return p;
}

SysexLink::Op::Op() = default;
SysexLink::Op::~Op() = default;
SysexLink::Op::Op(const Op &) = default;
SysexLink::Op &SysexLink::Op::operator=(const Op &) = default;

std::vector<uint16_t> SysexLink::data16(const std::vector<uint8_t> &pk) {
    std::vector<uint16_t> w;
    for (size_t i = 4; i + 3 < pk.size(); i += 3)
        w.push_back(uint16_t((pk[i] & 0x0F) << 12 | (pk[i + 1] & 0x3F) << 6 | (pk[i + 2] & 0x3F)));
    return w;
}

void SysexLink::cancel() {
    ops_.clear();
    search_.reset();
    state_ = State::Idle;
    job_ = 0;
}

void SysexLink::fail(const std::string &why) {
    error = why;
    cancel();
    done_ = Done::Error;
}

void SysexLink::ack() { send_(packet(channel_, kResponse, {0, kAck})); }

void SysexLink::sendData(const std::vector<uint16_t> &w) {
    std::vector<uint8_t> p = {0xF0, 0x0F, 0x03, uint8_t(channel_)};
    for (uint16_t v : w) {
        p.push_back(uint8_t(v >> 12 & 0x0F));
        p.push_back(uint8_t(v >> 6 & 0x3F));
        p.push_back(uint8_t(v & 0x3F));
    }
    p.push_back(0xF7);
    send_(p);
}

SysexLink::Op SysexLink::param(int inst, int layer, int ws, int page, int item, uint32_t value) {
    Op o;
    o.cmd = kPutParameter;
    o.param = true;
    o.words = addr(inst, layer, ws);
    o.words.push_back(page & 0x7F);
    o.words.push_back(item & 0x7F);
    off20(o.words, value);                 // 24-bit value as two 12-bit words
    o.settle = 3.0;                        // measured: the OS answers NAK when hurried
    return o;
}

void SysexLink::sendOp() {
    if (cur_.before) { auto b = cur_.before; cur_.before = nullptr; b(); }
    std::vector<int> w = cur_.words;
    if (debug_) {
        std::fprintf(stderr, "[sx] out cmd %02X%s", cur_.cmd, cur_.probe ? " probe" : "");
        for (int x : w) std::fprintf(stderr, " %02X", x);
        std::fprintf(stderr, "\n");
    }
    send_(packet(channel_, cur_.cmd, w));
}

void SysexLink::start(double now) {
    if (ops_.empty()) { finish(now); return; }
    cur_ = ops_.front();
    ops_.pop_front();
    tries_ = 0;
    sendOp();
    state_ = State::Sent;
    deadline_ = now + kReplyTimeout;
    if (cur_.param) { state_ = State::Quiet; nextAt_ = now + cur_.settle; }
    if (total_ > 0) percent = std::min(99, 100 * (total_ - int(ops_.size()) - 1) / total_);
}

void SysexLink::finish(double) {
    state_ = State::Idle;
    if (job_ == 1) { assemble(); done_ = Done::Instrument; }
    else if (job_ == 2) done_ = Done::WaveSample;
    else if (job_ == 3) done_ = Done::Sent;
    job_ = 0;
    percent = 100;
}

void SysexLink::receive(uint8_t b, double now) {
    if (b == 0xF0) { in_.assign(1, b); inSx_ = true; return; }
    if (!inSx_) return;
    if (b >= 0xF8) return;                       // realtime bytes may come in between
    in_.push_back(b);
    if (state_ == State::Data) deadline_ = now + kDataGap;   // a long block is still coming
    if (b == 0xF7) {
        inSx_ = false;
        if (in_.size() >= 6 && in_[1] == 0x0F && in_[2] == 0x03) packetIn(in_, now);
        in_.clear();
    }
}

void SysexLink::packetIn(const std::vector<uint8_t> &pk, double now) {
    if (debug_) {
        std::fprintf(stderr, "[sx] in %s (state %d, cmd %02X)", pk.size() > 24 ? "data" : "", int(state_), cur_.cmd);
        for (size_t i = 0; i < pk.size() && i < 24; ++i) std::fprintf(stderr, " %02X", pk[i]);
        std::fprintf(stderr, "\n");
    }
    const bool response = pk.size() == 8 && pk[4] == kResponse;
    switch (state_) {
    case State::Sent:
        if (response) {
            const int st = pk[6];
            if (st == kAck && cur_.simple) { state_ = State::Quiet; nextAt_ = now + cur_.settle; return; }
            if (st == kAck && !cur_.payload.empty()) {           // the EPS takes the data block
                sendData(cur_.payload);
                state_ = State::PutData;
                deadline_ = now + kDataGap + cur_.payload.size() * 3 / 3125.0;
                return;
            }
            if (st == kAck) return;                              // the PUT header follows
            if (st == kWait) { ack(); deadline_ = now + kWaitTimeout; return; }
            if (st == kNak && tries_ < 4) {                      // busy: the same again a bit later
                ++tries_;
                nextAt_ = now + 1.0;
                deadline_ = now + 1.0 + kReplyTimeout;
                return;
            }
            if (cur_.probe && st == kBadRange) {
                state_ = State::Quiet;
                nextAt_ = now + 0.2;
                auto f = cur_.probed;
                if (f) f(false);
                return;
            }
            char b[64];
            std::snprintf(b, sizeof b, "the EPS answered status %02X to command %02X", st, cur_.cmd);
            fail(b);
            return;
        }
        // the PUT header
        if (cur_.probe) {                                        // valid range: let the EPS time out
            state_ = State::ProbeWait;
            nextAt_ = now + kProbeWait;
            auto f = cur_.probed;
            if (f) f(true);
            return;
        }
        ack();
        state_ = State::Data;
        deadline_ = now + kDataGap;
        return;
    case State::PutData:
        if (response && pk[6] == kAck) { state_ = State::Quiet; nextAt_ = now + cur_.settle; return; }
        if (response) {
            char b[64];
            std::snprintf(b, sizeof b, "the EPS refused the data (status %02X, command %02X)", pk[6], cur_.cmd);
            fail(b);
        }
        return;
    case State::Quiet:
        if (response && cur_.param && pk[6] != kAck) {
            char b[64];
            std::snprintf(b, sizeof b, "the EPS refused a parameter (status %02X)", pk[6]);
            fail(b);
        }
        return;
    case State::Data: {
        ack();
        const std::vector<uint16_t> w = data16(pk);
        state_ = State::Quiet;                                   // the EPS answers the closing ACK
        nextAt_ = now + kQuiet;
        auto f = cur_.data;
        if (f) f(w);
        return;
    }
    default:
        return;                                                  // answers to a closing ACK, other talk
    }
}

void SysexLink::tick(double now) {
    switch (state_) {
    case State::Idle: return;
    case State::Sent:
        if (nextAt_ > 0 && now >= nextAt_) { nextAt_ = 0; sendOp(); return; }   // after a NAK
        if (now > deadline_) {
            if (++tries_ <= 3) { sendOp(); deadline_ = now + kReplyTimeout; return; }
            fail("no answer from the EPS (MIDI SYS-EX on? cables both ways?)");
        }
        return;
    case State::Data:
        if (now > deadline_) fail("the data block did not come");
        return;
    case State::PutData:
        if (now > deadline_) {
            // OS 1.30 may not confirm the block: go on and check by reading back
            state_ = State::Quiet;
            nextAt_ = now;
        }
        return;
    case State::Quiet:
    case State::ProbeWait:
        if (now >= nextAt_) { nextAt_ = 0; start(now); }
        return;
    }
}

// ---- jobs -------------------------------------------------------------------------

bool SysexLink::fetchWaveSample(int inst, int layer, int ws, double now) {
    if (busy()) return false;
    job_ = 2;
    inst_ = inst;
    pcm.clear();
    error.clear();
    step = "wavesample parameters";
    percent = 0;
    total_ = 0;
    Op op;
    op.cmd = kGetWsParams;
    op.words = addr(inst, layer, ws);
    op.data = [this, inst, layer, ws](const std::vector<uint16_t> &w) {
        std::vector<uint8_t> b(kWsHeader, 0);
        for (size_t i = 0; i < w.size() && 10 + 2 * i + 1 < b.size(); ++i) {
            b[10 + 2 * i] = uint8_t(w[i] >> 8);
            b[11 + 2 * i] = uint8_t(w[i]);
        }
        if (b[kWsCopyOf]) { fail("wavesample " + std::to_string(ws) + " is a copy of " + std::to_string(b[kWsCopyOf])); return; }
        name.clear();
        for (int i = 0; i < 12; ++i) name += char(b[10 + 2 * i]);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        sampleRate = rateOf(b[kWsRateIdx]);
        const uint32_t rawEnd = uint32_t(b[kWsRawEnd]) << 24 | uint32_t(b[kWsRawEnd + 2]) << 16 |
                                uint32_t(b[kWsRawEnd + 4]) << 8 | b[kWsRawEnd + 6];
        step = "length";
        findLength(inst, layer, ws, rawEnd >> 9, 0, 0);
    };
    ops_.push_back(op);
    start(now);
    return true;
}

bool SysexLink::fetchInstrument(int inst, double now) {
    if (busy()) return false;
    job_ = 1;
    inst_ = inst;
    img_.assign(1024, 0);
    end_ = 656;
    error.clear();
    step = "instrument";
    percent = 0;
    total_ = 0;
    Op op;
    op.cmd = kGetInstrument;
    op.words = addr(inst, 0, 1);
    op.data = [this, inst](const std::vector<uint16_t> &w) {
        for (size_t i = 0; i < w.size() && 11 + 2 * i < 656; ++i) {
            img_[10 + 2 * i] = uint8_t(w[i] >> 8);
            img_[11 + 2 * i] = uint8_t(w[i]);
        }
        name.clear();
        for (int i = 0; i < 12; ++i) name += char(img_[10 + 2 * i]);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        // The blocks and where they go: the offset tables of the header.
        std::vector<uint32_t> &layerOff = layerOff_, &wsOff = wsOff_, &all = allOff_;
        layerOff.clear();
        all.clear();
        wsOff.assign(128, 0);
        for (int i = 0; i < 8; ++i) {
            const uint32_t o = enc24(img_.data() + 100 + 4 * i);
            if (!o) break;
            layerOff.push_back(o);
            all.push_back(o);
        }
        for (int i = 0; i < 128; ++i) {
            wsOff[size_t(i)] = enc24(img_.data() + 132 + 4 * i);
            if (wsOff[size_t(i)]) all.push_back(wsOff[size_t(i)]);
        }
        std::sort(all.begin(), all.end());
        auto grow = [this](uint32_t n) {
            if (img_.size() < n) img_.resize(n, 0);
            end_ = std::max(end_, n);
        };
        for (size_t li = 0; li < layerOff.size(); ++li) {
            Op lo;
            lo.cmd = kGetLayer;
            lo.words = addr(inst, int(li), 1);
            const uint32_t o = layerOff[li];
            lo.data = [this, o, grow](const std::vector<uint16_t> &lw) {
                grow(o + kLayerLen);
                putLong(img_.data() + o, kLayerLen);
                for (size_t i = 0; i < lw.size() && 10 + 2 * i + 1 < kLayerLen; ++i) {
                    img_[o + 10 + 2 * i] = uint8_t(lw[i] >> 8);
                    img_[o + 11 + 2 * i] = uint8_t(lw[i]);
                }
            };
            ops_.push_back(lo);
        }
        // Wavesamples, asked with the layer whose key map uses them (else layer 0):
        // queued when the last layer is in and the key maps are known.
        const size_t nLayers = layerOff.size();
        auto queueWs = [this, inst, grow]() {
            const std::vector<uint32_t> &wOff = wsOff_, &allO = allOff_, &lOff = layerOff_;
            for (int ws = 1; ws < 128; ++ws) {
                const uint32_t o = wOff[size_t(ws)];
                if (!o) continue;
                int owner = 0;
                for (size_t li = 0; li < lOff.size(); ++li) {
                    const uint32_t L = lOff[li];
                    for (int k = 0; k < 88; ++k)                 // key map: 88 words at layer byte 48
                        if (L + 48 + 2 * k < img_.size() && img_[L + 48 + 2 * k] == ws) owner = int(li);
                }
                const auto nx = std::upper_bound(allO.begin(), allO.end(), o);
                const uint32_t next = nx == allO.end() ? 0 : *nx;
                Op wo;
                wo.cmd = kGetWsParams;
                wo.words = addr(inst, owner, ws);
                wo.data = [this, inst, owner, ws, o, next, grow](const std::vector<uint16_t> &blk) {
                    grow(o + kWsHeader);
                    for (size_t i = 0; i < blk.size() && 10 + 2 * i + 1 < kWsHeader; ++i) {
                        img_[o + 10 + 2 * i] = uint8_t(blk[i] >> 8);
                        img_[o + 11 + 2 * i] = uint8_t(blk[i]);
                    }
                    putLong(img_.data() + o, kWsHeader);
                    if (img_[o + kWsCopyOf]) return;             // a copy: no samples of its own
                    const uint8_t *b = img_.data() + o;
                    const uint32_t rawEnd = uint32_t(b[kWsRawEnd]) << 24 | uint32_t(b[kWsRawEnd + 2]) << 16 |
                                            uint32_t(b[kWsRawEnd + 4]) << 8 | b[kWsRawEnd + 6];
                    const uint32_t cand = next > o + kWsHeader ? (next - o - kWsHeader) / 2 : 0;
                    findLength(inst, owner, ws, rawEnd >> 9, cand, o);
                };
                ops_.push_back(wo);
                ++total_;
            }
        };
        // Pitch tables: asked through a layer that uses them (layer value 2-9 = table 1-8).
        std::vector<uint32_t> ptOff;
        for (int i = 0; i < 8; ++i) ptOff.push_back(enc24(img_.data() + 612 + 4 * i));
        auto queuePt = [this, inst, ptOff, grow]() {
            const std::vector<uint32_t> &lOff = layerOff_;
            std::vector<bool> done(8, false);
            for (size_t li = 0; li < lOff.size(); ++li) {
                const int t = int(img_[lOff[li] + 44]) - 2;
                if (t < 0 || t > 7 || !ptOff[size_t(t)] || done[size_t(t)]) continue;
                done[size_t(t)] = true;
                const uint32_t o = ptOff[size_t(t)];
                Op po;
                po.cmd = kGetPitchTable;
                po.words = addr(inst, int(li), 1);
                po.data = [this, o, grow](const std::vector<uint16_t> &pt) {
                    grow(o + kLayerLen);
                    putLong(img_.data() + o, kLayerLen);
                    for (size_t i = 0; i < pt.size() && 10 + 2 * i + 1 < kLayerLen; ++i) {
                        img_[o + 10 + 2 * i] = uint8_t(pt[i] >> 8);
                        img_[o + 11 + 2 * i] = uint8_t(pt[i]);
                    }
                };
                ops_.push_back(po);
                ++total_;
            }
        };
        if (nLayers == 0) { fail("the instrument has no layers"); return; }
        // the last layer's data callback queues the wavesamples
        Op &last = ops_.back();
        auto prev = last.data;
        last.data = [prev, queueWs, queuePt](const std::vector<uint16_t> &lw) { prev(lw); queuePt(); queueWs(); };
        total_ = int(ops_.size()) + 1;
        step = "layers";
    };
    ops_.push_back(op);
    start(now);
    return true;
}

// The number of samples: the largest end offset the EPS accepts. lo is known to
// be valid (the sample end), cand a guess (the room up to the next block).
void SysexLink::findLength(int inst, int layer, int ws, uint32_t lo, uint32_t cand, uint32_t blockOff) {
    struct S { uint32_t lo, hi; std::vector<uint32_t> guesses; };
    auto st = std::make_shared<S>();
    st->lo = lo;
    st->hi = 0x100000;                                    // past the largest offset: invalid
    if (cand > lo) st->guesses = {cand + 1, cand};        // used from the back: cand, then cand + 1
    else st->guesses = {lo + 64, lo + 16};
    auto self = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weak = self;
    search_ = self;                                       // keeps it alive until the next search
    *self = [this, inst, layer, ws, blockOff, st, weak]() {
        if (st->hi - st->lo <= 1) {
            if (job_ == 1) {
                const uint32_t n = st->lo;
                if (img_.size() < blockOff + kWsHeader + 2 * n) img_.resize(blockOff + kWsHeader + 2 * n, 0);
                end_ = std::max<uint32_t>(end_, blockOff + kWsHeader + 2 * n);
                putLong(img_.data() + blockOff, kWsHeader + 2 * n);
            } else {
                pcm.assign(st->lo, 0);
            }
            queueWaveData(inst, layer, ws, st->lo, blockOff);
            return;
        }
        uint32_t m = 0;
        while (!st->guesses.empty()) {
            const uint32_t g = st->guesses.back();
            st->guesses.pop_back();
            if (g > st->lo && g < st->hi) { m = g; break; }
        }
        if (!m) m = st->lo + (st->hi - st->lo) / 2;
        Op p;
        p.cmd = kGetWsData;
        p.probe = true;
        p.words = addr(inst, layer, ws);
        off20(p.words, m);
        off20(p.words, m);
        p.probed = [st, m, weak](bool valid) {
            if (valid) st->lo = m; else st->hi = m;
            if (auto f = weak.lock()) (*f)();
        };
        ops_.push_front(p);
    };
    (*self)();
}

void SysexLink::queueWaveData(int inst, int layer, int ws, uint32_t n, uint32_t blockOff) {
    std::vector<Op> chunk;
    for (uint32_t s = 0; s < n; s += kChunk) {
        const uint32_t e = std::min(n, s + kChunk);
        Op d;
        d.cmd = kGetWsData;
        d.words = addr(inst, layer, ws);
        off20(d.words, s);
        off20(d.words, e);
        d.data = [this, s, e, blockOff](const std::vector<uint16_t> &w) {
            step = "samples";
            for (uint32_t i = 0; i < e - s && i < w.size(); ++i) {
                if (job_ == 1) {
                    uint8_t *p = img_.data() + blockOff + kWsHeader + 2 * (s + i);
                    p[0] = uint8_t(w[i] >> 8);
                    p[1] = uint8_t(w[i]);
                } else {
                    pcm[s + i] = int16_t(w[i]);
                }
            }
        };
        chunk.push_back(d);
    }
    // right after the length probes, before the other wavesamples
    for (auto it = chunk.rbegin(); it != chunk.rend(); ++it) ops_.push_front(*it);
    total_ += int(chunk.size());
}

// ---- sending ---------------------------------------------------------------------

void SysexLink::queuePutData(int inst, int layer, int ws, const std::vector<int16_t> &smp) {
    // Make the wavesample tiny (start 0, end 1, truncate), then write the samples
    // from the start: writing past the end makes it longer. A GET first makes it
    // the EPS's edit wavesample (without it TRUNCATE is refused).
    Op g;
    g.cmd = kGetWsParams;
    g.probe = true;                        // only the header: the EPS times out by itself
    g.words = addr(inst, layer, ws);
    g.probed = [](bool) {};
    ops_.push_back(g);
    ops_.push_back(param(inst, layer, ws, 0x20, 0x15, 0));
    ops_.push_back(param(inst, layer, ws, 0x20, 0x16, 1));
    Op t;
    t.cmd = kTruncateWs;
    t.simple = true;
    t.settle = 2.5;
    t.words = addr(inst, layer, ws);
    ops_.push_back(t);
    for (uint32_t s = 0; s < smp.size(); s += kChunk) {
        const uint32_t e = std::min<uint32_t>(uint32_t(smp.size()), s + kChunk);
        Op d;
        d.cmd = kPutWsData;
        d.words = addr(inst, layer, ws);
        off20(d.words, s);
        off20(d.words, e);
        for (uint32_t i = s; i < e; ++i) d.payload.push_back(uint16_t(smp[i]));
        d.before = [this]() { step = "samples"; };
        ops_.push_back(d);
    }
}

bool SysexLink::sendWaveSample(int inst, int layer, int ws, const std::vector<int16_t> &samples, double now) {
    if (busy() || samples.size() < 2) return false;
    job_ = 3;
    error.clear();
    step = "samples";
    percent = 0;
    // Write over the wavesample from its start (it grows if needed), then set its
    // end and cut what is behind it (TRUNCATE before writing is refused when the
    // wavesample has copies).
    const uint32_t n = uint32_t(samples.size());
    for (uint32_t s0 = 0; s0 < n; s0 += kChunk) {
        const uint32_t e = std::min(n, s0 + kChunk);
        Op d;
        d.cmd = kPutWsData;
        d.words = addr(inst, layer, ws);
        off20(d.words, s0);
        off20(d.words, e);
        for (uint32_t i = s0; i < e; ++i) d.payload.push_back(uint16_t(samples[i]));
        ops_.push_back(d);
    }
    Op g;
    g.cmd = kGetWsParams;
    g.probe = true;
    g.words = addr(inst, layer, ws);
    g.probed = [](bool) {};
    ops_.push_back(g);
    ops_.push_back(param(inst, layer, ws, 0x20, 0x15, 0));       // sample start
    ops_.push_back(param(inst, layer, ws, 0x20, 0x16, n - 1));   // sample end
    ops_.push_back(param(inst, layer, ws, 0x20, 0x17, 0));       // loop start
    ops_.push_back(param(inst, layer, ws, 0x20, 0x18, n - 1));   // loop end
    Op t;
    t.cmd = kTruncateWs;
    t.simple = true;
    t.settle = 2.5;
    t.words = addr(inst, layer, ws);
    ops_.push_back(t);
    total_ = int(ops_.size());
    start(now);
    return true;
}

bool SysexLink::sendInstrument(int inst, const std::vector<uint8_t> &f, double now) {
    if (busy() || f.size() < 656) return false;
    job_ = 3;
    error.clear();
    step = "instrument";
    percent = 0;
    std::vector<uint32_t> layerOff, wsOff(128, 0);
    for (int i = 0; i < 8; ++i) {
        const uint32_t o = enc24(f.data() + 100 + 4 * i);
        if (!o) break;
        if (o + kLayerLen > f.size()) { error = "layer outside the file"; return false; }
        layerOff.push_back(o);
    }
    for (int i = 1; i < 128; ++i) {
        const uint32_t o = enc24(f.data() + 132 + 4 * i);
        if (o && o + kWsHeader <= f.size()) wsOff[size_t(i)] = o;
    }
    if (layerOff.empty()) { error = "no layers"; job_ = 0; return false; }
    auto words16 = [&f](uint32_t from, uint32_t bytes) {
        std::vector<uint16_t> w;
        for (uint32_t i = 0; i + 1 < bytes; i += 2) w.push_back(uint16_t(f[from + i] << 8 | f[from + i + 1]));
        return w;
    };
    // which layer each wavesample belongs to (its key map)
    std::vector<int> owner(128, -1);
    for (size_t li = 0; li < layerOff.size(); ++li)
        for (int k = 0; k < 88; ++k) {
            const int w = f[layerOff[li] + 48 + 2 * k];
            if (w > 0 && w < 128 && wsOff[size_t(w)] && owner[size_t(w)] < 0) owner[size_t(w)] = int(li);
        }
    // The OS 1.30 numbers wavesamples itself, one after the other in the order
    // they are made (CREATE LAYER makes an empty layer, CREATE and COPY WAVESAMPLE
    // ignore the number given). Made layer by layer, in order, they get the
    // file's numbers when those run 1, 2, 3 ... through the layers; num maps.
    auto samplesOf = [&f](uint32_t o) {
        const uint8_t *p = f.data() + o;
        const uint32_t len = (uint32_t(p[2] << 8 | p[3]) >> 4) << 12 | (uint32_t(p[0] << 8 | p[1]) >> 4);
        const uint32_t n = len > kWsHeader ? (len - kWsHeader) / 2 : 0;
        std::vector<int16_t> smp(n);
        for (uint32_t i = 0; i < n && o + kWsHeader + 2 * i + 1 < f.size(); ++i)
            smp[i] = int16_t(f[o + kWsHeader + 2 * i] << 8 | f[o + kWsHeader + 2 * i + 1]);
        // The OS appends 8 zero samples to every wavesample itself (measured); a
        // file has them already: they are not sent again.
        if (smp.size() > 10 && std::all_of(smp.end() - 8, smp.end(), [](int16_t v) { return v == 0; }))
            smp.resize(smp.size() - 8);
        return smp;
    };
    std::vector<int> num(128, 0);
    int next = 1;

    Op ci;
    ci.cmd = kCreateInstrument;
    ci.simple = true;
    ci.settle = 1.5;
    ci.words = addr(inst, 0, 1);
    ops_.push_back(ci);
    for (size_t li = 0; li < layerOff.size(); ++li) {
        Op cl;
        cl.cmd = kCreateLayer;
        cl.simple = true;
        cl.settle = 1.5;
        cl.words = addr(inst, int(li), next);
        ops_.push_back(cl);
        for (int w = 1; w < 128; ++w) {
            if (owner[size_t(w)] != int(li)) continue;
            const int src = f[wsOff[size_t(w)] + kWsCopyOf];
            Op c;
            c.simple = true;
            c.settle = 4.0;
            if (src && src < 128 && num[size_t(src)]) {     // a copy shares the data of its source
                c.cmd = kCopyWs;
                c.words = addr(inst, owner[size_t(src)], num[size_t(src)]);
                std::vector<int> d = addr(inst, int(li), next);
                c.words.insert(c.words.end(), d.begin(), d.end());
                c.words.push_back(0); c.words.push_back(0);      // data flag clear: shared
                num[size_t(w)] = next++;
                ops_.push_back(c);
            } else {
                // made and filled before anything copies it (data into a wavesample
                // that has copies is refused)
                c.cmd = kCreateWs;
                c.words = addr(inst, int(li), next);
                num[size_t(w)] = next++;
                ops_.push_back(c);
                const std::vector<int16_t> smp = samplesOf(wsOff[size_t(w)]);
                if (smp.size() >= 2) queuePutData(inst, int(li), num[size_t(w)], smp);
            }
        }
    }
    // then all parameter blocks
    for (int w = 1; w < 128; ++w) {
        if (!wsOff[size_t(w)] || owner[size_t(w)] < 0) continue;
        Op p;
        p.cmd = kPutWsParams;
        p.words = addr(inst, owner[size_t(w)], num[size_t(w)]);
        p.payload = words16(wsOff[size_t(w)] + 10, kWsHeader - 10);
        p.payload.resize(139);
        // the copy reference in the block is a wavesample number too
        const int src = f[wsOff[size_t(w)] + kWsCopyOf];
        if (src && src < 128) p.payload[12] = uint16_t(num[size_t(src)] << 8 | (p.payload[12] & 0xFF));
        p.before = [this]() { step = "parameters"; };
        ops_.push_back(p);
    }
    for (size_t li = 0; li < layerOff.size(); ++li) {
        Op p;
        p.cmd = kPutLayer;
        p.words = addr(inst, int(li), 1);
        p.payload = words16(layerOff[li] + 10, kLayerLen - 10);
        p.payload.resize(107);
        ops_.push_back(p);
    }
    // The instrument block: read what the EPS has (its own offsets), change the
    // first 29 words (name, MIDI, patches, key range, transpose), send it back.
    const std::vector<uint16_t> head = words16(10, 58);
    Op g;
    g.cmd = kGetInstrument;
    g.words = addr(inst, 0, 1);
    g.data = [this, inst, head](const std::vector<uint16_t> &cur) {
        Op p;
        p.cmd = kPutInstrument;
        p.words = addr(inst, 0, 1);
        p.payload = cur;
        for (size_t i = 0; i < head.size() && i < p.payload.size(); ++i) p.payload[i] = head[i];
        ops_.push_front(p);
    };
    ops_.push_back(g);
    total_ = int(ops_.size());
    start(now);
    return true;
}

void SysexLink::assemble() {
    const uint32_t total = (end_ + 511) / 512 * 512;
    img_.resize(total, 0);
    putLong(img_.data(), total);
    // The effect block cannot be transferred: no effect (the EPS then uses its own).
    for (int i = 644; i < 648; ++i) img_[size_t(i)] = 0;
    file = img_;
}

} // namespace eps
