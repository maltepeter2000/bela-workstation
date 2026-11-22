# pdoly8~ -- Pd external and desktop test
#
#   make            build pdoly8~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make bela-gem   cross-build for the Bela Gem (aarch64) into ../../build/bela-gem (all
#                   instruments side by side, needs zig)
#   make bela       build on the Bela board itself (Cortex-A8, NEON, hard float)
#   make bela-zig   cross-build the first Bela's binary (ARMv7 Cortex-A8, NEON, hard float) into
#                   bela/pepper and bela/oled from Linux, with zig, and regenerate both projects
#   make mac-zig    cross-build a universal macOS binary (arm64 + x86_64) into bin/macos
#                   from Linux, with zig (pip install ziglang) and llvm-lipo
#   make test       desktop test of the core; ./pdoly8_test
#   make demos      desktop test + demo WAVs in demos/
#   make gen        controller layer, pads, translator, editor, help patch
# The same targets (make, mac-zig, bela-gem, gen, clean) in every instrument of bela-workstation.
# pd/ is the Pd package (external, abstractions, help, presets, user bank).
#
# PDINCLUDE must point to the folder containing m_pd.h, e.g.
#   Linux (puredata-dev):  /usr/include/pd
#   macOS:                 /Applications/Pd-0.55-2.app/Contents/Resources/src
#   Bela:                  /usr/local/include/libpd   (check: find / -name m_pd.h)

CC      ?= cc
CFLAGS  ?= -O2 -std=c99 -Wall -Wextra -Wshadow
# The core must stay in single precision (Cortex-A8 VFP/NEON): flag any
# accidental double arithmetic there.
CORE_FLAGS = -Wdouble-promotion -Werror
LDLIBS  = -lm -lpthread

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

EXTERNAL  = pd/pdoly8~.$(EXT)
SRC = pd/pdoly8~.c pd/pdoly8_core.c
DEP = $(SRC) pd/pdoly8_core.h
BELA_FLAGS = -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ffast-math

all external: $(EXTERNAL)
$(EXTERNAL): $(DEP)
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" $(SHARED) -o '$@' $(SRC) $(LDLIBS)

bela: $(DEP)
	$(CC) $(BELA_FLAGS) -std=c99 -Wall -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" -shared \
	    -o 'pd/pdoly8~.pd_linux' $(SRC) -lm -lpthread

ZIG  ?= python3 -m ziglang
bela-zig: $(DEP)
	$(ZIG) cc -target arm-linux-gnueabihf.2.24 -mcpu=cortex_a8 -O3 -ffast-math -std=c99 -fPIC -fvisibility=hidden \
	    -I"$(PDINCLUDE)" -shared -s -o 'bela/pepper/pdoly8~.pd_linux' $(SRC) -lm -lpthread
	cp 'bela/pepper/pdoly8~.pd_linux' bela/oled/
	python3 tools/gen_p8cc.py
	python3 tools/gen_bela.py
	python3 tools/gen_bela_oled.py
	python3 tools/gen_cc_readme.py

# ---- Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux (as juno~) ----------
#   make bela-gem   pdoly8~.pd_linux and the abstractions into ../../build/bela-gem
# glibc 2.31 or newer on the board. On the board itself: make PDINCLUDE=/usr/local/include/libpd
GEMDIR = ../../build/bela-gem
bela-gem: $(DEP)
	mkdir -p $(GEMDIR)
	$(ZIG) cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O3 -ffast-math -std=c99 -fPIC -fvisibility=hidden \
	    -Ithird_party/pd -shared -s -o '$(GEMDIR)/pdoly8~.pd_linux' $(SRC) -lm -lpthread
	cp pd/p8cc.pd pd/p8pads.pd pd/p8nav.pd pd/lkdisplay.pd pd/ufread.pd $(GEMDIR)/
	mkdir -p $(GEMDIR)/presets && cp pd/presets/poly800mk2-factory.txt $(GEMDIR)/presets/

LIPO ?= llvm-lipo-18
mac-zig: $(DEP)
	mkdir -p bin/macos
	for a in aarch64 x86_64; do \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" \
	    -shared -Wl,-undefined,dynamic_lookup -o bin/macos/pdoly8~.$$a $(SRC) || exit 1; \
	done
	$(LIPO) -create bin/macos/pdoly8~.aarch64 bin/macos/pdoly8~.x86_64 -output 'bin/macos/pdoly8~.pd_darwin'
	rm -f bin/macos/pdoly8~.aarch64 bin/macos/pdoly8~.x86_64
	cp 'pd/pdoly8~-help.pd' pd/p8factory.pd pd/p8user.pd pd/p8cc.pd pd/p8edit.pd pd/p8pads.pd pd/p8nav.pd \
	    pd/lkdisplay.pd pd/ufread.pd bin/macos/
	mkdir -p bin/macos/presets && cp pd/presets/poly800mk2-factory.txt bin/macos/presets/

test: pdoly8_test
pdoly8_core.o: pd/pdoly8_core.c pd/pdoly8_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ pd/pdoly8_core.c
pdoly8_test: tests/pdoly8_test.c pdoly8_core.o pd/pdoly8_core.h
	$(CC) $(CFLAGS) -Ipd -o $@ tests/pdoly8_test.c pdoly8_core.o $(LDLIBS)

demos: pdoly8_test
	mkdir -p demos && ./pdoly8_test demos

gen help:
	python3 tools/gen_p8cc.py
	python3 tools/gen_editor.py
	python3 tools/gen_help.py
	cd pd && python3 ../../../common/tools/lkdisplay.py lkdisplay.pd
	cd pd && python3 ../../../common/tools/ufread.py ufread.pd

presets: pd/presets/poly800mk2-factory.syx
	python3 tools/p8sysex.py syx2txt pd/presets/poly800mk2-factory.syx pd/presets/poly800mk2-factory.txt

clean:
	rm -f pdoly8_test pdoly8_core.o 'pd/pdoly8~.pd_linux' 'pd/pdoly8~.pd_darwin'
	rm -rf demos

.PHONY: all external bela bela-zig bela-gem mac-zig test demos gen help presets clean
