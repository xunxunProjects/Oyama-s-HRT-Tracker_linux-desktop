# Dependencies

Oyama's HRT Tracker is a native Qt 6 / PostgreSQL desktop application. Linux,
FreeBSD, OpenBSD, and NetBSD builds are produced separately on each platform;
their binaries are not interchangeable. This page explains the dependencies used by
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

### NetBSD

```sh
su -
export PKG_PATH=https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/x86_64/11.0_2026Q3/All
pkg_add cmake ninja-build pkgconf libsodium nlohmann-json \
  postgresql18-client postgresql18-server \
  qt6-qtbase qt6-qtdeclarative noto-cjk-fonts bash
```

The pkgsrc repository path is release-specific. The example above is for
NetBSD 11.0; use the matching `11.0_YYYYQn` directory for another release.
NetBSD package archives use `.tgz` names but are XZ-compressed.

Do not build or test as root: `initdb` refuses to run with root privileges.
The full integration suite starts several PostgreSQL clusters at once. Raise
the SysV semaphore limits before running `make test`, and persist them in
`/etc/sysctl.conf` if the VM will run the suite repeatedly:

```sh
sysctl -w kern.ipc.semmni=128
sysctl -w kern.ipc.semmns=1024
sysctl -w kern.ipc.semmnu=256
```

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
make SECURITY=0      # security profile off (debug only)
make test-both       # build and test both security variants
sudo make install    # install under /usr/local by default
make PREFIX=/opt/oyama-hrt
make package PACKAGE=dist/oyama-hrt-netbsd-amd64.tar.gz
make clean
```

`make package` stages the files under `STAGE_DIR` (default `dist/stage`) using
`DESTDIR`, then writes a gzip tarball. Set `PACKAGE` to a platform-specific
artifact name; the default is `dist/oyama-hrt.tar.gz`.

### Security profiles

`OYAMA_SECURITY_PROFILE` selects `off`, `standard`, or `full`; the default is
`standard`. `standard` and `full` enable stack protection,
`_FORTIFY_SOURCE=2`, and linker hardening (`-Wl,-z,relro,-z,now`). `off`
disables those extra modules for a debug build but still uses the vault. See
`docs/security/profiles.md` for the full feature table.

The Makefile defaults to `SECURITY=1`, which selects `standard`. Set
`SECURITY=0` to select `off`. `OYAMA_SECURITY_MODE` is derived from the profile
and no longer needs to be passed directly.

### Build profiles

The default build uses Qt 6.4 and C++20-compatible sources compiled as C++11
(`OYAMA_QT_MAJOR=6`). Select the Qt major with `-DOYAMA_QT_MAJOR=5` for the
optional Qt 5.15.2 compatibility build, and the C++ standard with
`-DCMAKE_CXX_STANDARD=11` or `-DCMAKE_CXX_STANDARD=20`. See
`docs/security/profiles.md` for the security profiles and
`CMakePresets.json` for the named configurations.

The equivalent direct CMake commands are:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DOYAMA_BUILD_QT_SHELL=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Use `su` instead of `sudo` on a base NetBSD system where `sudo` is not
installed. `-j2` is deliberately portable; raise it only when the host has
enough memory, because each integration test can start a PostgreSQL cluster.

The current release suite has 34 tests. Integration tests return `77` (skip) when
the PostgreSQL server tools are unavailable. OpenBSD and NetBSD must be rebuilt
and tested on their native hosts or VMs; a Linux result is not evidence for
either binary.

The integration tests create their own throwaway clusters under `TMPDIR` and
set their own XDG paths. If they fail with state-related messages such as
`schema_migrations already exists` or `managed legacy data must start
uninitialized`, rerun with a clean `TMPDIR` and make sure `OYAMA_CONFIG_FILE` is
not exported in your shell; a stale value from manual testing can override the
test configuration. Use `ctest --test-dir build -V` to capture the exact
scenario and cluster path.

### Vendored build

The default build uses the system packages listed above. To build the large
dependencies from the pinned sources instead:

```sh
scripts/bootstrap-third-party.sh           # online, verifies every hash
scripts/bootstrap-third-party.sh --offline # from third_party/cache
cmake -S . -B build-vendored -G Ninja \
  -DOYAMA_THIRD_PARTY=vendored -DCMAKE_BUILD_TYPE=Release
```

The vendored prefix lives in `third_party/prefix` and is not committed.
`OYAMA_THIRD_PARTY=system` remains the default for distribution packaging.

## Runtime requirements

- **Linux:** glibc, Qt 6 runtime libraries, libpq 14+, and a local PostgreSQL
  server.
- **FreeBSD:** FreeBSD 15.x, Qt 6, libpq, PostgreSQL, and an X11 window manager
  for the desktop shell.
- **OpenBSD:** Qt 6, libpq, and PostgreSQL from the native package set.
- **NetBSD:** NetBSD 11.0, Qt 6, libpq 14+, PostgreSQL 18 server and client
  from pkgsrc, and an X11 window manager for the desktop shell.

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
