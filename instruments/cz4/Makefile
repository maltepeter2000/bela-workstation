# CZ1 and CZ4 -- Pd externals and desktop tests
#
#   make                 build cz1~ and cz4~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make cz1 / make cz4  one of them
#   make bela            build both on the first Bela board itself (Cortex-A8, NEON, hard float)
#   make bela-gem        cross-build both for the Bela Gem (aarch64) into ../../build/bela-gem (all
#                        instruments side by side, needs zig)
#   make test            desktop tests: ./cz_test, ./tone_test <file.syx>, ./cz4_render [file.syx]
#   make demos           cz1 demo WAVs in demos/
#   make gen             cz4_params.h, cz4cc.pd, cz4pads.pd, cz4edit.pd and both help patches from tools/
#   make mac-zig         universal macOS binaries (arm64 + x86_64) and the patches into
#                        bin/macos (needs zig: pip install ziglang, and llvm-lipo)
# The same targets (make, mac-zig, bela-gem, gen, clean) in every instrument of bela-workstation.
# pd/ is the Pd package (externals, abstractions, help), src/ the C core shared by both.
#
# PDINCLUDE must point to the folder containing m_pd.h, e.g.
#   Linux (puredata-dev):  /usr/include/pd
#   macOS:                 /Applications/Pd-0.55-2.app/Contents/Resources/src
#   Bela:                  /usr/local/include/libpd   (check: find / -name m_pd.h)

CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wshadow
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wshadow
# The cores must stay in single precision (Cortex-A8 VFP/NEON): flag any
# accidental double arithmetic there.
CORE_FLAGS = -Wdouble-promotion -Werror
LDLIBS  = -lm
INC     = -Isrc -Ipd

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  EXT       = pd_darwin
  PDINCLUDE ?= /Applications/Pd.app/Contents/Resources/src
  SHARED    = -bundle -undefined dynamic_lookup
else
  EXT       = pd_linux
  PDINCLUDE ?= /usr/include/pd
  SHARED    = -shared
endif

CORE_C = src/cz_core.c src/cz_tone.c
CZ4_SRC = pd/cz4~.cpp pd/cz4_core.cpp
BELA_FLAGS = -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ffast-math

all: cz1 cz4

cz1: pd/cz1~.$(EXT)
pd/cz1~.$(EXT): pd/cz1~.c src/cz_core.c src/cz_core.h
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -Isrc -I"$(PDINCLUDE)" $(SHARED) -o '$@' pd/cz1~.c src/cz_core.c $(LDLIBS)

cz4: pd/cz4~.$(EXT)
pd/cz4~.$(EXT): $(CZ4_SRC) pd/cz4_core.h pd/cz4_params.h $(CORE_C) src/cz_core.h src/cz_tone.h
	$(CC) $(CFLAGS) -fPIC $(INC) -c src/cz_core.c -o pd/cz_core.o
	$(CC) $(CFLAGS) -fPIC $(INC) -c src/cz_tone.c -o pd/cz_tone.o
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden $(INC) -I"$(PDINCLUDE)" $(SHARED) -o '$@' \
	    $(CZ4_SRC) pd/cz_core.o pd/cz_tone.o $(LDLIBS)
	rm -f pd/cz_core.o pd/cz_tone.o

bela:
	$(CC) $(BELA_FLAGS) -std=c99 -Wall -fPIC -fvisibility=hidden -Isrc -I"$(PDINCLUDE)" -shared \
	    -o 'pd/cz1~.pd_linux' pd/cz1~.c src/cz_core.c -lm
	$(CC) $(BELA_FLAGS) -std=c99 -fPIC $(INC) -c src/cz_core.c -o pd/cz_core.o
	$(CC) $(BELA_FLAGS) -std=c99 -fPIC $(INC) -c src/cz_tone.c -o pd/cz_tone.o
	$(CXX) $(BELA_FLAGS) -std=c++17 -Wall -fPIC -fvisibility=hidden $(INC) -I"$(PDINCLUDE)" -shared \
	    -o 'pd/cz4~.pd_linux' $(CZ4_SRC) pd/cz_core.o pd/cz_tone.o -lm
	rm -f pd/cz_core.o pd/cz_tone.o

