// eps_surface — the Launchkey surface of eps~: PLAY and EDIT, the pages of eps_pages.h,
// the pads with fixed meanings and their colours. No Pd here: the surface turns pads and
// encoders into messages for eps~ (the same ones the editor sends) and says which page the
// encoders act on; eps~ reads and writes the values and draws the display.
//
//   PLAY  pads: 36 > 38 INIT (twice) 39 PAT> 41 SMPL 42 OPEN
//               44 < 46 SAVE (twice) 47 PAT< 48 SEL 50 EXIT 51 MODE
//         > <: through the folder shown (nothing loads), OPEN: a folder: into it, a patch:
//         load it; EXIT: one level up (the top: the library, images as folders). The display
//         shows where it is after each step. Cyan > <, green OPEN, yellow EXIT, red SMPL.
//         encoders: CC page 1 = P1 PERFORM, CC page 2 = P2 PLAY (the controller layer's numbers)
//   EDIT  pads: 36-43 the areas INST LAYER WAVE PITCH FILTER AMP LFO ENV (again: next page)
//               44 SEL 45 LAY> 46 WS> 47 ENV# 48 O/N 49 CMD 50 SMPL 51 MODE
//         INST again: the effect (E20), its pages of eight parameters (VAR first), then INST
//         again. On E20, as the EPS: O/N steps FX= OFF INST BANK ROM 1-13; SEL + O/N: LOAD
//         EFFECT (an effect file into the bank effect; again: the next), O/N loads it; CMD:
//         COPY TO BANK? (SEL + CMD: COPY TO INST?, SAVE BANK EFFECT?, DELETE INST EFFECT?),
//         CMD again: do it.
//         Another pad: no.
//         encoders: encoder n = cell n of the page shown, whatever CC page the Launchkey is on
//   SEL held: LAY> WS> ENV# step backwards, a key selects its layer and wavesample silently.
//   MODE (51, bottom right, in both layers): PLAY <-> EDIT; blue in PLAY, purple in EDIT.
//   SMPL: the SAMPLE page and the level display, then record, then stop (PLAY ROOT KEY).
//   CMD:  the COMMAND page, then pressed twice: run the command; O/N old <-> new; with a
//         result to keep CMD is green: pressed, it keeps the new one.
#pragma once

#include <string>
#include <vector>

#include "eps_pages.h"

namespace eps {

class Surface {
public:
    enum Mode { Play = 0, Edit = 1 };
    // What eps~ reports to the surface.
    struct Status {
        int rec = 0;        // 0 off, 1 level, 2 armed, 3 recording, 4 root key
        int cmd = 0;        // 0 idle, 1 busy, 2 NEW plays, 3 OLD plays
        int env = 2;        // 0..2 = ENV1..ENV3
        bool navDir = false;  // PLAY: the folder browser shows a folder (OPEN lights)
        bool navTop = true;   // PLAY: the browser is at the top (the library): no UP
        int fxPages = 0;      // pages of eight effect parameters (0 = no effect loaded)
        int fxPage = 0;       // the one shown
        int fxSources = 0;    // effect files to load (0: none or no ROM / OS)
        bool fxSystem = false;  // ROM and OS there: the effects work
    };
    // A message for eps~ ("presetstep 1", "rec start" ...) or, starting with '@', for the
    // display: "@page" (show the page's legend), "@mode" (PLAY / EDIT changed: pad legend),
    // "@ask <what>" (pressed once, press again), "@cmdask".
    using Actions = std::vector<std::string>;

    Mode mode = Play;
    int page = 0;           // kPages index
    int fwPage = 0;         // the CC page the Launchkey sent last (feedback goes there)
    bool selHeld = false;

    static constexpr double kTwiceMs = 1500.0;   // INIT, SAVE, CMD: the second press within

    // The controller layer's CC pages (common/tools/ctlayer.py): page and encoder of a CC.
    static bool ccPage(int cc, int &pg, int &enc);
    static int ccOf(int pg, int enc);            // -1 = none

    // An encoder: the cell it acts on (-1 = not for the surface). In PLAY the CC page
    // chooses P1 / P2; pageChanged tells that the page changed with it.
    int encoder(int cc, bool &pageChanged);
    Actions pad(int note, bool down, double nowMs, const Status &st);
    Actions setMode(Mode m);
    Actions setPage(int p);                      // also switches the mode
    void keyUsed() { selUsed_ = true; }          // a key played while SEL is held

    // Velocity of each pad LED (the firmware's palette: colour + 32 flashing + 64 pulsing,
    // 16 + n dimmed); pads 36..51.
    void padColours(const Status &st, double nowMs, int out[16]) const;
    // Something waits for its second press (the display and the LEDs need a timer).
    bool armed(double nowMs) const;
    const Page &current() const { return kPages[page]; }
    int area() const { return kPages[page].area; }

private:
    int lastPlay_ = 0, lastEdit_ = 2;
    double initArm_ = -1e9, saveArm_ = -1e9, cmdArm_ = -1e9;
public:
    bool saveTo = false;     // PLAY: SAVE pressed, SAVE TO? (navigate, SAVE again: save there)
    bool saveBank = false;   // PLAY: SEL + SAVE: SAVE BANK TO? (the same, the bank)
    bool delAsk = false;     // PLAY: SEL + OPEN pressed, DELETE? (SEL: yes, anything else: no)
    bool fxAsk = false;      // E20: SEL + O/N pressed, LOAD EFFECT (SEL + O/N: the next, O/N: load)
    int fxCmd = 0;           // E20: CMD: 1 COPY TO BANK? 2 COPY TO INST? 3 SAVE BANK FX? 4 DELETE INST FX?
private:
    bool selUsed_ = false;
    int selNote_ = -1;            // the pad held as SEL
    Actions smpl(const Status &st);
};

} // namespace eps
