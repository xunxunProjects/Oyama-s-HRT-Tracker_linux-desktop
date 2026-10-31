# Storage layer

C++20 + libpq + libsodium. This layer owns the local PostgreSQL schema,
migrations, the encrypted vault, and typed access to application data. It has
no Qt, network, or UI dependency.

## Connection rules

- PostgreSQL is reached through a Unix-domain socket only.
- No TCP host is accepted, even if `PGHOST` contains one.
- No database password is stored or prompted for; the server must use
  peer/ident auth or a local trust configuration.
- One `Connection` belongs to one thread. Do not share it between threads.
- All writes run in a `Transaction`; `Database` does not enable autocommit
  tricks.

`ConnectionConfig::from_environment()` reads `OYAMA_PG_*` first and then the
matching libpq `PG*` variables. `ConnectionConfig::find_socket_dir()` searches
the usual local socket directories, including `$XDG_RUNTIME_DIR` and
`/run/postgresql`. `ConnectionConfig::to_conninfo()` produces an escaped libpq
connection string for the C vault boundary.

## Application-managed cluster

`LocalCluster` owns an optional PostgreSQL cluster for users who do not already
run a server. It uses only local Unix sockets and never enables TCP:

- `initdb -D <data> --auth-local=peer --auth-host=reject` under
  `$XDG_DATA_HOME/oyama-hrt/pgdata` (0700);
- socket directory `$XDG_RUNTIME_DIR/oyama-hrt` (fallback
  `$XDG_DATA_HOME/oyama-hrt/run`), socket permissions 0700;
- generated `oyama-runtime.conf` included by `postgresql.conf`, with
  `listen_addresses = ''`;
- `pg_ctl status/start/stop`, no shell execution;
- `ensure_running()` for application startup and `stop()` for clean exit.

`AppConfig` persists the selected backend in
`$XDG_CONFIG_HOME/oyama-hrt/config` (mode 0600), or in the path named by
`OYAMA_CONFIG_FILE`. `default_data_dir()` and `default_socket_dir()` expose the
XDG defaults. The CLI's `db setup`, `db start`, `db stop`, and
`db cluster-status` commands are thin wrappers around these APIs.

Server binaries are looked up in the configured `server_bin_dir`, then
`PG_BINDIR`, then absolute `PATH` entries. A missing server is reported with
distribution installation hints; nothing is installed automatically.

## Migrations

SQL files live in `migrations/` and are named `NNNN_name.sql`. The runner:

1. loads and sorts files, rejecting malformed names and duplicate versions;
2. takes `pg_advisory_xact_lock`;
3. creates `schema_migrations` if needed;
4. rejects an applied file whose checksum changed, or an applied version whose
   file disappeared;
5. applies every pending file in one transaction and records its checksum.

Migrations are forward-only. A destructive change needs an export/rebuild plan;
do not edit an already-applied file.

## Encrypted vault

Application data is encrypted before it reaches libpq. PostgreSQL sees only:

- `vault_meta` — a single row with the format version, Argon2id parameters and
  salt, AEAD algorithm, wrap nonce, and the wrapped Data Encryption Key (DEK);
- `vault_records(id uuid PRIMARY KEY, blob bytea)` — one authenticated,
  versioned ciphertext blob per logical record.

The DEK is random, generated at setup, wrapped by an Argon2id-derived KEK, and
exists only in application memory after unlock. `VaultStore::lock()` and
`oyama_vault_lock()` zero it. Key material is never sent to PostgreSQL, written
to WAL, logged, or placed in a backup.

The C99 ABI is `include/oyama/vault.h` (`oyama_vault_*`). It is the only code
that talks to libpq for vault records. C++ wrappers live in `vault_store.h`,
strict JSON codecs in `vault_codec.h`, and repositories in
`vault_*_repository.h`. The encrypted design is documented in
`docs/superpowers/specs/2026-10-31-encrypted-vault-design.md`.

PostgreSQL cannot filter or sort encrypted payloads. Repositories scan the
requested kind, decrypt in memory, filter by mode, and sort there.
`VaultDoseEventRepository::list(mode)` sorts by `(time_h, id)`. This is
intentionally simple for personal data volumes. Record IDs remain clear so a
record can be addressed; the UUID is bound into the AEAD associated data, so
swapping blobs between IDs fails authentication.

Migrations 0001 and 0002 are retained for legacy import and old clusters. The
current application does not read or write those plaintext tables after a vault
is created; `0003_vault_schema.sql` adds the vault tables.

## Repositories

All repositories take a `VaultStore&` and use the strict codecs. A malformed or
out-of-range payload throws `VaultCodecError`; records are never silently
dropped.

- `VaultSettingsRepository` — the global settings singleton.
- `VaultDoseEventRepository`, `VaultLabResultRepository` — typed dose and lab
  records, filtered and ordered in memory.
- `VaultJsonRecordRepository` — dose templates, quick doses, supplies, and plan
  items; the record kind is selected by `JsonRecordKind`, never by caller text.
- `VaultPreferencesRepository`, `VaultTombstoneRepository` — UI preferences and
  import tombstones.

## Backup import/export

`backup.h` exposes only the vault-backed path:

- `export_encrypted_backup(VaultStore&, path, now_ms, phrase)` writes the
  `OYAMAEBK` v1 envelope: Argon2id + AES-256-GCM (or XChaCha20-Poly1305 when
  AES-GCM is unavailable), fresh salt and nonce, and mode 0600. This is the CLI
  default.
- `preview_backup(VaultStore&, path, now_ms, phrase)` and
  `import_backup(VaultStore&, path, now_ms, phrase)` auto-detect encrypted
  files. The phrase is required for encrypted input, and decryption happens
  before any database write. Import runs in one vault transaction.
- `export_backup(VaultStore&, path, now_ms)` writes the explicit plaintext
  upstream-compatible v1 JSON. The CLI exposes this only behind `--plaintext`
  and warns that the file contains health data.

The JSON format intentionally follows upstream: stamped categories (`events`,
`labResults`, `doseTemplates`) use `updatedAt` LWW with deterministic
fingerprint tie-breaking; tombstones use 5-minute future tolerance, 180-day
TTL, and a 5000-per-kind cap. `quickDoses`, `supplies`, and `plan` have no
upstream timestamps, so an incoming id that already exists locally blocks that
category rather than overwriting silently. Upstream v1 has no `birthYear`; the
optional top-level extension is preserved here but lost through an upstream
round-trip. Lab records without an assay are stored as `immuno`.

## Tests

Unit tests cover the C vault crypto, codecs, passphrase handling, migrations, and
backup crypto. PostgreSQL integration tests cover setup/unlock, record CRUD,
repositories, legacy import and promotion, the app controller, and the CLI.

`run_with_temp_postgres.sh` and its wrappers start a throwaway cluster with
`initdb` and `pg_ctl`, create `oyama_hrt_test`, and run the test binary. They
exit 77 when server binaries are unavailable, which CTest reports as skipped.
Set `PG_BINDIR` if `initdb` is not on `PATH`.

## Not implemented yet

- Calibration history storage and model integration.
- Qt/QML view models beyond the vault-gated M2 shell.
