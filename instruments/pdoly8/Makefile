# pdoly8~ -- Pd external and desktop test
#
#   make            build pdoly8~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make bela       build on the Bela board itself (Cortex-A8, NEON, hard float)
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
LDLIBS  = -lm

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
	    -o 'pdoly8~.pd_linux' pdoly8~.c pdoly8_core.c -lm

test: pdoly8_test
pdoly8_core.o: pdoly8_core.c pdoly8_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ pdoly8_core.c
pdoly8_test: pdoly8_test.c pdoly8_core.o pdoly8_core.h
	$(CC) $(CFLAGS) -o $@ pdoly8_test.c pdoly8_core.o $(LDLIBS)

demos: pdoly8_test
	mkdir -p demos && ./pdoly8_test demos

help:
	python3 tools/gen_help.py 'pdoly8~-help.pd'

clean:
	rm -f pdoly8_test pdoly8_core.o 'pdoly8~.pd_linux' 'pdoly8~.pd_darwin'
	rm -rf demos

.PHONY: external bela test demos help clean
