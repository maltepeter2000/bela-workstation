#!/usr/bin/env python3
"""Colours shared by the help patches (gen_help.py, gen_juno_help.py) and the editors
(gen_juno_editor.py ...), so help and editor of every instrument look alike.

    YELLOW  the writing surface of the help, and every second row of an editor
    ROW     the other rows of an editor (a step darker than YELLOW)
    PINK    the strip on the left of the help
    BLUE    the panel on the right of the help, the heading squares, the row titles
    PATCH   the band of an editor with name, presets, user bank and MIDI (a light BLUE)
    SETUP   rows of an editor that belong to the instrument, not to the patch (two tones);
            also the second kind of row that follows a selection (d50~: the TONE rows)
    FOLLOW  rows of an editor that follow a selection (d50~: the selected PARTIAL; two tones)
    SELECT  the band of an editor that makes such a selection
    WIDGET  background of sliders, toggles, bangs
    INK     text and lines
"""
YELLOW = "#fafaf2"
ROW = "#efefe4"
PINK = "#ffcfd6"
BLUE = "#1e3791"
PATCH = "#e4ecf4"
SETUP = ("#f3ecdf", "#e8dfcc")
FOLLOW = ("#e6eef6", "#d6e2ee")
SELECT = "#f4ece4"
WIDGET = "#fcfcfc"
INK = "#000000"
