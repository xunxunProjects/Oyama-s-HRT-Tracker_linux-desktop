# Oyama's HRT Tracker — Linux / BSD Desktop Design

- **Status**: Draft, awaiting owner approval
- **Date**: 2026-10-29
- **Target repository**: `xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop`
- **Upstream repository**: `xunxunProjects/Oyama-s-HRT-Tracker` (v1.5.1)
- **License**: MIT

> **Superseded:** §10.3 (plaintext backup export) and §11 (security model)
> are superseded by
> [`2026-10-31-encrypted-vault-design.md`](2026-10-31-encrypted-vault-design.md).
> The vault design adds application-layer encryption, a paper recovery phrase,
> encrypted backups by default, legacy plaintext migration, and the revised
> threat model. The rest of this document remains the baseline desktop design.

---

## 0. In One Sentence

Rewrite the web version as a native desktop application for Linux and BSD with Qt 6 / QML, visually consistent with the web version: no login, no cloud, no telemetry; the pharmacokinetics core is implemented in C99, the interface in C++20 + QML, data is stored in a local PostgreSQL, and import/export follows the upstream `oyama-hrt-complete-backup` format.

---

## 1. Background

The upstream application consists of a React + TypeScript frontend, a Cloudflare Worker backend, and a Tauri v1 desktop shell. Tauri's release matrix builds only macOS (dmg) and Windows (msi), so Linux users can currently only use it through Docker/a local service plus a browser, and BSD has no official path.

This repository is not a patch branch of upstream but an independent implementation:

- Upstream continues to serve Web, Windows, macOS, and the account system.
- This repository serves only Linux/BSD desktop and is allowed to diverge from upstream long-term.
- The two sides maintain verifiable consistency through **model vectors** and the **backup format**, rather than sharing code.

---

## 2. Settled Decisions

| # | Decision | Notes |
|---|------|------|
| D1 | Independent repository, allowed to diverge long-term | Not a fork of upstream; will not merge upstream commits |
| D2 | Qt 6 + QML native interface | Visually aligned with the web version; no embedded WebEngine |
| D3 | C99 core + C++20 application layer | C++23 is used only for features confirmed available; C++23 is not a minimum requirement |
| D4 | Local PostgreSQL storage | Owner's decision; accessed via libpq; the database layer is isolated behind a repository interface |
| D5 | No login, no accounts | Also removes sharing, community voting, device pairing, F2F, Tool API, cloud sync, and the admin back office |
| D6 | Keep export/import | Follows upstream complete backup format v1, ensuring users can migrate between the two versions |
| D7 | Linux and BSD only | Windows/macOS are out of scope |
| D8 | X11 + Wayland | KDE, XFCE, and Niri are the primary test targets; no DE-specific code |
| D9 | Model consistency via golden vectors | Upstream tests exported as JSON; the C implementation runs the same vector set |
| D10 | Zero network by default | Makes no network requests; PostgreSQL uses a Unix socket |

> D4 is the only decision in this design that is in tension with the "local-first, easy to install" goal. See §14 Risks and the mitigation in §5.3.

---

## 3. Goals, Non-Goals, Anti-Goals

### 3.1 Goals

1. Linux/BSD users get a native, offline desktop application they can build themselves.
2. Core computation results match upstream, and any discrepancy can be caught by vector tests.
3. Users' health data exists only on the local machine; exported files can be imported by upstream and vice versa.
4. Usable on KDE (Wayland/X11), XFCE (X11), Niri (Wayland), and FreeBSD (X11).
5. A simple source build path: CMake + Ninja, one command to configure and one to build.

### 3.2 Non-Goals

- Accounts, login, passwords, 2FA, session management.
- Cloud sync, real-time sharing, community voting, F2F, Tool API.
- Multi-user or family sharing.
- Windows, macOS, mobile.
- Feature parity with all upstream functionality in the first version.
- Bundling Postgres binaries or automatically installing system packages.

### 3.3 Anti-Goals

- Model results quietly diverging from upstream.
- Import/export losing records or writing wrong fields.
- Requiring users to install Node, npm, Tauri, or Rust for the sake of "native".
- Any form of telemetry, crash reporting, or remote configuration.
- Sending health data over the network.

---

## 4. Architecture

