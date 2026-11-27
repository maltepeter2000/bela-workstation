// The Launchkey surface of eps~ (engine/eps_surface.h, pages from tools/eps_pages.py): part of
// eps~.cpp, included after the recorder and the wavesample commands.
//
//   lk 0/1             the surface off / on (eps.i~ switches it on; off: CCs, pads as before)
//   lk edit [0/1]      EDIT (or PLAY); lk mode: switch; lk page <id> (P1 .. E19); lk legend
//   lk sync            send the pad colours and the encoder values again
//   lk pad <note> <velocity>, lk cc <cc> <value>: as from the Launchkey (tests, other controllers)
//   padchannel n       the channel of the pads (eps.i~: argument 2, 10 if none)
// The encoders send CCs of the controller layer's pages on the instrument's channel; the
// values of the page shown go back as CC on the page the Launchkey is on (it takes them,
// so the encoder starts there). The display: one small line, then two big lines of 10
// (the chosen option underlined) or a picture; drawn with lkgfx.

void eps_command_core(t_eps* x, t_symbol* s, int argc, t_atom* argv);

// ---- values of a cell ------------------------------------------------------------------

struct LkRange { int lo, hi; };

LkRange lk_rec_range(const std::string& n) {
    if (n == "rate") return {0, 6};
    if (n == "thresh") return {0, 19};
    if (n == "pre") return {0, 127};
    if (n == "input" || n == "dest") return {0, 3};
    return {0, 1};                                   // monitor
}

// cmd opt: what the option of the command is (volume_smooth: smoothing, copy_ws: the layer)
const char* lk_opt_key(const t_eps* x) {
    const auto c = static_cast<eps::WaveCmd>(x->wave->cmd);
    if (c == eps::WaveCmd::Smooth) return "smooth";
    if (c == eps::WaveCmd::CopyWs) return "layer";
    if (c == eps::WaveCmd::CopyLayer) return "data";
    if (c == eps::WaveCmd::CopyParams || c == eps::WaveCmd::CopyData || c == eps::WaveCmd::AddData ||
        c == eps::WaveCmd::Mix || c == eps::WaveCmd::Merge || c == eps::WaveCmd::Splice) return "ws";
    return "";
}

// cmd rate: for mix, merge and splice the balance (they keep the rate)
bool lk_balance(const t_eps* x) {
    const auto c = static_cast<eps::WaveCmd>(x->wave->cmd);
    return c == eps::WaveCmd::Mix || c == eps::WaveCmd::Merge || c == eps::WaveCmd::Splice;
}

LkRange lk_cmd_range(const t_eps* x, std::string n) {
    if (n == "opt") n = lk_opt_key(x);
    if (n == "rate" && lk_balance(x)) n = "balance";
    if (n == "balance") return {-127, 127};
    if (n == "cmd") return {0, static_cast<int>(eps::WaveCmd::Count) - 1};
    if (n == "range") return {0, 3};
    if (n == "depth") return {0, 5};
    if (n == "zone") return {1, 99};
    if (n == "sstart" || n == "send") return {0, 200};
    if (n == "rate") return {0, 6};
    if (n == "smooth") return {0, 4};
    if (n == "layer") return {1, 8};
    if (n == "data") return {0, 1};
    if (n == "ws") return {1, 127};
    return {0, 0};
}

int lk_rec_value(const t_eps* x, const std::string& n) {
    const Recorder& R = *x->rec;
    if (n == "rate") return R.rate;
    if (n == "thresh") return R.thresh;
    if (n == "pre") return R.pre;
    if (n == "input") return R.input;
    if (n == "dest") return R.dest;
    return R.monitor ? 1 : 0;
}

