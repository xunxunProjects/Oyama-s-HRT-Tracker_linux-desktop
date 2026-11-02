# Security Hardening Roadmap — Projects 0/A/V/B/C/D

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.
>
> **This is an umbrella roadmap, not the final implementation plan for every
> phase.** Before executing a phase, write its detailed task plan in
> `docs/superpowers/plans/YYYY-MM-DD-<phase>.md` using the same format. Each
> phase must produce working, testable software on its own.

**Goal:** Add BIP39 recovery phrases, optional capture protection, panic
locking, Xen key isolation, and vendored dependencies, with a C++11 baseline and
Qt 6.4 as the default, without weakening the existing encrypted vault or
breaking `main`.

**Architecture:** Keep the existing C99 vault and PostgreSQL storage. Add
project-internal C99 security modules under `include/oyama/security/` and
`security/src/`, a Qt wrapper layer in `app/src/`, and a separate
`oyama-keyd` guest service for the full profile. Land the work as six
sequential branches and PRs: `project/0-supply-chain`,
`project/a-cpp11-qt`, `project/v-bip39-phrase`, `project/b-panic-lock`,
`project/c-capture-guard`, `project/d-xen-keyd`.

**Tech Stack:** C99, C++11, Qt 6.4+ (default), Qt 5.15.2 + KDE Qt5 patches
(optional compatibility build), PostgreSQL 14.x, libsodium, nlohmann/json
3.11.x, libxenvchan/libxenstore (full profile only), CMake/Ninja, Xen 4.x (full
profile only).

**Spec:** `docs/superpowers/specs/2026-11-02-security-architecture-design.md`

## Global Constraints

- **C++11 everywhere.** `CMAKE_CXX_STANDARD 11`, `CMAKE_CXX_STANDARD_REQUIRED ON`,
  `CMAKE_CXX_EXTENSIONS OFF`. The same sources must also compile with
  `-std=c++20` in a compatibility CI job.
- **Qt 6.4 default.** `find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml
  Quick QuickControls2)` for the default build. No Qt 5-only or Qt 6-only API
  outside `#if QT_VERSION` blocks in shared code.
- **Qt 5.15.2 compatibility.** `-DOYAMA_QT_MAJOR=5` selects
  `find_package(Qt5 5.15 REQUIRED ...)` and the generated `qml5` tree. The
  compatibility build is optional and must not change the default.
- **C99 security headers.** Public security APIs are `extern "C"`-safe C99 and
  compile with `-std=c99 -pedantic-errors`.
- **Profiles.** `OYAMA_SECURITY_PROFILE=off|standard|full`, default `standard`.
  `off` must not compile or link capture/panic/vchan code. `standard` must not
  link them either. `full` must link all of them. The vault stays in all three.
- **BIP39 for new phrases.** New vaults use 24 words (256-bit entropy) from the
  BIP39 English wordlist. The vault KEK stays Argon2id; BIP39's PBKDF2 seed
  derivation is not used. Existing vaults keep their original phrase.
- **No weakening of the vault.** The existing Argon2id parameters, AEAD
  selection, and backup format are unchanged. BIP39 changes only how new
  phrases are generated and validated.
- **No network at build time after bootstrap.** `scripts/bootstrap-third-party.sh
  --offline` must build from `third_party/cache/` alone.
- **Every third-party archive** has a pinned version and SHA-256 in
  `third_party/versions.cmake`.
- **Tests use synthetic data only.** No real health data, phrase, or key in a
  test, log, or screenshot.
- **Every task ends with a commit.** Do not combine task commits. Do not mix
  formatting-only changes with behavior changes.
- **Each branch rebases on `main` after the previous PR merges.** Keep `main`
  green at every merge point.
- **Full test command:** `cmake --build build-release -j && ctest --test-dir
  build-release --output-on-failure`. Use `PG_BINDIR=/tmp/opencode/pg18/bin` when
  the PostgreSQL tools are not on `PATH`.
- **Qt 5.15 caveat:** the open-source 5.15.2 release is EOL. The compatibility
  build must apply the pinned KDE Qt5 patch collection. Record the patch set
  hash in `versions.cmake`. The default Qt 6.4 build is the recommended one.

## Review Focus

1. **Panic path under a locked vault** — pressing the panic hotkey while the
   vault is already locked must be idempotent, must not crash, and must leave
   the gray screen visible. Test in Phase B.
2. **Screenshot tool that starts and exits within one poll interval** — the
   guard must not leave the overlay stuck, and must not miss a tool that stays
   alive. Test in Phase C.
3. **C++11 build with `-pedantic-errors`** — every use of `std::filesystem`,
   `std::optional`, `std::string_view`, `[[nodiscard]]`, `make_unique`, and
   generic lambdas must be replaced or aliased. Test in Phase A.
4. **Dual Qt QML** — the default Qt 6.4 build uses versionless imports, the
   Qt 5.15.2 build uses the generated versioned tree, and `T.qml`/`Theme.qml`
   resolve as singletons in both. Test in Phase A.
5. **BIP39 phrase compatibility** — a generated 24-word phrase validates, a
   bad checksum is rejected, and an old non-BIP39 vault still unlocks. Test in
   Phase V.
6. **Key guest protocol framing** — a truncated, oversized, or wrong-version
   frame must be rejected without allocating unbounded memory or crashing the
   key guest. Test in Phase D.
7. **Profile isolation** — an `off` or `standard` build must not contain the
   capture/panic/vchan symbols. Test in Phases 0/B/C/D.

---

## Phase 0 — Supply chain and build profiles

**Branch:** `project/0-supply-chain`
**PR title:** `build: vendor third-party sources and add security profiles`
**Depends on:** nothing.

**Deliverables:**

- `third_party/versions.cmake` with pinned versions and SHA-256 for Qt, KDE
  patches, PostgreSQL, libsodium, nlohmann/json, and Xen tools.
- `scripts/bootstrap-third-party.sh` (download or offline cache, verify,
  patch, build, install to `third_party/prefix`).
- `scripts/verify-third-party.sh` and the `supply_chain_hashes` CTest.
- `cmake/OyamaThirdParty.cmake` and CMake integration.
- `OYAMA_SECURITY_PROFILE` cache variable with `off|standard|full`.
- `third_party/README.md`, `docs/dependencies.md` update, `THIRD_PARTY_NOTICES.md`
  update, `CHANGELOG.md` entry.
- SBOM generation (`third_party/sbom.json`, CycloneDX).

### Task 0.1: Pin versions and hashes

**Files:**
- Create: `third_party/versions.cmake`
- Create: `scripts/verify-third-party.sh`
- Create: `third_party/README.md`
- Modify: `CMakeLists.txt` (add the `supply_chain_hashes` test)

**Interfaces:**
- Produces: CMake variables `OYAMA_<DEP>_VERSION`, `OYAMA_<DEP>_URL`,
  `OYAMA_<DEP>_SHA256`, `OYAMA_<DEP>_PATCHES`, for `QT`, `PG`, `SODIUM`,
  `JSON`, `XEN`.

- [ ] **Step 1: Write the failing test**

`scripts/verify-third-party.sh` exits non-zero when a cached archive is missing
or its hash differs. Add a CTest that runs it and expects `SKIP_RETURN_CODE 77`
when `third_party/cache/` is empty.

