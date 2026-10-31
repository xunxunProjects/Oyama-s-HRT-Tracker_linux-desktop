#!/usr/bin/env bash
set -euo pipefail

binary="${1:?usage: run_legacy_migration_worker_test.sh <legacy_migration_worker_test>}"
pg_bindir="${PG_BINDIR:-$(pg_config --bindir 2>/dev/null || true)}"

for tool in initdb pg_ctl createdb psql; do
    if [[ -z "$pg_bindir" || ! -x "$pg_bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server tools not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

root="$(mktemp -d "${TMPDIR:-/tmp}/oyama-worker-test.XXXXXX")"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
migrations="$repo_root/storage/migrations"
seed="$repo_root/storage/tests/legacy_seed.sql"
user="$(id -un)"
base_port=$((57000 + RANDOM % 500))

cleanup() {
    for cluster in "$root"/*/ "$root"/*-migrate/; do
        if [[ -f "${cluster}postmaster.pid" ]]; then
            "$pg_bindir/pg_ctl" -D "${cluster%/}" -m immediate stop \
                >/dev/null 2>&1 || true
        fi
    done
    rm -rf "$root"
}
trap cleanup EXIT

start_cluster() {
    local data="$1"
    local socket="$2"
    local log="$3"
    local port="$4"

    mkdir -p "$socket"
    chmod 700 "$socket"
    "$pg_bindir/initdb" -D "$data" --auth-local=trust --auth-host=reject \
        --no-locale --encoding=UTF8 --no-sync >/dev/null
    "$pg_bindir/pg_ctl" -D "$data" -l "$log" -w -t 30 \
        -o "-k $socket -p $port -c listen_addresses=''" start >/dev/null
}

# Creates a stopped managed legacy cluster with migrations 0001/0002 and the
# shared seed.  extra_invalid=1 adds a row that the strict route codec rejects.
make_legacy_cluster() {
    local name="$1"
    local port="$2"
    local extra_invalid="$3"
    local data="$root/$name-data"
    local socket="$root/$name-socket"
    local log="$root/$name-postgres.log"

    start_cluster "$data" "$socket" "$log" "$port"
    "$pg_bindir/createdb" -h "$socket" -p "$port" -U "$user" "$name"
    "$pg_bindir/psql" -q -v ON_ERROR_STOP=1 -h "$socket" -p "$port" \
        -U "$user" -d "$name" \
        -f "$migrations/0001_initial_schema.sql" \
        -f "$migrations/0002_settings_timestamps.sql" \
        -f "$seed" >/dev/null
    if [[ "$extra_invalid" == "1" ]]; then
        "$pg_bindir/psql" -q -v ON_ERROR_STOP=1 -h "$socket" -p "$port" \
            -U "$user" -d "$name" >/dev/null <<'SQL'
INSERT INTO dose_events
    (id, mode, route, ester, time_h, dose_mg, extras, updated_at, created_at)
VALUES
    ('00000000-0000-0000-0000-0000000000aa', 'transfem', 'badroute', 'E2',
     1.0, 2.0, '{}'::jsonb, NULL, 0);
SQL
    fi
    "$pg_bindir/pg_ctl" -D "$data" -m fast -w stop >/dev/null

    cat >"$root/$name.conf" <<EOF
version=1
backend=managed
socket_dir=$socket
data_dir=$data
server_bin_dir=$pg_bindir
port=$port
dbname=$name
user=$user
EOF
    chmod 600 "$root/$name.conf"
}

make_legacy_cluster migration_success "$base_port" 0
make_legacy_cluster migration_failure "$((base_port + 1))" 1

export OYAMA_TEST_MIGRATION_SUCCESS_CONFIG="$root/migration_success.conf"
export OYAMA_TEST_MIGRATION_FAILURE_CONFIG="$root/migration_failure.conf"

OYAMA_WORKER_SCENARIO=success "$binary"
OYAMA_WORKER_SCENARIO=failure "$binary"
