# tx81z: every target is made in pd/ (the Pd package: external, abstractions, help patch).
#   make            build for this machine (into pd/)
#   make mac-zig    universal macOS binary and the patches into bin/macos (zig, llvm-lipo)
#   make bela-gem   cross-built for the Bela Gem (zig) into ../../build/bela-gem (all instruments
#                   side by side, the rack's folder) and into the Bela project(s) in bela/
#   make gen        the generated files (parameter header, controller layer, pads, editor, help)
#   make clean
TARGETS = all mac-zig bela-gem gen clean
$(TARGETS):
	$(MAKE) -C pd $@
.PHONY: $(TARGETS)