```text
┌──────────────────────────────────────────────┐
│ app/  C++20 + Qt 6                           │
│  QML views  ←→  C++ view models              │
│  charts / dialogs / i18n / settings          │
└───────────────┬──────────────────────────────┘
                │ pure function calls (C ABI)
┌───────────────▼──────────────────────────────┐
│ core/  C99                                   │
│  PK model, units, dose advice, calibration,  │
│  plan projection                             │
│  no I/O, no global state, deterministic      │
└───────────────┬──────────────────────────────┘
                │ repository interfaces
┌───────────────▼──────────────────────────────┐
│ storage/  C++20 + libpq                      │
│  schema migrations, transactions, queries,   │
│  import/export                               │
└───────────────┬──────────────────────────────┘
                │ Unix socket
┌───────────────▼──────────────────────────────┐
│ local PostgreSQL 14+                         │
└──────────────────────────────────────────────┘
```

### 4.1 Layers

| Directory | Language | Responsibility | Dependencies |
|------|------|------|------|
| `core/` | C99 | Pharmacokinetics, unit conversion, dose suggestions, calibration, regimen pacing | libm only |
| `storage/` | C++20 | libpq wrapper, migrations, transactions, repositories | libpq, core |
| `app/` | C++20 + QML | Windows, view models, charts, settings, i18n | Qt 6, storage, core |
| `cli/` | C99/C++20 | Initialize database, import/export, run vectors | core, storage |
| `tests/` | C99/C++20 + Qt Test | Unit, integration, QML, vectors | everything |

### 4.2 Why a C99 Core

- No Qt dependency; it can be compiled separately into a static library and reused by both the CLI and tests.
- Stable C ABI; simple to call from C++ without a binding generator.
- The model is pure mathematics and needs no RAII, exceptions, or templates.
- Upstream `logic.ts` is written in a pure-function style, making function-by-function porting the cheapest approach.

### 4.3 Why C++20 instead of C++23

- Qt 6 itself only requires C++17; C++20 provides concepts/ranges/`std::span`, which is enough.
- No standard library fully implements C++23; Clang versions on FreeBSD/OpenBSD vary.
- Minimum toolchain: GCC 11+ or Clang 14+.
- C++23 features (such as `std::expected`, `std::print`) are used only behind detection macros and are not part of the public interface.

### 4.4 Threading Model

- The QML/UI thread only handles rendering and lightweight state.
- Simulations run on `QThreadPool` worker threads; input is an immutable array of events, and output is returned via signals.
- libpq connections are not shared across threads; each worker thread holds its own connection or uses a small connection pool.
- Database writes happen in transactions; the UI updates optimistic state first, then rolls back and notifies on failure.

---

## 5. Data Model (PostgreSQL)

### 5.1 Tables

```sql
-- schema version (maintained by the migration runner; name/checksum prevent applied files from being rewritten)
schema_migrations(version integer primary key, name text not null,
                  checksum text not null, applied_at timestamptz not null);

-- global settings, single row; consistent with upstream: weight/PK parameters/calibration/birth year are not split by mode
settings(
  id boolean primary key default true check (id),
  body_weight_kg double precision,
  birth_year integer,
  pk_params jsonb,                 -- null = use default parameters
  pk_params_updated_at bigint,
  calibration_method text not null default 'mipd',
  calibration_history_mode text not null default 'retrospective',
  updated_at bigint not null
);

dose_events(
  id uuid primary key,
  mode text not null check (mode in ('transfem','transmasc')),
  route text not null,
  ester text not null,
  time_h double precision not null,   -- hours since 1970, consistent with upstream
  dose_mg double precision not null,
  extras jsonb not null default '{}',
  updated_at bigint,
  created_at bigint not null
);

lab_results(
  id uuid primary key,
  mode text not null check (mode in ('transfem','transmasc')),
  time_h double precision not null,
  value double precision not null,
  unit text not null,
  assay text not null check (assay in ('immuno','lcms')),
  updated_at bigint,
  created_at bigint not null
);

dose_templates(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
quick_doses(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
supplies(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);
plan_items(id uuid primary key, mode text not null, payload jsonb not null, updated_at bigint);

-- needed for import merge semantics; may be empty when records are never deleted on a single machine
tombstones(kind text not null, record_id uuid not null, deleted_at bigint not null,
           primary key (kind, record_id));

preferences(key text primary key, value jsonb not null, updated_at bigint not null);
```