int lk_cmd_value(const t_eps* x, std::string n) {
    const eps::WaveArgs& A = x->wave->args;
    if (n == "opt") n = lk_opt_key(x);
    if ((n == "rate" && lk_balance(x)) || n == "balance") return A.balance;
    if (n == "cmd") return x->wave->cmd;
    if (n == "range") return static_cast<int>(A.range);
    if (n == "depth") return A.depth;
    if (n == "zone") return A.zone;
    if (n == "sstart") return A.scaleStart;
    if (n == "send") return A.scaleEnd;
    if (n == "rate") return A.rate;
    if (n == "smooth") return A.smooth;
    if (n == "layer") return A.toLayer + 1;
    if (n == "data") return A.withData ? 1 : 0;
    if (n == "ws") return A.toWs;
    return 0;
}

int lk_to127(LkRange r, int v) {
    Control c{"", 'S', r.lo, r.hi, r.lo, "", ""};
    return to127(c, v);
}
int lk_from127(LkRange r, int v) {
    Control c{"", 'S', r.lo, r.hi, r.lo, "", ""};
    return from127(c, v);
}

bool lk_cell_valid(const t_eps* x, const eps::PageCell& c) {
    if (c.kind == 'e') return x->sampler && eps::Editor::find(c.target) >= 0;
    if (c.kind == 'x' && !std::strcmp(c.target, "opt")) return *lk_opt_key(x) != 0;
    return c.kind == 'c' || c.kind == 'r' || c.kind == 'x';
}

int lk_cell_get(t_eps* x, const eps::PageCell& c) {
    if (!lk_cell_valid(x, c)) return 0;
    switch (c.kind) {
        case 'c': {
            const Control* k = find_control(c.target);
            return k ? to127(*k, x->value[control_index(k)]) : 0;
        }
        case 'e': return x->editor->get127(x->sampler->editInstrument(), eps::Editor::find(c.target));
        case 'r': return lk_to127(lk_rec_range(c.target), lk_rec_value(x, c.target));
        case 'x': return lk_to127(lk_cmd_range(x, c.target), lk_cmd_value(x, c.target));
        default: return 0;
    }
}

// The value as the display shows it; names: true if it is one of a list (underlined).
std::string lk_cell_text(t_eps* x, const eps::PageCell& c, bool& names) {
    names = false;
    if (c.kind == '-') return "NOT YET";
    if (c.kind == 'k' || c.kind == 'a') return c.target;
    if (!lk_cell_valid(x, c)) return "--";
    switch (c.kind) {
        case 'c': {
            const Control* k = find_control(c.target);
            names = k->names[0] != 0;
            return value_text(*k, x->value[control_index(k)]);
        }
        case 'e': {
            const int id = eps::Editor::find(c.target);
            const char* kind = eps::kEditParams[id].kind;
            names = std::strchr(kind, '|') != nullptr || !std::strcmp(kind, "src");
            return x->editor->text(x->sampler->editInstrument(), id);
        }
        case 'r': {
            const std::string n = c.target;
            const int v = lk_rec_value(x, n);
            static const char* const kIn[4] = {"INLET", "IN 1", "IN 2", "IN 1+2"};
            names = n != "thresh" && n != "pre";
            if (n == "rate") return rate_text(rec_rate_hz(*x->rec));
            if (n == "input") return kIn[v];
            if (n == "dest") return kDestNames[v];
            if (n == "monitor") return v ? "ON" : "OFF";
            if (n == "pre") return std::to_string(v) + "MS";
            return v ? std::to_string(v) : "OFF";
        }
        case 'x': {
            std::string n = c.target;
            if (n == "opt") n = lk_opt_key(x);
            if (n == "rate" && lk_balance(x)) n = "balance";
            const int v = lk_cmd_value(x, n);
            if (n == "balance") return v == 0 ? "EQUAL" : (v < 0 ? "SRC " : "DEST ") + std::to_string(std::abs(v));
            names = n == "cmd" || n == "range" || n == "smooth" || n == "data";
            if (n == "data") return v ? "WITH DATA" : "PARAMS";
            if (n == "ws") {                         // the wavesample with its layer: L2 WS3
                if (!x->sampler || !x->sampler->instrument().waveSamples.count(v)) return v ? "WS " + std::to_string(v) + " --" : "--";
                const eps::Instrument& in = x->sampler->instrument();
                for (size_t l = 0; l < in.layers.size(); ++l)
                    for (uint8_t k : in.layers[l].keyMap)
                        if (k == v) return "L" + std::to_string(l + 1) + " WS" + std::to_string(v);
                return "WS " + std::to_string(v);
            }
            if (n == "cmd") return eps::waveCmdName(static_cast<eps::WaveCmd>(v));
            if (n == "range") return eps::waveRangeName(static_cast<eps::WaveRange>(v));
            if (n == "smooth") return kSmoothNames[v];
            if (n == "rate") return rate_text(eps::sampleRateOfIndex(eps::kRecordRateIndex[v]));
            if (n == "depth") {
                char b[16];
                std::snprintf(b, sizeof b, "%.1fDB", eps::scaleDepthDb(v));
                return b;
            }
            if (n == "zone" || n == "sstart" || n == "send") return std::to_string(v) + "%";
            return std::to_string(v);
        }
        default: return "";
    }
}

