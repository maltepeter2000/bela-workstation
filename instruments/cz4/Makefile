# cz4~ -- Pd external and desktop test
#
#   make            build cz4~ for this machine (needs Pd's m_pd.h, see PDINCLUDE)
#   make bela       build on the Bela board itself (Cortex-A8, NEON, hard float)
#   make test       desktop test of the core; ./cz_test
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

EXTERNAL  = cz4~.$(EXT)
BELA_FLAGS = -O3 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ffast-math

external: $(EXTERNAL)
$(EXTERNAL): cz4~.c cz_core.c cz_core.h
	$(CC) $(CFLAGS) -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" $(SHARED) -o '$@' cz4~.c cz_core.c $(LDLIBS)

bela: cz4~.c cz_core.c cz_core.h
	$(CC) $(BELA_FLAGS) -std=c99 -Wall -fPIC -fvisibility=hidden -I"$(PDINCLUDE)" -shared \
	    -o 'cz4~.pd_linux' cz4~.c cz_core.c -lm

test: cz_test
cz_core.o: cz_core.c cz_core.h
	$(CC) $(CFLAGS) $(CORE_FLAGS) -c -o $@ cz_core.c
cz_test: cz_test.c cz_core.o cz_core.h
	$(CC) $(CFLAGS) -o $@ cz_test.c cz_core.o $(LDLIBS)

demos: cz_test
	mkdir -p demos && ./cz_test demos

help:
	python3 tools/gen_help.py 'cz4~-help.pd'

clean:
	rm -f cz_test cz_core.o 'cz4~.pd_linux' 'cz4~.pd_darwin'
	rm -rf demos

.PHONY: external bela test demos help clean