- [ ] **Step 2: Run it to verify it fails**

Run: `ctest --test-dir build -R supply_chain_hashes --output-on-failure`
Expected: FAIL because `third_party/versions.cmake` does not exist.

- [ ] **Step 3: Implement the pin file and verifier**

`third_party/versions.cmake` contains one block per dependency:

```cmake
# Default build; system Qt is acceptable for off/standard.
set(OYAMA_QT6_VERSION "6.4.x")
set(OYAMA_QT6_URL "https://download.qt.io/archive/qt/6.4/6.4.x/single/qt-everywhere-src-6.4.x.tar.xz")
set(OYAMA_QT6_SHA256 "<filled at bootstrap time>")

# Optional compatibility build.
set(OYAMA_QT5_VERSION "5.15.2")
set(OYAMA_QT5_URL "https://download.qt.io/archive/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.tar.xz")
set(OYAMA_QT5_SHA256 "<filled at bootstrap time>")
set(OYAMA_QT5_PATCHES "patches/qt5-kde-<hash>.patch")
```

The verifier uses `sha256sum -c` (Linux) or `shasum -a 256 -c` (BSD) and
prints a table of expected/actual.

- [ ] **Step 4: Run the test to verify it passes (or skips)**

Run: `ctest --test-dir build -R supply_chain_hashes --output-on-failure`
Expected: PASS when the cache is present, SKIP (77) when absent.

- [ ] **Step 5: Commit**

```bash
git add third_party/versions.cmake third_party/README.md scripts/verify-third-party.sh CMakeLists.txt
git commit -m "build(supply-chain): pin third-party versions and hashes"
```

### Task 0.2: Bootstrap script

**Files:**
- Create: `scripts/bootstrap-third-party.sh`
- Create: `third_party/patches/README.md`
- Modify: `third_party/versions.cmake`

**Interfaces:**
- Produces: `third_party/prefix/.stamp` after a successful build.
- Consumes: `third_party/versions.cmake`.

- [ ] **Step 1: Write the failing test**

A shell test `scripts/tests/bootstrap_offline_test.sh` runs
`bootstrap-third-party.sh --offline --only json` in a temporary prefix with an
empty cache and expects a hard error naming the missing archive.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash scripts/tests/bootstrap_offline_test.sh`
Expected: FAIL because the script does not exist.

- [ ] **Step 3: Implement the script**

The script must:

- accept `--offline`, `--only <dep>`, `--prefix <dir>`, `--cache <dir>`,
  `--jobs <n>`;
- verify SHA-256 before extraction;
- apply patches with `patch -p1 --forward`;
- build in dependency order: `json`, `sodium`, `pg`, `qt6` (default) or `qt5` (compatibility), `xen`;
- use `cmake --install` or the dependency's `make install` with `DESTDIR` or
  `--prefix`, never `sudo`;
- write `.stamp` only after every requested dependency succeeds;
- be idempotent: skip a dependency whose stamp matches.

- [ ] **Step 4: Run the test to verify it passes**

Run: `bash scripts/tests/bootstrap_offline_test.sh`
Expected: PASS (the script exits non-zero with a clear message).

- [ ] **Step 5: Commit**

```bash
git add scripts/bootstrap-third-party.sh scripts/tests/bootstrap_offline_test.sh third_party/patches/README.md
git commit -m "build(supply-chain): add offline bootstrap script"
```

### Task 0.3: CMake integration

**Files:**
- Create: `cmake/OyamaThirdParty.cmake`
- Modify: `CMakeLists.txt`
- Modify: `docs/dependencies.md`

**Interfaces:**
- Produces: `OYAMA_THIRD_PARTY` (`vendored|system`, default `vendored` for
  release builds), and a `CMAKE_PREFIX_PATH` entry for `third_party/prefix`.

- [ ] **Step 1: Write the failing test**

Configure with `-DOYAMA_THIRD_PARTY=vendored` and no `third_party/prefix/.stamp`.
Expected: a fatal error that names `scripts/bootstrap-third-party.sh`.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake -S . -B /tmp/oyama-vendored -DOYAMA_THIRD_PARTY=vendored`
Expected: FAIL with the current CMake because the option is unknown.

- [ ] **Step 3: Implement the module and wire it in**

Include `cmake/OyamaThirdParty.cmake` before `find_package(PostgreSQL)` and
`find_package(Qt6)` (or `Qt5` when `OYAMA_QT_MAJOR=5`). Use `find_package(... CONFIG)` for Qt and
`pkg_check_modules` for libsodium. Keep the system path working.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake -S . -B /tmp/oyama-vendored -DOYAMA_THIRD_PARTY=vendored`
Expected: PASS after the stamp exists; FAIL with a clear message when it does
not.

- [ ] **Step 5: Commit**

```bash
git add cmake/OyamaThirdParty.cmake CMakeLists.txt docs/dependencies.md
git commit -m "build(supply-chain): integrate vendored dependency prefix"
```

### Task 0.4: Security profile skeleton

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `cli/main.cpp` (version output)
- Create: `docs/security/profiles.md`
- Test: `cli/tests/run_cli_integration.sh`

**Interfaces:**
- Produces: `OYAMA_SECURITY_PROFILE` cache variable; compile definitions
  `OYAMA_SECURITY_PROFILE_OFF`, `OYAMA_SECURITY_PROFILE_STANDARD`,
  `OYAMA_SECURITY_PROFILE_FULL`; `oyama-hrt-cli version --verbose` prints
  `security-profile: <name>`.

- [ ] **Step 1: Write the failing test**

Add a CTest `cli_version_profile` that runs
`oyama-hrt-cli version --verbose` and expects
`PASS_REGULAR_EXPRESSION "security-profile: standard"` for the default build.

- [ ] **Step 2: Run it to verify it fails**

Run: `ctest --test-dir build -R cli_version_profile --output-on-failure`
Expected: FAIL because the flag and output do not exist.

- [ ] **Step 3: Implement the profile switch**

Validate the value; map it to `OYAMA_SECURITY_MODE`; add the compile
definitions; add the CLI output. `off` sets `OYAMA_SECURITY_MODE=OFF`;
`standard` and `full` set it `ON`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -R cli_version_profile --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cli/main.cpp cli/tests/run_cli_integration.sh docs/security/profiles.md
git commit -m "build(security): add off/standard/full profiles"
```

### Task 0.5: SBOM and release docs

**Files:**
- Modify: `scripts/bootstrap-third-party.sh`
- Create: `third_party/sbom.schema.json` (or a documented generator)
- Modify: `THIRD_PARTY_NOTICES.md`, `README.md`, `CHANGELOG.md`

- [ ] **Step 1: Generate the SBOM**

The bootstrap script writes `third_party/sbom.json` with one component per
dependency: name, version, URL, SHA-256, license, and whether it is built from
source or provided by the system.

- [ ] **Step 2: Verify the SBOM is valid JSON and covers every dependency**

Run: `python3 -c "import json; json.load(open('third_party/sbom.json'))"`
Expected: exit 0.

- [ ] **Step 3: Update notices and docs**

Add the license texts and the Qt 5.15.2 EOL caveat. Document the three profiles
in `docs/security/profiles.md`, including Qt 6.4 as the default and Qt 5.15.2
as the optional compatibility build.

