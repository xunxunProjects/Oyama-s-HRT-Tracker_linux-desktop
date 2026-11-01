# Phase 0 — Supply Chain and Security Profiles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the build reproducible from pinned, checksum-verified third-party
sources and add the `off` / `standard` / `full` security-profile switch without
changing application behavior.

**Architecture:** `third_party/versions.cmake` is the single source of truth for
versions, URLs, hashes, and patches. `scripts/third-party-info.cmake` prints one
dependency's fields for shell scripts. `scripts/bootstrap-third-party.sh`
downloads (or uses an offline cache), verifies, patches, and builds into
`third_party/prefix`. `cmake/OyamaThirdParty.cmake` prepends that prefix when
`OYAMA_THIRD_PARTY=vendored`. `OYAMA_SECURITY_PROFILE` selects `off`, `standard`,
or `full` and drives the existing compiler-hardening switch. No capture, panic,
or keyd code is added in this phase.

**Tech Stack:** CMake 3.21+, Bash 4+, Ninja, C99/C++20 (the C++11 port is Phase
A), PostgreSQL 14.x, libsodium 1.0.20, nlohmann/json 3.11.3, Qt 6.4.3 (default)
and Qt 5.15.2 (optional compatibility), Xen 4.17.5 (full profile only).

**Spec:** `docs/superpowers/specs/2026-11-02-security-architecture-design.md`

**Roadmap:** `docs/superpowers/plans/2026-11-02-security-hardening-roadmap.md`

## Global Constraints

- **One source of truth.** Every version, URL, SHA-256, and patch path lives in
  `third_party/versions.cmake`. Scripts and CMake read it; no second list.
- **Offline is a hard mode.** `scripts/bootstrap-third-party.sh --offline` must
  never access the network. A missing archive is an error, not a fallback.
- **No `sudo`.** Bootstrap installs only into `third_party/prefix` or the
  caller-supplied `--prefix`.
- **Hash before extract.** A downloaded archive is hashed before any extraction
  or patch step. A mismatch aborts the whole run.
- **No tracked build output.** `third_party/cache/`, `third_party/build/`, and
  `third_party/prefix/` are gitignored.
- **Profile isolation.** `off` must define `OYAMA_SECURITY_PROFILE_OFF=1` and
  must not enable compiler hardening. `standard` is the default. `full` enables
  the same compiler hardening as `standard`; its extra modules arrive in later
  phases.
- **Do not change runtime behavior in this phase.** The vault, storage layer,
  CLI commands, and QML shell keep their current behavior. The only new CLI
  output is `version --verbose`.
- **Qt 6.4 stays the default.** `OYAMA_QT_MAJOR=5` is only a compatibility
  switch added in Phase A. Phase 0 pins both source archives but does not
  require Qt 5 for the default build.
- **Existing test command:** `cmake --build build -j && ctest --test-dir build
  --output-on-failure`. Use `PG_BINDIR=/tmp/opencode/pg18/bin` when the
  PostgreSQL server tools are not on `PATH`.
- **Commit per task.** Do not mix formatting-only changes with behavior changes.

## Review Focus

1. **Offline bootstrap with an empty cache** — must fail with the missing
   archive name and exit non-zero, never attempt a download. Test in Task 0.2.
2. **Hash mismatch** — a corrupted cache file must abort before extraction and
   name the expected and actual hashes. Test in Task 0.1.
3. **Vendored prefix without a stamp** — `-DOYAMA_THIRD_PARTY=vendored` must
   fail with the bootstrap command in the message, not fall back to system
   libraries. Test in Task 0.3.
4. **Profile/`OYAMA_SECURITY_MODE` interaction** — `off` must disable compiler
   hardening even if the cache still says `OYAMA_SECURITY_MODE=ON`; `standard`
   and `full` must enable it even if the cache says `OFF`. Test in Task 0.4.
5. **Qt 5 archive is optional** — a default Qt 6 build must not download or
   require the Qt 5 archive. Test in Task 0.2 and Task 0.3.
6. **SBOM coverage** — every dependency in `versions.cmake` appears exactly once
   in `sbom.json`, with its version and hash. Test in Task 0.5.

---

## Task 0.1: Pin versions and hashes

**Files:**
- Create: `third_party/versions.cmake`
- Create: `scripts/third-party-info.cmake`
- Create: `scripts/verify-third-party.sh`
- Create: `scripts/tests/verify_third_party_test.sh`
- Create: `third_party/README.md`
- Modify: `.gitignore`
- Modify: `CMakeLists.txt` (add the `supply_chain_hashes` test)

