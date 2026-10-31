# Oyama's HRT Tracker — Linux / BSD Desktop

A native Qt 6 desktop application for Linux and BSD. Local-only: no account,
no cloud, no telemetry. Health records live in a local PostgreSQL database and
are encrypted at the application layer before they reach PostgreSQL. Data moves
in and out through the upstream `oyama-hrt-complete-backup` format, encrypted by
default.

**Status: model core, encrypted storage, legacy migration, encrypted backups,
and an early vault-gated Qt/QML shell implemented (M2).** The C99
pharmacokinetic core is in `core/`, with a strict C test and a comparison
harness that checks it against the upstream TypeScript model to machine
precision. `storage/` contains the PostgreSQL schema, forward-only migration
runner, the encrypted vault and its C ABI, typed repositories, the
application-managed cluster lifecycle, and the LWW/tombstone backup merge;
`cli/` exposes setup, vault, migration, data, and backup commands. `app/` has a
Qt 6/QML shell with setup/unlock/lock gates and Overview, Add dose, History, and
Settings screens wired to the encrypted vault and the PK model, including
route-specific dose extras and in-place dose editing. It is an early milestone:
Labs, Plan, Supplies, Forecast, packaging, and full upstream UI parity are still
ahead.

- Design: [`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`](docs/superpowers/specs/2026-10-29-linux-desktop-design.md)
- PK comparison: [`tools/compare/README.md`](tools/compare/README.md)
- Upstream project: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- License: MIT (see [`LICENSE`](LICENSE))

## Build

Requirements: CMake 3.21+, Ninja (or another CMake generator), a C99 compiler,
a C++20 compiler, libpq 14 or newer, and libsodium 1.0.18 or newer. PostgreSQL
server 14+ is needed for the database and for the integration tests. For the
application-managed cluster, `initdb`, `pg_ctl`, and `postgres` must be on
`PATH` or in a directory passed with `--pg-bin-dir`/`PG_BINDIR`. The Qt shell
additionally needs Qt 6.4+ with the Core, Qml, Quick, and QuickControls2
modules; it is skipped automatically when those are missing and can be disabled
explicitly with `-DOYAMA_BUILD_QT_SHELL=OFF`. There is no network, Node, Rust,
Qt WebEngine, or Electron dependency. Qt is used only for the local desktop UI.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The PK test binary is `build/pk_smoke_test`; the comparison dumper is
`build/oyama_pk_dump` (see `tools/compare/README.md`). Storage tests include
`storage_test`, `vault_store_test`, `vault_codec_test`, `vault_repository_test`,
`legacy_migration_test`, `legacy_promotion_test`, `backup_crypto_test`, and
`backup_test`; the integration CTests start throwaway PostgreSQL clusters when
`initdb`/`pg_ctl` are available, or skip. Set `PG_BINDIR=/path/to/postgres/bin`
if the server tools are not on `PATH`.

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

After the cluster exists, create the encrypted vault and its paper recovery
phrase:

```sh
build/oyama-hrt-cli vault setup
```

The normal interactive setup generates seven EFF Diceware words (about 90 bits
of entropy), prints them once, and asks you to write them down and re-enter
three positions. Write the phrase on paper, keep it offline, and store it
somewhere you trust, such as a safe. There is no password reset, recovery key,
or support override: losing the phrase makes the vault permanently
unreadable. Do not paste it into a clipboard, a chat window, a shell argument,
or a file on the same machine.

`vault setup --phrase-stdin` accepts an externally supplied phrase for
provisioning and tests; normal users should use the interactive generator. The
CLI reads data-command phrases from one stdin line with the global
`--phrase-stdin` option. It never accepts the phrase as a command-line
argument.

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
build/oyama-hrt-cli vault status
build/oyama-hrt-cli vault setup
build/oyama-hrt-cli vault unlock-check --phrase-stdin
build/oyama-hrt-cli vault change-phrase --phrase-stdin --new-phrase-stdin
build/oyama-hrt-cli db migrate-vault --yes --phrase-stdin
build/oyama-hrt-cli settings set --weight 70 --birth-year 1995 --phrase-stdin
build/oyama-hrt-cli doses add --phrase-stdin \
  --mode transfem --route oral --ester E2 --dose-mg 2 --time-h 480000
