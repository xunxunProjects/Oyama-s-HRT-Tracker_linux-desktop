# Portable build wrapper for Linux, FreeBSD, and OpenBSD.
#
# Everything here is intentionally plain Make: no GNU-only functions are
# required. The real build configuration remains CMake; this is the short path
# documented in docs/dependencies.md.
#
#   make                 configure + Release build
#   make test            build, then run CTest
#   sudo make install    install to PREFIX (default /usr/local)
#   make clean           remove BUILD_DIR

CMAKE ?= cmake
BUILD_DIR ?= build
BUILD_TYPE ?= Release
PREFIX ?= /usr/local
PG_BINDIR ?=
JOBS ?= 2

.PHONY: all configure build test install clean

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -G Ninja \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DOYAMA_BUILD_QT_SHELL=ON \
		-DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)

test: build
	PG_BINDIR="$(PG_BINDIR)" ctest --test-dir $(BUILD_DIR) --output-on-failure

install: build
	$(CMAKE) --install $(BUILD_DIR)

clean:
	rm -rf "$(BUILD_DIR)"