void lk_send(t_eps* x, const std::string& msg) {
    t_binbuf* b = binbuf_new();
    binbuf_text(b, msg.c_str(), msg.size());
    const int n = binbuf_getnatom(b);
    t_atom* v = binbuf_getvec(b);
    if (n > 0 && v[0].a_type == A_SYMBOL) eps_command_core(x, atom_getsymbol(v), n - 1, v + 1);
    binbuf_free(b);
}

void lk_cell_set(t_eps* x, const eps::PageCell& c, int v) {
    char b[96];
    switch (c.kind) {
        case 'c': std::snprintf(b, sizeof b, "n %s %d", c.target, v); break;
        case 'e': std::snprintf(b, sizeof b, "e %s %d", c.target, v); break;
        case 'r': std::snprintf(b, sizeof b, "rec %s %d", c.target, lk_from127(lk_rec_range(c.target), v)); break;
        case 'x': {
            std::string n = !std::strcmp(c.target, "opt") ? lk_opt_key(x) : c.target;
            if (n == "rate" && lk_balance(x)) n = "balance";
            if (n.empty()) return;
            if (n == "cmd") std::snprintf(b, sizeof b, "cmd %d", lk_from127(lk_cmd_range(x, n), v));
            else std::snprintf(b, sizeof b, "cmd %s %d", n.c_str(), lk_from127(lk_cmd_range(x, n), v));
            break;
        }
        default: return;
    }
    lk_send(x, b);
}

// ---- the display ------------------------------------------------------------------------

std::string lk_up(std::string s, size_t n) {
    for (char& ch : s) {
        if (ch == '_') ch = ' ';
        else if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 32);
    }
    if (s.size() > n) s.resize(n);
    return s;
}

// The small line: where the encoders are and what is selected.
std::string lk_context(t_eps* x) {
    const eps::Surface& S = *x->lk;
    const eps::Page& p = S.current();
    char b[64];
    if (S.mode == eps::Surface::Play) {
        static const char* const kPat[4] = {"00", "0*", "*0", "**"};
        std::snprintf(b, sizeof b, "%s %s %s", p.id, p.title, kPat[x->value[kC_patch] & 3]);
    } else if (x->sampler) {
        const int pos = x->editor->wsPosition(x->sampler->instrument());
        if (p.area == 7)
            std::snprintf(b, sizeof b, "%s ENV%d L%d WS%d", p.id, x->editor->env + 1, x->editor->layer + 1, pos);
        else
            std::snprintf(b, sizeof b, "%s %s L%d WS%d", p.id, p.title, x->editor->layer + 1, pos);
    } else {
        std::snprintf(b, sizeof b, "%s %s", p.id, p.title);
    }
    return lk_up(b, 21);
}

