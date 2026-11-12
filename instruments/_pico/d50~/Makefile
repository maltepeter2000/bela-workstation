# d50~ -- Roland D-50 external for Pure Data.
#
#   make                 build for this machine
#   make install         copy into Pd's externals folder (see PDLIBDIR)
#
# Building needs no ROM data. To make sound the object needs d50_pcm.bin,
# which tools/make_d50_data.py writes from your own ROM images.

lib.name = d50~
class.sources = d50~.cpp
common.sources = d50_core.cpp
datafiles = d50~-help.pd

cflags = -std=c++17 -DD5_HOST_BUILD -Iengine -fvisibility=hidden
suppress-wunused = yes

# libc++ needs a newer deployment target than pd-lib-builder's default.
ifeq ($(shell uname -s),Darwin)
cflags += -mmacosx-version-min=10.13
endif

PDLIBBUILDER_DIR = ../pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

# The engine's headers are large inline functions by design.
CXXFLAGS := $(filter-out -Winline,$(CXXFLAGS)) -Wno-cast-function-type
