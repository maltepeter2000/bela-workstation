// eps_surface — see eps_surface.h.
#include "eps_surface.h"

#include <cstdio>

namespace eps {

namespace {

bool reserved(int cc) {
    return cc == 32 || cc == 38 || (cc >= 64 && cc <= 69) || cc == 84 || cc == 88 || (cc >= 96 && cc <= 101);
}

// pad colours (launchkey_firmware cfw/drum_pads.c)
constexpr int kOff = 16;          // black, dimmed: dark (0 would give the pad back to the Launchkey)
constexpr int kWhite = 1, kRed = 2, kOrange = 3, kGreen = 5;
constexpr int kDim = 16, kFlash = 32, kPulse = 64;

int sampleColour(int rec) {
    switch (rec) {
        case 1: return kRed;                  // level display
        case 2: return kRed + kFlash;         // armed
        case 3: return kRed + kFlash;         // recording
        case 4: return kRed + kPulse;         // play the root key
        default: return kDim + kRed;
    }
}

}  // namespace

bool Surface::ccPage(int cc, int &pg, int &enc) {
    if (cc < 20 || cc > 119 || reserved(cc)) return false;
    int k = 0;
    for (int c = 20; c < cc; ++c)
        if (!reserved(c)) ++k;
    pg = k / 8;
    enc = k % 8;
    return true;
}

int Surface::ccOf(int pg, int enc) {
    int k = 0;
    for (int c = 20; c < 120; ++c) {
        if (reserved(c)) continue;
        if (k == pg * 8 + enc) return c;
        ++k;
    }
    return -1;
}

int Surface::encoder(int cc, bool &pageChanged) {
    pageChanged = false;
    int pg = 0, enc = 0;
    if (!ccPage(cc, pg, enc)) return -1;
    fwPage = pg;
    if (mode == Play) {
        if (pg > 1) return -1;                   // P1 / P2 only
        if (page != pg) {
            page = lastPlay_ = pg;
            pageChanged = true;
        }
    }
    return kPages[page].cell[enc].kind ? enc : -1;
}

Surface::Actions Surface::setMode(Mode m) {
    Actions a;
    if (m == mode) return a;
    if (mode == Edit && kPages[page].area == kAreaSample) a.push_back("rec level 0");
    mode = m;
    page = m == Play ? lastPlay_ : lastEdit_;
    a.push_back("@mode");
    return a;
}

Surface::Actions Surface::setPage(int p) {
    Actions a;
    if (p < 0 || p >= kPageCount) return a;
    const bool play = kPages[p].area < 0;
    if (mode == Edit && kPages[page].area == kAreaSample && kPages[p].area != kAreaSample)
        a.push_back("rec level 0");
    mode = play ? Play : Edit;
    page = p;
    (play ? lastPlay_ : lastEdit_) = p;
    a.push_back("@page");
    return a;
}

Surface::Actions Surface::smpl(const Status &st) {
    if (mode != Edit || kPages[page].area != kAreaSample) {
        Actions a = setPage(kAreas[kAreaSample].first);
        if (st.rec == 0) a.push_back("rec level 1");
        return a;
    }
    switch (st.rec) {
        case 0: return {"rec level 1"};
        case 1: return {"rec start"};
        default: return {"rec stop"};             // armed, recording: stop; root key: cancel
    }
}

Surface::Actions Surface::pad(int note, bool down, double now, const Status &st) {
    const int i = note - 36;
    if (i < 0 || i > 15) return {};
    if (!down && note == selNote_) {             // SEL let go (also after it switched the mode)
        selHeld = false;
        selNote_ = -1;
        if (selUsed_) selTap_ = -1e9;
        return {};
    }
    const bool sel = mode == Play ? i == 12 : i == 8;
    if (sel) {
        if (down) {
            selHeld = true;
            selNote_ = note;
            selUsed_ = false;
            if (now - selTap_ < kTapMs) {
                selTap_ = -1e9;
                return setMode(mode == Play ? Edit : Play);
            }
            selTap_ = now;
        }
        return {};
    }
    if (!down) return {};
    if (selHeld) selUsed_ = true;
    if (mode == Play) {
        switch (i) {
            case 0: return {"presetstep 1"};
            case 8: return {"presetstep -1"};
            case 1: return {"userstep 1"};
            case 9: return {"userstep -1"};
            case 2:
                if (now - initArm_ < kTwiceMs) { initArm_ = -1e9; return {"init"}; }
                initArm_ = now;
                return {"@ask INIT"};
            case 10:
                if (now - saveArm_ < kTwiceMs) { saveArm_ = -1e9; return {"save"}; }
                saveArm_ = now;
                return {"@ask SAVE"};
            case 3: return {"cycle patch"};
            case 11: return {"cycle patch -1"};
            case 4: return smpl(st);
            default: return {};
        }
    }
    if (i < 8) {                                 // an area; again: its next page
        const Area &ar = kAreas[i];
        int p = ar.first;
        if (kPages[page].area == i) p = ar.first + (page - ar.first + 1) % ar.count;
        return setPage(p);
    }
    char b[48];
    switch (i) {
        case 9: return {selHeld ? "sel layerstep -1" : "sel layerstep 1"};
        case 10: return {selHeld ? "sel wsstep -1" : "sel wsstep 1"};
        case 11:
            std::snprintf(b, sizeof b, "sel env %d", (st.env + (selHeld ? 2 : 1)) % 3 + 1);
            return {b};
        case 12: return smpl(st);
        case 13:
            if (kPages[page].area != kAreaCommand) return setPage(kAreas[kAreaCommand].first);
            if (st.cmd == 1) return {};
            if (now - cmdArm_ < kTwiceMs) { cmdArm_ = -1e9; return {"cmd do"}; }
            cmdArm_ = now;
            return {"@cmdask"};
        case 14:
            if (st.cmd == 2) return {"cmd old"};
            if (st.cmd == 3) return {"cmd new"};
            return {};
        case 15:
            if (st.cmd >= 2) return {"cmd keep"};
            return {};
        default: return {};
    }
}

bool Surface::armed(double now) const {
    return now - initArm_ < kTwiceMs || now - saveArm_ < kTwiceMs || now - cmdArm_ < kTwiceMs;
}

void Surface::padColours(const Status &st, double now, int out[16]) const {
    for (int i = 0; i < 16; ++i) out[i] = kOff;
    const int avail = kDim + kWhite;
    if (mode == Play) {
        for (int i : {0, 1, 3, 8, 9, 11}) out[i] = avail;
        out[2] = now - initArm_ < kTwiceMs ? kWhite + kFlash : avail;
        out[10] = now - saveArm_ < kTwiceMs ? kWhite + kFlash : avail;
        out[4] = sampleColour(st.rec);
        out[12] = selHeld ? kWhite : avail;
        return;
    }
    const int a = kPages[page].area;
    for (int i = 0; i < 8; ++i) out[i] = a == i ? kWhite : avail;
    out[8] = selHeld ? kWhite : avail;
    out[9] = out[10] = out[11] = avail;
    out[12] = a == kAreaSample && st.rec == 0 ? kWhite : sampleColour(st.rec);
    if (st.cmd == 1) out[13] = kWhite + kPulse;
    else if (now - cmdArm_ < kTwiceMs) out[13] = kWhite + kFlash;
    else out[13] = a == kAreaCommand ? kWhite : avail;
    out[14] = st.cmd == 3 ? kOrange : (st.cmd == 2 ? kGreen : kOff);
    out[15] = st.cmd >= 2 ? kGreen : kOff;
}

} // namespace eps
