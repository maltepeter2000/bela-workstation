# bela-workstation: the same target in every instrument (see README.md "Building").
#   make bela-gem   all instruments cross-built for the Bela Gem into build/bela-gem
#   make mac-zig    all Mac packages (instruments/*/bin/macos)
#   make gen        all generated files
#   make dist       the branch pd (one folder for Pd's externals, git pull): mac-zig, bela-gem,
#                   then common/tools/dist.py; make dist-push pushes it
#   make all, make clean
INSTRUMENTS = juno d50 cz4 pdoly8 tx81z
TARGETS = all mac-zig bela-gem gen clean
$(TARGETS):
	@for i in $(INSTRUMENTS); do $(MAKE) -C instruments/$$i $@ || exit 1; done
	@if [ "$@" = gen ]; then python3 rack/gen_rack.py; fi
.PHONY: $(TARGETS)

dist: mac-zig bela-gem
	python3 common/tools/dist.py
dist-push: dist
	python3 common/tools/dist.py --push
.PHONY: dist dist-push
