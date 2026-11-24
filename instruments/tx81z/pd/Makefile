# Build the tx81z~ (firmware emulation) and tx81zn~ (native engine) Pd externals.
#   macOS:      make            -> tx81z~.pd_darwin, tx81zn~.pd_darwin
#   Linux/Bela: make            -> tx81z~.pd_linux, tx81zn~.pd_linux
#   Bela Gem:   make bela-gem   -> cross-built into ../../../build/bela-gem (all instruments
#                                  side by side) and the Bela projects ../bela/*-pd (needs zig)
#   macOS from Linux: make mac-zig -> universal binaries and the patches into ../bin/macos
#   make gen    the generated files (parameter header, controller layers, pads, editors, help, Bela)
# On Bela the libpd headers of the board are used automatically and the
# code is tuned for the board's CPU (Cortex-A8 NEON / Bela Gem Cortex-A53).
# Override with PD_INCLUDE=...

ROOT := ..
YMFM := $(ROOT)/third_party/ymfm/src

# prefer the Pd headers installed on the system (Bela: libpd), else the bundled one
ifneq ($(wildcard /usr/local/include/libpd/m_pd.h),)
  PD_INCLUDE ?= /usr/local/include/libpd
else
  PD_INCLUDE ?= $(ROOT)/third_party/pd
endif

SRC := tx81z~.cpp \
       $(ROOT)/src/tx81z.cpp \
       $(ROOT)/src/cpu/hd6303x.cpp \
       $(YMFM)/ymfm_opz.cpp $(YMFM)/ymfm_misc.cpp $(YMFM)/ymfm_adpcm.cpp $(YMFM)/ymfm_ssg.cpp

NATIVE := $(ROOT)/src/native
NSRC := tx81zn~.cpp $(NATIVE)/txnative.cpp $(NATIVE)/opz/ymfmx_opz.cpp $(NATIVE)/opz/opzcore.cpp

CXXFLAGS ?= -O3
CXXFLAGS += -fPIC -fvisibility=hidden -I$(PD_INCLUDE) -I$(ROOT)/src

UNAME := $(shell uname -s)
ARCH  := $(shell uname -m)
ifeq ($(UNAME),Darwin)
  EXT := pd_darwin
  LDFLAGS += -bundle -undefined dynamic_lookup -arch arm64 -arch x86_64
  CXXFLAGS += -arch arm64 -arch x86_64 -mmacosx-version-min=10.13
else
  EXT := pd_linux
  LDFLAGS += -shared
  ifeq ($(ARCH),armv7l)
    # Bela (BeagleBone / PocketBeagle, Cortex-A8)
    CXXFLAGS += -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize
  endif
  ifeq ($(ARCH),aarch64)
    # Bela Gem (Cortex-A53)
    CXXFLAGS += -mcpu=cortex-a53
  endif
endif

CXX ?= c++

all: tx81z~.$(EXT) tx81zn~.$(EXT)

tx81z~.$(EXT): $(SRC) $(ROOT)/src/*.h $(ROOT)/src/cpu/*.h txshared.h txbank.h tx81z_params.h
	$(CXX) -std=c++14 $(CXXFLAGS) -I$(YMFM) -I$(NATIVE) $(SRC) $(LDFLAGS) -o $@
	@echo "built $@ (Pd headers: $(PD_INCLUDE))"

tx81zn~.$(EXT): $(NSRC) $(NATIVE)/*.h $(NATIVE)/opz/*.h $(NATIVE)/opz/*.ipp txshared.h txbank.h tx81z_params.h
	$(CXX) -std=c++17 $(CXXFLAGS) -I$(NATIVE) $(NSRC) $(LDFLAGS) -o $@
	@echo "built $@ (Pd headers: $(PD_INCLUDE))"

# Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux (as juno~): make bela-gem
# puts both externals and the abstractions into ../../../build/bela-gem (make dist puts them
# on the branch pd, which the Bela projects in ../bela use). glibc 2.31 or newer on the board. On the board itself build_on_bela.sh does the same.
ZIG ?= python3 -m ziglang
GEMDIR := ../../../build/bela-gem
GEM := $(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O3 -fPIC -fvisibility=hidden \
       -I$(ROOT)/third_party/pd -I$(ROOT)/src -shared -s
bela-gem:
	mkdir -p $(GEMDIR)
	$(GEM) -std=c++17 -I$(NATIVE) $(NSRC) -o $(GEMDIR)/tx81zn~.pd_linux
	$(GEM) -std=c++14 -I$(YMFM) -I$(NATIVE) $(SRC) -o $(GEMDIR)/tx81z~.pd_linux
	cp txfmcc.pd txpads.pd txcc.pd lkdisplay.pd $(GEMDIR)/

# macOS package, cross-built from Linux (as juno~): universal binaries (arm64 + x86_64) and the
# patches into ../bin/macos. Needs zig (pip install ziglang) and llvm-lipo.
LIPO ?= llvm-lipo-18
MACDIR := ../bin/macos
MAC = -O3 -fPIC -fvisibility=hidden -I$(ROOT)/third_party/pd -I$(ROOT)/src -shared -Wl,-undefined,dynamic_lookup
mac-zig:
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) c++ -target $$a-macos.11.0 $(MAC) -std=c++17 -I$(NATIVE) $(NSRC) -o $(MACDIR)/txn.$$a || exit 1; \
	  $(ZIG) c++ -target $$a-macos.11.0 $(MAC) -std=c++14 -I$(YMFM) -I$(NATIVE) $(SRC) -o $(MACDIR)/tx.$$a || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/txn.aarch64 $(MACDIR)/txn.x86_64 -output '$(MACDIR)/tx81zn~.pd_darwin'
	$(LIPO) -create $(MACDIR)/tx.aarch64 $(MACDIR)/tx.x86_64 -output '$(MACDIR)/tx81z~.pd_darwin'
	rm -f $(MACDIR)/*.aarch64 $(MACDIR)/*.x86_64
	cp 'tx81zn~-help.pd' 'tx81z~-help.pd' txcc.pd txedit.pd txfm.pd txfmcc.pd txpads.pd \
	    lkdisplay.pd 'tx81zn.i~.pd' 'tx81zn.i~-help.pd' 'tx81z.i~.pd' 'tx81z.i~-help.pd' $(MACDIR)/

gen:
	python3 ../tools/tx81z_params.py
	python3 ../tools/gen_txcc.py
	python3 ../tools/gen_editor.py
	python3 ../tools/gen_fmedit.py
	python3 ../tools/gen_help.py
	python3 ../tools/gen_help_native.py
	python3 ../../../common/tools/lkdisplay.py lkdisplay.pd
	python3 ../tools/gen_bela.py
	python3 ../tools/gen_bela.py --native
	python3 ../tools/gen_tx_inst.py

clean:
	rm -f tx81z~.pd_darwin tx81z~.pd_linux tx81zn~.pd_darwin tx81zn~.pd_linux

.PHONY: all bela-gem mac-zig gen clean
