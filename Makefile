# Portable build wrapper for Linux, FreeBSD, OpenBSD, and NetBSD.
#
# Everything here is intentionally plain Make: no GNU-only functions are
# required. The real build configuration remains CMake; this is the short path
# documented in docs/dependencies.md.
#
#   make                 configure + Release build (standard profile)
#   make SECURITY=0      configure + Release build (security profile off)
#   make test            build, then run CTest
#   make test-both       build and test both security variants
#   sudo make install    install to PREFIX (default /usr/local)
#   make package         stage the install and create PACKAGE tarball
#   make clean           remove BUILD_DIR

CMAKE ?= cmake
BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= /usr/local
PG_BINDIR ?=
JOBS ?= 2
STAGE_DIR ?= dist/stage
PACKAGE ?= dist/oyama-hrt.tar.gz
SECURITY ?= 1

SECURITY_FLAG = -DOYAMA_SECURITY_PROFILE=$(if $(filter 1,$(SECURITY)),standard,off)

.PHONY: all configure build test test-both test-security-off install package clean

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		$(SECURITY_FLAG) \
		-DOYAMA_BUILD_QT_SHELL=ON \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)

test: build
	PG_BINDIR="$(PG_BINDIR)" ctest --test-dir $(BUILD_DIR) --output-on-failure

test-security-off:
	$(MAKE) SECURITY=0 BUILD_DIR=build-insecure clean
	$(MAKE) SECURITY=0 BUILD_DIR=build-insecure configure
	$(MAKE) SECURITY=0 BUILD_DIR=build-insecure build
	PG_BINDIR="$(PG_BINDIR)" ctest --test-dir build-insecure --output-on-failure

test-both: test test-security-off

install: build
	$(CMAKE) --install $(BUILD_DIR)

package: build
	rm -rf "$(STAGE_DIR)" "$(PACKAGE)"
	mkdir -p "$(STAGE_DIR)" dist
	DESTDIR="$(STAGE_DIR)" $(CMAKE) --install $(BUILD_DIR)
	tar -C "$(STAGE_DIR)" -czf "$(PACKAGE)" .

clean:
	rm -rf "$(BUILD_DIR)" build-insecure
