# eps: every target is made in pd/ (the Pd package: external, abstractions, help patch).
#   make            build for this machine (into pd/)
#   make install    into Pd's externals folder (pd-lib-builder, see PDLIBDIR)
#   make mac-zig    universal macOS binary and the patches into bin/macos (zig, llvm-lipo)
#   make bela-gem   cross-built for the Bela Gem (zig) into ../../build/bela-gem (all instruments
#                   side by side, the rack's folder)
#   make gen        the generated files (parameter header, controller layer, pads, editor, help)
#   make test       engine tests with synthetic disk images
#   make tools      eps_dump, demo_real, read_all_files into build/
#   make clean
TARGETS = all install mac-zig bela-gem gen test tools clean
$(TARGETS):
	$(MAKE) -C pd $@
.PHONY: $(TARGETS)
