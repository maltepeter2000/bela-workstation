# eps~ -- Ensoniq EPS / EPS-16 PLUS / ASR-10 instruments for Pure Data.
#
#   make                 build for this machine
#   make install         copy into Pd's externals folder (see PDLIBDIR)
#   make test            engine tests (synthetic disk images, no Ensoniq data needed)
#
# Needs no data files to build. To play, put Ensoniq instruments (EFE) or disk images
# (IMG, EDE, EDA, GKH) into a folder eps/ beside your patch (or beside the external).

ENGINE = engine/eps_loader.cpp engine/es5505.cpp engine/eps_sampler.cpp engine/eps_edit.cpp engine/eps_fs.cpp engine/eps_sysex.cpp engine/eps_record.cpp engine/eps_wave.cpp engine/eps_surface.cpp engine/es5510.cpp engine/eps_fx.cpp engine/eps_bank.cpp
# the 68000 of the effect machine (Musashi, MIT; C): compiled as C, linked in
MUSASHI = ../third_party/musashi/m68kcpu.c ../third_party/musashi/m68kops.c
MUSASHI_FLAGS = -DM68K_INSTRUCTION_HOOK=M68K_OPT_ON -I../third_party/musashi

lib.name = eps~
class.sources = eps~.cpp
common.sources = $(ENGINE) $(MUSASHI)
datafiles = eps~-help.pd epscc.pd epspads.pd epsedit.pd lkdisplay.pd eps.i~.pd eps.i~-help.pd

cflags = -Iengine $(MUSASHI_FLAGS) -fvisibility=hidden -pthread
cxx.flags = -std=c++17
ldlibs = -pthread
suppress-wunused = yes

# libc++ needs a newer deployment target than pd-lib-builder's default.
ifeq ($(shell uname -s),Darwin)
cflags += -mmacosx-version-min=10.13
endif

PDLIBBUILDER_DIR = ../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

