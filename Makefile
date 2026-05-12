.DEFAULT_GOAL := host

BUILD_HOST ?= build/host
BUILD_PSP ?= build/psp
PSP_PACKAGE ?= build/package/glyph
FIXTURE_DIR ?= build/fixtures
HOST_TARGET ?= glyph
PSP_TARGET ?= glyph
TINY_EPUB ?= $(FIXTURE_DIR)/tiny.epub

CMAKE ?= cmake
CTEST ?= ctest
PYTHON ?= python3
PSP_CMAKE ?= psp-cmake
CLANG_FORMAT ?= clang-format

SRC_FORMAT_FILES := $(shell find src tests -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) 2>/dev/null)

.PHONY: help
help:
	@printf '%s\n' \
		'Targets:' \
		'  make check-deps     Check Linux/macOS host and PSP tool dependencies' \
		'  make host           Configure and build host SDL2 app' \
		'  make run-host       Build and run host SDL2 app' \
		'  make test           Build host app/tests and run CTest' \
		'  make fixture-epub   Generate a deterministic tiny EPUB fixture' \
		'  make psp            Configure and build PSP EBOOT.PBP via psp-cmake' \
		'  make package-psp    Build PSP install folder with assets and sample book' \
		'  make sample-book    Generate books/glyph-sample.epub for local smoke tests' \
		'  make run-ppsspp     Run PPSSPP harness detection or smoke launch' \
		'  make format         Apply clang-format to source files' \
		'  make check-format   Verify clang-format without modifying files' \
		'  make clean          Remove build directories'

.PHONY: check-deps
check-deps:
	$(PYTHON) tools/dev/check_deps.py

.PHONY: host
host:
	@command -v $(CMAKE) >/dev/null || { echo 'Missing cmake. Run make check-deps.'; exit 1; }
	$(CMAKE) -S . -B $(BUILD_HOST) -DGLYPH_BUILD_PSP=OFF -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build $(BUILD_HOST) --target $(HOST_TARGET)

.PHONY: run-host
run-host: host
	$(BUILD_HOST)/$(HOST_TARGET)

.PHONY: test
test:
	@command -v $(CMAKE) >/dev/null || { echo 'Missing cmake. Run make check-deps.'; exit 1; }
	$(CMAKE) -S . -B $(BUILD_HOST) -DGLYPH_BUILD_PSP=OFF -DGLYPH_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build $(BUILD_HOST)
	$(CTEST) --test-dir $(BUILD_HOST) --output-on-failure

.PHONY: fixture-epub
fixture-epub:
	$(PYTHON) tools/fixtures/make_tiny_epub.py --output "$(TINY_EPUB)" --force --check --sha256

.PHONY: psp
psp:
	@command -v $(PSP_CMAKE) >/dev/null || { echo 'Missing psp-cmake. Run make check-deps.'; exit 1; }
	mkdir -p $(BUILD_PSP)
	cd $(BUILD_PSP) && $(PSP_CMAKE) ../.. -DGLYPH_BUILD_PSP=ON -DCMAKE_BUILD_TYPE=MinSizeRel
	$(CMAKE) --build $(BUILD_PSP) --target $(PSP_TARGET)

.PHONY: run-ppsspp
run-ppsspp:
	@if [ -x tools/harness/run_ppsspp_smoke.py ]; then \
		if [ -f "$(BUILD_PSP)/EBOOT.PBP" ]; then \
			$(PYTHON) tools/harness/run_ppsspp_smoke.py --eboot "$(BUILD_PSP)/EBOOT.PBP"; \
		else \
			echo 'No PSP EBOOT found at $(BUILD_PSP)/EBOOT.PBP; running PPSSPP detection only.'; \
			$(PYTHON) tools/harness/run_ppsspp_smoke.py; \
		fi; \
	else \
		echo 'PPSSPP harness not present.'; \
	fi

.PHONY: sample-book
sample-book:
	$(PYTHON) tools/fixtures/make_tiny_epub.py --output books/glyph-sample.epub --force --check

.PHONY: package-psp
package-psp: psp sample-book
	rm -rf "$(PSP_PACKAGE)"
	mkdir -p "$(PSP_PACKAGE)/assets/fonts" "$(PSP_PACKAGE)/books"
	cp "$(BUILD_PSP)/EBOOT.PBP" "$(PSP_PACKAGE)/EBOOT.PBP"
	cp books/glyph-sample.epub "$(PSP_PACKAGE)/books/glyph-sample.epub"
	cp 'assets/fonts/AtkinsonHyperlegibleNext[wght].ttf' "$(PSP_PACKAGE)/assets/fonts/"
	cp 'assets/fonts/AtkinsonHyperlegibleNext-Italic[wght].ttf' "$(PSP_PACKAGE)/assets/fonts/"
	cp assets/fonts/OFL.txt assets/fonts/README.md "$(PSP_PACKAGE)/assets/fonts/"

.PHONY: format
format:
	@command -v $(CLANG_FORMAT) >/dev/null || { echo 'Missing clang-format. Run make check-deps.'; exit 1; }
	@if [ -n "$(SRC_FORMAT_FILES)" ]; then $(CLANG_FORMAT) -i $(SRC_FORMAT_FILES); fi

.PHONY: check-format
check-format:
	@command -v $(CLANG_FORMAT) >/dev/null || { echo 'Missing clang-format. Run make check-deps.'; exit 1; }
	@if [ -n "$(SRC_FORMAT_FILES)" ]; then $(CLANG_FORMAT) --dry-run --Werror $(SRC_FORMAT_FILES); fi

.PHONY: clean
clean:
	rm -rf build build-host build-psp
