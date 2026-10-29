#!/usr/bin/env bash
set -euo pipefail

# Runs storage_test against a throwaway PostgreSQL cluster.  The cluster listens
# only on a private Unix socket; no TCP port is opened.  If server binaries are
# unavailable the test is reported as skipped (CTest SKIP_RETURN_CODE 77).

test_binary="${1:?usage: run_with_temp_postgres.sh /path/to/storage_test}"

if [[ -n "${OYAMA_TEST_PG_SOCKET:-}" && -n "${OYAMA_TEST_PG_DB:-}" ]]; then
    exec "$test_binary"
fi

if [[ -n "${PG_BINDIR:-}" ]]; then
    bindir="$PG_BINDIR"
elif command -v pg_config >/dev/null 2>&1; then
    bindir="$(pg_config --bindir)"
else
    bindir=""
fi

for tool in initdb pg_ctl createdb; do
    if [[ -z "$bindir" || ! -x "$bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server binaries not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-pg-test.XXXXXX")"
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

"$bindir/createdb" -h "$socket_dir" -p "$port" -U "$(id -un)" oyama_hrt_test

OYAMA_TEST_PG_SOCKET="$socket_dir" \
OYAMA_TEST_PG_DB="oyama_hrt_test" \
OYAMA_TEST_PG_USER="$(id -un)" \
OYAMA_TEST_PG_PORT="$port" \
    "$test_binary"
