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

## Not implemented yet

- Backup import/export and last-write-wins merge.
- Calibration history storage and model integration.
- Qt/QML view models; those belong in `app/` and are intentionally deferred.
