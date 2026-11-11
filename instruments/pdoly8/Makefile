# pdoly8~ -- Pd external and desktop test
#
#   make            build pdoly8~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make bela       build on the Bela board itself (Cortex-A8, NEON, hard float)
#   make bela-zig   cross-build the Bela binary (ARMv7 Cortex-A8, NEON, hard float) into bela/
#                   from Linux, with zig; and regenerate bela/project/
#   make mac-zig    cross-build a universal macOS binary (arm64 + x86_64) into bin/macos
#                   from Linux, with zig (pip install ziglang) and llvm-lipo
#   make test       desktop test of the core; ./pdoly8_test
#   make demos      desktop test + demo WAVs in demos/
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

EXTERNAL  = pdoly8~.$(EXT)
BELA_FLAGS = -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ffast-math

external: $(EXTERNAL)
$(EXTERNAL): pdoly8~.c pdoly8_core.c pdoly8_core.h
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" $(SHARED) -o '$@' pdoly8~.c pdoly8_core.c $(LDLIBS)

bela: pdoly8~.c pdoly8_core.c pdoly8_core.h
	$(CC) $(BELA_FLAGS) -std=c99 -Wall -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" -shared \
	    -o 'pdoly8~.pd_linux' pdoly8~.c pdoly8_core.c -lm -lpthread

ZIG  ?= python3 -m ziglang
bela-zig: pdoly8~.c pdoly8_core.c pdoly8_core.h
	$(ZIG) cc -target arm-linux-gnueabihf.2.24 -mcpu=cortex_a8 -O3 -ffast-math -std=c99 -fPIC -fvisibility=hidden \
	    -I"$(PDINCLUDE)" -shared -s -o 'bela/pdoly8~.pd_linux' pdoly8~.c pdoly8_core.c -lm -lpthread
	cp 'bela/pdoly8~.pd_linux' bela-oled/
	python3 tools/gen_p8cc.py
	python3 tools/gen_bela.py
	python3 tools/gen_bela_oled.py
	python3 tools/gen_cc_readme.py

LIPO ?= llvm-lipo-18
mac-zig: pdoly8~.c pdoly8_core.c pdoly8_core.h
	mkdir -p bin/macos
	for a in aarch64 x86_64; do \
	  $(ZIG) cc -target $$a-macos.11.0 -O2 -std=c99 -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" \
	    -shared -Wl,-undefined,dynamic_lookup -o bin/macos/pdoly8~.$$a pdoly8~.c pdoly8_core.c || exit 1; \
	done
	$(LIPO) -create bin/macos/pdoly8~.aarch64 bin/macos/pdoly8~.x86_64 -output 'bin/macos/pdoly8~.pd_darwin'
	rm -f bin/macos/pdoly8~.aarch64 bin/macos/pdoly8~.x86_64
	cp 'pdoly8~-help.pd' p8factory.pd p8user.pd p8cc.pd bin/macos/
	mkdir -p bin/macos/presets && cp presets/poly800mk2-factory.txt bin/macos/presets/

test: pdoly8_test
pdoly8_core.o: pdoly8_core.c pdoly8_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ pdoly8_core.c
pdoly8_test: pdoly8_test.c pdoly8_core.o pdoly8_core.h
	$(CC) $(CFLAGS) -o $@ pdoly8_test.c pdoly8_core.o $(LDLIBS)

demos: pdoly8_test
	mkdir -p demos && ./pdoly8_test demos

help:
	python3 tools/gen_help.py 'pdoly8~-help.pd'

presets: presets/poly800mk2-factory.syx
	python3 tools/p8sysex.py syx2txt presets/poly800mk2-factory.syx presets/poly800mk2-factory.txt

clean:
	rm -f pdoly8_test pdoly8_core.o 'pdoly8~.pd_linux' 'pdoly8~.pd_darwin'
	rm -rf demos

.PHONY: external bela bela-zig mac-zig test demos help presets clean
