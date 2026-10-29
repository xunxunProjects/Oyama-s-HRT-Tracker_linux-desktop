# Oyama's HRT Tracker — Linux / BSD Desktop

A native Qt 6 desktop application for Linux and BSD. Local-only: no account,
no cloud, no telemetry. Health records live in a local PostgreSQL database, and
data moves in and out through the upstream `oyama-hrt-complete-backup` format.

**Status: model core, storage foundation, local database setup, backup
import/export, and an early Qt/QML shell implemented (M2).** The C99
pharmacokinetic core is in `core/`, with a strict C test and a comparison
harness that checks it against the upstream TypeScript model to machine
precision. `storage/` contains the PostgreSQL schema, forward-only migration
runner, typed repositories, the application-managed cluster lifecycle, and the
LWW/tombstone backup merge; `cli/` exposes setup, migration, data, and backup
commands. `app/` now has a Qt 6/QML shell with Overview, Add dose, History, and
Settings screens wired to the local database and the PK model, including
route-specific dose extras and in-place dose editing. It is an early milestone:
Labs, Plan, Supplies, Forecast, packaging, and full upstream UI parity are still
ahead.

- Design: [`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`](docs/superpowers/specs/2026-10-29-linux-desktop-design.md)
- PK comparison: [`tools/compare/README.md`](tools/compare/README.md)
- Upstream project: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- License: MIT (see [`LICENSE`](LICENSE))

## Build

Requirements: CMake 3.21+, Ninja (or another CMake generator), a C99 compiler,
a C++20 compiler, and libpq 14 or newer. PostgreSQL server 14+ is needed for the
database and for the integration tests. For the application-managed cluster,
`initdb`, `pg_ctl`, and `postgres` must be on `PATH` or in a directory passed
with `--pg-bin-dir`/`PG_BINDIR`. The Qt shell additionally needs Qt 6.4+ with
the Core, Qml, Quick, and QuickControls2 modules; it is skipped automatically
when those are missing and can be disabled explicitly with
`-DOYAMA_BUILD_QT_SHELL=OFF`. There is no network, OpenSSL, Node, Rust, Qt
WebEngine, or Electron dependency. Qt is used only for the local desktop UI.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The PK test binary is `build/pk_smoke_test`; the comparison dumper is
`build/oyama_pk_dump` (see `tools/compare/README.md`). The storage tests are
`build/storage_test` and `build/backup_test`; the integration CTests start
throwaway PostgreSQL clusters when `initdb`/`pg_ctl` are available, or skip.
Set `PG_BINDIR=/path/to/postgres/bin` if the server tools are not on `PATH`.

The Qt shell is `build/oyama-hrt-app`. It uses the database configured by
`oyama-hrt-cli db setup`. A headless QML smoke check is available with
`build/oyama-hrt-app --self-test` (run it with `QT_QPA_PLATFORM=offscreen
QT_QUICK_BACKEND=software`); CTest exposes the same check as `qt_shell_smoke`.
CTest also runs `app_controller_test`, which exercises settings, dose extras,
in-place editing, and deletion against a throwaway PostgreSQL cluster.
See [`app/README.md`](app/README.md) for the screen scope, supported dose
extras, and current limitations.

To install the CLI and its migrations:

```sh
cmake --install build --prefix /usr/local
```

The CLI looks for migrations in `OYAMA_MIGRATIONS_DIR`, then in the source
tree, then in `<prefix>/share/oyama-hrt/migrations`. Override the location with
`--migrations DIR` when running from a relocated install.

## Local database

The application talks to PostgreSQL through a local Unix socket only; it does
not open a TCP port or use a password.

### First-run setup

Run the setup wizard once. With a terminal it asks whether to use an existing
local server or create an application-managed cluster:

```sh
build/oyama-hrt-cli db setup
```

For scripts and tests, select a backend explicitly:

```sh
# Use an existing local server (recommended when one is already running).
build/oyama-hrt-cli db setup --existing --yes --socket-dir /run/postgresql

# Create and start an application-managed cluster.
build/oyama-hrt-cli db setup --managed --yes
```

`db setup` writes a private config file at
`$XDG_CONFIG_HOME/oyama-hrt/config` (default
`~/.config/oyama-hrt/config`, mode 0600). It records the backend, socket
directory, port, database name, user, and, for a managed cluster, the data
directory and PostgreSQL binary directory. Subsequent commands use that config
automatically; command-line connection flags still override it.

The managed cluster:

- lives in `$XDG_DATA_HOME/oyama-hrt/pgdata` (default
  `~/.local/share/oyama-hrt/pgdata`, mode 0700);