SOURCES = eps~.cpp eps_params.h eps_lk.inc $(ENGINE) $(wildcard engine/*.h)

# ---- macOS package, cross-built from Linux -----------------------------------
#   make mac-zig    universal binary (arm64 + x86_64) and the patches into ../bin/macos
# Needs zig (pip install ziglang) and llvm-lipo.
ZIG  ?= python3 -m ziglang
LIPO ?= llvm-lipo-18
MACDIR = ../bin/macos
mac-zig: $(SOURCES)
	mkdir -p $(MACDIR)
	for a in aarch64 x86_64; do \
	  for m in $(MUSASHI); do \
	    $(ZIG) cc -target $$a-macos.11.0 -O2 -w -fPIC $(MUSASHI_FLAGS) -c $$m -o ../build/mac-$$a-$$(basename $$m .c).o || exit 1; \
	  done; \
	  $(ZIG) c++ -target $$a-macos.11.0 -O2 -std=c++17 -Iengine -I../third_party/pd $(MUSASHI_FLAGS) \
	    -fPIC -fvisibility=hidden -shared -Wl,-undefined,dynamic_lookup \
	    -o $(MACDIR)/eps.$$a 'eps~.cpp' $(ENGINE) ../build/mac-$$a-*.o || exit 1; \
	done
	$(LIPO) -create $(MACDIR)/eps.aarch64 $(MACDIR)/eps.x86_64 -output '$(MACDIR)/eps~.pd_darwin'
	rm -f $(MACDIR)/eps.aarch64 $(MACDIR)/eps.x86_64
	cp 'eps~-help.pd' epscc.pd epspads.pd epsedit.pd lkdisplay.pd 'eps.i~.pd' 'eps.i~-help.pd' $(MACDIR)/

# ---- Bela Gem (64-bit ARM, Cortex-A53), cross-built from Linux ------------------
#   make bela-gem   eps~.pd_linux and the abstractions into ../../../build/bela-gem (all
#                   instruments side by side; make dist puts them on the branch pd)
# glibc 2.31 or newer on the board, libc++ linked in. On the board itself a plain make works too.
GEMDIR = ../../../build/bela-gem
bela-gem: $(SOURCES)
	mkdir -p $(GEMDIR)
	for m in $(MUSASHI); do \
	  $(ZIG) cc -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -w -fPIC $(MUSASHI_FLAGS) -c $$m -o ../build/gem-$$(basename $$m .c).o || exit 1; \
	done
	$(ZIG) c++ -target aarch64-linux-gnu.2.31 -mcpu=cortex_a53 -O2 -std=c++17 \
	  -Iengine -I../third_party/pd -pthread $(MUSASHI_FLAGS) \
	  -fPIC -fvisibility=hidden -shared -s -o '$(GEMDIR)/eps~.pd_linux' 'eps~.cpp' $(ENGINE) ../build/gem-*.o
	cp epscc.pd epspads.pd lkdisplay.pd $(GEMDIR)/

# ---- generated files ---------------------------------------------------------
#   make gen        parameter header, controller layer, pads, editor, help patch and eps.i~ (last)
gen:
	python3 ../tools/gen_eps_tables.py
	python3 ../tools/eps_params.py
	python3 ../tools/eps_edit_params.py
	python3 ../tools/eps_pages.py
	python3 ../tools/gen_epscc.py
	python3 ../tools/gen_eps_editor.py
	python3 ../tools/gen_eps_help.py
	python3 ../../../common/tools/lkdisplay.py lkdisplay.pd
	python3 ../tools/gen_eps_inst.py

# ---- tests of the engine (loader, ES5505, sampler) ----------------------------
#   make test       synthetic fixtures (tests/make_fixtures.py), ASan/UBSan
# the 68000 for the tests and tools (C, built once)
MUSASHI_O = ../build/m68kcpu.o ../build/m68kops.o
../build/%.o: ../third_party/musashi/%.c
	mkdir -p ../build
	$(CC) -O2 -w $(MUSASHI_FLAGS) -c $< -o $@

TESTDIR = ../tests
FIX = ../build/fixtures
SAN = -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all
test: $(MUSASHI_O) $(ENGINE) $(wildcard engine/*.h) $(wildcard $(TESTDIR)/*.cpp)
	mkdir -p $(FIX)
	python3 $(TESTDIR)/make_fixtures.py $(FIX)
	$(CXX) $(SAN) -o ../build/test_loader $(TESTDIR)/test_loader.cpp engine/eps_loader.cpp
	$(CXX) $(SAN) -o ../build/fuzz_loader $(TESTDIR)/fuzz_loader.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	$(CXX) $(SAN) -o ../build/render_test $(TESTDIR)/render_test.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	../build/test_loader $(FIX)
	../build/fuzz_loader $(FIX)
	../build/render_test $(FIX)
	$(CXX) $(SAN) -o ../build/roundtrip $(TESTDIR)/roundtrip.cpp engine/eps_loader.cpp
	$(CXX) $(SAN) -o ../build/test_fs $(TESTDIR)/test_fs.cpp engine/eps_fs.cpp engine/eps_loader.cpp
	../build/roundtrip $(FIX)/test.efe $(FIX)/test.img $(FIX)/test.ede
	../build/test_fs $(FIX)
	$(CXX) $(SAN) -o ../build/test_record $(TESTDIR)/test_record.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	../build/test_record $(FIX)
	$(CXX) $(SAN) -o ../build/test_wave $(TESTDIR)/test_wave.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	../build/test_wave
	$(CXX) $(SAN) -o ../build/test_surface $(TESTDIR)/test_surface.cpp engine/eps_surface.cpp
	../build/test_surface
	$(CXX) $(SAN) -o ../build/test_es5510 $(TESTDIR)/test_es5510.cpp engine/es5510.cpp
	../build/test_es5510
	$(CXX) $(SAN) -o ../build/test_bank $(TESTDIR)/test_bank.cpp engine/eps_bank.cpp
	../build/test_bank

# Tools: eps_dump (list an image, show an instrument, write its wavesamples as WAV),
# demo_real (render a demo of an instrument file), read_all_files (check an image's FAT),
# eps_fs_tool (write into an image: ls, mkdir, put, check, undo).
tools: $(ENGINE) $(MUSASHI_O)
	mkdir -p ../build
	$(CXX) -std=c++17 -O2 -o ../build/eps_dump $(TESTDIR)/eps_dump.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	$(CXX) -std=c++17 -O2 -o ../build/demo_real $(TESTDIR)/demo_real.cpp $(ENGINE) $(MUSASHI_O) $(MUSASHI_FLAGS)
	$(CXX) -std=c++17 -O2 -o ../build/fxprobe $(TESTDIR)/fxprobe.cpp engine/eps_fx.cpp engine/es5510.cpp $(MUSASHI_O) $(MUSASHI_FLAGS)
	$(CXX) -std=c++17 -O2 -o ../build/read_all_files $(TESTDIR)/read_all_files.cpp engine/eps_loader.cpp
	$(CXX) -std=c++17 -O2 -o ../build/eps_fs_tool $(TESTDIR)/eps_fs_tool.cpp engine/eps_fs.cpp engine/eps_loader.cpp

.PHONY: mac-zig bela-gem gen test tools