**Interfaces:**
- Produces: `OYAMA_QT6_VERSION`, `OYAMA_QT6_URL`, `OYAMA_QT6_SHA256`,
  `OYAMA_QT6_PATCHES`; the same quartet for `QT5`, `PG`, `SODIUM`, `JSON`, and
  `XEN`.
- Produces: `scripts/third-party-info.cmake`, invoked as
  `cmake -DNAME=<dep> -P scripts/third-party-info.cmake`, which prints
  `VERSION=...`, `URL=...`, `SHA256=...`, `PATCHES=...` one per line.
- Produces: `scripts/verify-third-party.sh [--versions FILE] [--cache DIR]
  [dep...]`; exits 0 on match, 1 on mismatch/missing/unpinned, 77 when the cache
  directory is absent or empty.

- [ ] **Step 1: Write the failing shell test**

Create `scripts/tests/verify_third_party_test.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/oyama-verify.XXXXXX")"
trap 'rm -rf "$work"' EXIT

printf 'payload\n' >"$work/test.tar.gz"
sha="$(sha256sum "$work/test.tar.gz" | awk '{print $1}')"

cat >"$work/versions.cmake" <<EOF
set(OYAMA_TEST_VERSION "1.0")
set(OYAMA_TEST_URL "https://example.invalid/test-1.0.tar.gz")
set(OYAMA_TEST_SHA256 "$sha")
set(OYAMA_TEST_PATCHES "")
EOF

mkdir -p "$work/cache"
cp "$work/test.tar.gz" "$work/cache/test-1.0.tar.gz"

# Match: exit 0.
"$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test

# Mismatch: exit 1 and name both hashes.
printf 'tampered\n' >"$work/cache/test-1.0.tar.gz"
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test \
    >"$work/out" 2>&1; then
    echo "expected mismatch to fail" >&2
    exit 1
fi
grep -q "$sha" "$work/out"
grep -q "MISMATCH" "$work/out"

# Missing archive: exit 1.
rm "$work/cache/test-1.0.tar.gz"
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test \
    >"$work/out" 2>&1; then
    echo "expected missing archive to fail" >&2
    exit 1
fi
grep -q "MISSING" "$work/out"

# Empty cache: exit 77.
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test \
    >"$work/out" 2>&1; then
    echo "expected empty cache to skip" >&2
    exit 1
else
    status=$?
fi
test "$status" -eq 77
grep -q "SKIP" "$work/out"

echo "verify_third_party_test: PASS"
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash scripts/tests/verify_third_party_test.sh`
Expected: FAIL because `scripts/verify-third-party.sh` does not exist.

- [ ] **Step 3: Implement the pin file, info script, verifier, and ignore rules**

`third_party/versions.cmake`:

```cmake
# Pinned third-party sources. Update a version and its SHA-256 together.
# The hashes are filled from the official checksum files (or computed once
# from the official URL) and must never be edited by hand without a version
# change.

set(OYAMA_QT6_VERSION "6.4.3")
set(OYAMA_QT6_URL "https://download.qt.io/archive/qt/6.4/6.4.3/single/qt-everywhere-src-6.4.3.tar.xz")
set(OYAMA_QT6_SHA256 "<fill>")
set(OYAMA_QT6_PATCHES "")

set(OYAMA_QT5_VERSION "5.15.2")
set(OYAMA_QT5_URL "https://download.qt.io/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz")
set(OYAMA_QT5_SHA256 "<fill>")
set(OYAMA_QT5_PATCHES "patches/qt5-kde-5.15.2.tar.gz")

set(OYAMA_PG_VERSION "14.18")
set(OYAMA_PG_URL "https://ftp.postgresql.org/pub/source/v14.18/postgresql-14.18.tar.bz2")
set(OYAMA_PG_SHA256 "<fill>")

set(OYAMA_SODIUM_VERSION "1.0.20")
set(OYAMA_SODIUM_URL "https://download.libsodium.org/libsodium/releases/libsodium-1.0.20.tar.gz")
set(OYAMA_SODIUM_SHA256 "<fill>")

set(OYAMA_JSON_VERSION "3.11.3")
set(OYAMA_JSON_URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz")
set(OYAMA_JSON_SHA256 "<fill>")

set(OYAMA_XEN_VERSION "4.17.5")
set(OYAMA_XEN_URL "https://downloads.xenproject.org/release/xen/4.17.5/xen-4.17.5.tar.gz")
set(OYAMA_XEN_SHA256 "<fill>")
```

