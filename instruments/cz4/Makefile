# CZ1 and CZ4 -- Pd externals and desktop tests
#
#   make                 build cz1~ and cz4~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make cz1 / make cz4  one of them
#   make bela            build both on the Bela board itself (Cortex-A8, NEON, hard float)
#   make test            desktop tests: ./cz_test, ./tone_test <file.syx>, ./cz4_render [file.syx]
#   make demos           cz1 demo WAVs in demos/
#   make generate        cz4_params.h, cz4cc.pd, cz4edit.pd and both help patches from tools/
#   make mac-zig         universal macOS binaries (arm64 + x86_64) and the patches into
#                        dist/macos, zipped (needs zig: pip install ziglang, and llvm-lipo)
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
INC     = -Icore -Icz4

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

CORE_C = core/cz_core.c core/cz_tone.c
CZ4_SRC = cz4/cz4~.cpp cz4/cz4_core.cpp
BELA_FLAGS = -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ffast-math

all: cz1 cz4

cz1: cz1/cz1~.$(EXT)
cz1/cz1~.$(EXT): cz1/cz1~.c core/cz_core.c core/cz_core.h
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -Icore -I"$(PDINCLUDE)" $(SHARED) -o '$@' cz1/cz1~.c core/cz_core.c $(LDLIBS)

cz4: cz4/cz4~.$(EXT)
cz4/cz4~.$(EXT): $(CZ4_SRC) cz4/cz4_core.h cz4/cz4_params.h $(CORE_C) core/cz_core.h core/cz_tone.h
	$(CC) $(CFLAGS) -fPIC $(INC) -c core/cz_core.c -o cz4/cz_core.o
	$(CC) $(CFLAGS) -fPIC $(INC) -c core/cz_tone.c -o cz4/cz_tone.o
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden $(INC) -I"$(PDINCLUDE)" $(SHARED) -o '$@' \
	    $(CZ4_SRC) cz4/cz_core.o cz4/cz_tone.o $(LDLIBS)
	rm -f cz4/cz_core.o cz4/cz_tone.o

bela:
	$(CC) $(BELA_FLAGS) -std=c99 -Wall -fPIC -fvisibility=hidden -Icore -I"$(PDINCLUDE)" -shared \
	    -o 'cz1/cz1~.pd_linux' cz1/cz1~.c core/cz_core.c -lm
	$(CC) $(BELA_FLAGS) -std=c99 -fPIC $(INC) -c core/cz_core.c -o cz4/cz_core.o
	$(CC) $(BELA_FLAGS) -std=c99 -fPIC $(INC) -c core/cz_tone.c -o cz4/cz_tone.o
	$(CXX) $(BELA_FLAGS) -std=c++17 -Wall -fPIC -fvisibility=hidden $(INC) -I"$(PDINCLUDE)" -shared \
	    -o 'cz4/cz4~.pd_linux' $(CZ4_SRC) cz4/cz_core.o cz4/cz_tone.o -lm
	rm -f cz4/cz_core.o cz4/cz_tone.o

# ---- macOS package, cross-built from Linux (as juno~ and d50~) ---------------
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18
MACDIR = dist/macos
mac-zig:
	rm -rf $(MACDIR) && mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 -Icore -Ithird_party/pd -fPIC -fvisibility=hidden \
	    -shared -Wl,-undefined,dynamic_lookup -o $(MACDIR)/cz1.$$a cz1/cz1~.c core/cz_core.c || exit 1; \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 $(INC) -fPIC -c core/cz_core.c -o $(MACDIR)/core.$$a.o || exit 1; \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 $(INC) -fPIC -c core/cz_tone.c -o $(MACDIR)/tone.$$a.o || exit 1; \
	  $(ZIG) c++ -target $$a-macos.11.0 -O2 -std=c++17 $(INC) -Ithird_party/pd -fPIC -fvisibility=hidden \
	    -shared -Wl,-undefined,dynamic_lookup -o $(MACDIR)/cz4.$$a $(CZ4_SRC) \
	    $(MACDIR)/core.$$a.o $(MACDIR)/tone.$$a.o || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/cz1.aarch64 $(MACDIR)/cz1.x86_64 -output '$(MACDIR)/cz1~.pd_darwin'
	$(LIPO) -create $(MACDIR)/cz4.aarch64 $(MACDIR)/cz4.x86_64 -output '$(MACDIR)/cz4~.pd_darwin'
	rm -f $(MACDIR)/*.aarch64 $(MACDIR)/*.x86_64 $(MACDIR)/*.o
	cp 'cz1/cz1~-help.pd' 'cz4/cz4~-help.pd' cz4/cz4cc.pd cz4/cz4edit.pd cz4/cz4test.pd $(MACDIR)/
	mkdir -p $(MACDIR)/test && cp -r cz4/test/seq cz4/test/tones cz4/test/cz4test-bank.syx $(MACDIR)/test/
	cp tools/cz_compare.py tools/cz4_tests.py $(MACDIR)/test/
	cd dist && rm -f cz-macos-universal.zip && zip -qr cz-macos-universal.zip macos

test: cz_test tone_test cz4_render
cz_core.o: core/cz_core.c core/cz_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ core/cz_core.c
cz_tone.o: core/cz_tone.c core/cz_tone.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ core/cz_tone.c
cz_test: tests/cz_test.c cz_core.o core/cz_core.h
	$(CC) $(CFLAGS) -Icore -o $@ tests/cz_test.c cz_core.o $(LDLIBS)
tone_test: tests/tone_test.c cz_tone.o
	$(CC) $(CFLAGS) -Icore -o $@ tests/tone_test.c cz_tone.o $(LDLIBS)
cz4_render: tests/cz4_render.cpp cz4/cz4_core.cpp cz4/cz4_core.h cz4/cz4_params.h cz_core.o cz_tone.o
	$(CXX) $(CXXFLAGS) $(INC) -o $@ tests/cz4_render.cpp cz4/cz4_core.cpp cz_core.o cz_tone.o $(LDLIBS)

demos: cz_test
	mkdir -p demos && ./cz_test demos

generate:
	python3 tools/cz4_params.py
	python3 tools/gen_cz4cc.py
	python3 tools/gen_cz4_editor.py
	python3 tools/gen_cz4_help.py
	python3 tools/gen_cz1_help.py 'cz1/cz1~-help.pd'
	python3 tools/cz4_tests.py
	python3 tools/gen_cz4_testrig.py

clean:
	rm -f cz_test tone_test cz4_render *.o cz4/*.o 'cz1/cz1~.pd_linux' 'cz1/cz1~.pd_darwin' \
	    'cz4/cz4~.pd_linux' 'cz4/cz4~.pd_darwin'
	rm -rf demos

.PHONY: all cz1 cz4 bela test demos generate mac-zig clean