# ---- macOS package, cross-built from Linux (as juno~ and d50~) ---------------
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18
MACDIR = bin/macos
mac-zig:
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 -Isrc -Ithird_party/pd -fPIC -fvisibility=hidden \
	    -shared -Wl,-undefined,dynamic_lookup -o $(MACDIR)/cz1.$$a pd/cz1~.c src/cz_core.c || exit 1; \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 $(INC) -fPIC -c src/cz_core.c -o $(MACDIR)/core.$$a.o || exit 1; \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 $(INC) -fPIC -c src/cz_tone.c -o $(MACDIR)/tone.$$a.o || exit 1; \
	  $(ZIG) c++ -target $$a-macos.11.0 -O2 -std=c++17 $(INC) -Ithird_party/pd -fPIC -fvisibility=hidden \
	    -shared -Wl,-undefined,dynamic_lookup -o $(MACDIR)/cz4.$$a $(CZ4_SRC) \
	    $(MACDIR)/core.$$a.o $(MACDIR)/tone.$$a.o || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/cz1.aarch64 $(MACDIR)/cz1.x86_64 -output '$(MACDIR)/cz1~.pd_darwin'
	$(LIPO) -create $(MACDIR)/cz4.aarch64 $(MACDIR)/cz4.x86_64 -output '$(MACDIR)/cz4~.pd_darwin'
	rm -f $(MACDIR)/*.aarch64 $(MACDIR)/*.x86_64 $(MACDIR)/*.o
	cp 'pd/cz1~-help.pd' 'pd/cz4~-help.pd' pd/cz4cc.pd pd/cz4pads.pd pd/cz4edit.pd pd/cz4test.pd pd/lkdisplay.pd $(MACDIR)/
	mkdir -p $(MACDIR)/test && cp -r pd/test/seq pd/test/tones pd/test/cz4test-bank*.syx $(MACDIR)/test/
	cp tools/cz_compare.py tools/cz4_tests.py $(MACDIR)/test/

# ---- Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux (as juno~) ----------
# glibc 2.31 or newer on the board. On the board itself a plain make works too.
BELADIR = ../../build/bela-gem
bela-gem:
	mkdir -p $(BELADIR)
	$(ZIG) cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c99 -Isrc -Ithird_party/pd -fPIC \
	  -fvisibility=hidden -shared -s -o '$(BELADIR)/cz1~.pd_linux' pd/cz1~.c src/cz_core.c -lm
	$(ZIG) cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c99 $(INC) -fPIC -c src/cz_core.c -o $(BELADIR)/core.o
	$(ZIG) cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c99 $(INC) -fPIC -c src/cz_tone.c -o $(BELADIR)/tone.o
	$(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c++17 $(INC) -Ithird_party/pd -fPIC \
	  -fvisibility=hidden -shared -s -o '$(BELADIR)/cz4~.pd_linux' $(CZ4_SRC) $(BELADIR)/core.o $(BELADIR)/tone.o
	rm -f $(BELADIR)/core.o $(BELADIR)/tone.o
	cp pd/cz4cc.pd pd/cz4pads.pd pd/lkdisplay.pd $(BELADIR)/

test: cz_test tone_test cz4_render
cz_core.o: src/cz_core.c src/cz_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ src/cz_core.c
cz_tone.o: src/cz_tone.c src/cz_tone.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ src/cz_tone.c
cz_test: tests/cz_test.c cz_core.o src/cz_core.h
	$(CC) $(CFLAGS) -Isrc -o $@ tests/cz_test.c cz_core.o $(LDLIBS)
tone_test: tests/tone_test.c cz_tone.o
	$(CC) $(CFLAGS) -Isrc -o $@ tests/tone_test.c cz_tone.o $(LDLIBS)
cz4_render: tests/cz4_render.cpp pd/cz4_core.cpp pd/cz4_core.h pd/cz4_params.h cz_core.o cz_tone.o
	$(CXX) $(CXXFLAGS) $(INC) -o $@ tests/cz4_render.cpp pd/cz4_core.cpp cz_core.o cz_tone.o $(LDLIBS)

demos: cz_test
	mkdir -p demos && ./cz_test demos

gen generate:
	python3 tools/cz4_params.py
	python3 tools/gen_cz4cc.py
	python3 tools/gen_cz4_editor.py
	python3 tools/gen_cz4_help.py
	cd pd && python3 ../../../common/tools/lkdisplay.py lkdisplay.pd
	python3 tools/gen_cz1_help.py 'pd/cz1~-help.pd'
	python3 tools/cz4_tests.py
	python3 tools/gen_cz4_testrig.py

clean:
	rm -f cz_test tone_test cz4_render *.o pd/*.o 'pd/cz1~.pd_linux' 'pd/cz1~.pd_darwin' \
	    'pd/cz4~.pd_linux' 'pd/cz4~.pd_darwin'
	rm -rf demos

.PHONY: all cz1 cz4 bela bela-gem test demos gen generate mac-zig clean
