# Dependencies

Oyama's HRT Tracker is a native Qt 6 / PostgreSQL desktop application. Linux,
FreeBSD, and OpenBSD builds are produced separately on each platform; their
binaries are not interchangeable. This page explains the dependencies used by
the top-level `Makefile`; the finer CMake options are in the main
[`README.md`](../README.md).

## Common requirements

| Dependency | Version | Purpose |
| --- | --- | --- |
| CMake | 3.21+ | Configure and build |
| Ninja | A current release | Default `Makefile` generator |
| C compiler | C99 | PK core |
| C++ compiler | C++20 | Desktop app and storage layer |
| PostgreSQL | 14+ | Database server and libpq |
| libsodium | 1.0.18+ | Vault encryption (Argon2id, AES-GCM, XChaCha20-Poly1305) |
| nlohmann/json | 3.11+ | Storage and backup JSON parsing |
| Bash | 4+ | Integration test scripts only |
| Qt | 6.4+ | Core, Qml, Quick, QuickControls2 |

`initdb`, `pg_ctl`, and `postgres` are needed for the application-managed local
cluster and for integration tests. If they are not on `PATH`, pass the server
binary directory explicitly:

```sh
make test PG_BINDIR=/usr/local/pgsql/bin
```

If Qt is missing, the desktop shell is skipped automatically. To build only the
CLI, disable it explicitly:

```sh
cmake -S . -B build -DOYAMA_BUILD_QT_SHELL=OFF
```

## Package names

Package names change between releases; verify them with the platform's package
manager. These are known-good starting points.

### Gentoo

```sh
sudo emerge --ask dev-build/cmake dev-build/ninja dev-db/postgresql \
  dev-qt/qtbase:6 dev-qt/qtdeclarative:6
```

### Arch Linux

```sh
sudo pacman -S --needed cmake ninja postgresql postgresql-libs \
  qt6-base qt6-declarative
```

### Debian / Ubuntu

```sh
sudo apt install cmake ninja-build build-essential libpq-dev \
  libsodium-dev nlohmann-json3-dev postgresql \
  qt6-base-dev qt6-declarative-dev
```

### FreeBSD

```sh
sudo pkg install cmake ninja git-lite libsodium nlohmann-json \
  postgresql18-client postgresql18-server \
  qt6-base qt6-declarative noto-sc bash
```

`noto-sc` provides the Simplified Chinese UI font. A Plasma X11 session also
needs `plasma6-kwin-x11`; `plasma6-kwin` alone provides only the Wayland
compositor on FreeBSD.

### OpenBSD

```sh
doas pkg_add cmake ninja libsodium nlohmann-json \
  postgresql-client postgresql-server \
  qt6-qtbase qt6-qtdeclarative noto-cjk bash
```

OpenBSD package names vary by release and repository. Build natively on an
OpenBSD host or VM; do not run PostgreSQL test clusters as root.

`pkg_add` accepts package names, not ports paths. For example,
`pkg_add textproc/nlohmann-json` fails with `file:textproc/: empty`; use
`pkg_add nlohmann-json` instead.

OpenBSD's base clang does not add `/usr/local/include` to the header search
path. This project adds it in its OpenBSD CMake block, so the command above
works as written. If you configure an older checkout or a custom build and see
`nlohmann/json.hpp` not found, add
`-DCMAKE_CXX_FLAGS="-I/usr/local/include"` to the CMake configure command.

## Build and install

The top-level `Makefile` wraps the normal CMake workflow:

```sh
make                 # configure and build the default target
make JOBS=2          # limit parallel compilation
make test            # run CTest
sudo make install    # install under /usr/local by default
make PREFIX=/opt/oyama-hrt
make clean
```

The equivalent direct CMake commands are:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DOYAMA_BUILD_QT_SHELL=ON
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The 0.5.0 release suite has 32 tests. Integration tests return `77` (skip) when
the PostgreSQL server tools are unavailable. OpenBSD must be rebuilt and tested
on an OpenBSD host or VM; a Linux result is not evidence for an OpenBSD binary.

The integration tests create their own throwaway clusters under `TMPDIR` and
set their own XDG paths. If they fail with state-related messages such as
`schema_migrations already exists` or `managed legacy data must start
uninitialized`, rerun with a clean `TMPDIR` and make sure `OYAMA_CONFIG_FILE` is
not exported in your shell; a stale value from manual testing can override the
test configuration. Use `ctest --test-dir build -V` to capture the exact
scenario and cluster path.

## Runtime requirements

- **Linux:** glibc, Qt 6 runtime libraries, libpq 14+, and a local PostgreSQL
  server.
- **FreeBSD:** FreeBSD 15.x, Qt 6, libpq, PostgreSQL, and an X11 window manager
  for the desktop shell.
- **OpenBSD:** Qt 6, libpq, and PostgreSQL from the native package set.

The application is local-only: it has no account system, telemetry, or cloud
sync. The managed cluster listens only on a Unix socket and does not open a TCP
port.

Migrations are located in this order:

1. `OYAMA_MIGRATIONS_DIR`
2. the source-tree path recorded at build time
3. `share/oyama-hrt/migrations` under the install prefix
4. `storage/migrations` relative to the current directory

For a relocatable release directory, set the variable explicitly:

```sh
export OYAMA_MIGRATIONS_DIR="$PWD/share/oyama-hrt/migrations"
```
