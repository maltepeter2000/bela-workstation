# sverb~: the reverb of Signalsmith Basics as a Pd object (send effect by default).
#   make            build for this machine into pd/
#   make mac-zig    universal macOS binary and the help patch into bin/macos (zig, llvm-lipo)
#   make bela-gem   cross-built for the Bela Gem (zig) into ../../build/bela-gem (beside the
#                   instruments; make dist puts it on the branch pd)
#   make clean
# Needs only a C++17 compiler; the Signalsmith headers and m_pd.h are in third_party.

SRC   = pd/sverb~.cpp
INC   = -Ithird_party/signalsmith -Ithird_party/pd
FLAGS = -std=c++17 -O2 -fPIC -fvisibility=hidden $(INC)
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18

ifeq ($(shell uname -s),Darwin)
EXT    = pd_darwin
LDFLAG = -shared -undefined dynamic_lookup -mmacosx-version-min=10.13
else
EXT    = pd_linux
LDFLAG = -shared
endif
CXX ?= c++

all: pd/sverb~.$(EXT)
pd/sverb~.$(EXT): $(SRC) third_party/signalsmith/signalsmith-basics/reverb.h
	$(CXX) $(FLAGS) $(LDFLAG) -o '$@' '$(SRC)'

# ---- macOS package, cross-built from Linux -----------------------------------
MACDIR = bin/macos
mac-zig: $(SRC)
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) c++ -target $$a-macos.11.0 $(FLAGS) -shared -Wl,-undefined,dynamic_lookup \
	    -o $(MACDIR)/sverb.$$a '$(SRC)' || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/sverb.aarch64 $(MACDIR)/sverb.x86_64 -output '$(MACDIR)/sverb~.pd_darwin'
	rm -f $(MACDIR)/sverb.aarch64 $(MACDIR)/sverb.x86_64
	cp 'pd/sverb~-help.pd' pd/vrs23-clap.wav ../vrs23/skpreset.pd $(MACDIR)/

# ---- Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux ------------------
# glibc 2.31 or newer on the board, libc++ linked in. On the board itself a plain make works too.
GEMDIR = ../../build/bela-gem
bela-gem: $(SRC)
	mkdir -p $(GEMDIR)
	$(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 $(FLAGS) -shared -s \
	  -o '$(GEMDIR)/sverb~.pd_linux' '$(SRC)'

gen:
	python3 tools/gen_help.py

clean:
	rm -f pd/*.pd_linux pd/*.pd_darwin

.PHONY: all mac-zig bela-gem gen clean
