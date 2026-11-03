# Oyama's HRT Tracker — Linux / BSD Desktop

A native Qt 6 desktop application for Linux and BSD. Local-only: no account,
no cloud, no telemetry. Health records live in a local PostgreSQL database and
are encrypted at the application layer before they reach PostgreSQL. Data moves
in and out through the upstream `oyama-hrt-complete-backup` format, encrypted by
default.

**Current version: 0.6.0** (released 2026-11-03). Download it from the
[0.6.0 release](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/tag/v0.6.0).

## Support

> [!IMPORTANT]
> **Support is self-service.**
> **Do not contact the developer for support. Read the man pages, the
> documentation in `docs/`, and the GitHub wiki first; only open a GitHub issue
> if those do not answer your question.**
>
> Start with `man oyama-hrt-cli`, `man oyama-hrt-app`, [`docs/README.md`](docs/README.md),
> [`docs/SUPPORT.md`](docs/SUPPORT.md), and the
> [GitHub wiki](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki).
> Security vulnerabilities go through [`SECURITY.md`](SECURITY.md), never a
> public issue.

**Status: model core, encrypted storage, legacy migration, encrypted backups,
security profiles, and a vault-gated Qt/QML shell implemented (M9).** The C99
pharmacokinetic core is in `core/`, with a strict C test and a comparison
harness that checks it against the upstream TypeScript model to machine
precision. `storage/` contains the PostgreSQL schema, forward-only migration
runner, the encrypted vault and its C ABI, typed repositories, the
application-managed cluster lifecycle, and the LWW/tombstone backup merge;
`cli/` exposes setup, vault, migration, data, and backup commands. `app/` has a
Qt 6/QML shell with setup/unlock/lock gates and Overview, Add dose, History, and
Settings screens wired to the encrypted vault and the PK model, including
route-specific dose extras and in-place dose editing. Version 0.6.0 also ships
`off`/`standard`/`full` security profiles, 24-word BIP39 recovery phrases,
memory hygiene and panic lock, a best-effort capture guard, and a reference Xen
key-isolation deployment. Labs, Plan, Supplies, Forecast, and full upstream UI
parity are still ahead.

- Design: [`docs/superpowers/specs/2026-10-29-linux-desktop-design.md`](docs/superpowers/specs/2026-10-29-linux-desktop-design.md)
- PK comparison: [`tools/compare/README.md`](tools/compare/README.md)
- Upstream project: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker>
- License: MIT (see [`LICENSE`](LICENSE))
- Contributing: [`CONTRIBUTING.md`](CONTRIBUTING.md)
- AI contributors: [`AI-CONTRIBUTORS.md`](AI-CONTRIBUTORS.md)
- Changelog: [`CHANGELOG.md`](CHANGELOG.md)
- Donations: [`DONATIONS.md`](DONATIONS.md)
- Documentation index: [`docs/README.md`](docs/README.md)
- Support policy: [`docs/SUPPORT.md`](docs/SUPPORT.md)
- Security policy: [`SECURITY.md`](SECURITY.md)
- Wiki: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki>
- Release 0.6.0: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/tag/v0.6.0>
- Release verification: <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/download/v0.6.0/VERIFYING.md>

## Build

Requirements: CMake 3.21+, Ninja (or another CMake generator), a C99 compiler,
a C++ compiler (the default build requests C++20; the tree keeps a C++11
compatibility baseline and compile check), libpq 14 or newer, libsodium 1.0.18
or newer, and nlohmann/json 3.11 or newer. PostgreSQL
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

For the short path on Linux, FreeBSD, OpenBSD, or NetBSD, use the portable wrapper:

```sh
make            # configure + Release build in build/
make test
sudo make install
make package PACKAGE=dist/oyama-hrt-netbsd-amd64.tar.gz
```

The package names and platform-specific notes are in
[`docs/dependencies.md`](docs/dependencies.md). OpenBSD and NetBSD are native
build targets; their binaries are not interchangeable with the Linux or
FreeBSD builds.

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

## Build profiles

The build supports three security profiles selected with
`-DOYAMA_SECURITY_PROFILE=off|standard|full` (default `standard`):

