#!/usr/bin/env bash
set -euo pipefail

# Runs legacy_migration_test against one throwaway PostgreSQL cluster with
# three databases: the legacy schema plus seed data, a vault-only database, and
# a fresh database with all migrations and only the default settings row.  The
# cluster listens only on a private Unix socket; no TCP port is opened.  If
# server binaries are unavailable the test is reported as skipped.

test_binary="${1:?usage: run_legacy_migration_test.sh /path/to/legacy_migration_test}"

if [[ -n "${OYAMA_TEST_LEGACY_PG_SOCKET:-}" &&
      -n "${OYAMA_TEST_LEGACY_PG_DB:-}" &&
      -n "${OYAMA_TEST_VAULT_PG_SOCKET:-}" &&
      -n "${OYAMA_TEST_VAULT_PG_DB:-}" &&
      -n "${OYAMA_TEST_FRESH_PG_SOCKET:-}" &&
      -n "${OYAMA_TEST_FRESH_PG_DB:-}" ]]; then
    exec "$test_binary"
fi

if [[ -n "${PG_BINDIR:-}" ]]; then
    bindir="$PG_BINDIR"
elif command -v pg_config >/dev/null 2>&1; then
    bindir="$(pg_config --bindir)"
else
    bindir=""
fi

for tool in initdb pg_ctl createdb psql; do
    if [[ -z "$bindir" || ! -x "$bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server binaries not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
migrations="$repo_root/storage/migrations"
seed="$repo_root/storage/tests/legacy_seed.sql"

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-legacy-pg-test.XXXXXX")"
data="$workdir/data"
socket_dir="$workdir/socket"
mkdir -p "$socket_dir"
chmod 700 "$workdir" "$socket_dir"

cleanup() {
    if [[ -f "$data/postmaster.pid" ]]; then
        "$bindir/pg_ctl" -D "$data" -m immediate -w stop >/dev/null 2>&1 || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

port=$((55000 + RANDOM % 1000))
for _ in $(seq 1 20); do
    if [[ ! -e "$socket_dir/.s.PGSQL.$port" ]]; then
        break
    fi
    port=$((55000 + RANDOM % 1000))
done

"$bindir/initdb" -D "$data" -A trust --no-locale -E UTF8 --no-sync \
    >"$workdir/initdb.log" 2>&1
"$bindir/pg_ctl" -D "$data" -w \
    -o "-k $socket_dir -p $port -c listen_addresses= -c fsync=off -c synchronous_commit=off" \
    -l "$workdir/postgres.log" start >/dev/null

psql_db() {
    local db="$1"
    shift
    "$bindir/psql" -h "$socket_dir" -p "$port" -U "$(id -un)" -d "$db" \
        -v ON_ERROR_STOP=1 "$@" >/dev/null
}

for db in oyama_legacy oyama_vault oyama_fresh; do
    "$bindir/createdb" -h "$socket_dir" -p "$port" -U "$(id -un)" "$db"
done

psql_db oyama_legacy -f "$migrations/0001_initial_schema.sql"
psql_db oyama_legacy -f "$migrations/0002_settings_timestamps.sql"
psql_db oyama_legacy -f "$seed"

psql_db oyama_vault -f "$migrations/0003_vault_schema.sql"

psql_db oyama_fresh -f "$migrations/0001_initial_schema.sql"
psql_db oyama_fresh -f "$migrations/0002_settings_timestamps.sql"
psql_db oyama_fresh -f "$migrations/0003_vault_schema.sql"

export OYAMA_TEST_LEGACY_PG_SOCKET="$socket_dir"
export OYAMA_TEST_LEGACY_PG_DB="oyama_legacy"
export OYAMA_TEST_LEGACY_PG_USER="$(id -un)"
export OYAMA_TEST_LEGACY_PG_PORT="$port"

export OYAMA_TEST_VAULT_PG_SOCKET="$socket_dir"
export OYAMA_TEST_VAULT_PG_DB="oyama_vault"
export OYAMA_TEST_VAULT_PG_USER="$(id -un)"
export OYAMA_TEST_VAULT_PG_PORT="$port"

export OYAMA_TEST_FRESH_PG_SOCKET="$socket_dir"
export OYAMA_TEST_FRESH_PG_DB="oyama_fresh"
export OYAMA_TEST_FRESH_PG_USER="$(id -un)"
export OYAMA_TEST_FRESH_PG_PORT="$port"

"$test_binary"
