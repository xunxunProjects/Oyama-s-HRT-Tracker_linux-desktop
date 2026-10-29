#!/usr/bin/env bash
set -euo pipefail

# End-to-end CLI test against a throwaway socket-only PostgreSQL cluster.

cli="${1:?usage: run_cli_integration.sh /path/to/oyama-hrt-cli}"
migrations="${2:?usage: run_cli_integration.sh CLI MIGRATIONS_DIR}"

if [[ -n "${PG_BINDIR:-}" ]]; then
    bindir="$PG_BINDIR"
elif command -v pg_config >/dev/null 2>&1; then
    bindir="$(pg_config --bindir)"
else
    bindir=""
fi

for tool in initdb pg_ctl; do
    if [[ -z "$bindir" || ! -x "$bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server binaries not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-cli-test.XXXXXX")"
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

port=$((56000 + RANDOM % 1000))
for _ in $(seq 1 20); do
    if [[ ! -e "$socket_dir/.s.PGSQL.$port" ]]; then
        break
    fi
    port=$((56000 + RANDOM % 1000))
done

"$bindir/initdb" -D "$data" -A trust --no-locale -E UTF8 --no-sync \
    >"$workdir/initdb.log" 2>&1
"$bindir/pg_ctl" -D "$data" -w \
    -o "-k $socket_dir -p $port -c listen_addresses= -c fsync=off -c synchronous_commit=off" \
    -l "$workdir/postgres.log" start >/dev/null

run_cli() {
    "$cli" --socket-dir "$socket_dir" --port "$port" --user "$(id -un)" \
        --dbname oyama_hrt_cli_test --migrations "$migrations" "$@"
}

init_output="$(run_cli db init)"
grep -q 'created database oyama_hrt_cli_test' <<<"$init_output"
grep -q 'schema version 1' <<<"$init_output"
grep -q 'applied 1 migration(s)' <<<"$init_output"

ping_output="$(run_cli db ping)"
grep -q $'database\toyama_hrt_cli_test' <<<"$ping_output"

status_output="$(run_cli db status)"
grep -q $'1\tapplied\tok\tinitial_schema' <<<"$status_output"

settings_output="$(run_cli settings set --weight 70 --birth-year 1990 \
    --calibration mipd --history-mode retrospective)"
grep -q $'body_weight_kg\t70' <<<"$settings_output"
grep -q $'birth_year\t1990' <<<"$settings_output"
grep -q $'calibration_method\tmipd' <<<"$settings_output"

dose_output="$(run_cli doses add --mode transfem --route oral --ester E2 \
    --dose-mg 2 --time-h 480000)"
grep -q '^added ' <<<"$dose_output"
dose_id="${dose_output#added }"

list_output="$(run_cli doses list --mode transfem)"
grep -q "$dose_id" <<<"$list_output"
grep -q $'oral\tE2\t480000' <<<"$list_output"

run_cli doses remove --id "$dose_id" >/dev/null
list_output="$(run_cli doses list --mode transfem)"
if grep -q "$dose_id" <<<"$list_output"; then
    echo "dose was not removed" >&2
    exit 1
fi

echo "cli_integration: ok"
