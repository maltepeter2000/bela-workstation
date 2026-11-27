#!/usr/bin/env python3
"""Generate the help patch of eps.i~ and eps~ (one patch, written under both names).
Never edit the help patch by hand: change this script and run it again. Names, ranges and
CC pages come from tools/eps_params.py, the drawing helpers from common/tools/helpdraw.py.

    python3 tools/gen_eps_help.py       writes pd/eps.i~-help.pd and pd/eps~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import ctlayer                                    # noqa: E402
import helpdraw as H                              # noqa: E402
import eps_params as T                            # noqa: E402
import gen_eps_editor as E                        # noqa: E402  (rows and labels of the editor)

OBJ = "eps.i~"
FILES = ("eps.i~-help.pd", "eps~-help.pd")
TITLE = "eps.i~ and eps~"
SUBTITLE = "Ensoniq EPS / EPS-16 PLUS / ASR-10 instruments"

CC = ctlayer.cc_map(T.CC_ROWS)


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


OBJECT = [
    "eps.i~ is the whole instrument in one object: eps~, the editor, the Launchkey surface "
    "(encoders, pads and display, see Launchkey), MIDI in and out. Put it in a patch, put your Ensoniq "
    "instruments into a folder eps beside the patch, connect the two left outlets to dac~ "
    "and play. Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the channel of the pads "
    "(0 or none = 10, the Drums Channel), argument 3 the library folder (relative to the "
    "patch's folder or ~/..., e.g. [eps.i~ 1 10 ~/Documents/Pd/eps-library]). Several eps.i~ do not get in each other's way, "
    "each has its own editor and plays its own instrument.",
    "eps.i~ samples Pd's audio input 1 (input in the SAMPLE band: none, 1, 2 or both) and "
    "whatever comes into its inlet 2: record new wavesamples with the SAMPLE band of the "
    "editor, see Sampling.",
    "eps~ is the bare engine inside, for patches of your own: inlet 2 takes the velocity of "
    "a note sent as a float, inlet 3 raw MIDI bytes from midiin, inlet 4 the audio to "
    "sample, and the floats of outlet 3 "
    "(MIDI thru) go to midiout. eps~ alone takes the library beside the external, or the "
    "folder given with library. Open eps.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of eps.i~. Every change shows up in it, from the controller, "
    "an instrument change or messages. refresh asks for all values again.",
    "Rows of 8 below the PATCH band, titled as in Controls: row k is page k of the "
    "controller layer. Each cell: name and label, a slider and a number box with the value as "
    "from a pot (0 to 127), the box beside it the value itself.",
    "PATCH band. name: the instrument that plays. library N: load instrument N of the "
    "whole library. reload: the instrument again, edits dropped. init: all controls back to "
    "0, the instrument stays. rescan: read the library folder again after adding files.",
    "folder: where you are, as on the EPS: library is the top (disk images and EFE files), "
    "an image and its directories below (DISK1.img/BASSES). up: one folder up. file: the "
    "selected file with the arrows before and after it, a directory ends in a slash. open: "
    "enter the directory or load the instrument. Loading an instrument shows its folder.",
    "Saving always writes the Ensoniq format, so the EPS reads it. The offsets of the "
    "controls (cutoff, attack ...) go into the instrument first and are set back to 0. "
    "replace: over the file the instrument was loaded from. save copy: a new file in the "
    "folder shown, named after the instrument with a number. new name and save as: a new "
    "file of that name in the folder shown (in an image, or an EFE at the top). new folder: "
    "a directory of that name, inside an image. Names: up to 12 characters, A-Z 0-9 space * "
    "- + (what the EPS lets you type), underscore for a space. See Library and saving.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the controller feedback. "
    "thru: see Messages, thru. size: files in the folder, library size: instruments in all.",
    "The other rows edit the loaded instrument itself (mouse only, see Instrument rows). "
    "SELECT band: the layer and the wavesample the LAYER and WS rows show, with their names "
    "and the key range, root key, rate and length of the wavesample. envelope: which one the "
    "ENV rows show (1 pitch, 2 filter, 3 amp). follow key (on by default): a played key "
    "selects its layer and wavesample, the rows jump to their values, and when several layers "
    "play the key, the layer chosen stays and its wavesample follows. zero crossings (on by "
    "default): sample and loop points move to the nearest rising zero crossing.",
    "SAMPLE band: the wave recorder, see Sampling. rate with its arrows, threshold, pre "
    "ms, input (Pd's audio input: none, 1, 2, 1+2), into (new WS, replace WS, new layer, new instrument), level: the level display "
    "(the bars and the star of the threshold), rec and stop, then the state and what it "
    "says. root key, low key, high key: the next key you play sets it for the selected "
    "wavesample. monitor: hear the input, on or off whatever the recorder does (off by default, "
    "with a microphone it feeds back).",
    "COMMAND band: the wavesample commands of the EPS for the selected wavesample, see "
    "Wavesample commands. command with its arrows, range (sample, loop, start to loop, loop "
    "to end), do, then old and new to compare, keep or cancel. depth, zone %, scale % (start "
    "and end), rate, smooth, copy to layer and with data are the values the commands take.",
    "Sample and loop points have two cells each: the first sets the position coarsely, in "
    "steps of 1/127 of the sample, the second (fine) moves it by -64 to +63 samples from "
    "there. Loop pos moves loop start and end together, the loop keeps its length "
    "(and pitch), as LOOPPOS on the EPS, and shows the loop start. The box shows the sample number. Edits change the playing instrument at once "
    "(the next note, filter, amp, pan, LFO and wave modulation also sounding notes). They "
    "are kept with replace, save copy or save as, a reload or an instrument change drops "
    "them.",
]

INLETS = [
    "eps.i~ 2, eps~ 4 signal: the audio to sample.",
    "1 float: note number, velocity 100.",
    "1 list: note, velocity and, if you like, the MIDI channel.",
    "  MIDI in eps.i~: notes, bend, pressure (channel and poly),",
    "  program change, CC 1 wheel, 2 XCTRL, 4 pedal, 7 volume,",
    "  64 sustain.",
    "1 anything: a message, see Messages.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: info, name, 0-127 and value text for every",
    "  change, the instrument that plays, its name, the channels.",
    "3 loaded model layers wavesamples: an instrument is in.",
    "3 display title value: two lines for a display: a control",
    "  (Cutoff, -12), the instrument (Preset 3, CLASSIC_PAD),",
    "  Loading, saving (Saved, PAD_BRIGHT).",
    "3 folder text n, bsel k n kind name: the folder shown,",
    "  its size and the selected file (kind dir or inst).",
    "3 saved: an instrument was written.",
    "3 lkgfx bytes: a picture for the Launchkey display (envelope,",
    "  filter, LFO). lkgfx 0: off, the text line stays.",
    "3 recstate off / level / armed / recording / rootkey, recinfo",
    "  text, recmeter text (level display), recset rate / thresh /",
    "  pre / dest: the recorder. sampled ws layer: a recording is",
    "  in the instrument.",
    "3 cmdstate idle / busy / new / old, cmdinfo text, cmdset",
    "  name value text: the wavesample commands.",
]

MESSAGES = [
    "name v: set a control, see Controls.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "cycle name [-1]: the next (previous) value, round and round.",
    "dump: every control as info.",
    "e name v: set an instrument parameter (see Instrument",
    "  rows) from 0 to 127. einfo name: report it as info.",
    "edump: the selection (sel) and every instrument parameter.",
    "sel layer N, sel ws N, sel env N (1-3): select what the",
    "  LAYER, WS and ENV rows edit; sel layerstep / wsstep +-1.",
    "  ws N counts within the layer (WS 1..n of it, as on the EPS).",
    "  Answers sel <layer> <ws> <env> <layers> <layer name>",
    "  <ws name> <info> <wavesamples in the layer>.",
    "follow 0/1: a played key selects its layer and wavesample.",
    "zerox 0/1: points snap to zero crossings.",
    "library [dir]: use this folder as the library and read",
    "  its list. Without dir: eps beside the external.",
    "libset <dir>: the library folder (relative to the patch's",
    "  folder or ~/...), kept in eps-library.txt beside the patch.",
    "libhome <dir>, libarg <dir>: from eps.i~ at load (the folder",
    "  eps beside the patch, argument 3). Answer: libdir <dir>.",
    "rescan: read the library folder again.",
    "preset n: load library instrument n (from 0), controls",
    "  back to 0.",
    "presetstep 1 or -1: the next or previous instrument.",
    "open file [k]: load instrument k (from 0) of any file,",
    "  without the library. A name without a path is beside",
    "  the external.",
    "presets: list the library as preset n name.",
    "browse: report the folder shown (folder, bsel).",
    "bstep 1 or -1, bselect k: move the selection in the folder.",
    "benter: enter the selected directory or load the selected",
    "  instrument. bup: the folder above.",
    "userstep 1 or -1: the next or previous instrument in the",
    "  folder of the one playing.",
    "nav step 1 or -1: the next or previous entry of the folder",
    "  shown (an instrument loads, a folder is only shown). nav",
    "  open: into the folder shown. nav up: one level up (the",
    "  Launchkey pads > < OPEN EXIT).",
    "fx 0 or 1: the effect off (dry) or on. fx: the effect and its",
    "  parameters (outlet: effect name rate count, then fxparam",
    "  k name lo hi value item for each).",
    "fxparam k value: parameter k of the effect.",
    "delete: delete the entry selected in the folder shown (an",
    "  instrument, an empty folder, an EFE file).",
    "init: every control back to 0, the instrument stays.",
    "reload: the instrument again, edits dropped.",
    "saveas name: write the instrument as a new file into the",
    "  folder shown: into the image, or name.efe at the top.",
    "  Refused if the name is there.",
    "replace: write it over the file it was loaded from.",
    "save: saveas with a free name, the instrument's and a",
    "  number (the pads' save).",
    "mkdir name: a new directory in the folder shown (images).",
    "SysEx with a real EPS-16 PLUS (MIDI both ways, SYS-EX ON in",
    "  its EDIT / System-MIDI page):",
    "epsch n: the EPS's MIDI base channel, 1-16 (default 1).",
    "fetch i: instrument i (1-8) from the EPS's memory into the",
    "  folder shown as a new file, then loaded. The effect does",
    "  not come along (SysEx cannot carry it).",
    "fetchws i l w: wavesample w of layer l as name.wav into the",
    "  library folder.",
    "send i: the instrument that plays into the EPS's empty",
    "  instrument i (1-8; delete it there first). Copies share",
    "  their data as in the file. The effect stays the EPS's.",
    "sendws i l w file.wav: the WAV (library folder) into",
    "  wavesample w of layer l, which must exist. One that has",
    "  copies keeps its old length (the EPS will not cut it).",
    "sxcancel: stop a transfer. Outlet 3: sysex step percent,",
    "  fetched, sent.",
    "  All three write the offsets into the instrument first.",
    "The wave recorder (see Sampling):",
    "rec rate n (0-6: 44.6 29.8 22.3 17.9 14.9 12.8 11.2 kHz),",
    "  rec ratestep 1 or -1.",
    "rec thresh n: 0 (record at once) to 19 (-54 to 0 dB).",
    "rec pre ms: pre-trigger, 0 to 127 ms (default 15).",
    "rec dest n: 0 a new wavesample in the selected layer, 1",
    "  replace the selected wavesample, 2 a new layer, 3 a new",
    "  instrument.",
    "rec level 0/1: the level display (recmeter) without",
    "  recording.",
    "rec start: record (from the threshold). rec stop: stop, then",
    "  the next key is the root key. Stop again: cancel.",
    "rec root n: the root key without playing it.",
    "rec learn root / lo / hi: the next key played sets the root",
    "  key / lowest / highest key of the selected wavesample.",
    "rec monitor 0/1: hear the input (on / off, always).",
    "lkgfx 0/1: the Launchkey pictures off / on (default on).",
    "env copy / env paste: the selected envelope of the selected",
    "  wavesample into a buffer / from it (E16 COPY and PSTE).",
    "lk 0/1: the Launchkey surface off / on (eps.i~: on).",
    "lk edit [0/1], lk mode: EDIT / PLAY. lk page <P1 .. E19>.",
    "lk legend: show the pads. lk sync: colours and values again.",
    "lk pad <note> <velocity>, lk cc <cc> <value>: as from the",
    "  Launchkey. padchannel n: the pads' channel (default 10).",
    "lk status: answers lk <play|edit> <page> <on>; lkscreen",
    "  <small> <line 1> <line 2>: what the display shows.",
    "rec input n: eps.i~ records Pd's audio input 0 none, 1 in 1",
    "  (default), 2 in 2, 3 both, inlet 2 always.",
    "rec status: report the recorder's settings.",
    "The wavesample commands (see Wavesample commands):",
    "cmd name or cmd n: choose one (truncate crossfade",
    "  reverse_xfade ensemble_xfade bowtie_xfade convert_rate",
    "  normalize fade_in fade_out volume_smooth scale_data",
    "  reverse_data invert_data clear_data new_ws copy_ws",
    "  delete_ws new_layer copy_layer delete_layer copy_params",
    "  bidir_xfade loop_longer synth_loop copy_data add_data",
    "  replicate_data mix merge splice),",
    "  cmd step 1 or -1.",
    "cmd range n: 0 sample, 1 loop, 2 start to loop, 3 loop to end.",
    "cmd depth n (0-5: 3.0 3.5 4.0 4.5 5.5 6.0 dB), cmd zone %,",
    "  cmd sstart %, cmd send % (scale data, 0-200), cmd rate n",
    "  (0-6, convert), cmd smooth n (0-4), cmd layer n, cmd data",
    "  0/1 (copy with its data), cmd ws n (copy_params: to WS n).",
    "cmd do: run it on the selected wavesample. cmd old, cmd new:",
    "  hear before and after. cmd keep, cmd cancel: decide.",
    "cmd status: report the settings.",
    "bend f: pitch wheel, -1 to 1, times bend.",
    "mod f: modulation wheel, 0 to 1 (EPS source WHEEL).",
    "pressure f: channel pressure, 0 to 1 (PRESS).",
    "polypressure note f: pressure of one key (PRESS).",
    "pedal f, xctrl f: the sources PEDAL and XCTRL, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback, 1 to 16.",
    "thru 0/1: MIDI thru, on by default.",
    "status: report instrument, channels, library size and",
    "  sounding voices.",
    "stop, reset: all notes off.",
]

PRESETS = [
    "Where the library is: argument 3 of eps.i~ if there is one, else the folder chosen "
    "with choose in the editor (kept in eps-library.txt beside the patch, so it stays when "
    "the patch is opened again), else the folder eps beside the patch. Keep it apart from "
    "the folder with eps.i~ itself, so an update of eps never touches it.",
    "The library holds Ensoniq disk images (IMG from a floppy, "
    "HDA or IMG images of the hard disk on the SD card of a ZuluSCSI or BlueSCSI, EDE, EDA, "
    "GKH) and EFE files. The editor shows it as on the EPS: each image is a folder with its "
    "directories, the EFE files lie at the top. preset N, presetstep and program changes "
    "go through all instruments, in the order of the file names and the directories. An "
    "instrument is read in the background: notes go on sounding until it is in.",
    "Saving keeps the Ensoniq format: into an image the file goes as the EPS writes it "
    "(checked against the original operating system: it lists and loads what eps~ saved), "
    "at the top level as an EFE. So your library can go back onto the SD card of the "
    "ZuluSCSI as it is. A directory holds 38 files, names have 12 characters.",
    "Before eps~ writes into an image it checks it: the free block count must match the "
    "FAT and every directory its file count, otherwise it refuses and changes nothing. Every "
    "block it changes is first copied to <image>.undo beside the image: build/eps_fs_tool "
    "undo <image> puts the image back as it was before the first change (and deletes the "
    "journal). Delete the .undo file once you are happy, and keep a copy of your SD card "
    "anyway.",
    "There is one library in Pd, not one per object: two eps.i~ in one patch share it. "
    "Save the patch first, an unsaved patch has no folder of its own.",
    "The controls of eps~ are offsets over the instrument while you play. Saving writes "
    "transpose, tune, cutoff, filter 2, filter env, attack, decay and release into the "
    "instrument (into every wavesample) and sets them back to 0. vibrato, wave position, "
    "bend range, patch select and vibrato rate are playing controls, the EPS has no place "
    "for them in an instrument: they stay. volume and voices belong to the instrument "
    "object.",
]

SAMPLING = [
    "As on the EPS (SAMPLE, Musician's Manual section 5): the audio in becomes a wavesample "
    "of the instrument that plays. eps.i~ takes Pd's audio input (input in the SAMPLE band, "
    "1 by default) plus its inlet 2, eps~ its inlet 4. Pd's audio must be on (DSP) with an "
    "input device chosen in the audio settings, on a Mac Pd needs the permission for the "
    "microphone (System Settings, Privacy). level shows whether anything comes in. There is "
    "no waveform display: set the points in the WS POINTS row afterwards, by ear.",
    "Choose the rate (the seven of the EPS-16 PLUS, 44.6 to 11.2 kHz: lower rates hold "
    "longer sounds, they are band-limited as by the EPS's input filter) and where the "
    "recording goes: new WS (a new wavesample in the layer selected in the SELECT band), "
    "replace WS (the selected wavesample: its keys and parameters stay, its loop too if it "
    "fits), new layer (up to 8), new instrument (UNNAMED INST, as the EPS makes it). The "
    "recording uses the free sample memory of the EPS-16 PLUS (1024K words with the "
    "instrument): recinfo shows the seconds left.",
    "level shows the input as bars, the star is the threshold. threshold 0 records at once "
    "on rec, 1 to 19 (-54 to 0 dB in steps of 3) waits until the input crosses it, pre ms "
    "keeps that much from before (10 to 20 ms are usually right). stop ends the recording "
    "(full memory too), then the editor says PLAY ROOT KEY: the next key you play is the "
    "key at which the recording plays back as recorded, it does not sound. Then PLAY LOW KEY "
    "and PLAY HIGH KEY: the next two keys set the range of the new wavesample, a Launchkey pad "
    "(or rec learn 0) keeps the keys it got.",
    "Key mapping as on the EPS: a new wavesample gets the keys between its root key and "
    "the root keys of its neighbours in the layer, split at the midpoints (the whole "
    "keyboard when it is the first). Change it with root key, low key and high key (the "
    "next key played sets it for the selected wavesample) or the cells WS key low / high: "
    "keys it gives up go to its neighbour.",
    "The recording is in the instrument that plays, not yet on disk: save it with replace, "
    "save copy or save as (a new instrument with save as or save copy). The file is the "
    "EPS's own: the original operating system loads and plays what eps~ recorded.",
]

COMMANDS = [
    "The commands of the EPS's COMMAND/Wave, LFO and Amp pages (Musician's Manual section 6) "
    "for the wavesample selected in the SELECT band. After do the result plays at once, as "
    "KEEP=OLD NEW on the EPS: old and new switch between before and after, keep takes the "
    "new one, cancel the old one. Editing, loading, saving or the next command keep what "
    "plays. The algorithms follow the manual's descriptions, the results are alike, not the "
    "same bits.",
    "truncate: cuts the data before sample start and after sample end (not for a wavesample "
    "with copies, as on the EPS). crossfade: the end of the loop fades into the data before "
    "loop start (zone: % of the loop, depth: 3 dB smooth to 6 dB linear). reverse_xfade: into "
    "the data after loop start, backwards. ensemble_xfade: the whole loop. bowtie_xfade: "
    "around both loop points, zone up to 50 %, needs data after loop end. The messages "
    "INCREMENT LOOPSTART and DECREMENT LOOPEND say there is not enough data before or after "
    "the loop. convert_rate: to one of the seven rates, the pitch stays, all points move.",
    "normalize: the whole data to full level. fade_in, fade_out, scale_data (from scale % "
    "start to end, the depth curve), reverse_data, invert_data, clear_data, volume_smooth "
    "(evens out the level, smooth from very fine to very coarse, at least 256 samples): over "
    "the range. Data commands change the copies that share the data.",
    "new_ws: a small square wave on the keys around C4 in the selected layer. copy_ws: a copy "
    "into copy to layer on the keys the original has (params only: it plays the same data, "
    "with data: its own). delete_ws: not if copies use its data.",
    "new_layer: a new layer (up to 8) with a small square wave on all keys, in patch 00. "
    "copy_layer: the selected layer into a new one, its wavesamples as copies (with data: "
    "their own), in the same patches. delete_layer: the selected layer and its wavesamples, "
    "not the last layer and not if another layer plays copies of them, the patches follow. "
    "copy_params: all parameters of the selected wavesample to WS n (to WS in the editor, "
    "encoder 8 on the Launchkey), not its data, points, loop mode, root key, tuning, rate and "
    "keys. All of them can be heard with old / new before keep.",
    "These follow the descriptions in the EPS-16 PLUS External Command Specification (not "
    "measured in the OS): bidir_xfade makes both loop ends symmetric over the fade zone (at "
    "most 50 % of the loop) for a loop that plays forward and back. loop_longer (Make Loop "
    "Longer): after the loop comes the loop reversed, the faded-out end and the faded-in "
    "reversed copy overlap over the fade zone, the loop becomes twice as long less the zone. "
    "synth_loop: the loop made again from its spectrum with random phases, smooth 0 to 4 "
    "smooths the spectrum (at most 65536 samples).",
    "copy_data / add_data: the range of the selected wavesample written over / added to WS n "
    "from its sample start (to WS, add with clip prevention: the sum scaled to fit). "
    "replicate_data: the range repeated after itself up to the end of the data. mix (Mix "
    "Wavesamples): the selected one, the source, mixed into WS n, balance -127 all source, 0 "
    "half each, 127 all destination. merge: the source, then WS n, crossfaded over the fade "
    "zone (% of the shorter one, before the loop of WS n), into WS n, balance -127 to 127 "
    "turns the destination or the source down. splice: the same without a fade. mix, merge "
    "and splice need the same rate, root key and tuning (else PITCH DIFFERENCE).",
    "alf (row MORE, E5 ALF on the Launchkey): AUTO-LOOP FINDING as on the EPS, on by default. "
    "A moved loop start or end goes to the place nearby where the loop joins best.",
]

ABOUT = [
    "eps~ reads the instruments of the Ensoniq EPS, EPS-16 PLUS and ASR-10 from their own "
    "files: layers, wavesamples with their samples, loops and loop modes, envelopes, filters, "
    "pan, patch select and the wave modulation (transwave). It plays them on a model of the "
    "ES5505 voice chip of the EPS-16 PLUS, taken from MAME: interpolation, the 4-pole filter "
    "in its four arrangements, the 8-bit logarithmic volume, forward, backward and "
    "bidirectional loops, the 2/1024 pitch steps.",
    "What the EPS operating system does is rebuilt on top: voice allocation, layers and patch "
    "select, the three envelopes (pitch, filter, amp) with the two-stage release, attack "
    "velocity, keyboard time scaling and the finite mode, the LFO with its seven waves, delay, "
    "reset and humanize, random pitch, the modulation of pitch, both filters, amp and pan from "
    "the 15 sources (LFO, RANDM, ENV1, ENV2, PR+VL, VEL, VEL1, VEL2, KBD, PITCH, WHEEL, PEDAL, "
    "XCTRL, PRESS, WL+PR, numbered as in the External Command Specification), crossfades, layer glide and all seven wave modulation types "
    "(loop position, sample start, both, loop start, loop end, loop start-X, transwave).",
    "Calibrated against the original operating system (OS 1.30 in an emulator, register "
    "traces of test instruments): cutoff scale of both filters, volume and envelope level law, "
    "pan law, boost, envelope times and shapes, release, LFO speeds and depth, wave "
    "modulation types and range, the pitch, "
    "filter and key amounts, the mod sources 0-11. Still approximations: the soft/hard "
    "velocity levels, amp and pan modulation amounts, crossfade curves, glide (every mode is "
    "mono with glide). Not there: pitch tables. The controls of "
    "eps~ are offsets over the instrument, 0 plays it as stored.",
    "Effects: the instrument's effect runs as on the EPS-16 PLUS, with the ROM and OS of your "
    "EPS: put eps16plus-rom.bin (the ROM, 128 KB, both chips as one file) and eps16plus-os.efe "
    "(the OS 1.30 file) into the library. A 68000 runs the effect's own driver code with the "
    "helpers of ROM and OS and an ES5510 (ESP) the effect program at the effect's rate (29.8, "
    "44.6 or 78.1 kHz). The wavesamples go to the effect by their output bus, bus 3 (aux) stays "
    "dry. Without the two files eps~ plays dry. Mod sources of the effect: bend, wheel, xctrl, "
    "pedal, sustain, pressure. Parameter changes are not yet saved with the instrument.",
    "The output runs at Pd's sample rate, not at the 29.76 kHz of the EPS-16 PLUS. Up to 32 "
    "voices, 20 by default.",
]

DATA = [
    "Building needs no data. To play, put Ensoniq instruments into the folder eps beside your "
    "patch: single files (.efe from EpsLin, EnDiskEx or Chicken Translator) or whole disk "
    "images (.img from a floppy, an SD card of a ZuluSCSI or BlueSCSI, .ede and .eda from "
    "Giebler's tools, .gkh). HFE floppy images are converted to IMG first. The EPS, the "
    "EPS-16 PLUS and the ASR-10 share the format, tested so far with EPS-16 PLUS instruments.",
    "For eps.i~ two files are enough: eps~ (the external) and eps.i~.pd in a folder on Pd's "
    "search path.",
]

CREDITS = ("Credits: Modelled on the Ensoniq EPS-16 PLUS; ES5505 after MAME's es5506.cpp by "
           "Aaron Giles; file format after ConvertWithMoss by Juergen Mossgraber with help by "
           "Claude")

import eps_pages as PG                             # noqa: E402


def page_line(p):
    pid, title, area, cells = p
    names = " ".join(c[0] if c[2] != "-" else c[0] + "(-)" for c in cells if c)
    return f"{pid} {title}: {names}"


LAUNCHKEY = [
    "eps~ drives the Launchkey Mini MK4 itself (custom firmware, pads in Drum mode): the "
    "encoders, the pads and the display follow two layers, PLAY and EDIT. The pads have fixed "
    "meanings, the display shows what they do. The bottom right pad 51 is MODE in both layers: "
    "pressed, it switches PLAY and EDIT, blue in PLAY, purple in EDIT. On the switch the display shows the pads for 1.5 s.",
    "PLAY pads, as on the EPS: the browser starts at the top level (the library, the images "
    "as folders). 36 > and 44 < (cyan) go through the folder shown, nothing loads. 42 OPEN "
    "(green, bright on a folder): a folder: into it, a patch: load it. 50 EXIT (yellow): one "
    "level up. After each step the display shows where you are (the folder, what is "
    "selected). Also: 38 INIT (twice), 39 PAT> 47 PAT< (patch select, pink), 41 SMPL (red), "
    "46 SAVE (SAVE TO?: go to the folder with > < OPEN EXIT, SAVE again: a copy with a free "
    "name there, another pad: no), 48 SEL (held + OPEN: DELETE? the entry shown, SEL: yes, "
    "another pad: no, a folder only when empty), 51 MODE. The encoders: CC page 1 of the "
    "Launchkey is P1 PERFORM, page 2 is P2 PLAY (the same CC numbers as the controller layer "
    "of the Bela).",
    "EDIT pads, top row: the areas INST LAYER WAVE PITCH FILTER AMP LFO ENV (pressed again: "
    "the next page of the area), each in its own colour: white, yellow, cyan, pink, lime, "
    "amber, sky, magenta, bright while its page is shown. Bottom row: 44 SEL (held: LAY> WS> ENV# step backwards, a key "
    "selects its layer and wavesample without sound), 45 LAY> (next layer), 46 WS> (next "
    "wavesample of the layer), 47 ENV# (ENV1 > ENV2 > ENV3), 48 O/N, 49 CMD, 50 SMPL, 51 MODE. "
    "The keys play, follow key selects. The encoders act on the page shown, whatever CC page "
    "the Launchkey is on: encoder n is cell n.",
    "SMPL: the SAMPLE page E18 and the level display, pressed again: record (from the "
    "threshold), again: stop, then play the root key. CMD: the COMMAND page E19, the command "
    "chosen with encoder 1, pressed twice: run it. Then O/N plays the old or the new version "
    "(orange / green), CMD (now green) keeps the new one, anything else keeps what plays.",
    "The display: one small line (page, layer, wavesample), then two big lines of 10, the "
    "chosen option underlined, or a picture (envelope, filters, LFO). After a page change it "
    "shows the names of the 8 encoders. The values of the page go back to the encoders as CC "
    "(the firmware takes them). Pad colours: white = chosen, dimmed = there, dark = no "
    "function, red = sampling (flashing while recording), orange = old, green = new.",
    "The pages, encoders 1 to 8 ((-) = not there yet): " + " | ".join(page_line(p) for p in PG.PAGES) + ".",
    "The bare eps~ has the surface off (lk 1 switches it on). epscc and epspads, the controller "
    "layer and the pads of the other instruments, stay for patches of your own and the Bela.",
]

CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as the row there, page k "
    "on the controller is row k. Each line: name (the label in the editor), range, CC number "
    "and what it does. Page 1 is P1 PERFORM of the Launchkey: the two filters, the filter "
    "envelope and the amp envelope. Page 2 is P2 PLAY: volume, transpose, wave position, "
    "filter envelope. The row MORE has no CC: the editor and the pads (patch select).",
]

HELP = {nm: T.CONTROLS[nm][6] for nm in T.CONTROLS}


def control_line(nm):
    lo, hi = rng(nm)
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {lo}..{hi}{cc}: {HELP[nm]}"


import eps_edit_params as EP                       # noqa: E402

EDIT_INTRO = [
    "The rows below the controls edit the instrument (mouse only, message e). Each line: "
    "name (label) and range of the value as stored, what it is.",
]


def edit_line(nm):
    _, lbl, scope, _, lo, hi, kind, hlp = EP.P[nm]
    rng_ = "0..127" if scope in "pf" else f"{lo}..{hi}"
    return f"{nm} ({lbl}) {rng_}: {hlp}"


def main():
    rows = [(f"{title}  {E.cc_text([n for n in names if n])}".replace(":", ""), [n for n in names if n])
            for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", LAUNCHKEY),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Library and saving", PRESETS),
                      ("text", "Sampling", SAMPLING),
                      ("text", "Wavesample commands", COMMANDS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line),
                      ("controls", EDIT_INTRO, [(t, [n for n in ns if n]) for t, ns in EP.ROWS_LEFT + EP.ROWS_RIGHT],
                       edit_line, "Instrument rows")])


if __name__ == "__main__":
    main()