Fill every `<fill>` by downloading the archive from the pinned URL and running
`sha256sum <archive>`. For Qt 5, also pin the KDE Qt5 patch collection tarball
and its hash, and update `OYAMA_QT5_PATCHES` to that file name. The Qt 5 archive
and patch collection are optional for the default build.

`scripts/third-party-info.cmake`:

```cmake
# Usage: cmake -DNAME=qt6 -P scripts/third-party-info.cmake
if(NOT DEFINED NAME)
    message(FATAL_ERROR "NAME is required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../third_party/versions.cmake")
string(TOUPPER "${NAME}" _name)
foreach(_field VERSION URL SHA256 PATCHES)
    if(DEFINED OYAMA_${_name}_${_field})
        message("${_field}=${OYAMA_${_name}_${_field}}")
    endif()
endforeach()
```

`scripts/verify-third-party.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
versions="$root/third_party/versions.cmake"
cache="$root/third_party/cache"
deps=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --versions) versions="${2:?}"; shift 2 ;;
        --cache) cache="${2:?}"; shift 2 ;;
        -h|--help)
            echo "usage: verify-third-party.sh [--versions FILE] [--cache DIR] [dep...]"
            exit 0 ;;
        *) deps+=("$1"); shift ;;
    esac
done

if [[ ${#deps[@]} -eq 0 ]]; then
    deps=(qt6 qt5 pg sodium json xen)
fi

if [[ ! -d "$cache" ]]; then
    echo "SKIP: cache directory not present: $cache"
    exit 77
fi

status=0
found=0
for dep in "${deps[@]}"; do
    info="$(cmake -DNAME="$dep" -P "$root/scripts/third-party-info.cmake" 2>/dev/null || true)"
    if [[ -z "$info" ]]; then
        echo "UNKNOWN: no entry for $dep" >&2
        status=1
        continue
    fi
    url="$(sed -n 's/^URL=//p' <<<"$info")"
    expected="$(sed -n 's/^SHA256=//p' <<<"$info")"
    file="$cache/$(basename "$url")"

    if [[ ! -f "$file" ]]; then
        echo "MISSING: $file" >&2
        status=1
        continue
    fi
    found=1

    if [[ "$expected" == "<fill>" || -z "$expected" ]]; then
        echo "UNPINNED: $dep has no SHA-256 in versions.cmake" >&2
        status=1
        continue
    fi

    actual="$(sha256sum "$file" | awk '{print $1}')"
    if [[ "$actual" != "$expected" ]]; then
        echo "MISMATCH: $file" >&2
        echo "  expected $expected" >&2
        echo "  actual   $actual" >&2
        status=1
        continue
    fi
    echo "OK: $dep $(basename "$file")"
done

if [[ $found -eq 0 && $status -eq 0 ]]; then
    echo "SKIP: cache is empty: $cache"
    exit 77
fi

exit "$status"
```

Make it executable: `chmod +x scripts/verify-third-party.sh
scripts/tests/verify_third_party_test.sh`.

`.gitignore` additions:

```text
# Vendored third-party sources and build output
third_party/cache/
third_party/build/
third_party/prefix/
```

`third_party/README.md` explains the layout, the offline cache, how to fill
hashes, and that the prefix is never committed.

- [ ] **Step 4: Add the CTest and run both tests**

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
add_test(NAME supply_chain_hashes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify-third-party.sh)
set_tests_properties(supply_chain_hashes PROPERTIES SKIP_RETURN_CODE 77)
```

Run:

```bash
bash scripts/tests/verify_third_party_test.sh
ctest --test-dir build -R supply_chain_hashes --output-on-failure
```

Expected: the shell test prints `PASS`; CTest passes when the cache is populated
or reports `SKIP` (77) when it is absent.

- [ ] **Step 5: Commit**

```bash
git add third_party/versions.cmake third_party/README.md \
  scripts/third-party-info.cmake scripts/verify-third-party.sh \
  scripts/tests/verify_third_party_test.sh .gitignore CMakeLists.txt
