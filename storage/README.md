# Storage layer

C++20 + libpq. This layer owns the local PostgreSQL schema, migrations, and
typed access to application data. It has no Qt, network, or UI dependency.

## Connection rules

- PostgreSQL is reached through a Unix-domain socket only.
- No TCP host is accepted, even if `PGHOST` contains one.
- No password is stored or prompted for; the server must use peer/ident auth or
  a local trust configuration.
- One `Connection` belongs to one thread. Do not share it between threads.
- All writes run in a `Transaction`; `Database` does not enable autocommit
  tricks.

`ConnectionConfig::from_environment()` reads `OYAMA_PG_*` first and then the
matching libpq `PG*` variables. `ConnectionConfig::find_socket_dir()` searches
the usual local socket directories, including `$XDG_RUNTIME_DIR` and
`/run/postgresql`.

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

## Repositories

- `SettingsRepository` — the single global settings row (weight, birth year,
  PK params, calibration).
- `DoseEventRepository`, `LabResultRepository` — typed dose and lab rows.
- `JsonRecordRepository` — dose templates, quick doses, supplies, plan items;
  the table is selected by `JsonRecordKind`, never by caller text.
- `PreferencesRepository`, `TombstoneRepository` — UI preferences and import
  tombstones.

Repositories validate UUIDs and basic numeric/text constraints before sending
SQL. JSON columns are checked by the schema to be JSON objects where required.

## Backup import/export

`backup.h` exposes the upstream-compatible complete-backup path:

- `preview_backup(Connection&, path, now_ms)` parses and validates the bundle,
  computes the LWW/tombstone merge, and reports counts, blocked categories,
  issues, and statistics without writing.
- `import_backup(Connection&, path, now_ms)` applies the same merge in one
  transaction. Root-envelope errors throw; a blocked category is left untouched
  while other categories are still applied.
- `export_backup(Connection&, path, now_ms)` writes a complete v1 bundle with
  mode 0600 and includes the local `birthYear` extension.

The format intentionally follows upstream: stamped categories (`events`,
`labResults`, `doseTemplates`) use `updatedAt` LWW with deterministic
fingerprint tie-breaking; tombstones use 5-minute future tolerance, 180-day
TTL, and a 5000-per-kind cap. `quickDoses`, `supplies`, and `plan` have no
upstream timestamps, so an incoming id that already exists locally blocks that
category rather than overwriting silently. Upstream v1 has no `birthYear`; the
optional top-level extension is preserved here but lost through an upstream
round-trip. IDs are required to be UUIDs by this database, and a lab without an
`assay` is stored as `immuno`.

## Tests

`storage_test` runs unit checks. When `OYAMA_TEST_PG_SOCKET` and
`OYAMA_TEST_PG_DB` are set it also runs integration checks against that
database, creating and dropping a private schema.

`run_with_temp_postgres.sh` starts a throwaway cluster with `initdb` and
`pg_ctl`, creates `oyama_hrt_test`, and runs the same binary. It exits 77 when
server binaries are unavailable, which CTest reports as skipped. Set
`PG_BINDIR` if `initdb` is not on `PATH`.

`cli/tests/run_cli_managed_integration.sh` exercises the full managed-cluster
path: setup, config persistence, ping, status, stop/start, idempotent setup,
existing-cluster selection, and the missing-tools error.
`backup_test` runs backup/merge integration checks; `cli/tests/run_cli_backup_integration.sh`
exercises `backup export/preview/import` and the invalid-envelope path.

## Not implemented yet

- Calibration history storage and model integration.
- Qt/QML view models beyond the M2 shell's `AppController`.
