# Oyama's HRT Tracker — Linux / BSD Desktop

A native Qt 6 desktop application for Linux and BSD. Local-only: no account,
no cloud, no telemetry. Health records live in a local PostgreSQL database, and
data moves in and out through the upstream `oyama-hrt-complete-backup` format.

**Status: model core and storage foundation implemented; Qt application shell
not started.** The C99 pharmacokinetic core is in `core/`, with a strict C test
and a comparison harness that checks it against the upstream TypeScript model to
machine precision. `storage/` contains the PostgreSQL schema, forward-only
migration runner, and typed repositories; `cli/` exposes the migration and data
commands. The Qt/QML application and desktop packaging are still ahead.

- Design: [`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`](docs/superpowers/specs/2026-10-29-linux-desktop-design.md)
- PK comparison: [`tools/compare/README.md`](tools/compare/README.md)
- Upstream project: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- License: MIT (see [`LICENSE`](LICENSE))

## Build

Requirements: CMake 3.21+, Ninja (or another CMake generator), a C99 compiler,
a C++20 compiler, and libpq 14 or newer. PostgreSQL server 14+ is needed only
for the integration tests and for running the CLI against a database. There is
no network, OpenSSL, Node, Rust, or Qt dependency in this part of the tree.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The PK test binary is `build/pk_smoke_test`; the comparison dumper is
`build/oyama_pk_dump` (see `tools/compare/README.md`). The storage test is
`build/storage_test`; the `storage_integration` and `cli_integration` CTests
start throwaway PostgreSQL clusters when `initdb`/`pg_ctl` are available, or
skip. Set `PG_BINDIR=/path/to/postgres/bin` if the server tools are not on
`PATH`.

To install the CLI and its migrations:

```sh
cmake --install build --prefix /usr/local
```

The CLI looks for migrations in `OYAMA_MIGRATIONS_DIR`, then in the source
tree, then in `<prefix>/share/oyama-hrt/migrations`. Override the location with
`--migrations DIR` when running from a relocated install.

## Local database

The application talks to PostgreSQL through a local Unix socket only; it does
not open a TCP port or use a password. Point the CLI at an existing cluster:

```sh
build/oyama-hrt-cli --socket-dir /run/postgresql db init
build/oyama-hrt-cli --socket-dir /run/postgresql db status
build/oyama-hrt-cli --socket-dir /run/postgresql settings set --weight 70 --birth-year 1995
build/oyama-hrt-cli --socket-dir /run/postgresql doses add \
  --mode transfem --route oral --ester E2 --dose-mg 2 --time-h 480000
```

`db init` creates the `oyama_hrt` database when missing and applies all pending
migrations in one transaction. Migrations live in `storage/migrations/` and are
forward-only; the runner records a checksum for each applied file.

The app-managed PostgreSQL cluster lifecycle (`initdb`, `pg_ctl`, first-start
wizard) is designed but not implemented yet; the current connection manager
expects an existing cluster.

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
迁移调度和基础 CLI 也已经落地；Qt/QML 界面和桌面打包尚未开始。

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
app/                   Qt/QML shell (deferred; see app/README.md)
tools/compare/         upstream numerical comparison harness
docs/
  superpowers/specs/   design documents
  spec/                model and format contracts (added during M0)
```