Design notes:

- `time_h` uses `double precision`, consistent with upstream `number` semantics, avoiding curve drift caused by integer conversion.
- The tables that store the record body as `jsonb` (templates/quick doses/supplies/plan) are there to keep pace with upstream field changes early on; once things stabilize, we will decide whether to split them into columns.
- `updated_at` uses epoch milliseconds, preserving upstream's last-write-wins semantics so behavior is consistent during import merges.
- The two modes (transfem/transmasc) are independent; switching modes does not share dose/lab/plan records; however **body weight, birth year, PK parameters, calibration method, and history mode are global settings**, consistent with upstream `SHARED_SUFFIXES` behavior.
- Default calibration is `mipd` / `retrospective`, consistent with the default behavior of upstream `normalizeCalibrationMethod` and `cal-history-mode`; age correction is off by default and enabled by a switch in settings.
- No `users`, `sessions`, `shares`, `community_vote`, `f2f_*`, or similar tables are created.

### 5.2 Migrations

- SQL files live in `storage/migrations/NNNN_*.sql` and are ordered by filename.
- Applied in a transaction at startup, using `pg_advisory_xact_lock` to prevent concurrent migration by multiple instances.
- Each applied file records an FNV-1a 64-bit checksum; migration startup is refused if a file has been rewritten, is missing, or has a duplicate version.
- Migrations move forward only and never roll back; destructive changes must come with an export/rebuild strategy.
- The first version of the schema is frozen by M1; after that, changes are additive only.

### 5.3 Connection and Initialization (Mitigating Postgres UX)

On first launch, two options are offered:

1. **Connect to an existing cluster (recommended default)**
   - Connect via a Unix socket in `$XDG_RUNTIME_DIR`, `/run/postgresql`, or a user-specified socket directory.
   - Use peer/ident authentication; no password is stored.
   - The database name defaults to `oyama_hrt`; the role is the current system user.

2. **Application-managed local cluster (optional)**
   - Detect `initdb`, `pg_ctl`, and `postgres` on PATH.
   - Initialize in `$XDG_DATA_HOME/oyama-hrt/pgdata` with directory permissions 0700.
   - Listen only on a Unix socket (`$XDG_RUNTIME_DIR/oyama-hrt`); do not open TCP.
   - Start when the application launches, stop when it exits; recover on the next launch after a crash.
   - Do not install Postgres packages automatically; show distribution install commands when missing.

If neither is available, the application shows a setup wizard and documentation links and does not enter the main interface. This flow must be completed in M1, otherwise the application cannot start.

**Implementation Addendum (2026-10-29)**

- `storage::LocalCluster` provides `initialize()`, `start()`, `stop()`, `ensure_running()`, and `state()`; the Qt application will later call `ensure_running()` at startup and `stop()` on normal exit, with `pg_ctl` recovering after a crash on the next launch.
- The managed cluster's `postgresql.conf` includes a generated config via `include_if_exists = 'oyama-runtime.conf'`, fixing `listen_addresses = ''`, the port, the socket directory, and `unix_socket_permissions = 0700`; even running `pg_ctl start` directly, bypassing the application, will not listen on TCP.
- The selection is saved in `$XDG_CONFIG_HOME/oyama-hrt/config` (overridable with `OYAMA_CONFIG_FILE`, file mode 0600), recording backend, socket, port, database name, and user; managed mode additionally records the data dir and PostgreSQL binary directory.
- The managed socket directory defaults to `$XDG_RUNTIME_DIR/oyama-hrt`; when `XDG_RUNTIME_DIR` is absent it falls back to `$XDG_DATA_HOME/oyama-hrt/run`. `OYAMA_PG_RUNTIME_DIR` can override this. The socket path must be shorter than the Unix domain socket limit.
- The first-run wizard is currently implemented as `oyama-hrt-cli db setup` (terminal interaction; scripts use `--existing`/`--managed --yes`). The Qt UI will later reuse the same `AppConfig`/`LocalCluster` API rather than reimplement the logic.
- Automatic PostgreSQL installation remains forbidden; when tools are missing, the command line prints installation hints for Debian/Ubuntu, Fedora, Arch, Gentoo, FreeBSD, and OpenBSD.

