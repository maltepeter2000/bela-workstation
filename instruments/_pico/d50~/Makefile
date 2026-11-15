# d50~ -- Roland D-50 external for Pure Data.
#
#   make                 build for this machine
#   make install         copy into Pd's externals folder (see PDLIBDIR)
#
# Building needs no ROM data. To make sound the object needs d50_pcm.bin,
# which tools/make_d50_data.py writes from your own ROM images.

lib.name = d50~
class.sources = d50~.cpp
common.sources = d50_core.cpp
datafiles = d50~-help.pd d50cc.pd d50edit.pd

cflags = -std=c++17 -DD5_HOST_BUILD -Iengine -fvisibility=hidden
suppress-wunused = yes

# libc++ needs a newer deployment target than pd-lib-builder's default.
ifeq ($(shell uname -s),Darwin)
cflags += -mmacosx-version-min=10.13
endif

PDLIBBUILDER_DIR = ../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# ---- macOS package, cross-built from Linux (as in PDoly800) -----------------
#   make mac-zig    universal binary (arm64 + x86_64) and the patches into ../bin/macos
# Needs zig (pip install ziglang) and llvm-lipo. No ROM data goes into the package.
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18
MACDIR = ../bin/macos
mac-zig: d50~.cpp d50_core.cpp d50_core.h d50_params.h
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) c++ -target $$a-macos.11.0 -O2 -std=c++17 -DD5_HOST_BUILD -Iengine -I../third_party/pd \
	    -fPIC -fvisibility=hidden -shared -Wl,-undefined,dynamic_lookup \
	    -o $(MACDIR)/d50.$$a 'd50~.cpp' d50_core.cpp || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/d50.aarch64 $(MACDIR)/d50.x86_64 -output '$(MACDIR)/d50~.pd_darwin'
	rm -f $(MACDIR)/d50.aarch64 $(MACDIR)/d50.x86_64
	cp 'd50~-help.pd' d50cc.pd d50edit.pd $(MACDIR)/

# ---- generated files ---------------------------------------------------------
#   make gen        parameter header, controller layer, editor and help patch
gen:
	python3 ../tools/d50_params.py
	python3 ../tools/gen_d50cc.py
	python3 ../tools/gen_editor.py
	python3 ../tools/gen_help.py