git commit -m "build(supply-chain): pin third-party versions and hashes"
```

---

## Task 0.2: Bootstrap script

**Files:**
- Create: `scripts/bootstrap-third-party.sh`
- Create: `scripts/tests/bootstrap_offline_test.sh`
- Create: `third_party/patches/README.md`

**Interfaces:**
- Produces: `third_party/prefix/.stamp-<dep>` after each dependency and
  `third_party/prefix/.stamp` after every requested dependency succeeds.
- Consumes: `third_party/versions.cmake` through
  `scripts/third-party-info.cmake`.
- CLI: `bootstrap-third-party.sh [--offline] [--only DEP] [--prefix DIR]
  [--cache DIR] [--jobs N] [--with-xen]`.

- [ ] **Step 1: Write the failing offline test**

Create `scripts/tests/bootstrap_offline_test.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/oyama-bootstrap.XXXXXX")"
trap 'rm -rf "$work"' EXIT

if "$root/scripts/bootstrap-third-party.sh" \
    --offline --only json --cache "$work/cache" --prefix "$work/prefix" \
    >"$work/out" 2>&1; then
    echo "expected offline bootstrap with an empty cache to fail" >&2
    exit 1
fi
grep -q "json.tar.xz" "$work/out"
grep -q "offline" "$work/out"
echo "bootstrap_offline_test: PASS"
```

- [ ] **Step 2: Run it to verify it fails**

Run: `bash scripts/tests/bootstrap_offline_test.sh`
Expected: FAIL because `scripts/bootstrap-third-party.sh` does not exist.

- [ ] **Step 3: Implement the bootstrap script**

The script must:

1. Resolve `root`, defaults `cache="$root/third_party/cache"`,
   `prefix="$root/third_party/prefix"`, `jobs="$(nproc 2>/dev/null || echo 2)"`.
2. Parse `--offline`, `--only`, `--prefix`, `--cache`, `--jobs`, `--with-xen`.
3. Define `deps=(json sodium pg qt6)` by default; replace `qt6` with `qt5` when
   `--only qt5` is used; add `xen` only with `--with-xen`. If `--only` is given,
   use exactly that dependency.
4. For each dependency, read `VERSION`, `URL`, `SHA256`, `PATCHES` with
   `cmake -DNAME="$dep" -P scripts/third-party-info.cmake`.
5. If `third_party/prefix/.stamp-$dep` exists and its contents match
   `$dep $VERSION $SHA256`, skip.
6. If the cache file is absent:
   - `--offline`: print `offline: missing <file>; populate third_party/cache`
     and exit 1.
   - online: `curl -fL --retry 3 --connect-timeout 20 -o "$file" "$url"`.
7. Verify SHA-256 with `sha256sum` (Linux) or `shasum -a 256` (BSD). Abort on
   mismatch.
8. Extract into `third_party/build/$dep-$VERSION`, apply patches with
   `patch -p1 --forward`, configure, build, and install into `$prefix`.
9. Write `.stamp-$dep` with `$dep $VERSION $SHA256` only after the install
   succeeds.
10. After all requested dependencies, write `$prefix/.stamp` with the list of
    dependencies and versions.
11. Never call `sudo`, never write outside `$prefix`, `$cache`, and
    `third_party/build`.

Build recipes (keep them in functions so `--only` works):

- `build_json`: `cmake -S . -B build -DJSON_BuildTests=OFF
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_BUILD_TYPE=Release`; build; install.
- `build_sodium`: `./configure --prefix="$prefix" --disable-dependency-tracking`;
  `make -j"$jobs"`; `make install`.
- `build_pg`: `./configure --prefix="$prefix" --without-readline --without-zlib`;
  `make -j"$jobs"`; `make install`.
- `build_qt6` / `build_qt5`: `./configure -prefix "$prefix" -opensource
  -confirm-license -release -nomake examples -nomake tests` plus `-skip` flags
  for modules the app does not use; `cmake --build . -j"$jobs"`; `cmake --install
  .`. Qt 5 uses `make -j"$jobs"` and `make install` after `configure`.
- `build_xen`: documented as Phase D work. In Phase 0, `--with-xen` downloads
  and verifies the archive, then exits with
  `xen build is implemented in Phase D` unless `OYAMA_XEN_BUILD=1` is set. This
  keeps the pin and hash testable without pretending the full Xen toolstack is
  built.

`third_party/patches/README.md` records the patch source, the upstream commit,
and the fact that each patch must be applied with `patch -p1 --forward`.

- [ ] **Step 4: Run the offline test and a cache-hit test**

Run:

```bash
bash scripts/tests/bootstrap_offline_test.sh
```

Expected: PASS.

Then prepare a tiny cache for `json` and run:

```bash
mkdir -p /tmp/oyama-cache
curl -fL -o /tmp/oyama-cache/json.tar.xz \
  https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