---

## 6. Model Contract (golden vectors)

### 6.1 Authoritative Sources

- Algorithm description: upstream `Algorithm Explanation.md`.
- Behavioral source: upstream `logic.ts` and its tests.
- Independent cross-validation: upstream `tests/reference/pkcoreUpstream.ts` (a port of the Swift model).

### 6.2 Vector Format

`docs/spec/pk-vectors.json`:

```json
{
  "format": "oyama-hrt-pk-vectors",
  "version": 1,
  "upstreamCommit": "<sha>",
  "cases": [
    {
      "id": "simulation/overlapping-routes-sum",
      "source": "tests/simulation.test.ts",
      "input": { "...": "..." },
      "expect": { "...": "..." },
      "tolerance": { "relative": 1e-6, "absolute": 1e-9 }
    }
  ]
}
```

- Every vector must be traceable to a specific test file or case.
- Categories: injections (EB/EV/EC/EN/EU/E2), patches, gel, oral, sublingual, CPA, testosterone, calibration (off/ekf/ou_kalman/mipd), age, units, dose cleaning, regimen pacing, and thresholds for suggestion text.
- Floating-point comparison: concentration-like values use relative 1e-6; classification/text/selection results must match exactly.

### 6.3 Execution

- `cli/hrt-vectors` reads the JSON, calls `core`, and outputs failing cases and maximum error.
- Registered with CTest as `core-vectors`; CI must pass.
- When upstream adds or changes a model, the vector file in this repository must be updated and pass before a release.
- Any **intentional** deviation from upstream behavior must be recorded in `docs/spec/divergences.md` with the reason, impact, and version number.

### 6.4 Acceptance

At the end of M1, `core` must pass **all** vectors exported in M0, covering both modes, all modelled routes and esters, CPA, testosterone, age correction, and the four calibration methods (`off`/`ekf`/`ou_kalman`/`mipd`).

"We'll support the main routes first and the rest later" is not acceptable: an unmodelled route that can be selected in the UI will silently produce a wrong curve. Drugs that are genuinely not modelled (upstream `UNMODELLED_DRUGS`) are recorded only, do not participate in simulation, and are clearly marked in the UI.

If a category cannot be ported for the time being, the corresponding entry must be disabled in the UI rather than allowed to return an approximation.

---

## 7. Interface Scope

### 7.1 Visual Principles

- Align with upstream's color palette, corner radii, cards, type hierarchy, and dark mode; pixel-perfect parity is not a goal, but users should recognize at a glance that it is the same product.
- Desktop layout: left navigation + content area; minimum window 900×600; narrow windows collapse to top navigation.
- Charts reuse upstream's axes, colors, and interactions (zoom, hover readouts, forecast segments).
- Copy reuses upstream i18n keys to avoid two sets of terminology.

### 7.2 First Version (M3–M4)

| Page | Content |
|------|------|
| Onboarding | Disclaimer, mode selection, weight/birth year (optional), database initialization |
| Overview | Current estimated concentration, today's dose, concentration curve, forecast toggle |
| Add dose | Route, ester/drug, dose, time, extras |
| History | Grouped by day, edit, delete |
| Plan / What-if | Plan editing, frequency, simulation comparison |
| Settings | Mode, weight, birth year (including age correction toggle), PK parameters, calibration method, appearance, language |
| Labs (read-only) | List of imported lab results, ensuring data used for calibration is visible |
| Export / Import | Complete backup export, preview, merge/replace import |

### 7.3 Second Version (M5+)

- Labs: lab results, unit conversion, calibration points.
- Supplies: inventory, consumption, run-out forecast.
- Forecast.
- Dose templates / quick doses.
- Advanced calibration methods and history modes.

### 7.4 i18n

Upstream supports `zh`, `zh-TW`, `yue`, `en`, `ja`, `ko`, and `tr`. This repository reuses the same key set in JSON/`.ts` form; the first version supports at least zh + en; other languages are supplied as data and are not hard-coded in QML.

---

## 8. Desktop Integration and Compatibility Matrix

### 8.1 Principles

