#!/usr/bin/env bash
set -euo pipefail

binary="${1:?usage: run_app_controller_test.sh <app_controller_test>}"
pg_bindir="${PG_BINDIR:-$(pg_config --bindir 2>/dev/null || true)}"

for tool in initdb pg_ctl createdb; do
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

cleanup() {
    if [[ -f "$data/postmaster.pid" ]]; then
        "$pg_bindir/pg_ctl" -D "$data" -m immediate stop >/dev/null 2>&1 || true
    fi
    rm -rf "$root"
}
trap cleanup EXIT

mkdir -p "$socket_dir"
chmod 700 "$socket_dir"

"$pg_bindir/initdb" -D "$data" --auth-local=trust --auth-host=reject \
    --no-locale --encoding=UTF8 --no-sync >/dev/null
"$pg_bindir/pg_ctl" -D "$data" -l "$log" -w -t 30 \
    -o "-k $socket_dir -p $port -c listen_addresses=''" start >/dev/null
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

export OYAMA_CONFIG_FILE="$config"
export OYAMA_MIGRATIONS_DIR="$(cd "$(dirname "$0")/../.." && pwd)/storage/migrations"

"$binary"