// Two lines of 10 from a longer text, broken at a space if there is one.
void lk_split(const std::string& t, std::string& a, std::string& b) {
    const std::string s = lk_up(t, 64);
    if (s.size() <= 10) { a = s; b.clear(); return; }
    size_t cut = s.rfind(' ', 10);
    if (cut == std::string::npos || cut == 0) cut = 10;
    a = s.substr(0, cut);
    b = s.substr(cut + (s[cut] == ' ' ? 1 : 0));
    if (b.size() > 10) b.resize(10);
}

void lk_screen(t_eps* x, int kind, const std::string& small, const std::string& b1, const std::string& b2,
               int ulFrom = -1, int ulLen = 0) {
    LkScreen& s = *x->scr;
    if (kind != 12) x->legendUntil = 0;           // another screen ends the pad legend
    s.b1 = lk_up(b1, 10);
    s.b2 = lk_up(b2, 10);
    s.ulFrom = ulFrom;
    s.ulLen = ulLen;
    lkg_pd_show(x->gfx, kind, lk_up(small, 21).c_str(), "");
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(spaces_to_underscore(lk_up(small, 21)).c_str()));
    SETSYMBOL(&a[1], gensym(s.b1.empty() ? "-" : spaces_to_underscore(s.b1).c_str()));
    SETSYMBOL(&a[2], gensym(s.b2.empty() ? "-" : spaces_to_underscore(s.b2).c_str()));
    outlet_anything(x->info, gensym("lkscreen"), 3, a);
}

bool lk_drawing(const t_eps* x) { return x->lkOn && x->gfx->on; }

void lk_show_page(t_eps* x) {
    if (!lk_drawing(x)) {
        out_display_text(x, x->lk->current().id, x->lk->current().title);
        return;
    }
    lk_screen(x, 11, lk_context(x), "", "");
}

void lk_show_home(t_eps* x) {
    if (x->lk->mode == eps::Surface::Edit) { lk_show_page(x); return; }
    if (!lk_drawing(x)) return;
    std::string a, b;
    lk_split(x->sampler ? x->sampler->instrument().name : std::string("NO INSTRUMENT"), a, b);
    const std::string folder = folder_text(*x->bfile, *x->bdir);
    static const char* const kPat[4] = {"00", "0*", "*0", "**"};
    lk_screen(x, 10, lk_up(folder, 17) + " " + kPat[x->value[kC_patch] & 3], a, b);
}

// A value changed (an encoder, the editor, a message): the long name and the value, or the
// picture. Returns false when the surface does not draw (then the text line as before).
bool lk_shown(t_eps* x, const char* name, const std::string& label, const std::string& value, int pic) {
    if (!lk_drawing(x)) return false;
    std::string lng = lk_up(label, 10);
    bool names = false;
    const eps::Page& p = x->lk->current();
    for (const eps::PageCell& c : p.cell)
        if (c.kind && !std::strcmp(c.target, name)) {
            lng = c.longName;
            lk_cell_text(x, c, names);
        }
    if (pic > 0) {
        x->legendUntil = 0;
        lkg_pd_show(x->gfx, pic, lk_up(std::string(p.id) + " " + lng + " " + value, 21).c_str(), "");
        return true;
    }
    const std::string v = lk_up(value, 10);
    lk_screen(x, 10, lk_context(x), lng, v, names ? 0 : -1, names ? static_cast<int>(v.size()) : 0);
    return true;
}

// The text line of eps~ (loading, saved, KEEP=OLD NEW ...) as a screen of the surface.
bool lk_text(t_eps* x, const std::string& title, const std::string& value) {
    if (!lk_drawing(x)) return false;
    if (value == "KEEP=OLD NEW" || value == "KEEP=OLD_NEW") {
        const bool old = x->wave->showingOld;
        lk_screen(x, 10, title, "KEEP?", "OLD NEW", old ? 0 : 4, 3);
        return true;
    }
    std::string a, b;
    if (value.size() > 10 || value.empty() || value == "-") {
        lk_split(value == "-" ? title : value, a, b);
        lk_screen(x, 10, value.empty() || value == "-" ? std::string() : title, a, b);
    } else {
        lk_screen(x, 10, std::string(), title, value);
    }
    return true;
}