- [ ] **Step 4: Commit**

```bash
git add scripts/bootstrap-third-party.sh third_party/sbom.json THIRD_PARTY_NOTICES.md README.md CHANGELOG.md
git commit -m "docs(supply-chain): add SBOM and profile documentation"
```

**Phase 0 acceptance gate:** `make test` passes with `OYAMA_THIRD_PARTY=system`;
`scripts/bootstrap-third-party.sh --offline` passes from a prepared cache; the
default profile is `standard`; the default build uses Qt 6.4; `off` and `full`
configure and build.

---

## Phase A — C++11 baseline and dual Qt build

**Branch:** `project/a-cpp11-qt`
**PR title:** `build: add C++11 baseline and dual Qt 6.4/5.15 support`
**Depends on:** Phase 0.

**Deliverables:**

- `include/oyama/compat/` headers with C++11 fallbacks and C++17/20 aliases.
- `storage/`, `cli/`, `app/src/` compile with `-std=c++11 -pedantic-errors`.
- Qt 6.4 remains the default build (`OYAMA_QT_MAJOR=6`).
- Optional Qt 5.15.2 compatibility build (`OYAMA_QT_MAJOR=5`) with a generated
  versioned QML tree.
- Test matrix: default Qt 6.4, Qt 5.15.2, C++11, C++20 compatibility, screenshot
  matrix on both Qt majors.

### Task A.1: Compatibility headers and tests

**Files:**
- Create: `include/oyama/compat/optional.h`
- Create: `include/oyama/compat/string_view.h`
- Create: `include/oyama/compat/filesystem.h`
- Create: `include/oyama/compat/attributes.h`
- Create: `include/oyama/compat/make_unique.h`
- Create: `include/oyama/compat/algorithm.h`
- Create: `storage/tests/compat_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama::optional<T>`, `oyama::nullopt`, `oyama::string_view`,
  `oyama::fs::path` and the used filesystem functions, `OYAMA_NODISCARD`,
  `OYAMA_MAYBE_UNUSED`, `oyama::make_unique`, `oyama::clamp`, `oyama::size`,
  `oyama::data`, `oyama::as_const`.

- [ ] **Step 1: Write the failing tests**