- listens on `$XDG_RUNTIME_DIR/oyama-hrt` (fallback
  `$XDG_DATA_HOME/oyama-hrt/run`) with `listen_addresses = ''` and socket
  permissions 0700;
- uses peer authentication and never opens TCP;
- is started and stopped with `db start` / `db stop`;
- is recovered on the next start after a crash;
- never installs PostgreSQL packages automatically. If the server tools are
  missing, setup prints distribution-specific install hints.

`db init` and `db migrate` start a configured managed cluster automatically if
it is stopped. `db cluster-status` shows the configured backend and state.

### Commands

```sh
build/oyama-hrt-cli db cluster-status
build/oyama-hrt-cli db ping
build/oyama-hrt-cli db init
build/oyama-hrt-cli db status
build/oyama-hrt-cli settings set --weight 70 --birth-year 1995
build/oyama-hrt-cli doses add \
  --mode transfem --route oral --ester E2 --dose-mg 2 --time-h 480000
build/oyama-hrt-cli backup export ~/oyama-backup.json
build/oyama-hrt-cli backup preview ~/oyama-backup.json
build/oyama-hrt-cli backup import ~/oyama-backup.json
```

`db init` creates the `oyama_hrt` database when missing and applies all pending
migrations in one transaction. Migrations live in `storage/migrations/` and are
forward-only; the runner records a checksum for each applied file.

### Backup import/export

Backups use the upstream `oyama-hrt-complete-backup` format, version 1. Export
includes the optional `birthYear` extension; upstream ignores it, so a
round-trip through the upstream app loses that field (the CLI preview reports
it). Import and export are plaintext JSON and contain health data; export writes
the file with mode 0600 and prints a warning.

Merge behavior follows upstream:

- `updatedAt` last-write-wins for events, labs, and dose templates;
- tombstones have a 5-minute future tolerance, a 180-day TTL, and a 5000-per-kind
  cap;
- quick doses, supplies, and plan items are unstamped: if the same ID exists
  locally, that whole category is blocked rather than overwriting silently;
- each category is validated independently; blocked categories leave local data
  untouched while other categories still merge;
- import runs in one transaction, so a failure leaves the database unchanged.

Our schema stores UUID record IDs, so non-UUID IDs from another client are
rejected rather than rewritten. Lab records without an assay are imported as
`immuno` because the local schema requires an assay value.

The storage library exposes `LocalCluster`; the Qt shell calls
`ensure_running()` during application startup, applies migrations, and loads
settings and doses through the same repositories as the CLI. The app leaves a
managed cluster running on exit; use `oyama-hrt-cli db stop` to stop it.

> [!WARNING]
> **Not medical advice.** This application shows estimates from a
> pharmacokinetic model. Those estimates can differ substantially from real
> blood levels. Do not start, stop, or adjust any medication based on this
> application alone.

## 中文简介

这是 Oyama's HRT Tracker 的 Linux / BSD 原生桌面版，使用 Qt 6 + QML 重写界面，
核心药代动力学模型用 C99 实现。它只在本机运行，没有登录、云同步、遥测或分享功能；
数据存在本地 PostgreSQL 中，通过导出/导入与主仓库版本互通。

目前 C99 药代动力学核心已经完成，并与上游 TypeScript 模型做了逐点对比
（36 个场景、413,323 个字段，最大相对误差约 1.1e-15）。PostgreSQL 存储层、
迁移调度、托管本地 cluster 生命周期、首次设置向导、备份导入/导出与
LWW/删除合并、基础 CLI 也已经落地。Qt 6/QML 桌面壳已经进入 M2：Overview、
Add dose、History、Settings 四个页面已接入本地数据库和 PK 模型，并支持
按给药途径的 extras 与剂量就地编辑；Plan、Labs、Supplies、Forecast、
桌面打包以及完整界面对齐仍未完成。

## Repository layout

```text
core/
  include/oyama/       stable C99 PK API
  src/                 PK implementation
  tests/               C99 behavioural tests
storage/
  include/oyama/       C++20 storage API (connection, migrations, repositories)
  src/                 libpq implementation
  migrations/          forward-only SQL schema migrations
  tests/               unit and PostgreSQL integration tests
cli/                   oyama-hrt-cli (database and data commands)
app/                   Qt 6/QML shell (M2; see app/README.md)
tools/compare/         upstream numerical comparison harness
docs/
  superpowers/specs/   design documents
  spec/                model and format contracts (added during M0)
```