- `off` — debug profile; disables the extra security modules but still uses the
  vault.
- `standard` — default for ordinary users.
- `full` — secure release; enables capture guard, panic lock, and key isolation,
  and is the only profile that ships `oyama-keyd`.

Qt 6.4+ is the default for every profile; the optional
`-DOYAMA_QT_MAJOR=5` selects the Qt 5.15.2 compatibility build. With the
Makefile wrapper, `make` builds `standard` and `make SECURITY=0` builds `off`.

See [`docs/security/profiles.md`](docs/security/profiles.md) for the feature
table, the compiler-hardening differences, and the
`oyama-hrt-cli version --verbose` report. The pinned dependency sources and the
offline bootstrap workflow are in
[`docs/dependencies.md`](docs/dependencies.md#vendored-build).

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
build/oyama-hrt-cli --socket-dir /run/postgresql db setup --existing --yes

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

The normal interactive setup generates a 24-word BIP39 mnemonic (256 bits of
entropy plus an 8-bit checksum), prints it once, and asks you to write it down
and re-enter three positions. Write the phrase on paper, keep it offline, and
store it somewhere you trust, such as a safe. There is no password reset,
recovery key, or support override: losing the phrase makes the vault
permanently unreadable. Do not paste it into a clipboard, a chat window, a
shell argument, or a file on the same machine. Existing vaults keep their
original phrase; `vault change-phrase` may upgrade a vault to a BIP39 phrase,
but no migration is forced.

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
build/oyama-hrt-cli --phrase-stdin settings set --weight 70 --birth-year 1995
build/oyama-hrt-cli --phrase-stdin doses add \
  --mode transfem --route oral --ester E2 --dose-mg 2 --time-h 480000
build/oyama-hrt-cli --phrase-stdin backup export ~/oyama-backup.oyamaebk
build/oyama-hrt-cli --phrase-stdin backup preview ~/oyama-backup.oyamaebk
build/oyama-hrt-cli --phrase-stdin backup import ~/oyama-backup.oyamaebk
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
`oyama-hrt-cli --phrase-stdin backup import old.json --plaintext`.

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

## Project status

Version 0.6.0 is a security-focused release. The encrypted local vault first
shipped in 0.5.0; 0.6.0 adds `off`/`standard`/`full` security profiles, pinned
third-party sources with checksums and an offline bootstrap path, 24-word BIP39
recovery phrases for new vaults, memory hygiene and a full-profile panic lock, a
best-effort full-profile capture guard, and a reference Xen key-isolation
deployment. The C99 pharmacokinetic core has been compared point-by-point with
the upstream TypeScript model across 36 scenarios and 413,323 fields, with a
maximum relative error of about 1.1e-15. The PostgreSQL storage layer, migration
runner, managed cluster lifecycle, application-level vault, legacy plaintext
migration, encrypted backups, first-run setup, LWW/tombstone merge, and the
CLI are implemented.

The first launch generates a 24-word BIP39 recovery phrase, shows it once, and
asks you to write it down. Existing vaults keep their original phrase. There is
no recovery service and no back door; if the phrase is lost, the data is lost.
Backups are encrypted by default; `--plaintext` exports the
upstream-compatible JSON only when explicitly requested.

The release publishes nine platform archives (Linux, FreeBSD, and OpenBSD
amd64, each in `off`, `standard`, and `full`) plus a source tarball. The Qt
6/QML shell implements the vault setup, unlock, and lock screens plus Overview,
Add dose, History, and Settings. Plan, Labs, Supplies, Forecast, and the
remaining upstream UI parity work are still ahead.

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
  include/oyama/       C++ storage API (vault, connection, migrations, repositories)
  src/                 libpq implementation
  migrations/          forward-only SQL schema migrations
  tests/               unit and PostgreSQL integration tests
cli/                   oyama-hrt-cli (database and data commands)
app/                   Qt 6/QML shell (M9; see app/README.md)
tools/compare/         upstream numerical comparison harness
docs/
  superpowers/specs/   design documents
  spec/                model and format contracts (added during M0)
```