scripts/bootstrap-third-party.sh --offline --only json \
  --cache /tmp/oyama-cache --prefix /tmp/oyama-prefix
test -f /tmp/oyama-prefix/.stamp-json
```

Expected: the stamp exists. Remove `/tmp/oyama-prefix` and `/tmp/oyama-cache`
after the check.

- [ ] **Step 5: Commit**

```bash
git add scripts/bootstrap-third-party.sh scripts/tests/bootstrap_offline_test.sh \
  third_party/patches/README.md
git commit -m "build(supply-chain): add offline bootstrap script"
```

---

## Task 0.3: CMake integration

**Files:**
- Create: `cmake/OyamaThirdParty.cmake`
- Modify: `CMakeLists.txt`
- Modify: `docs/dependencies.md`

**Interfaces:**
- Produces: `OYAMA_THIRD_PARTY` cache string (`system` default, `vendored`
  optional), a `CMAKE_PREFIX_PATH` entry for `third_party/prefix`, and a
  `PKG_CONFIG_PATH` entry for `third_party/prefix/lib/pkgconfig`.
- Consumes: `third_party/prefix/.stamp` from Task 0.2.

- [ ] **Step 1: Write the failing configure check**

Run:

```bash
cmake -S . -B /tmp/oyama-vendored -G Ninja \
  -DOYAMA_THIRD_PARTY=vendored -DOYAMA_BUILD_QT_SHELL=OFF
```

Expected: FAIL with the current CMake because `OYAMA_THIRD_PARTY` is unknown and
the build silently uses system libraries.

- [ ] **Step 2: Implement `cmake/OyamaThirdParty.cmake`**

```cmake
set(OYAMA_THIRD_PARTY "system" CACHE STRING
    "Third-party source: system or vendored")
set_property(CACHE OYAMA_THIRD_PARTY PROPERTY STRINGS system vendored)

if(NOT OYAMA_THIRD_PARTY MATCHES "^(system|vendored)$")
    message(FATAL_ERROR "OYAMA_THIRD_PARTY must be system or vendored")
endif()

