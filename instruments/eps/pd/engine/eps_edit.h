// eps_edit — the instrument editor of eps~: reads and changes the parameters of the
// loaded instrument (instrument header, the selected layer, wavesample and envelope).
// The table of parameters comes from tools/eps_edit_params.py (eps_edit_params.h).
// Values travel as 0..127 (a pot, a slider) like the controls of eps~; the text
// shows the value as on the EPS.
#pragma once

#include <vector>
#include "eps_loader.h"
#include "eps_edit_params.h"

#include <cstdint>
#include <string>

namespace eps {

class Editor {
public:
    // Selection: layer position in Instrument::layers (0-based), wavesample number,
    // envelope 0 = ENV1 (pitch), 1 = ENV2 (filter), 2 = ENV3 (amp).
    int layer = 0, ws = 1, env = 2;
    bool follow = true;     // select the wavesample under the last key played
    bool zeroCross = true;  // sample and loop points snap to zero crossings

    static int find(const char *name);           // -1 = no such parameter
    // Clamp the selection to what the instrument has (after a load).
    void fit(const Instrument &in);
    // The wavesamples of the selected layer in key order (as on the EPS: WS 1..n of a layer).
    std::vector<int> layerWaveSamples(const Instrument &in) const;
    int wsPosition(const Instrument &in) const;   // 1-based within the layer, 0 = not in it
    void selectWs(const Instrument &in, int pos); // 1-based within the layer
    void stepWs(const Instrument &in, int d);
    // Select layer / wavesample for a played key; true if the selection changed.
    bool followKey(const Instrument &in, int key, int patchMask);

    int get127(Instrument &in, int id) const;     // the value as 0..127
    std::string text(Instrument &in, int id) const;   // one word
    void set127(Instrument &in, int id, double v);    // change the instrument

    // The selection as text: "A0-A3 root B2 25000Hz 51208" (spaces as '_').
    std::string selectionInfo(const Instrument &in) const;

private:
    int value(Instrument &in, int id) const;      // in the parameter's own units
    void setValue(Instrument &in, int id, int v);
    Layer *layerP(Instrument &in) const;
    WaveSample *wsP(Instrument &in) const;
    Envelope *envP(Instrument &in) const;
    uint32_t point(const WaveSample &w, int i) const;
    void setPoint(Instrument &in, WaveSample &w, int i, int64_t pos);
    uint32_t anchor_[5] = {0, 0, 0, 0, 0};        // coarse position of each point (4: loop position)
};

} // namespace eps