// The level display of the SAMPLE page as a picture: the bar, the threshold dotted, the state.
void lk_meter(t_eps* x, float db) {
    if (!lk_drawing(x) || x->lk->area() != eps::kAreaSample) return;
    x->scr->db = db;
    const Recorder& R = *x->rec;
    char b[48];
    std::snprintf(b, sizeof b, "E18 %s THRS %d", rate_text(rec_rate_hz(R)).c_str(), R.thresh);
    x->legendUntil = 0;
    lkg_pd_show(x->gfx, 13, lk_up(b, 21).c_str(), "");
}

void lk_draw(t_eps* x, lkg_t* g, int kind) {
    const LkScreen& s = *x->scr;
    if (kind == 10) {                    // two big lines, the chosen option underlined
        lkg_text2(g, 2, 11, s.b1.c_str());
        lkg_text2(g, 2, 29, s.b2.c_str());
        if (s.ulFrom >= 0 && s.ulLen > 0) {
            const int x0 = 2 + 12 * s.ulFrom, x1 = std::min(LKG_W - 1, x0 + 12 * s.ulLen - 3);
            lkg_hdots(g, x0, x1, 45, 2);
            lkg_hdots(g, x0 + 1, x1, 46, 2);
        }
    } else if (kind == 11) {             // the encoders of the page: 2 x 4 boxes
        const eps::Page& p = x->lk->current();
        for (int i = 0; i < 8; ++i) {
            const int bx = (i % 4) * 32, by = 10 + (i / 4) * 19;
            const eps::PageCell& c = p.cell[i];
            if (!c.kind) continue;
            lkg_rect(g, bx, by, 31, 18);
            const char* t = c.kind == '-' ? "--" : c.shortName;
            const int w = 6 * static_cast<int>(std::strlen(t)) - 1;
            lkg_text(g, bx + (31 - w) / 2, by + 6, t);
        }
    } else if (kind == 14) {             // nothing: an encoder without a setting
    } else if (kind == 13) {             // the level: -57 .. 0 dB over the width
        const Recorder& R = *x->rec;
        const int w = std::clamp(static_cast<int>((s.db + 57.f) / 57.f * LKG_W), 0, LKG_W);
        lkg_rect(g, 0, 12, LKG_W, 16);
        if (w > 0) lkg_fill(g, 0, 12, w, 16);
        if (R.thresh > 0) {
            const int tx = std::clamp(R.thresh * LKG_W / 20, 1, LKG_W - 2);
            lkg_vdots(g, tx, 9, 31, 2);
        }
        char b[32];
        if (R.state == Recorder::Recording) std::snprintf(b, sizeof b, "REC %.1fS", R.len / R.recSr);
        else std::snprintf(b, sizeof b, "%s", R.state == Recorder::Armed ? "ARMED" : "LEVEL");
        lkg_text2(g, 2, 33, b);
    } else if (kind == 12) {             // the pads: top row, then bottom row, 4 a line
        const char* const* t = x->lk->mode == eps::Surface::Play ? eps::kPadsPlay : eps::kPadsEdit;
        for (int i = 0; i < 16; ++i) {
            const int line = i / 4, y = 9 + 9 * line + (line >= 2 ? 3 : 0);
            lkg_text(g, (i % 4) * 32, y, lk_up(t[i][0] ? t[i] : "-", 5).c_str());
        }
        lkg_hdots(g, 0, LKG_W - 1, 27, 2);
    }
}

// ---- encoders, pads, feedback ---------------------------------------------------------------

eps::Surface::Status lk_status(const t_eps* x) {
    eps::Surface::Status st;
    st.rec = x->rec->state;
    const t_eps::Wave& W = *x->wave;
    st.cmd = W.other ? (W.showingOld ? 3 : 2) : (x->jobkind == 2 && x->rec->waiting ? 1 : 0);
    if (x->jobkind == 2 && x->loader->state.load() != 0) st.cmd = 1;
    st.env = x->editor->env;
    return st;
}

