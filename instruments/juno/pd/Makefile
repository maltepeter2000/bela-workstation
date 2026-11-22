# juno~ -- Roland Juno-60 external for Pure Data, with Juno-106 patches and SysEx.
#
#   make                 build for this machine
#   make install         copy into Pd's externals folder (see PDLIBDIR)
#
# Needs no data files.
#   make OVERSAMPLE=1    the engine without oversampling (2 is the default)

OVERSAMPLE ?= 2
ENGINE = engine/juno.cpp engine/juno_params.cpp engine/juno_presets.cpp engine/juno_fx.cpp

lib.name = juno~
class.sources = juno~.cpp
common.sources = juno_core.cpp $(ENGINE)
datafiles = juno~-help.pd junocc.pd junopads.pd junoedit.pd lkdisplay.pd

DEFS = -DJUNO_HOST_BUILD -DJUNO_OVERSAMPLE=$(OVERSAMPLE)
cflags = -std=c++17 $(DEFS) -Iengine -fvisibility=hidden
suppress-wunused = yes

# libc++ needs a newer deployment target than pd-lib-builder's default.
ifeq ($(shell uname -s),Darwin)
cflags += -mmacosx-version-min=10.13
endif

PDLIBBUILDER_DIR = ../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# ---- macOS package, cross-built from Linux -----------------------------------
#   make mac-zig    universal binary (arm64 + x86_64) and the patches into ../bin/macos
# Needs zig (pip install ziglang) and llvm-lipo.
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18
MACDIR = ../bin/macos
mac-zig: juno~.cpp juno_core.cpp juno_core.h juno_params.h
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) c++ -target $$a-macos.11.0 -O2 -std=c++17 $(DEFS) -Iengine -I../third_party/pd \
	    -fPIC -fvisibility=hidden -shared -Wl,-undefined,dynamic_lookup \
	    -o $(MACDIR)/juno.$$a 'juno~.cpp' juno_core.cpp $(ENGINE) || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/juno.aarch64 $(MACDIR)/juno.x86_64 -output '$(MACDIR)/juno~.pd_darwin'
	rm -f $(MACDIR)/juno.aarch64 $(MACDIR)/juno.x86_64
	cp 'juno~-help.pd' junocc.pd junopads.pd junoedit.pd lkdisplay.pd $(MACDIR)/

# ---- Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux ------------------
#   make bela-gem   juno~.pd_linux and the abstractions into ../../../build/bela-gem (all
#                   instruments side by side; make dist puts them on the branch pd)
# glibc 2.31 or newer on the board, libc++ linked in. On the board itself a plain make works too.
GEMDIR = ../../../build/bela-gem
BELA_OS ?= 2
bela-gem: juno~.cpp juno_core.cpp juno_core.h juno_params.h
	mkdir -p $(GEMDIR)
	$(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c++17 \
	  -DJUNO_HOST_BUILD -DJUNO_OVERSAMPLE=$(BELA_OS) -Iengine -I../third_party/pd \
	  -fPIC -fvisibility=hidden -shared -s -o '$(GEMDIR)/juno~.pd_linux' 'juno~.cpp' juno_core.cpp $(ENGINE)
	cp junocc.pd junopads.pd lkdisplay.pd $(GEMDIR)/

# ---- generated files ---------------------------------------------------------
#   make gen        parameter header, controller layer, editor and help patch
gen:
	python3 ../tools/juno_params.py
	python3 ../tools/gen_junocc.py
	python3 ../tools/gen_juno_editor.py
	python3 ../tools/gen_juno_help.py
	python3 ../../../common/tools/lkdisplay.py lkdisplay.pd
	python3 ../tools/gen_juno_bela.py

.PHONY: mac-zig bela-gem gen