if(OYAMA_THIRD_PARTY STREQUAL "vendored")
    set(_oyama_prefix "${CMAKE_SOURCE_DIR}/third_party/prefix")
    if(NOT EXISTS "${_oyama_prefix}/.stamp")
        message(FATAL_ERROR
            "Vendored third-party prefix not found at ${_oyama_prefix}. "
            "Run scripts/bootstrap-third-party.sh first, or configure with "
            "-DOYAMA_THIRD_PARTY=system.")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${_oyama_prefix}")
    set(ENV{PKG_CONFIG_PATH}
        "${_oyama_prefix}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    message(STATUS "Third-party source: vendored (${_oyama_prefix})")
else()
    message(STATUS "Third-party source: system")
endif()
```

Include it in `CMakeLists.txt` immediately after
`include(GNUInstallDirs)` and before `find_package(PostgreSQL 14 REQUIRED)`:

```cmake
include(cmake/OyamaThirdParty.cmake)
```

In the Qt block, after `find_package(Qt6 6.4 QUIET ...)`, add:

```cmake
if(OYAMA_THIRD_PARTY STREQUAL "vendored" AND NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Vendored Qt 6 was requested but not found in third_party/prefix")
endif()
```

- [ ] **Step 3: Verify the system path still configures**

Run:

```bash
cmake -S . -B /tmp/oyama-system -G Ninja \
  -DOYAMA_THIRD_PARTY=system -DOYAMA_BUILD_QT_SHELL=OFF
cmake --build /tmp/oyama-system -j2
ctest --test-dir /tmp/oyama-system --output-on-failure
```

Expected: configure and build succeed; CTest passes or skips PostgreSQL tests.

- [ ] **Step 4: Verify the vendored error path**

Run the command from Step 1 again.
Expected: FAIL with the message naming
`scripts/bootstrap-third-party.sh`, not a linker error about system libs.

- [ ] **Step 5: Update `docs/dependencies.md`**

Add a “Vendored build” subsection:

```markdown
## Vendored build

The default build uses the system packages listed above. To build the large
dependencies from the pinned sources instead:

```sh
scripts/bootstrap-third-party.sh          # online, verifies every hash
scripts/bootstrap-third-party.sh --offline # from third_party/cache
cmake -S . -B build-vendored -G Ninja \
  -DOYAMA_THIRD_PARTY=vendored -DCMAKE_BUILD_TYPE=Release
```

The vendored prefix lives in `third_party/prefix` and is not committed.
`OYAMA_THIRD_PARTY=system` remains the default for distribution packaging.
```

- [ ] **Step 6: Commit**

```bash
git add cmake/OyamaThirdParty.cmake CMakeLists.txt docs/dependencies.md
git commit -m "build(supply-chain): integrate vendored dependency prefix"
```

---

## Task 0.4: Security profile skeleton

**Files:**
- Create: `include/oyama/security/profile.h`
- Create: `docs/security/profiles.md`
- Modify: `CMakeLists.txt`
- Modify: `Makefile`
- Modify: `cli/main.cpp`
- Modify: `docs/dependencies.md`
- Test: `cli_version_profile` CTest

**Interfaces:**
- Produces: `OYAMA_SECURITY_PROFILE` cache string (`off`, `standard`, `full`;
  default `standard`).
- Produces: compile definitions `OYAMA_SECURITY_PROFILE_OFF`,
  `OYAMA_SECURITY_PROFILE_STANDARD`, `OYAMA_SECURITY_PROFILE_FULL` (exactly one
  is `1`).
- Produces: `oyama_security_profile_name()` in
  `include/oyama/security/profile.h`.
- Produces: `oyama-hrt-cli version --verbose` prints
  `security-profile: <name>`.

- [ ] **Step 1: Write the failing test**

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
add_test(NAME cli_version_profile COMMAND oyama-hrt-cli version --verbose)
set_tests_properties(cli_version_profile PROPERTIES
    PASS_REGULAR_EXPRESSION "security-profile: standard")
```

Run:

```bash
cmake --build build -j2
ctest --test-dir build -R cli_version_profile --output-on-failure
```

Expected: FAIL because `version --verbose` prints no profile line.

- [ ] **Step 2: Implement the profile header**

`include/oyama/security/profile.h`:

```c
#ifndef OYAMA_SECURITY_PROFILE_H
#define OYAMA_SECURITY_PROFILE_H

#ifndef OYAMA_SECURITY_PROFILE_OFF
#define OYAMA_SECURITY_PROFILE_OFF 0
#endif
#ifndef OYAMA_SECURITY_PROFILE_STANDARD
#define OYAMA_SECURITY_PROFILE_STANDARD 0
#endif
#ifndef OYAMA_SECURITY_PROFILE_FULL
#define OYAMA_SECURITY_PROFILE_FULL 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *oyama_security_profile_name(void) {
#if OYAMA_SECURITY_PROFILE_FULL
    return "full";
#elif OYAMA_SECURITY_PROFILE_STANDARD
    return "standard";
#else
    return "off";
#endif
}

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 3: Implement the CMake profile switch**

Replace the existing `option(OYAMA_SECURITY_MODE ...)` at line 108 with the
profile block near the top, before the `if(OYAMA_SECURITY_MODE)` hardening block:

```cmake
set(OYAMA_SECURITY_PROFILE "standard" CACHE STRING
    "Security profile: off, standard, or full")
set_property(CACHE OYAMA_SECURITY_PROFILE PROPERTY STRINGS off standard full)

if(NOT OYAMA_SECURITY_PROFILE MATCHES "^(off|standard|full)$")
    message(FATAL_ERROR
        "OYAMA_SECURITY_PROFILE must be off, standard, or full")
endif()

set(OYAMA_SECURITY_PROFILE_OFF 0)
set(OYAMA_SECURITY_PROFILE_STANDARD 0)
set(OYAMA_SECURITY_PROFILE_FULL 0)

if(OYAMA_SECURITY_PROFILE STREQUAL "off")
    set(OYAMA_SECURITY_PROFILE_OFF 1)
    set(OYAMA_SECURITY_MODE OFF)
elseif(OYAMA_SECURITY_PROFILE STREQUAL "full")
    set(OYAMA_SECURITY_PROFILE_FULL 1)
    set(OYAMA_SECURITY_MODE ON)
else()
    set(OYAMA_SECURITY_PROFILE_STANDARD 1)
    set(OYAMA_SECURITY_MODE ON)
endif()

add_compile_definitions(
    OYAMA_SECURITY_PROFILE_OFF=${OYAMA_SECURITY_PROFILE_OFF}
    OYAMA_SECURITY_PROFILE_STANDARD=${OYAMA_SECURITY_PROFILE_STANDARD}
    OYAMA_SECURITY_PROFILE_FULL=${OYAMA_SECURITY_PROFILE_FULL})
```

Remove the old `option(OYAMA_SECURITY_MODE ...)` line entirely. The profile is
now authoritative; a stale `-DOYAMA_SECURITY_MODE=...` in the cache is ignored.
Add a status message:

```cmake
message(STATUS "Security profile: ${OYAMA_SECURITY_PROFILE}")
```

- [ ] **Step 4: Add the CLI output**

In `cli/main.cpp`, include the header near the other `oyama` includes:

```cpp
#include <oyama/security/profile.h>
```

Replace the `version` branch with:

```cpp
if (command == "version") {
    std::cout << "oyama-hrt-cli " << kVersion << '\n';
    if (index < args.size() && args[index] == "--verbose") {
        std::cout << "security-profile: "
                  << oyama_security_profile_name() << '\n';
    }
    return 0;
}
```

Update the usage text to say `version [--verbose]`.

- [ ] **Step 5: Update the Makefile and docs**

`Makefile`:

```make
SECURITY_FLAG = -DOYAMA_SECURITY_PROFILE=$(if $(filter 1,$(SECURITY)),standard,off)
```

`docs/dependencies.md` “Security mode” section: explain the three profiles,
that `make SECURITY=0` selects `off`, and that `OYAMA_SECURITY_MODE` is derived
from the profile and no longer needs to be passed directly.

`docs/security/profiles.md`: one table row per profile with the features from
the spec §4. State that `off` still uses the vault because there is no plaintext
storage mode.

- [ ] **Step 6: Run the tests for all three profiles**

Run:

```bash
cmake --build build -j2
ctest --test-dir build -R cli_version_profile --output-on-failure

cmake -S . -B /tmp/oyama-off -G Ninja -DOYAMA_SECURITY_PROFILE=off \
  -DOYAMA_BUILD_QT_SHELL=OFF
cmake --build /tmp/oyama-off -j2
/tmp/oyama-off/oyama-hrt-cli version --verbose | grep -q 'security-profile: off'

cmake -S . -B /tmp/oyama-full -G Ninja -DOYAMA_SECURITY_PROFILE=full \
  -DOYAMA_BUILD_QT_SHELL=OFF
cmake --build /tmp/oyama-full -j2
/tmp/oyama-full/oyama-hrt-cli version --verbose | grep -q 'security-profile: full'
```

Expected: all three report the matching profile; the `off` build does not pass
`-fstack-protector-strong` (check `compile_commands.json`).

- [ ] **Step 7: Commit**

```bash
git add include/oyama/security/profile.h docs/security/profiles.md \
  CMakeLists.txt Makefile cli/main.cpp docs/dependencies.md
git commit -m "build(security): add off/standard/full profiles"
```

---

## Task 0.5: SBOM and release documentation

**Files:**
- Create: `scripts/generate-sbom.cmake`
- Create: `scripts/tests/sbom_test.sh`
- Create: `third_party/sbom.json` (generated, committed)
- Modify: `THIRD_PARTY_NOTICES.md`
- Modify: `README.md`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Produces: `third_party/sbom.json` in CycloneDX 1.5 JSON form with one
  component per dependency.
- Consumes: `third_party/versions.cmake`.

- [ ] **Step 1: Write the failing test**

Create `scripts/tests/sbom_test.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="$(mktemp "${TMPDIR:-/tmp}/oyama-sbom.XXXXXX.json")"
trap 'rm -f "$out"' EXIT

cmake -DOUT="$out" -P "$root/scripts/generate-sbom.cmake"
test -s "$out"

python3 - "$out" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
assert data["bomFormat"] == "CycloneDX"
names = {c["name"] for c in data["components"]}
for expected in ("qt6", "qt5", "postgresql", "libsodium", "nlohmann-json", "xen"):
    assert expected in names, expected
print("sbom_test: PASS")
PY
```

Run: `bash scripts/tests/sbom_test.sh`
Expected: FAIL because `scripts/generate-sbom.cmake` does not exist.

- [ ] **Step 2: Implement the generator**

`scripts/generate-sbom.cmake` includes `third_party/versions.cmake`, maps each
CMake dependency to a component name, and writes a valid CycloneDX 1.5 document
with:

- `bomFormat`: `"CycloneDX"`
- `specVersion`: `"1.5"`
- `version`: `1`
- one component per dependency: `type: "library"`, `name`, `version`,
  `purl` (for example `pkg:generic/qt6@6.4.3`), and a `hashes` array with
  `alg: "SHA-256"` and the pinned hash.

Write JSON with `file(WRITE ...)` and `string(APPEND ...)`. Do not use
`python3` in the generator; the test may use Python for validation only.

- [ ] **Step 3: Generate and commit the SBOM**

Run:

```bash
cmake -DOUT=third_party/sbom.json -P scripts/generate-sbom.cmake
bash scripts/tests/sbom_test.sh
```

Expected: `sbom_test: PASS`.

- [ ] **Step 4: Update notices and docs**

`THIRD_PARTY_NOTICES.md`: add Qt 6.4.3 (LGPL-3.0/GPL), Qt 5.15.2
(LGPL-3.0/GPL, EOL), PostgreSQL (PostgreSQL License), libsodium (ISC),
nlohmann/json (MIT), Xen (GPL-2.0), and the BIP39 wordlist (MIT, added in Phase
V). State that the vendored build compiles from the pinned archives and that the
full license texts are in `third_party/prefix/share/doc` after bootstrap.

`README.md`: add a “Build profiles” section with the three profile names and a
link to `docs/security/profiles.md`.

`CHANGELOG.md`: add an “Unreleased” entry for the supply-chain pinning and
profiles.

- [ ] **Step 5: Run the full Phase 0 verification**

```bash
bash scripts/tests/verify_third_party_test.sh
bash scripts/tests/bootstrap_offline_test.sh
bash scripts/tests/sbom_test.sh
cmake -S . -B build -G Ninja -DOYAMA_THIRD_PARTY=system
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Expected: all shell tests pass; CTest passes (PostgreSQL tests may skip with
77); `cli_version_profile` passes.

- [ ] **Step 6: Commit**

```bash
git add scripts/generate-sbom.cmake scripts/tests/sbom_test.sh \
  third_party/sbom.json THIRD_PARTY_NOTICES.md README.md CHANGELOG.md
git commit -m "docs(supply-chain): add SBOM and profile documentation"
```

---

## Phase 0 acceptance gate

- [ ] `bash scripts/tests/verify_third_party_test.sh` passes.
- [ ] `bash scripts/tests/bootstrap_offline_test.sh` passes.
- [ ] `bash scripts/tests/sbom_test.sh` passes.
- [ ] `scripts/bootstrap-third-party.sh --offline --only json` succeeds from a
      prepared cache and fails clearly from an empty cache.
- [ ] `cmake -DOYAMA_THIRD_PARTY=vendored` fails with the bootstrap instruction
      when `third_party/prefix/.stamp` is absent.
- [ ] `cmake -DOYAMA_THIRD_PARTY=system` configures and builds.
- [ ] `OYAMA_SECURITY_PROFILE=off`, `standard`, and `full` all configure and
      build; `version --verbose` reports the matching name.
- [ ] The default profile is `standard` and the default Qt is 6.4.
- [ ] `third_party/sbom.json` is valid JSON and covers all six dependencies.
- [ ] `git status --short` shows no tracked `third_party/cache`,
      `third_party/build`, or `third_party/prefix` files.
- [ ] `ctest --test-dir build --output-on-failure` passes on the development
      host (PostgreSQL tests may skip with 77).

## Execution handoff

Plan complete and saved to
`docs/superpowers/plans/2026-11-02-phase-0-supply-chain.md`. Please review the
plan. Which execution approach would you prefer?

- **Subagent-driven** — a fresh subagent implements each task and a fresh
  reviewer checks it before the next one starts, then a whole-branch review at
  the end. Most thorough; costs a fresh context per task and per review.
- **Native** — I implement every task myself in this session, then one fresh
  reviewer checks the whole branch. Cheapest and fastest; no independent review
  until the end.

For Phase 0 I recommend **native**: the tasks are sequential build-system work
with shared shell/CMake helpers, and a mistake in the hash/prefix plumbing is
easier to catch in one continuous session than across five fresh contexts.