build/oyama-hrt-cli backup export ~/oyama-backup.json --phrase-stdin
build/oyama-hrt-cli backup preview ~/oyama-backup.json --phrase-stdin
build/oyama-hrt-cli backup import ~/oyama-backup.json --phrase-stdin
```

Every command that needs the vault reads one line from stdin when given
`--phrase-stdin`; the phrase is never accepted as an argv value. For interactive
use, read it without echo and pipe it:

```sh
read -r -s -p 'Vault phrase: ' VAULT_PHRASE; echo
printf '%s\n' "$VAULT_PHRASE" | build/oyama-hrt-cli --phrase-stdin settings show
unset VAULT_PHRASE
```

`db init` creates the `oyama_hrt` database when missing and applies all pending
migrations in one transaction. Migrations live in `storage/migrations/` and are
forward-only; the runner records a checksum for each applied file.

### Migrating an old plaintext cluster

For an application-managed cluster created by an older build, `db migrate-vault`
imports the old rows into a new vault and verifies the result:

```sh
build/oyama-hrt-cli db migrate-vault --yes --phrase-stdin
```

The command creates a sibling cluster at `<data_dir>-migrate`, applies the vault
schema, imports every legacy row in one transaction, compares canonical
SHA-256 digests of the source and target, and only then promotes the new cluster
into the original path. The old plaintext directory is renamed to
`<data_dir>-plaintext-<UTC timestamp>` and its path is printed. That directory is
still plaintext. Deleting it is not guaranteed erasure, especially on SSDs,
copy-on-write filesystems, snapshots, or backups; `shred` is best-effort only.
Full-disk encryption (for example LUKS2) is the reliable protection for the old
cluster, WAL, free pages, and any copies.

`--purge-old <exact path>` deletes that exact plaintext directory after a
confirmed migration and requires `--yes`. The app performs the same migration in
a background worker when the configured backend is managed. For an existing
(non-managed) server, the app refuses to migrate in place: export with the old
binary to JSON, then import that file with
`backup import old.json --plaintext --phrase-stdin`.

### Backup import/export

Backups are encrypted by default. `backup export FILE` writes an `OYAMAEBK` v1
envelope using Argon2id and AES-256-GCM (or XChaCha20-Poly1305 when AES-GCM is
unavailable), with a fresh salt and nonce and mode 0600. The vault phrase is used
unless `--backup-phrase-stdin` supplies a separate backup phrase. `backup
preview` and `backup import` auto-detect encrypted files; a missing or wrong
phrase, or any tampering, is rejected before the database is written.
`backup import --dry-run` previews without applying.

`backup export --plaintext FILE` writes the upstream
`oyama-hrt-complete-backup` JSON v1 format for interoperability. That file
contains health data in clear text; the CLI prints a warning and writes mode
0600. Use an encrypted volume or a pipe instead of leaving it on an ordinary
disk. Plaintext import requires the explicit `--plaintext` flag.

Merge behavior follows upstream:

- `updatedAt` last-write-wins for events, labs, and dose templates;
- tombstones have a 5-minute future tolerance, a 180-day TTL, and a 5000-per-kind
  cap;
- quick doses, supplies, and plan items are unstamped: if the same ID exists
  locally, that whole category is blocked rather than overwriting silently;
- each category is validated independently; blocked categories leave local data
  untouched while other categories still merge;
- import runs in one transaction, so a failure leaves the database unchanged.

The plaintext JSON includes the optional `birthYear` extension; upstream ignores
it, so a round-trip through the upstream app loses that field (the CLI preview
reports it). Our schema stores UUID record IDs, so non-UUID IDs from another
client are rejected rather than rewritten. Lab records without an assay are
imported as `immuno` because the local schema requires an assay value.

The storage library exposes `LocalCluster` and the `VaultStore`; the Qt shell
calls `ensure_running()` during application startup, applies migrations, shows
the setup or unlock gate, and only then loads settings and doses through the
vault-backed repositories. The app locks the vault after 15 minutes of
inactivity and on quit, zeroing the in-memory DEK. It leaves a managed cluster
running on exit; use `oyama-hrt-cli db stop` to stop it.

## Security and threat model

The vault protects data at rest: copied database files, WAL, backups, and a
powered-off disk contain ciphertext, not health records. The phrase is never
written by the application, logged, placed in argv, or offered through a
clipboard.

It does not protect against a malicious process or user that can act as your
local account while the vault is unlocked. Such an attacker can read process
memory, speak to the same PostgreSQL socket, or observe input. A cold-boot or
memory-freezing attacker can also recover keys or plaintext from a running
machine. Use full-disk encryption, a locked screen, a trusted operating system,
and suspend/hibernate settings you understand. The application cannot promise
that C++/Qt, libpq, PostgreSQL, the kernel page cache, swap, or a core dump
never held a copy.

There is no recovery phrase escrow and no back door. Losing the phrase means
losing the data. The old plaintext cluster, if any, remains a separate exposure
until it is removed or protected by full-disk encryption; deleting files is not
guaranteed erasure.

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
迁移调度、托管本地 cluster 生命周期、应用层加密 vault、旧明文数据迁移、
加密备份、首次设置向导、备份导入/导出与 LWW/删除合并、基础 CLI 也已经
落地。首次启动会生成 7 个词的恢复短语，只显示一次，需要手抄保存；没有
找回机制，丢失短语即无法恢复数据。备份默认加密，`--plaintext` 才导出
上游兼容的明文 JSON。Qt 6/QML 桌面壳已经进入 M2：Vault 设置/解锁/锁定
界面以及 Overview、Add dose、History、Settings 四个页面已接入加密 vault
和 PK 模型，并支持按给药途径的 extras 与剂量就地编辑；Plan、Labs、
Supplies、Forecast、桌面打包以及完整界面对齐仍未完成。

## Repository layout

```text
core/
  include/oyama/       stable C99 PK API
  src/                 PK implementation
  tests/               C99 behavioural tests
vault/
  include/oyama/       C99 encrypted-vault ABI
  src/                 libpq/libsodium vault implementation
  tests/               crypto and v1 vault tests
storage/
  include/oyama/       C++20 storage API (vault, connection, migrations, repositories)
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
