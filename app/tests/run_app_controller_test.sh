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
data="$root/data"
socket_dir="$root/socket"
log="$root/postgres.log"
config="$root/config"
port=$((56000 + RANDOM % 900))
user="$(id -un)"
db="oyama_app_controller_test"

managed_data="$root/managed-data"
managed_socket="$root/managed-socket"
managed_log="$root/managed-postgres.log"
managed_config="$root/managed-config"
managed_port=$((port + 1))
managed_db="oyama_app_managed_legacy"

cleanup() {
    for cluster in "$data" "$managed_data"; do
        if [[ -f "$cluster/postmaster.pid" ]]; then
            "$pg_bindir/pg_ctl" -D "$cluster" -m immediate stop \
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

# A managed cluster whose legacy tables contain data must be reported as
# unavailable; the app may not migrate it or create vault_meta.
start_cluster "$managed_data" "$managed_socket" "$managed_log" \
    "$managed_port"
"$pg_bindir/createdb" -h "$managed_socket" -p "$managed_port" -U "$user" \
    "$managed_db"
"$pg_bindir/psql" -q -v ON_ERROR_STOP=1 -h "$managed_socket" \
    -p "$managed_port" -U "$user" -d "$managed_db" <<'SQL'
CREATE TABLE dose_events (
    id uuid PRIMARY KEY,
    mode text NOT NULL,
    route text NOT NULL,
    ester text NOT NULL,
    time_h double precision NOT NULL,
    dose_mg double precision NOT NULL,
    extras jsonb NOT NULL DEFAULT '{}'::jsonb,
    updated_at bigint,
    created_at bigint NOT NULL
);
INSERT INTO dose_events
    (id, mode, route, ester, time_h, dose_mg, created_at)
VALUES
    ('00000000-0000-0000-0000-000000000001', 'transfem', 'oral', 'E2',
     1.0, 2.0, 0);
SQL

cat >"$managed_config" <<EOF
version=1
backend=managed
socket_dir=$managed_socket
data_dir=$managed_data
server_bin_dir=$pg_bindir
port=$managed_port
dbname=$managed_db
user=$user
EOF
chmod 600 "$managed_config"

export PG_BINDIR="$pg_bindir"
export OYAMA_CONFIG_FILE="$config"
export OYAMA_MIGRATIONS_DIR="$(cd "$(dirname "$0")/../.." && pwd)/storage/migrations"

"$binary"

OYAMA_APP_CONTROLLER_SCENARIO=managed-legacy \
    OYAMA_CONFIG_FILE="$managed_config" "$binary"