int lk_channel(const t_eps* x) { return x->channel ? x->channel : 1; }

void lk_refresh(t_eps* x, bool force) {
    if (!x->lkOn) return;
    eps::Surface& S = *x->lk;
    const eps::Page& p = S.current();
    for (int i = 0; i < 8; ++i) {
        const eps::PageCell& c = p.cell[i];
        if (!c.kind || c.kind == '-' || c.kind == 'k' || c.kind == 'a') continue;
        const int v = lk_cell_get(x, c);
        if (!force && v == x->fbSent[i]) continue;
        x->fbSent[i] = v;
        const int cc = eps::Surface::ccOf(S.fwPage, i);
        if (cc < 0) continue;
        outlet_float(x->info, 0xB0 | ((lk_channel(x) - 1) & 15));
        outlet_float(x->info, cc);
        outlet_float(x->info, v);
    }
    int col[16];
    S.padColours(lk_status(x), clock_gettimesince(0), col);
    for (int i = 0; i < 16; ++i) {
        if (!force && col[i] == x->ledSent[i]) continue;
        x->ledSent[i] = col[i];
        outlet_float(x->info, 0x90 | ((x->padch - 1) & 15));
        outlet_float(x->info, 36 + i);
        outlet_float(x->info, col[i]);
    }
    if (S.armed(clock_gettimesince(0)) || x->legendUntil > clock_gettimesince(0)) clock_delay(x->lkclock, 100);
}

void lk_new_page(t_eps* x) {
    for (int& v : x->fbSent) v = -1;
    lk_show_page(x);
}

void lk_do(t_eps* x, const eps::Surface::Actions& acts) {
    for (const std::string& a : acts) {
        if (a == "@page") {
            lk_new_page(x);
        } else if (a == "@mode") {
            for (int& v : x->fbSent) v = -1;
            if (lk_drawing(x)) {
                lk_screen(x, 12, x->lk->mode == eps::Surface::Play ? "PADS  PLAY" : "PADS  EDIT", "", "");
                x->legendUntil = clock_gettimesince(0) + 1500.0;
            } else {
                out_display_text(x, x->lk->mode == eps::Surface::Play ? "PLAY" : "EDIT", x->lk->current().id);
            }
        } else if (a.rfind("@ask ", 0) == 0) {
            const std::string w = a.substr(5);
            if (lk_drawing(x)) lk_screen(x, 10, "PRESS AGAIN TO " + w, w + "?", "");
            else out_display_text(x, w + "?", "press again");
        } else if (a == "@cmdask") {
            const std::string c = eps::waveCmdName(static_cast<eps::WaveCmd>(x->wave->cmd));
            if (!lk_drawing(x)) out_display_text(x, c, "press CMD again");
            else {
                std::string a1, b1;
                lk_split(c, a1, b1);
                lk_screen(x, 10, "PRESS CMD AGAIN", a1, b1.empty() ? "RUN?" : b1);
            }
        } else {
            lk_send(x, a);
        }
    }
}

void eps_lk_tick(t_eps* x) {
    const double now = clock_gettimesince(0);
    if (x->legendUntil > 0 && now >= x->legendUntil) {
        x->legendUntil = 0;
        lk_show_home(x);
    }
    lk_refresh(x, false);
}

