// eps_surface — the Launchkey surface of eps~: PLAY and EDIT, the pages of eps_pages.h,
// the pads with fixed meanings and their colours. No Pd here: the surface turns pads and
// encoders into messages for eps~ (the same ones the editor sends) and says which page the
// encoders act on; eps~ reads and writes the values and draws the display.
//
//   PLAY  pads: 36 LIB> 37 DIR> 38 INIT (twice) 39 PAT> 40 SMPL
//               44 LIB< 45 DIR< 46 SAVE (twice) 47 PAT< 48 SEL 51 MODE
//         encoders: CC page 1 = P1 PERFORM, CC page 2 = P2 PLAY (the controller layer's numbers)
//   EDIT  pads: 36-43 the areas INST LAYER WAVE PITCH FILTER AMP LFO ENV (again: next page)
//               44 SEL 45 LAY> 46 WS> 47 ENV# 48 SMPL 49 CMD 50 O/N 51 MODE
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
    bool selUsed_ = false;
    int selNote_ = -1;            // the pad held as SEL
    Actions smpl(const Status &st);
};

} // namespace eps
