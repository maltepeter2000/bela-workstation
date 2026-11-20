# Build the tx81z~ (firmware emulation) and tx81zn~ (native engine) Pd externals.
#   macOS:      make            -> tx81z~.pd_darwin, tx81zn~.pd_darwin
#   Linux/Bela: make            -> tx81z~.pd_linux, tx81zn~.pd_linux
#   Bela Gem:   make bela-gem   -> cross-built into ../bela-pd/*-pd (needs zig)
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

tx81z~.$(EXT): $(SRC) $(ROOT)/src/*.h $(ROOT)/src/cpu/*.h txshared.h tx81z_params.h
	$(CXX) -std=c++14 $(CXXFLAGS) -I$(YMFM) $(SRC) $(LDFLAGS) -o $@
	@echo "built $@ (Pd headers: $(PD_INCLUDE))"

tx81zn~.$(EXT): $(NSRC) $(NATIVE)/*.h $(NATIVE)/opz/*.h $(NATIVE)/opz/*.ipp txshared.h tx81z_params.h
	$(CXX) -std=c++17 $(CXXFLAGS) -I$(NATIVE) $(NSRC) $(LDFLAGS) -o $@
	@echo "built $@ (Pd headers: $(PD_INCLUDE))"

# Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux (as juno~): make bela-gem
# puts both externals and the abstractions into the Bela projects in ../bela-pd.
# glibc 2.31 or newer on the board. On the board itself build_on_bela.sh does the same.
ZIG ?= python3 -m ziglang
GEM := $(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O3 -fPIC -fvisibility=hidden \
       -I$(ROOT)/third_party/pd -I$(ROOT)/src -shared -s
bela-gem:
	$(GEM) -std=c++17 -I$(NATIVE) $(NSRC) -o ../bela-pd/tx81zn-pd/tx81zn~.pd_linux
	$(GEM) -std=c++14 -I$(YMFM) $(SRC) -o ../bela-pd/tx81z-pd/tx81z~.pd_linux
	cp txfmcc.pd ../bela-pd/tx81zn-pd/
	cp txcc.pd ../bela-pd/tx81z-pd/

clean:
	rm -f tx81z~.pd_darwin tx81z~.pd_linux tx81zn~.pd_darwin tx81zn~.pd_linux

.PHONY: all bela-gem clean