// An encoder of the Launchkey: true if the surface took it.
bool lk_encoder(t_eps* x, int cc, int v) {
    bool changed = false;
    const int cell = x->lk->encoder(cc, changed);
    if (changed) for (int& f : x->fbSent) f = -1;
    if (cell < 0) {                                  // an encoder with nothing on it: a black screen
        int pg = 0, enc = 0;
        if (!eps::Surface::ccPage(cc, pg, enc)) return false;
        if (x->lk->mode == eps::Surface::Play && pg > 1) return false;
        if (lk_drawing(x)) lk_screen(x, 14, "", "", "");
        return true;
    }
    const eps::PageCell& c = x->lk->current().cell[cell];
    if (c.kind == '-') {
        if (!lk_text(x, c.longName, "NOT YET")) out_display_text(x, c.longName, "not yet");
        return true;
    }
    if (c.kind == 'a') {                             // a message (copy / paste the envelope)
        lk_send(x, c.target);
        return true;
    }
    if (c.kind == 'k') {                             // a command: chosen on the COMMAND page
        lk_do(x, x->lk->setPage(eps::kAreas[eps::kAreaCommand].first));
        lk_send(x, std::string("cmd ") + c.target);
        if (lk_drawing(x)) {
            std::string a1, b1;
            lk_split(c.target, a1, b1);
            lk_screen(x, 10, "CMD TWICE: RUN IT", a1, b1);
        }
        return true;
    }
    lk_cell_set(x, c, v);
    if (c.kind == 'r' || c.kind == 'x') {          // their commands draw no screen of their own
        bool names = false;
        const std::string t = lk_up(lk_cell_text(x, c, names), 10);
        if (!lk_drawing(x)) out_display_text(x, c.longName, t);
        else lk_screen(x, 10, lk_context(x), c.longName, t, names ? 0 : -1, names ? int(t.size()) : 0);
    }
    x->fbSent[cell] = lk_cell_get(x, c);           // not echoed: the encoder is there already
    return true;
}

void lk_pad(t_eps* x, int note, bool down) {
    if (down && x->rec->learn) {                     // a pad ends PLAY LOW / HIGH KEY: the keys stay
        x->rec->learn = 0;
        x->rec->asking = false;
        out_display(x, "Keys", "kept");
        return;
    }
    lk_do(x, x->lk->pad(note, down, clock_gettimesince(0), lk_status(x)));
}

void lk_message(t_eps* x, int argc, t_atom* argv) {
    eps::Surface& S = *x->lk;
    if (argc >= 1 && argv[0].a_type == A_FLOAT) {
        x->lkOn = atom_getfloat(argv) != 0;
        if (x->lkOn) {
            lk_refresh(x, true);
            lk_show_home(x);
        }
        return;
    }
    if (argc < 1) return;
    const std::string w = atom_getsymbol(argv)->s_name;
    const int n = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : -1;
    if (w == "edit") {
        lk_do(x, S.setMode(n == 0 ? eps::Surface::Play : eps::Surface::Edit));
    } else if (w == "mode") {
        lk_do(x, S.setMode(S.mode == eps::Surface::Play ? eps::Surface::Edit : eps::Surface::Play));
    } else if (w == "page" && argc > 1 && argv[1].a_type == A_SYMBOL) {
        const char* id = atom_getsymbol(argv + 1)->s_name;
        for (int i = 0; i < eps::kPageCount; ++i)
            if (!strcasecmp(eps::kPages[i].id, id)) lk_do(x, S.setPage(i));
    } else if (w == "legend") {
        lk_do(x, {"@mode"});
    } else if (w == "sync") {
        lk_refresh(x, true);
        lk_show_home(x);
        return;
    } else if (w == "cc" && argc > 2) {          // lk cc <cc> <value>: as from an encoder
        lk_encoder(x, n, static_cast<int>(atom_getfloat(argv + 2)));
    } else if (w == "pad" && argc > 2) {
        lk_pad(x, n, atom_getfloat(argv + 2) > 0);
    } else if (w == "status") {
        t_atom a[3];
        SETSYMBOL(&a[0], gensym(S.mode == eps::Surface::Play ? "play" : "edit"));
        SETSYMBOL(&a[1], gensym(S.current().id));
        SETFLOAT(&a[2], x->lkOn);
        outlet_anything(x->info, gensym("lk"), 3, a);
        return;
    } else {
        pd_error(x, "eps~: lk: no method '%s'", w.c_str());
        return;
    }
    lk_refresh(x, false);
}
