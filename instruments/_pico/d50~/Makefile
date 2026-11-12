# d50~ -- Roland D-50 external for Pure Data.
#
#   make                 build for this machine
#   make install         copy into Pd's externals folder (see PDLIBDIR)
#
# The engine needs generated/d5_pcm_table.h, which tools/make_d50_data.py
# writes together with d50_pcm.bin from your own ROM images.

lib.name = d50~
class.sources = d50~.cpp
common.sources = d50_core.cpp
datafiles = d50~-help.pd

cflags = -std=c++17 -DD5_HOST_BUILD -Iengine -Igenerated -fvisibility=hidden
suppress-wunused = yes

ifeq ($(wildcard generated/d5_pcm_table.h),)
$(error generated/d5_pcm_table.h is missing: run "python3 ../tools/make_d50_data.py <folder with your D-50 ROM images>" first)
endif

PDLIBBUILDER_DIR = ../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# The engine's headers are large inline functions by design.
warn.flags = -Wall -Wextra -Wshadow -Wstrict-aliasing -Wno-cast-function-type