`compat_test.cpp` covers: empty optional, `value_or`, move-only `optional`,
`string_view` comparison and `substr`, `fs::path` join/`parent_path`/`filename`,
`create_directories`/`remove_all` in a temp directory, `make_unique`, and
`clamp`. Compile it with `-std=c++11 -pedantic-errors`.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target compat_test && ./build/compat_test`
Expected: FAIL because the headers do not exist.

- [ ] **Step 3: Implement the headers**

Use the `#if __cplusplus >= 201703L` alias pattern from the spec §5.5. The
fallbacks must be header-only and must not depend on libsodium, Qt, or libpq.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/compat_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/compat storage/tests/compat_test.cpp CMakeLists.txt
git commit -m "feat(compat): add C++11 fallbacks for C++17/20 facilities"
```

### Task A.2: Port storage to C++11

**Files:**
- Modify: `storage/src/*.cpp`, `storage/include/oyama/storage/*.h`
- Modify: `storage/tests/*.cpp`
- Modify: `CMakeLists.txt` (`CMAKE_CXX_STANDARD 11`)

**Interfaces:**
- Consumes: `oyama::optional`, `oyama::string_view`, `oyama::fs`.
- Produces: the same public storage API, now C++11-compatible.

- [ ] **Step 1: Add a C++11 compile-only target**

Add a `storage_cxx11_compile` target that compiles every storage source with
`-std=c++11 -pedantic-errors` and does not link. This is the failing test.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target storage_cxx11_compile`
Expected: FAIL with the first C++14/17 construct.

- [ ] **Step 3: Replace the constructs**

Mechanical replacements from the spec §5.5 table. Do not change behavior. Keep
the public headers unchanged where possible; use `oyama::` aliases inside.

- [ ] **Step 4: Run the full storage test suite**

Run: `ctest --test-dir build -R 'storage|vault|backup|migration' --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add storage CMakeLists.txt
git commit -m "refactor(storage): compile as C++11"
```

### Task A.3: Port CLI and app sources to C++11

**Files:**
- Modify: `cli/*.cpp`, `cli/*.h`
- Modify: `app/src/*.cpp`, `app/src/*.h`
- Modify: `app/tests/*.cpp`

- [ ] **Step 1: Add a C++11 compile-only target for the CLI and app sources**

Run: `cmake --build build --target app_cxx11_compile`
Expected: FAIL.

- [ ] **Step 2: Replace the remaining constructs**

Same mechanical replacements. Pay attention to `std::filesystem` in
`app_config.cpp` and `local_cluster.cpp`, `std::optional` in repositories, and
`std::string_view` in codecs.

- [ ] **Step 3: Run the CLI and app controller tests**

Run: `ctest --test-dir build -R 'cli_|app_controller|legacy_migration_worker' --output-on-failure`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add cli app
git commit -m "refactor(app): compile CLI and Qt sources as C++11"
```

### Task A.4: Qt major-version switch and default Qt 6.4 build

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `app/src/main.cpp`
- Modify: `CMakePresets.json`

**Interfaces:**
- Produces: `OYAMA_QT_MAJOR` cache variable (`6` default, `5` optional);
  `oyama-hrt-app` still linked to Qt 6 by default; a Qt5 branch that errors with
  a clear message until Task A.5 fills it in.

- [ ] **Step 1: Write the failing configure check**

Run: `cmake -S . -B /tmp/oyama-qt -DOYAMA_QT_MAJOR=5 -DOYAMA_BUILD_QT_SHELL=ON`
Expected: FAIL because the variable is not handled yet.

- [ ] **Step 2: Add the switch and keep the Qt 6 path**

Wrap the current Qt 6 block in
`if(OYAMA_QT_MAJOR STREQUAL "6")`. Add an `elseif(OYAMA_QT_MAJOR STREQUAL "5")`
branch that calls `message(FATAL_ERROR "Qt 5 build is implemented in Task A.5")`
for now. Validate that the value is `5` or `6`.

- [ ] **Step 3: Add the `#if QT_VERSION` load branch in `main.cpp`**

Keep `engine.loadFromModule` and `objectCreationFailed` for Qt 6; add the
`engine.load(QUrl("qrc:/qml/Main.qml"))` and `rootObjects().isEmpty()` path for
Qt 5. This is the startup-load branch; Task A.6 adds the event and window-API
branches.

- [ ] **Step 4: Build and run the default smoke test**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/oyama-hrt-app --self-test`
Expected: exit 0 with Qt 6.4.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt app/src/main.cpp CMakePresets.json
git commit -m "build(qt): add major-version switch and keep Qt 6.4 default"
```

### Task A.5: Qt 5.15.2 compatibility build and QML transform

**Files:**
- Create: `scripts/qml5ify.py`
- Create: `scripts/tests/qml5ify_test.py`
- Modify: `CMakeLists.txt`
- Modify: `app/src/main.cpp`

**Interfaces:**
- Produces: `${CMAKE_BINARY_DIR}/qml5/` containing copied QML files with
  versioned imports and a `qmldir`; `oyama-hrt-app` built against Qt 5.15.2
  when `OYAMA_QT_MAJOR=5`.

- [ ] **Step 1: Write the failing transform test**

`qml5ify_test.py` creates a temporary `app/qml` with `import QtQuick`,
`import QtQuick.Controls`, `import QtQuick.Layouts`, `import QtQuick.Shapes`,
and a nested JS import; it runs `qml5ify.py` and asserts the exact rewritten
lines plus the `qmldir`. It also asserts that an unknown `import QtFoo` fails.

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 scripts/tests/qml5ify_test.py`
Expected: FAIL because `qml5ify.py` does not exist.

- [ ] **Step 3: Implement the transform and the Qt 5 CMake branch**

`qml5ify.py` copies `app/qml` to the output directory, rewrites the four
QtQuick imports, writes `qmldir` with the `T` and `Theme` singletons, and
writes a `qml.qrc` listing every copied file. The CMake Qt 5 branch uses
`find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)`,
`add_executable`, `qt5_add_resources` on the generated qrc, and
`CMAKE_AUTOMOC`/`CMAKE_AUTORCC`. The Qt 6 branch is unchanged.

- [ ] **Step 4: Build and run the Qt 5 smoke test**

Run: `cmake -S . -B /tmp/oyama-qt5 -DOYAMA_QT_MAJOR=5 -DOYAMA_BUILD_QT_SHELL=ON && cmake --build /tmp/oyama-qt5 -j`
Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software /tmp/oyama-qt5/oyama-hrt-app --self-test`
Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add scripts/qml5ify.py scripts/tests/qml5ify_test.py CMakeLists.txt app/src/main.cpp
git commit -m "build(qt): add Qt 5.15.2 compatibility build"
```

### Task A.6: Qt 5 API differences

**Files:**
- Modify: `app/src/main.cpp`, `app/src/app_controller.cpp`
- Modify: `app/tests/ui_preferences_test.cpp`, `app/tests/app_controller_test.cpp`

- [ ] **Step 1: Add `#if QT_VERSION` branches**

Wrap the `QMouseEvent` constructor and `QQuickWindow::mapToGlobal` return type.
Replace `QGuiApplication::nativeInterface` if used. Check `QFontDatabase` and
`QQuickStyle` calls.

- [ ] **Step 2: Build with Qt 6.4 (default) and Qt 5.15.2 (compatibility)**

Run both builds. Expected: both compile; Qt 6.4 is the default and Qt 5.15.2
is the optional compatibility check.

- [ ] **Step 3: Run the CTest suite**

Run: `ctest --test-dir build --output-on-failure`
Expected: PASS (PostgreSQL tests may skip).

- [ ] **Step 4: Commit**

```bash
git add app/src app/tests
git commit -m "fix(qt): bridge Qt 5 and Qt 6 API differences"
```

### Task A.7: Test matrix and CI

**Files:**
- Modify: `CMakePresets.json`
- Create: `.github/workflows/` entries or a documented CI script
- Modify: `docs/dependencies.md`

- [ ] **Step 1: Add presets**

`linux-qt6-release` (default), `linux-qt5-release` (compatibility),
`linux-cxx11`, `linux-cxx20`, `freebsd-qt6-release`, and
`freebsd-qt5-release`.

- [ ] **Step 2: Run the C++11 and C++20 compile jobs**

Expected: both exit 0.

- [ ] **Step 3: Run the Qt 6.4 and Qt 5.15.2 CTest suites and screenshot matrices**

Expected: PASS on both, with no visual regressions.

- [ ] **Step 4: Commit**

```bash
git add CMakePresets.json docs/dependencies.md .github
git commit -m "ci: add Qt 6.4, Qt 5.15.2, C++11, and C++20 build jobs"
```

**Phase A acceptance gate:** `main` builds with C++11 and Qt 6.4 by default;
the Qt 5.15.2 compatibility build also passes; the full CTest suite passes on
both; the screenshot matrices show no regressions; the C++20 compatibility job
passes.

---

## Phase V — BIP39 recovery phrase

**Branch:** `project/v-bip39-phrase`
**PR title:** `feat(vault): generate and validate BIP39 recovery phrases`
**Depends on:** Phase A.

**Deliverables:**

- `include/oyama/bip39.h` with generate/validate/normalize functions.
- `vault/src/bip39.c` and `vault/src/bip39_wordlist.c` (2048 English words).
- `vault/tests/bip39_test.c` with Trezor/BIP39 vectors.
- CLI and Qt setup flow use the generator; existing vaults keep their phrases.
- `docs/security/recovery-phrase.md`.

### Task V.1: BIP39 generator and validator

**Files:**
- Create: `include/oyama/bip39.h`
- Create: `vault/src/bip39.c`
- Create: `vault/src/bip39_wordlist.c`
- Create: `vault/tests/bip39_test.c`
- Modify: `CMakeLists.txt` (add to `oyama_vault`)

**Interfaces:**
- Produces: `oyama_bip39_generate`, `oyama_bip39_validate`,
  `oyama_bip39_word_count`, `oyama_bip39_normalize`; `OYAMA_BIP39_WORDS` = 24;
  `OYAMA_BIP39_MAX_LEN` = 512.

- [ ] **Step 1: Write the failing tests**

`bip39_test.c` asserts:

- two known 256-bit BIP39 vectors produce the exact expected 24-word phrases;
- generate → validate round-trip 100 times;
- a bad checksum, a non-wordlist word, 12 words, and 25 words are rejected;
- `oyama_bip39_word_count` handles leading/trailing spaces;
- a test-only entropy failure hook returns `OYAMA_STATUS_CRYPTO`.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target bip39_test && ./build/bip39_test`
Expected: FAIL because the header and implementation do not exist.

- [ ] **Step 3: Implement the generator and validator**

Use 256 bits from `getrandom(2)` (Linux) or `arc4random_buf` (BSD), with a
`/dev/urandom` fallback. Compute the 8-bit checksum with
`crypto_hash_sha256`. Map 11-bit groups to the BIP39 English wordlist. Store the
wordlist as `static const char *const oyama_bip39_words[2048]`. `validate`
checks the word count, membership, and checksum; `normalize` collapses spaces
and lowercases ASCII.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/bip39_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/bip39.h vault/src/bip39.c vault/src/bip39_wordlist.c vault/tests/bip39_test.c CMakeLists.txt
git commit -m "feat(vault): add BIP39 phrase generator and validator"
```

### Task V.2: Vault and CLI integration

**Files:**
- Modify: `vault/src/vault.c` (include `bip39.h`, expose setup helper)
- Modify: `cli/main.cpp` (vault setup path)
- Create: `vault/tests/bip39_vault_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `oyama_bip39_generate`, `oyama_bip39_validate`.
- Produces: `oyama_vault_setup` still accepts any phrase; a new
  `oyama_vault_setup_generated` generates and returns a 24-word phrase for
  callers that do not have one.

- [ ] **Step 1: Write the failing test**

Create a vault with `oyama_vault_setup_generated`, assert the returned phrase
validates and has 24 words, lock, unlock with the same phrase, and assert the
data round-trips. Create a second vault with a legacy phrase and assert it
still unlocks. Change the legacy phrase to a generated one and assert the new
phrase works.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target bip39_vault_test && ./build/bip39_vault_test`
Expected: FAIL because `oyama_vault_setup_generated` does not exist.

- [ ] **Step 3: Implement the helper and CLI path**

`oyama_vault_setup_generated` calls `oyama_bip39_generate`, then delegates to
`oyama_vault_setup`, and copies the phrase into the caller's buffer. The CLI
`vault setup` prints the phrase once to stdout, warns that it is the only
recovery path, and does not write it to a log or history file.

- [ ] **Step 4: Run the test and the existing vault suite**

Run: `./build/bip39_vault_test`
Run: `ctest --test-dir build -R 'vault|cli_vault' --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add vault/src/vault.c cli/main.cpp vault/tests/bip39_vault_test.c CMakeLists.txt
git commit -m "feat(vault): use BIP39 phrases for new vaults"
```

### Task V.3: Qt setup UI

**Files:**
- Modify: `app/src/app_controller.cpp`
- Modify: `app/qml/SetupVaultPage.qml`
- Modify: `app/tests/app_controller_test.cpp`
- Modify: `app/src/main.cpp` (self-test)

**Interfaces:**
- Produces: `AppController::beginVaultSetup` fills `setupPhrase` with 24 BIP39
  words; the existing `setupChallenge` asks for three positions.

- [ ] **Step 1: Write the failing test**

`app_controller_test.cpp` calls `beginVaultSetup()`, splits `setupPhrase()` on
spaces, asserts 24 words, and validates the checksum through the C API. It also
asserts the challenge indices are unique and in range.

- [ ] **Step 2: Run it to verify it fails**

Run: `ctest --test-dir build -R app_controller_test --output-on-failure`
Expected: FAIL because the setup phrase is not BIP39.

- [ ] **Step 3: Implement the UI flow**

`SetupVaultPage.qml` shows the 24 words in a 4×6 grid, warns that they are the
only recovery path, and offers a copy action with a warning and a 60-second
clipboard clear. The confirmation step asks for three random positions and
does not allow the user to continue until they match.

- [ ] **Step 4: Run the test and the screenshot check**

Run: `ctest --test-dir build -R app_controller_test --output-on-failure`
Run: `app/tests/capture_ui.sh ./build/oyama-hrt-app ./build/screenshots`
Expected: PASS; inspect the setup page screenshot.

- [ ] **Step 5: Commit**

```bash
git add app/src/app_controller.cpp app/qml/SetupVaultPage.qml app/tests/app_controller_test.cpp app/src/main.cpp
git commit -m "feat(ui): show a 24-word BIP39 recovery phrase"
```

### Task V.4: Documentation and migration notes

**Files:**
- Create: `docs/security/recovery-phrase.md`
- Modify: `SECURITY.md`, `README.md`, `CHANGELOG.md`, `THIRD_PARTY_NOTICES.md`

- [ ] **Step 1: Document the phrase format and the limits**

Explain 24 words = 256 bits, the checksum, the English wordlist, and that the
vault KEK is still Argon2id. State that existing vaults keep their old phrase
and that losing the phrase loses the vault.

- [ ] **Step 2: Add the BIP39 wordlist license**

Record the MIT license and the `bitcoin/bips` source in
`THIRD_PARTY_NOTICES.md`.

- [ ] **Step 3: Commit**

```bash
git add docs/security/recovery-phrase.md SECURITY.md README.md CHANGELOG.md THIRD_PARTY_NOTICES.md
git commit -m "docs(vault): document the BIP39 recovery phrase"
```

**Phase V acceptance gate:** a new vault gets a valid 24-word phrase; the
checksum is enforced; an old vault still unlocks; the setup UI shows 24 words
and requires the challenge; the wordlist license is recorded.

---

## Phase B — Panic lock and memory hygiene

**Branch:** `project/b-panic-lock`
**PR title:** `feat(security): add panic lock and memory hygiene`
**Depends on:** Phases A and V.

**Deliverables:**

- `include/oyama/security/export.h`, `mem.h`, `panic.h`
- `security/src/mem.c`, `panic_x11.c`, `panic_wayland.c`, `panic_null.c`
- `security/tests/mem_test.c`, `panic_test.c`
- `app/src/panic_controller.{h,cpp}` (Qt wrapper)
- `app/qml/PanicLockPage.qml`
- `AppController::panicLock()` and the `panic` vault state
- Docs: `docs/security/panic.md`

### Task B.1: Memory hygiene library

**Files:**
- Create: `include/oyama/security/export.h`
- Create: `include/oyama/security/mem.h`
- Create: `security/src/mem.c`
- Create: `security/tests/mem_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama_secure_zero`, `oyama_secure_alloc`, `oyama_secure_free`,
  `oyama_mlock`, `oyama_munlock`, `oyama_disable_core_dumps`,
  `oyama_mark_dontdump`, `oyama_release_free_pages`.

- [ ] **Step 1: Write the failing tests**

`mem_test.c` asserts that `oyama_secure_zero` zeroes a buffer; that
`oyama_secure_alloc` returns a non-null mlocked pointer and that
`oyama_secure_free` succeeds; that `oyama_disable_core_dumps` returns 0 or a
documented unsupported code; that `oyama_mark_dontdump` returns 0 on Linux.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target mem_test && ./build/mem_test`
Expected: FAIL because the library does not exist.

- [ ] **Step 3: Implement the library**

Use `sodium_memzero`/`sodium_malloc` when available; otherwise
`explicit_bzero`, `mmap`, `mlock`, `mprotect`, `madvise(MADV_DONTDUMP)`, and
`setrlimit`. Fail closed on `mlock` failure.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/mem_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/security/export.h include/oyama/security/mem.h security/src/mem.c security/tests/mem_test.c CMakeLists.txt
git commit -m "feat(security): add memory hygiene primitives"
```

### Task B.2: Panic hotkey — X11 and null backends

**Files:**
- Create: `include/oyama/security/panic.h`
- Create: `security/src/panic_x11.c`
- Create: `security/src/panic_null.c`
- Create: `security/tests/panic_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama_panic_create`, `oyama_panic_set_hotkey`,
  `oyama_panic_start`, `oyama_panic_stop`, `oyama_panic_destroy`.

- [ ] **Step 1: Write the failing tests**

`panic_test.c` uses the null backend to assert that create/start/stop/destroy
are idempotent, that an invalid accelerator is rejected, and that the callback
fires when the test backend injects a key event. The X11 test is skipped when
`DISPLAY` is unset.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target panic_test && ./build/panic_test`
Expected: FAIL.

- [ ] **Step 3: Implement the null and X11 backends**

The X11 backend opens its own `Display*`, calls `XGrabKey` with `AnyModifier`,
and reads events in a dedicated thread. It must close the display and ungrab
the key on stop. The null backend exposes a test-only
`oyama_panic_test_inject` function guarded by `OYAMA_TESTING`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/panic_test`
Expected: PASS (X11 portion skipped without `DISPLAY`).

- [ ] **Step 5: Commit**

```bash
git add include/oyama/security/panic.h security/src/panic_x11.c security/src/panic_null.c security/tests/panic_test.c CMakeLists.txt
git commit -m "feat(security): add panic hotkey with X11 backend"
```

### Task B.3: Panic hotkey — Wayland portal backend

**Files:**
- Create: `security/src/panic_wayland.c`
- Modify: `security/src/panic.c` (backend selection)
- Modify: `security/tests/panic_test.c`

- [ ] **Step 1: Write the failing test**

The test asserts that the Wayland backend reports `best_effort=1` when the
portal is unavailable, and that the app-local fallback still registers a
shortcut.

- [ ] **Step 2: Implement the portal backend**

Use the `org.freedesktop.portal.GlobalShortcuts` D-Bus interface. If the portal
is absent, return `OYAMA_ERR_UNSUPPORTED` and let the Qt wrapper install a
`QShortcut`.

- [ ] **Step 3: Run the tests**

Run: `./build/panic_test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add security/src/panic_wayland.c security/src/panic.c security/tests/panic_test.c
git commit -m "feat(security): add Wayland global-shortcut backend"
```

### Task B.4: Qt panic controller and AppController integration

**Files:**
- Create: `app/src/panic_controller.h`
- Create: `app/src/panic_controller.cpp`
- Modify: `app/src/app_controller.h`
- Modify: `app/src/app_controller.cpp`
- Modify: `app/tests/app_controller_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `PanicController::start()`, `PanicController::stop()`,
  `AppController::panicLock()`, and the `panic` vault state.

- [ ] **Step 1: Write the failing test**

`app_controller_test.cpp` asserts that `panicLock()` on an unlocked vault sets
`vaultState()` to `panic`, clears `doses()` and `curve()`, and that
`unlockVault()` restores `unlocked`. It also asserts that a second
`panicLock()` is idempotent.

- [ ] **Step 2: Run it to verify it fails**

Run: `ctest --test-dir build -R app_controller_test --output-on-failure`
Expected: FAIL because `panicLock` does not exist.

- [ ] **Step 3: Implement the panic path**

Follow spec §5.2. Use `sodium_memzero` on every phrase buffer and
`oyama_release_free_pages()` after clearing. Gate the hotkey registration on
`OYAMA_SECURITY_PROFILE_FULL`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `ctest --test-dir build -R app_controller_test --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/src/panic_controller.h app/src/panic_controller.cpp app/src/app_controller.h app/src/app_controller.cpp app/tests/app_controller_test.cpp CMakeLists.txt
git commit -m "feat(app): add panic lock path"
```

### Task B.5: Panic lock UI

**Files:**
- Create: `app/qml/PanicLockPage.qml`
- Modify: `app/qml/Main.qml`
- Modify: `app/qml/qml.qrc`
- Modify: `app/src/main.cpp` (self-test)

**Interfaces:**
- Produces: `objectName: "panicLockPage"`, a `#808080` background, one phrase
  field, and an unlock button. The field accepts the BIP39 24-word phrase or a
  legacy phrase; there is no separate app password. `--self-test-panic` asserts
  the page is visible after `panicLock()` and hidden after a successful unlock.

- [ ] **Step 1: Write the failing self-test**

Add `--self-test-panic` to `main.cpp`; it triggers `panicLock()`, checks the
page visibility, and exits non-zero on failure.

- [ ] **Step 2: Run it to verify it fails**

Run: `QT_QPA_PLATFORM=offscreen ./build/oyama-hrt-app --self-test-panic`
Expected: FAIL because the page does not exist.

- [ ] **Step 3: Implement the QML page**

The page must cover the whole window, use `#808080`, have no navigation, and
call `app.unlockVault(phrase)` on submit. On failure, show a generic error and
keep the gray background.

- [ ] **Step 4: Run the self-test**

Run: `QT_QPA_PLATFORM=offscreen ./build/oyama-hrt-app --self-test-panic`
Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add app/qml/PanicLockPage.qml app/qml/Main.qml app/qml/qml.qrc app/src/main.cpp
git commit -m "feat(ui): add solid-gray panic lock screen"
```

### Task B.6: Docs and profile gating

**Files:**
- Create: `docs/security/panic.md`
- Modify: `SECURITY.md`, `README.md`, `CHANGELOG.md`

- [ ] **Step 1: Document the panic sequence and the limits**

State that the hotkey is best-effort on Wayland, that the vault phrase is
required, and that memory wiping cannot reach every copy (swap, hibernation,
kernel buffers).

- [ ] **Step 2: Verify the profile gating**

Build `off`, `standard`, and `full`; run `nm` on the app binary and assert that
the panic symbols are absent from `off` and `standard` and present in `full`.

- [ ] **Step 3: Commit**

```bash
git add docs/security/panic.md SECURITY.md README.md CHANGELOG.md
git commit -m "docs(security): document panic lock and profile gating"
```

**Phase B acceptance gate:** panic lock works on X11; the null backend tests
pass; the gray page is visible after panic and gone after unlock; the `off` and
`standard` binaries contain no panic symbols; docs state the limits.

---

## Phase C — Capture guard and screenshot blackout

**Branch:** `project/c-capture-guard`
**PR title:** `feat(security): black out the UI on screenshot detection`
**Depends on:** Phase A (can run in parallel with B after V).

**Deliverables:**

- `include/oyama/security/capture.h`
- `security/src/capture_proc.c`, `capture_dbus.c`, `capture_x11.c`,
  `capture_null.c`
- `security/tests/capture_test.c`
- `app/src/capture_guard.{h,cpp}`
- `app/qml/Main.qml` overlay
- Docs: `docs/security/capture-guard.md`

### Task C.1: Process watcher and null backend

**Files:**
- Create: `include/oyama/security/capture.h`
- Create: `security/src/capture_proc.c`
- Create: `security/src/capture_null.c`
- Create: `security/tests/capture_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama_capture_guard_create`, `oyama_capture_guard_start`,
  `oyama_capture_guard_stop`, `oyama_capture_guard_destroy`,
  `oyama_capture_guard_is_best_effort`.

- [ ] **Step 1: Write the failing tests**

The test creates a fake `/proc` tree (`OYAMA_CAPTURE_PROC_ROOT`) with a
directory named `1234` and a `comm` file containing `gnome-screenshot`; it
asserts the callback fires with the tool name and pid. A second test creates a
non-matching process and asserts no callback. A third test starts and stops the
guard repeatedly and asserts no leaks.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target capture_test && ./build/capture_test`
Expected: FAIL.

- [ ] **Step 3: Implement the process watcher**

Use a thread with `nanosleep` at `OYAMA_CAPTURE_POLL_MS` (default 100). Read
`/proc/<pid>/comm`, strip the newline, and compare against the built-in list
plus `OYAMA_CAPTURE_TOOLS`. Use an atomic stop flag and join the thread on stop.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `./build/capture_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/security/capture.h security/src/capture_proc.c security/src/capture_null.c security/tests/capture_test.c CMakeLists.txt
git commit -m "feat(security): add screenshot process watcher"
```

### Task C.2: D-Bus name watcher (optional)

**Files:**
- Create: `security/src/capture_dbus.c`
- Modify: `security/src/capture.c` (backend selection)
- Modify: `security/tests/capture_test.c`

- [ ] **Step 1: Write the failing test**

When `DBUS_SESSION_BUS_ADDRESS` is unset, the D-Bus backend reports
`OYAMA_ERR_UNSUPPORTED` and the process watcher still works.

- [ ] **Step 2: Implement the watcher**

Subscribe to `NameOwnerChanged` for `org.kde.KWin.ScreenShot2` and
`org.gnome.Shell.Screenshot`. Trigger only when the name is acquired, not when
it is released. Keep the connection private to the guard.

- [ ] **Step 3: Run the tests**

Run: `./build/capture_test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add security/src/capture_dbus.c security/src/capture.c security/tests/capture_test.c
git commit -m "feat(security): watch screenshot D-Bus services"
```

### Task C.3: X11 RECORD backend (optional)

**Files:**
- Create: `security/src/capture_x11.c`
- Modify: `CMakeLists.txt` (optional `libXtst`)

- [ ] **Step 1: Write the failing test**

The X11 backend is skipped when `DISPLAY` is unset. With `DISPLAY` set, it
starts and stops without crashing.

- [ ] **Step 2: Implement the RECORD watcher**

Use `XRecordCreateContext` with `XRecordAllClients` and the `GetImage` request
range. On a match, invoke the callback with `tool="x11-getimage"` and the client
pid when available.

- [ ] **Step 3: Run the test**

Run: `DISPLAY=:0 ./build/capture_test`
Expected: PASS or skip.

- [ ] **Step 4: Commit**

```bash
git add security/src/capture_x11.c CMakeLists.txt
git commit -m "feat(security): add X11 RECORD capture detection"
```

### Task C.4: Qt CaptureGuard and black overlay

**Files:**
- Create: `app/src/capture_guard.h`
- Create: `app/src/capture_guard.cpp`
- Modify: `app/qml/Main.qml`
- Modify: `app/src/main.cpp` (self-test)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `CaptureGuard::captureDetected(QString)`, `active`, `bestEffort`;
  `captureOverlay` with `objectName: "captureOverlay"`.

- [ ] **Step 1: Write the failing self-test**

`--self-test-capture` calls `CaptureGuard::testInject("gnome-screenshot")`,
asserts `captureOverlay.visible`, waits for the blackout timeout, and asserts
it is hidden.

- [ ] **Step 2: Run it to verify it fails**

Run: `QT_QPA_PLATFORM=offscreen ./build/oyama-hrt-app --self-test-capture`
Expected: FAIL.

- [ ] **Step 3: Implement the wrapper and overlay**

Marshal the C callback to the Qt main thread with
`QMetaObject::invokeMethod`. The overlay covers the window, uses `#000000`,
`z: 10000`, and `enabled: false` so it swallows input. Restore the title after
the timeout.

- [ ] **Step 4: Run the self-test**

Run: `QT_QPA_PLATFORM=offscreen ./build/oyama-hrt-app --self-test-capture`
Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add app/src/capture_guard.h app/src/capture_guard.cpp app/qml/Main.qml app/src/main.cpp CMakeLists.txt
git commit -m "feat(ui): black out the window on screenshot detection"
```

### Task C.5: Docs and limitations

**Files:**
- Create: `docs/security/capture-guard.md`
- Modify: `SECURITY.md`, `README.md`, `CHANGELOG.md`

- [ ] **Step 1: Document the detection layers and the best-effort warning**

Include the exact sentence from spec §5.3 and a list of detected tools.

- [ ] **Step 2: Verify profile gating**

`nm` must show no capture symbols in `off` and `standard`; `full` must show
them.

- [ ] **Step 3: Commit**

```bash
git add docs/security/capture-guard.md SECURITY.md README.md CHANGELOG.md
git commit -m "docs(security): document capture guard limits"
```

**Phase C acceptance gate:** the process watcher detects a fake
`gnome-screenshot` process; the overlay is visible and then hidden; `off` and
`standard` contain no capture symbols; docs state the best-effort limit.

---

## Phase D — Xen key isolation (`oyama-keyd`)

**Branch:** `project/d-xen-keyd`
**PR title:** `feat(security): isolate vault keys in a Xen key guest`
**Depends on:** Phases A, V, and B.

**Deliverables:**

- `include/oyama/security/vchan.h`, `keyproto.h`
- `keyd/` C99 daemon (D1 Unix socket, D2 vchan)
- `security/src/vchan.c` (libxenvchan wrapper)
- `security/tests/keyproto_test.c`, `vchan_test.c`
- `keyd/tests/` integration tests
- `deploy/xen/` reference configuration and docs
- `docs/security/key-isolation.md`

### Task D.1: Protocol codec

**Files:**
- Create: `include/oyama/security/keyproto.h`
- Create: `security/src/keyproto.c`
- Create: `security/tests/keyproto_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama_key_header`, `oyama_key_encode_header`,
  `oyama_key_decode_header`, `oyama_key_frame_valid`, and the op enum from spec
  §5.4.

- [ ] **Step 1: Write the failing tests**

Round-trip every op; reject a bad magic, a wrong version, an oversized
`payload_len`, a truncated header, and a header with trailing bytes. Fuzz with
1,000 random buffers and assert no crash.

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --build build --target keyproto_test && ./build/keyproto_test`
Expected: FAIL.

- [ ] **Step 3: Implement the codec**

Little-endian fixed-width encoding, no struct padding on the wire, and a hard
`OYAMA_KEY_MAX_FRAME` check before allocation.

- [ ] **Step 4: Run the tests**

Run: `./build/keyproto_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/security/keyproto.h security/src/keyproto.c security/tests/keyproto_test.c CMakeLists.txt
git commit -m "feat(keyd): add versioned key protocol codec"
```

### Task D.2: `oyama-keyd` D1 over a Unix socket

**Files:**
- Create: `keyd/main.c`
- Create: `keyd/server.c`, `keyd/server.h`
- Create: `keyd/tests/run_keyd_test.sh`
- Create: `keyd/tests/keyd_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `oyama-keyd --socket <path> --db <conninfo>`; it owns the vault
  operations and never returns the DEK.

- [ ] **Step 1: Write the failing test**

The test starts `oyama-keyd` on a temporary socket, sends `HELLO`, `UNLOCK`
with a test phrase, `PUT`, `GET`, `SCAN`, `LOCK`, and `PANIC`; it asserts the
round-trip and that `GET` after `LOCK` fails with `LOCKED`. A second test sends
a bad frame and asserts the daemon closes the connection and stays alive.

- [ ] **Step 2: Run it to verify it fails**

Run: `bash keyd/tests/run_keyd_test.sh ./build/oyama-keyd`
Expected: FAIL.

- [ ] **Step 3: Implement the daemon**

Reuse `oyama_vault` internally. Use `SO_PEERCRED` to check the peer UID.
Accept one request at a time; use a bounded frame buffer. On `PANIC`, call
`oyama_vault_lock`, zero all buffers, and keep the process alive.

- [ ] **Step 4: Run the test**

Run: `bash keyd/tests/run_keyd_test.sh ./build/oyama-keyd`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add keyd CMakeLists.txt
git commit -m "feat(keyd): add local key daemon over a Unix socket"
```

### Task D.3: `oyama_vault` remote backend

**Files:**
- Modify: `vault/src/vault.c`
- Create: `vault/src/vault_remote.c`
- Modify: `include/oyama/vault.h` (document `unix://` and `vchan://`)
- Create: `vault/tests/vault_remote_test.c`

**Interfaces:**
- Produces: `oyama_vault_open("unix:///path", ...)` and later
  `oyama_vault_open("vchan://<domain>/<port>", ...)` with the same vault API.

- [ ] **Step 1: Write the failing test**

Start `oyama-keyd`, open a remote vault, and run the existing vault test
sequence against it. Assert the local process never has the DEK by checking
that `vault->dek` is `NULL` (test-only accessor).

- [ ] **Step 2: Run it to verify it fails**

Run: `./build/vault_remote_test`
Expected: FAIL.

- [ ] **Step 3: Implement the remote backend**

Dispatch `put/get/remove/scan/clear/begin/commit/rollback` to the key daemon.
Keep the local implementation for `postgresql://` conninfo.

- [ ] **Step 4: Run the test**

Run: `./build/vault_remote_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add vault/src/vault.c vault/src/vault_remote.c include/oyama/vault.h vault/tests/vault_remote_test.c
git commit -m "feat(vault): add remote key-daemon backend"
```

### Task D.4: vchan transport

**Files:**
- Create: `include/oyama/security/vchan.h`
- Create: `security/src/vchan.c`
- Create: `security/tests/vchan_test.c`
- Modify: `CMakeLists.txt` (optional `libxenvchan`)

**Interfaces:**
- Produces: `oyama_vchan_connect`, `oyama_vchan_send`, `oyama_vchan_recv`,
  `oyama_vchan_close`. The test uses a socketpair fallback when Xen is absent.

- [ ] **Step 1: Write the failing test**

The test connects two endpoints over a socketpair, sends a framed message in
both directions, and asserts the payload. It skips the real vchan test when
`/dev/xen` is absent.

- [ ] **Step 2: Implement the transport**

Wrap `libxenvchan` behind the same send/recv API. Use a compile-time fallback
to `socketpair` for tests. Enforce the frame size limit.

- [ ] **Step 3: Run the test**

Run: `./build/vchan_test`
Expected: PASS or skip.

- [ ] **Step 4: Commit**

```bash
git add include/oyama/security/vchan.h security/src/vchan.c security/tests/vchan_test.c CMakeLists.txt
git commit -m "feat(keyd): add vchan transport"
```

### Task D.5: Key-guest hardening

**Files:**
- Modify: `keyd/main.c`, `keyd/server.c`
- Create: `keyd/seccomp.c`
- Create: `keyd/tests/run_keyd_hardening_test.sh`
- Create: `deploy/xen/keyd.config`

- [ ] **Step 1: Write the failing test**

The test asserts that `oyama-keyd` runs with `PR_SET_DUMPABLE=0`,
`RLIMIT_CORE=0`, and that `memfd_secret` is used when available. It checks
`/proc/<pid>/status` and `/proc/<pid>/maps`.

- [ ] **Step 2: Implement the hardening**

Apply `memfd_secret`/`mlock` to the DEK, `MADV_DONTDUMP`, `prctl`, `setrlimit`,
and a seccomp allowlist. Fail closed if the DEK cannot be mlocked.

- [ ] **Step 3: Run the test**

Run: `bash keyd/tests/run_keyd_hardening_test.sh ./build/oyama-keyd`
Expected: PASS or documented skip.

- [ ] **Step 4: Commit**

```bash
git add keyd deploy/xen/keyd.config
git commit -m "feat(keyd): harden the key guest process"
```

### Task D.6: Xen reference deployment

**Files:**
- Create: `deploy/xen/README.md`
- Create: `deploy/xen/oyama-ui.cfg`
- Create: `deploy/xen/oyama-key.cfg`
- Create: `deploy/xen/vchan-setup.sh`
- Modify: `docs/security/key-isolation.md`

- [ ] **Step 1: Write the deployment docs**

Document the two-guest topology, the vchan grant setup, the read-only key
rootfs, the absence of networking, and the dom0 limitation (SEV/TDX for real
dom0 resistance).

- [ ] **Step 2: Validate the configs**

Run `xl validate deploy/xen/oyama-ui.cfg` and
`xl validate deploy/xen/oyama-key.cfg` on a Xen host.
Expected: exit 0. On a non-Xen host, the script skips with a clear message.

- [ ] **Step 3: Commit**

```bash
git add deploy/xen docs/security/key-isolation.md
git commit -m "docs(keyd): add Xen reference deployment"
```

### Task D.7: Confidential-computing detection (optional)

**Files:**
- Create: `keyd/cc.c`, `keyd/cc.h`
- Modify: `keyd/main.c`
- Create: `keyd/tests/cc_test.c`
- Modify: `docs/security/key-isolation.md`

- [ ] **Step 1: Write the failing test**

The test asserts that `oyama_cc_available()` returns 0 on a machine without
`/dev/sev-guest` and without the TDX CPUID bit, and that the status output says
`confidential-computing: none`.

- [ ] **Step 2: Implement detection**

Check `/dev/sev-guest`, the SEV-SNP CPUID leaf, and the TDX CPUID bit. Report
the result; do not change the crypto path.

- [ ] **Step 3: Run the test**

Run: `./build/cc_test`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add keyd/cc.c keyd/cc.h keyd/tests/cc_test.c docs/security/key-isolation.md
git commit -m "feat(keyd): report confidential-computing capability"
```

**Phase D acceptance gate:** D1 works on an ordinary Linux machine; the remote
vault passes the existing vault test sequence; the key daemon survives malformed
frames; the vchan test passes on a Xen host; the deployment docs state the dom0
limitation.

---

## Merge order and PR checklist

| Step | Branch | Merge into `main` when |
| --- | --- | --- |
| 0 | `project/0-supply-chain` | Offline bootstrap and profile skeleton pass; no app behavior changes. |
| 1 | `project/a-cpp11-qt` | C++11, Qt 6.4 default, and Qt 5.15.2 compatibility builds pass; screenshot matrices are clean. |
| 2 | `project/v-bip39-phrase` | BIP39 vectors pass; old vaults still unlock; setup UI shows 24 words. |
| 3 | `project/b-panic-lock` | Panic tests pass; `off`/`standard` contain no panic symbols. |
| 4 | `project/c-capture-guard` | Capture tests pass; `off`/`standard` contain no capture symbols. |
| 5 | `project/d-xen-keyd` | D1 tests pass; D2/D3 documented and tested on Xen hardware. |

**Every PR must:**

- [ ] rebase on the current `main` and resolve conflicts in the branch;
- [ ] keep `ctest` green on Linux and at least one BSD;
- [ ] include the new tests and a `CHANGELOG.md` entry;
- [ ] update `README.md`, `SECURITY.md`, and `docs/dependencies.md` when the
      user-visible behavior or build requirements change;
- [ ] state the profile(s) it affects and the exact test commands run;
- [ ] not mix unrelated refactors or formatting changes.

**Release gate for the `full` profile:**

- [ ] `OYAMA_SECURITY_PROFILE=full` builds with vendored dependencies.
- [ ] The release uses Qt 6.4 by default; a Qt 5.15.2 build is optional and
      documented as EOL upstream.
- [ ] New vaults use a valid 24-word BIP39 phrase; old vaults still unlock.
- [ ] Panic lock works on X11 and Wayland (best-effort documented).
- [ ] Capture guard detects the test tools and blacks the window.
- [ ] `oyama-keyd` runs in a separate key guest and the UI guest never holds the
      DEK.
- [ ] `SECURITY.md` documents the dom0 limitation and the best-effort capture
      guard.
- [ ] The release notes do not claim “screenshot-proof” or “dom0-proof.”
