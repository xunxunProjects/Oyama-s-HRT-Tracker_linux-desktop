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

## Not implemented yet

- Application-managed `initdb`/`pg_ctl` lifecycle and the first-start wizard.
- Backup import/export and last-write-wins merge.
- Calibration history storage and model integration.
- Qt/QML view models; those belong in `app/` and are intentionally deferred.
