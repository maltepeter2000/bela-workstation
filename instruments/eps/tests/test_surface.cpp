// The Launchkey surface of eps~ (engine/eps_surface.h): pages, pads, colours.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "../pd/engine/eps_surface.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

using eps::Surface;

static bool has(const Surface::Actions& a, const std::string& m) {
    for (const auto& s : a) if (s == m) return true;
    return false;
}

static int page(const char* id) {
    for (int i = 0; i < eps::kPageCount; ++i)
        if (std::string(eps::kPages[i].id) == id) return i;
    return -1;
}

int main() {
    // the CC pages of the controller layer
    int pg = 0, enc = 0;
    CHECK(Surface::ccPage(20, pg, enc) && pg == 0 && enc == 0);
    CHECK(Surface::ccPage(33, pg, enc) && pg == 1 && enc == 4);      // 32 is left out
    CHECK(!Surface::ccPage(32, pg, enc) && !Surface::ccPage(64, pg, enc) && !Surface::ccPage(120, pg, enc));
    CHECK(Surface::ccOf(1, 4) == 33 && Surface::ccOf(10, 3) == 119 && Surface::ccOf(10, 4) == -1);

    Surface S;
    Surface::Status st;
    bool changed = false;
    // PLAY: CC page 1 = P1, page 2 = P2, the others nothing
    CHECK(S.encoder(21, changed) == 1 && !changed && S.page == page("P1"));
    CHECK(S.encoder(29, changed) == 1 && changed && S.page == page("P2"));
    CHECK(S.encoder(33, changed) == -1);                               // P2 cell 5 is empty
    CHECK(S.encoder(40, changed) == -1 && S.page == page("P2"));
    // pads in PLAY
    CHECK(has(S.pad(36, true, 0, st), "presetstep 1"));
    CHECK(has(S.pad(47, true, 0, st), "cycle patch -1"));
    CHECK(has(S.pad(38, true, 0, st), "@ask INIT"));
    CHECK(has(S.pad(38, true, 500, st), "init"));
    CHECK(has(S.pad(38, true, 3000, st), "@ask INIT"));                 // too late: asks again
    CHECK(has(S.pad(46, true, 5000, st), "@ask SAVE") && S.armed(5100) && !S.armed(7000));
    // the folders: DIR steps, OPEN, UP
    CHECK(has(S.pad(37, true, 6000, st), "nav step 1") && has(S.pad(45, true, 6100, st), "nav step -1"));
    CHECK(has(S.pad(41, true, 6200, st), "nav open") && has(S.pad(49, true, 6300, st), "nav up"));
    int col[16];
    S.padColours(st, 9000, col);
    CHECK(col[15] == 7 && col[0] == 20 && col[1] == 22 && col[3] == 25);   // MODE blue, LIB DIR PAT
    CHECK(col[5] == 16 && col[13] == 16 && col[14] == 18);     // no folder shown, at the top; SMPL
    st.navDir = true;
    st.navTop = false;
    S.padColours(st, 9000, col);
    CHECK(col[5] == 5 && col[13] == 21);
    st.navDir = false;
    st.navTop = true;
    // SEL tapped twice does nothing any more
    S.pad(48, true, 9500, st);
    S.pad(48, false, 9600, st);
    CHECK(S.pad(48, true, 9700, st).empty() && S.mode == Surface::Play);
    // MODE (51): EDIT, SEL let go after the switch
    CHECK(has(S.pad(51, true, 10000, st), "@mode") && S.mode == Surface::Edit && S.page == page("E1"));
    S.pad(48, false, 10300, st);           // SEL (pad 48 in PLAY) let go, though pad 48 is SMPL in EDIT
    CHECK(!S.selHeld);
    // areas: FILTER, again: FILTER MOD, again: FILTER
    CHECK(has(S.pad(40, true, 11000, st), "@page") && S.page == page("E9"));
    CHECK(S.pad(40, true, 11100, st).size() && S.page == page("E10"));
    S.pad(40, true, 11200, st);
    CHECK(S.page == page("E9"));
    // encoders in EDIT: cell n whatever CC page
    CHECK(S.encoder(20, changed) == 0 && S.encoder(29, changed) == 1 && S.fwPage == 1 && S.page == page("E9"));
    CHECK(S.encoder(119, changed) == 3);   // page 11, encoder 4
    // SEL held: backwards, ENV#
    st.env = 2;
    CHECK(has(S.pad(47, true, 12000, st), "sel env 1"));
    S.pad(44, true, 12100, st);
    CHECK(has(S.pad(45, true, 12200, st), "sel layerstep -1"));
    CHECK(has(S.pad(47, true, 12300, st), "sel env 2"));
    S.pad(44, false, 12400, st);
    CHECK(has(S.pad(46, true, 12500, st), "sel wsstep 1"));
    // SMPL: the page and the level display, then record, then stop
    CHECK(has(S.pad(50, true, 13000, st), "rec level 1") && S.page == page("E18"));
    st.rec = 1;
    CHECK(has(S.pad(50, true, 13100, st), "rec start"));
    st.rec = 3;
    CHECK(has(S.pad(50, true, 13200, st), "rec stop"));
    st.rec = 1;
    CHECK(has(S.pad(36, true, 13300, st), "rec level 0") && S.page == page("E1"));   // leaving SAMPLE
    st.rec = 0;
    // CMD: the page, then twice: run; O/N, KEEP
    S.padColours(st, 13900, col);
    CHECK(col[0] == 1 && col[1] == 20 && col[2] == 22 && col[7] == 28);   // INST shown: bright, the others dimmed
    CHECK(has(S.pad(49, true, 14000, st), "@page") && S.page == page("E19"));
    CHECK(has(S.pad(49, true, 14100, st), "@cmdask"));
    CHECK(has(S.pad(49, true, 14200, st), "cmd do"));
    st.cmd = 2;
    CHECK(has(S.pad(48, true, 15000, st), "cmd old"));
    st.cmd = 3;
    CHECK(has(S.pad(48, true, 15100, st), "cmd new"));
    S.padColours(st, 15150, col);
    CHECK(col[12] == 3 && col[13] == 5 && col[15] == 8);   // O/N orange, CMD green: keep, MODE purple
    CHECK(has(S.pad(49, true, 15200, st), "cmd keep"));
    st.cmd = 2;
    S.padColours(st, 15300, col);
    CHECK(col[12] == 5 && col[13] == 5 && col[8] == 17);   // O/N green
    st.cmd = 0;
    S.padColours(st, 15300, col);
    CHECK(col[12] == 16 && col[13] != 5 && col[15] == 8);
    // PLAY again: the page played last
    S.pad(44, true, 15900, st);            // SEL held over the switch
    CHECK(has(S.pad(51, true, 16000, st), "@mode") && S.mode == Surface::Play && S.page == page("P2"));
    S.pad(51, false, 16050, st);
    S.padColours(st, 16200, col);
    CHECK(col[0] == 20 && col[14] == 18 && col[5] == 16 && col[12] == 1);   // SEL held: white
    S.pad(48, false, 16300, st);
    CHECK(has(S.pad(50, true, 16400, st), "rec level 1") && S.page == page("E18"));   // SMPL from PLAY

    // every area has a page, the SAMPLE and COMMAND pages are where the pads go
    CHECK(eps::kPages[eps::kAreas[eps::kAreaSample].first].area == eps::kAreaSample);
    CHECK(std::string(eps::kPages[eps::kAreas[eps::kAreaCommand].first].id) == "E19");

    std::printf("test_surface: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