- No DE-specific code; rely only on XDG standards and Qt's cross-platform abstractions.
- Do not assume a system tray, global menu, title bar, or window manager decorations.
- File dialogs go through the XDG portal first; fall back to Qt dialogs when the portal is unavailable.
- Notifications go through `org.freedesktop.Notifications`; fail silently.
- Single instance via `QLocalServer`; a second launch raises the existing window.

### 8.2 Test Matrix

| System | Session | Desktop/WM | Priority |
|------|------|---------|--------|
| Linux | Wayland | KDE Plasma 6 | P0 |
| Linux | X11 | KDE Plasma 6 | P0 |
| Linux | X11 | XFCE 4.18 | P0 |
| Linux | Wayland | Niri | P0 |
| Linux | Wayland | GNOME 46 | P1 |
| FreeBSD 14 | X11 | XFCE | P0 |
| OpenBSD 7.x | X11 | XFCE | P1 |

Notes:

- **Niri is Wayland-only** and has no tray; when window decorations are client-side, a title bar cannot be assumed.
- **XFCE is primarily X11**; Wayland sessions are not a first-version target.
- **BSD's Wayland ecosystem is very weak**; the first version ships for X11; Niri does not run on BSD.
- FreeBSD's Qt 6 version is relatively new; OpenBSD's Qt 6 packages may lag and are treated as P1.

### 8.3 Desktop Files

- `.desktop` file, AppStream metainfo, SVG icon.
- Application ID TBD (see §15), candidate: `com.smirnovayama.hrttracker.LinuxDesktop`.
- Optional: register a MIME type for `.json` backup files for double-click import (may be omitted in the first version).

---

## 9. Build, Dependencies, Packaging

### 9.1 Dependencies

| Dependency | Minimum version | Purpose |
|------|----------|------|
| CMake | 3.21 | Build |
| Ninja | any | Build |
| C compiler | C99 (GCC 11+/Clang 14+) | core |
| C++ compiler | C++20 (GCC 11+/Clang 14+) | app/storage |
| Qt | 6.4 | Quick, Quick Controls 2, Svg, Concurrent |
| libpq | 14 | Database client |
| PostgreSQL server | 14 | Local database |
| OpenSSL | 3.x | Used only if upstream backup encryption enters scope |

No dependency on Node, npm, Rust, Tauri, or WebEngine.

