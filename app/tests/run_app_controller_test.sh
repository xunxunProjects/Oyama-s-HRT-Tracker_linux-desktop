#!/usr/bin/env bash
set -euo pipefail

binary="${1:?usage: run_app_controller_test.sh <app_controller_test>}"
pg_bindir="${PG_BINDIR:-$(pg_config --bindir 2>/dev/null || true)}"

for tool in initdb pg_ctl createdb psql; do
    if [[ -z "$pg_bindir" || ! -x "$pg_bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server tools not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

root="$(mktemp -d "${TMPDIR:-/tmp}/oyama-app-test.XXXXXX")"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
migrations="$repo_root/storage/migrations"
seed="$repo_root/storage/tests/legacy_seed.sql"
user="$(id -un)"

data="$root/data"
socket_dir="$root/socket"
log="$root/postgres.log"
config="$root/config"
port=$((56000 + RANDOM % 900))
db="oyama_app_controller_test"

cleanup() {
    for cluster in "$root"/*/; do
        if [[ -f "${cluster}postmaster.pid" ]]; then
            "$pg_bindir/pg_ctl" -D "${cluster%/}" -m immediate stop \
                >/dev/null 2>&1 || true
        fi
    done
    rm -rf "$root"
}
trap cleanup EXIT

start_cluster() {
    local cluster="$1"
    local socket="$2"
    local logfile="$3"
    local cluster_port="$4"

    mkdir -p "$socket"
    chmod 700 "$socket"
    "$pg_bindir/initdb" -D "$cluster" --auth-local=trust --auth-host=reject \
        --no-locale --encoding=UTF8 --no-sync >/dev/null
    "$pg_bindir/pg_ctl" -D "$cluster" -l "$logfile" -w -t 30 \
        -o "-k $socket -p $cluster_port -c listen_addresses=''" \
        start >/dev/null
}

# Stops a test cluster as soon as its scenario is done.  OpenBSD's default
# SEMMNS/SEMMNI limits are small, and three 100-connection PostgreSQL clusters
# running at once can exhaust the semaphore pool.
stop_cluster() {
    local cluster="$1"
    if [[ -f "$cluster/postmaster.pid" ]]; then
        "$pg_bindir/pg_ctl" -D "$cluster" -m fast -w stop \
            >/dev/null 2>&1 || true
    fi
}

# Creates a managed legacy cluster with the 0001/0002 schema and the shared
# seed, then stops it so the app must start it.  An extra invalid row is added
# when requested so the strict codec rejects the import.
make_managed_cluster() {
    local name="$1"
    local cluster_port="$2"
    local extra_invalid="$3"
    local cluster="$root/$name-data"
    local socket="$root/$name-socket"
    local logfile="$root/$name-postgres.log"

    start_cluster "$cluster" "$socket" "$logfile" "$cluster_port"
    "$pg_bindir/createdb" -h "$socket" -p "$cluster_port" -U "$user" "$name"
    "$pg_bindir/psql" -q -v ON_ERROR_STOP=1 -h "$socket" \
        -p "$cluster_port" -U "$user" -d "$name" \
        -f "$migrations/0001_initial_schema.sql" \
        -f "$migrations/0002_settings_timestamps.sql" \
        -f "$seed" >/dev/null
    if [[ "$extra_invalid" == "1" ]]; then
        "$pg_bindir/psql" -q -v ON_ERROR_STOP=1 -h "$socket" \
            -p "$cluster_port" -U "$user" -d "$name" >/dev/null <<'SQL'
INSERT INTO dose_events
    (id, mode, route, ester, time_h, dose_mg, extras, updated_at, created_at)
VALUES
    ('00000000-0000-0000-0000-0000000000aa', 'transfem', 'badroute', 'E2',
     1.0, 2.0, '{}'::jsonb, NULL, 0);
SQL
    fi
    "$pg_bindir/pg_ctl" -D "$cluster" -m fast -w stop >/dev/null

    cat >"$root/$name.conf" <<EOF
version=1
backend=managed
socket_dir=$socket
data_dir=$cluster
server_bin_dir=$pg_bindir
port=$cluster_port
dbname=$name
user=$user
EOF
    chmod 600 "$root/$name.conf"
}

start_cluster "$data" "$socket_dir" "$log" "$port"
"$pg_bindir/createdb" -h "$socket_dir" -p "$port" -U "$user" "$db"

cat >"$config" <<EOF
version=1
backend=existing
socket_dir=$socket_dir
port=$port
dbname=$db
user=$user
EOF
chmod 600 "$config"

# Three managed legacy fixtures: an untouched one for the pending-state checks,
# one that migrates through the worker, and one that fails, is repaired, and is
# retried.
make_managed_cluster managed_pending "$((port + 1))" 0
make_managed_cluster managed_migrate "$((port + 2))" 0
make_managed_cluster managed_retry "$((port + 3))" 1

# Simulate a failed prior attempt that left a target directory behind.
mkdir "$root/managed_pending-data-migrate"

export PG_BINDIR="$pg_bindir"
export OYAMA_CONFIG_FILE="$config"
export OYAMA_MIGRATIONS_DIR="$migrations"

"$binary"
stop_cluster "$data"

OYAMA_APP_CONTROLLER_SCENARIO=managed-legacy \
    OYAMA_CONFIG_FILE="$root/managed_pending.conf" "$binary"
stop_cluster "$root/managed_pending-data"

OYAMA_APP_CONTROLLER_SCENARIO=managed-legacy-migrate \
    OYAMA_CONFIG_FILE="$root/managed_migrate.conf" "$binary"

# Simulate a crash after the old directory became a plaintext sibling but
# before the verified target was renamed into place.
stop_cluster "$root/managed_migrate-data"
mv "$root/managed_migrate-data" "$root/managed_crash-data-migrate"
mkdir "$root/managed_crash-data-plaintext-20260101T000000Z"
mkdir -p "$root/managed_crash-socket" "$root/managed_crash-socket-migrate"
chmod 700 "$root/managed_crash-socket" "$root/managed_crash-socket-migrate"

cat >"$root/managed_crash.conf" <<EOF
version=1
backend=managed
socket_dir=$root/managed_crash-socket
data_dir=$root/managed_crash-data
server_bin_dir=$pg_bindir
port=$((port + 4))
dbname=managed_migrate
user=$user
EOF
chmod 600 "$root/managed_crash.conf"

OYAMA_APP_CONTROLLER_SCENARIO=managed-crash-recovery \
    OYAMA_CONFIG_FILE="$root/managed_crash.conf" "$binary"
stop_cluster "$root/managed_crash-data"

OYAMA_APP_CONTROLLER_SCENARIO=managed-legacy-retry \
    OYAMA_CONFIG_FILE="$root/managed_retry.conf" "$binary"
stop_cluster "$root/managed_retry-data"