### 9.2 Build Commands

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
cmake --install build/linux-release
```

`CMakePresets.json` provides `linux-release`, `linux-debug`, and `freebsd-release`.

### 9.3 Packaging

- CPack generates `deb`, `rpm`, and `tar.gz`.
- AppImage as an optional artifact (requires an additional toolchain).
- Flatpak manifest after M5.
- FreeBSD port after M5.
- The first version does not promise a binary repository; source buildability comes first.

### 9.4 CI

| Platform | Purpose |
|------|------|
| Ubuntu 24.04 | Main build, unit tests, ASan/UBSan |
| Fedora 40 | Qt/compiler compatibility |
| Arch Linux container | Rolling-release compatibility |
| FreeBSD 14 VM | BSD build + X11 smoke test |
| Xvfb / offscreen | QML smoke tests |

Postgres integration tests use a temporary cluster (`initdb` + `pg_ctl`) and do not depend on an external service.

---

## 10. Backup Interoperability

### 10.1 Format

- Uses upstream `oyama-hrt-complete-backup`, `formatVersion = 1`.
- Top-level fields: `format`, `formatVersion`, `createdAt`, `mode`, `modes`, `weight`, `weightUpdatedAt`, `pkParams`, `pkParamsUpdatedAt`, `calibration`, `preferences`.
- Under `modes`: `events`, `labResults`, `doseTemplates`, `deletions`, `quickDoses`, `supplies`, `plan`.
- This application has no account, so `sourceAccount` is omitted on export; it is ignored on import.
- Upstream v1 backups have no `birthYear` field. This application adds optional top-level extensions `birthYear` and `birthYearUpdatedAt`; upstream import ignores unknown top-level fields, so a "this app → upstream → this app" round trip loses the birth year. The import preview must point this out and must not claim losslessness.
- Apart from `birthYear`, the first version adds no other extension fields; avoid creating private data that upstream cannot preserve.

### 10.2 Import

- Preview first: record counts, conflict counts, deleted counts, scalar changes.
- Merge rules match upstream: `updatedAt` last-write-wins; tombstones have a 5-minute future tolerance, 180-day TTL, max 5000 per category; max 20000 records per category.
- Import runs in a transaction; failure rolls everything back.
- Unknown fields: top-level fields are preserved but ignored; fields inside records are dropped and reported, matching upstream behavior.

### 10.3 Export

- Outputs JSON that can be imported directly by the upstream web version.
- Self-check before export: must not export records that the upstream importer would reject.
- Plaintext export; the file contains health data; the export dialog must clearly warn.
- Encrypted `.hrtb` is not in the first version; implemented later via a separate OpenPGP-compatible layer.

### 10.4 Implementation Addendum (2026-10-29)

- `storage/backup.h` provides `preview_backup`, `import_backup`, and `export_backup`, using C++20 + nlohmann JSON, without introducing networking or OpenSSL.
- CLI commands are `backup export FILE`, `backup preview FILE`, and `backup import FILE [--dry-run]`.
- Merge implementation aligned with upstream: `events`/`labResults`/`doseTemplates` use `updatedAt` LWW; identical timestamps are deterministically broken by content fingerprint; tombstone future tolerance 5 minutes, TTL 180 days, max 5000 per category.
- `quickDoses`/`supplies`/`plan` have no upstream timestamps; if an imported id already exists locally, that whole category is blocked rather than silently overwritten.
- Import validates atomically per category; a root-envelope error rejects the entire file; when other categories succeed, they can still be applied in a single transaction.
- This database requires record ids to be UUIDs; upstream non-UUID ids are rejected. Upstream lab `assay` may be missing; this database stores it as `immuno` by default.
- Export includes the local `birthYear`/`birthYearUpdatedAt` extension; it is lost on an upstream round trip and must be flagged in the import preview.
- Export is plaintext JSON with file mode 0600; the UI must continue to show the health data warning.

---

## 11. Security and Privacy

- **Zero network**: the application contains no HTTP client; Postgres uses only a Unix socket. If sync is added in the future, it must be designed separately and off by default.
- **Zero telemetry**: no crash reporting, usage statistics, or remote configuration.
- **Database permissions**: data directory 0700; socket directory 0700; database role allows only the current system user.
- **Export files**: plaintext, warn users to keep them safe; full-disk encryption recommended.
- **Threat model**: a local user who can log in to the same system account can read the database; this is the same as any local application and is not addressed in the first version.
- **Memory safety**: the C++ side uses only RAII/smart pointers and forbids bare `new/delete`; CI runs ASan/UBSan; the C core runs `-fanalyzer`/clang-tidy.
- **Dependency updates**: minimum versions of Qt, libpq, and Postgres are updated with security advisories; OpenSSL is not statically linked.

---

## 12. Test Strategy

| Layer | Content | Tool |
|----|------|------|
| core | vectors, boundaries, randomized differential | CTest + custom runner |
| storage | migrations, transactions, import/export, concurrency | CTest + temporary Postgres |
| app | view models, units, formatting | Qt Test |
| QML | page smoke tests, navigation, dialogs | Qt Quick Test |
| compatibility | X11/Wayland/DE matrix | manual + CI smoke |
| security | ASan/UBSan, clang-tidy | CI |

Acceptance gates:

- All vectors pass.
- After importing a sample file exported by upstream, record counts, times, doses, and modes match the source file.
- Files exported by this application can be imported by the upstream web version (manual acceptance, once per milestone).

---

## 13. Milestones

| Milestone | Deliverable | Exit criteria |
|--------|------|----------|
| M0 | Repository, design document, vector extraction, UI inventory, backup samples, CMake skeleton | Document approved; vector file can be generated; empty CI run passes |
| M1 | `core` model + vector runner; Postgres schema + migrations; first-run wizard | All vectors pass; empty database can be initialized; import/export CLI usable |
| M2 | QML shell, Overview, Add dose, History | Can record, edit, delete, and see the curve |
| M3 | Plan/What-if, Settings, Export/Import UI, read-only Labs | Complete backup round trip loses no known fields |
| M4 | Labs editing, Supplies, Forecast | Upstream core features aligned |
| M5 | Packaging, BSD CI, desktop integration, documentation | deb/rpm installable; FreeBSD build passes |

"First version" means **M3**: a version that can be built from source and used daily to record and view curves, without public binary packages. **M5 is the first public binary release.** M4 is not a release blocker for M3, but imported Labs must be visible in read-only form in M3, otherwise calibration would use data the user cannot see.

---

## 14. Risks

| Risk | Impact | Mitigation |
|------|------|------|
| Postgres install/runtime barrier | Users cannot install it, and the application cannot start | First-run wizard; application-managed cluster; documentation; must be completed in M1 |
| Model drift | Results diverge from upstream | golden vectors + upstream CI; differences must be documented |
| UI rewrite effort | Long-term failure to deliver | Phased; main line first, then Labs/Supplies; reuse design tokens |
| Qt/BSD compatibility | BSD users cannot build | Qt 6.4 floor; FreeBSD CI; X11 first |
| C++ memory safety | Crashes/data corruption | RAII, smart pointers, ASan/UBSan, clang-tidy |
| Scope creep | Never reaches the first version | Explicit non-goals; reject accounts/social/sync outright |
| Two implementations diverge long-term | Doubled maintenance cost | Vector contract; backup format; share tests but not code |

---

## 15. Open Items (need owner or implementer sign-off)

| # | Question | Recommendation |
|---|------|------|
| Q1 | Application ID / .desktop name | `com.smirnovayama.hrttracker.LinuxDesktop`, avoiding conflict with upstream Tauri packages |
| Q2 | Chart implementation | Custom-drawn `QQuickPaintedItem`, no dependency on QtCharts/Qt Graphs, for easier alignment with Web charts |
| Q3 | Default Postgres initialization method | Probe existing clusters first; if none, offer an application-managed cluster |
| Q4 | First-version languages | zh + en; other languages added later as data |
| Q5 | Encrypted backups | Not in the first version; evaluate OpenPGP compatibility after M5 |
| Q6 | Package formats | First version source + deb/rpm; AppImage optional |
| Q7 | Minimum Qt | 6.4 (covers Debian 12 / Ubuntu 24.04); FreeBSD uses the ports version |

The implementer may decide the details of Q2, Q3, and Q6 without changing product behavior; Q1, Q4, and Q5 need owner confirmation.

---

## 16. Repository and Branch Strategy

- `main` is protected:
  - Merges must go through a Pull Request (0 approvals allowed; the author may merge their own).
  - CI checks must pass (enabled once CI exists).
  - Force push and branch deletion are forbidden.
  - Linear history is required; merges use squash.
- Branch naming: `feat/…`, `fix/…`, `docs/…`, `chore/…`.
- Commit messages use Conventional Commits.
- Vector and format files under `docs/spec/` are frozen per version; changes require a separate PR.
- Upstream model changes enter this repository through "update the vectors"; do not cherry-pick code directly.

---

## 17. Builder Check Handling Record

After the design document was written, an independent implementer did a "can a plan be made from this" check. The six questions and their handling are as follows:

| # | Question | Handling |
|---|------|------|
| B1 | How much of the model must M1 port | **Port everything**: both modes, all modelled routes, CPA, testosterone, age, and the four calibration methods. Uncovered entries are disabled in the UI and do not return approximations. This is a fairly large scope decision; the owner may request a reduction, but any reduction must also disable the corresponding entries. |
| B2 | Are weight/birth year/PK/calibration global or per-mode | **Global**. The schema changed to a single-row `settings`; record classes are still partitioned by `mode`; consistent with upstream `SHARED_SUFFIXES`. |
| B3 | How does birth year get in and out of backups | Top-level extension fields `birthYear`/`birthYearUpdatedAt`; upstream ignores them, so they are lost on a round trip, and the import preview notes this. |
| B4 | Which milestone is the first version | **M3 is the first source version usable day to day; M5 is the first public binary release.** |
| B5 | What about backup categories that only have storage and no UI | M3 still imports them fully into the database; Labs provides a read-only list so calibration inputs are visible; Supplies/templates/quick doses show counts in the import preview, with full UI in M4. |
| B6 | Do model defaults align with upstream | **Aligned**: `mipd` / `retrospective`; age correction off by default, enabled by a settings toggle. |

---

## 18. Approval Record

- [ ] Owner approves this design document
- [ ] After approval, proceed to the implementation plan (`writing-plans`)
- [ ] M0 starts
