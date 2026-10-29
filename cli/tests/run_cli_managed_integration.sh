#!/usr/bin/env bash
set -euo pipefail

# End-to-end test of the application-managed PostgreSQL lifecycle and the
# first-run setup command.  Uses a temporary XDG tree and no external service.

cli="${1:?usage: run_cli_managed_integration.sh /path/to/oyama-hrt-cli}"
migrations="${2:?usage: run_cli_managed_integration.sh CLI MIGRATIONS_DIR}"

if [[ -n "${PG_BINDIR:-}" ]]; then
    bindir="$PG_BINDIR"
elif command -v pg_config >/dev/null 2>&1; then
    bindir="$(pg_config --bindir)"
else
    bindir=""
fi

for tool in initdb pg_ctl postgres; do
    if [[ -z "$bindir" || ! -x "$bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server binaries not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

root="$(mktemp -d "${TMPDIR:-/tmp}/oyama-managed-test.XXXXXX")"
export XDG_CONFIG_HOME="$root/config"
export XDG_DATA_HOME="$root/data"
export XDG_RUNTIME_DIR="$root/runtime"
export PG_BINDIR="$bindir"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$root" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"

dbname="oyama_hrt_managed_test"
port=$((57000 + RANDOM % 1000))
data_dir="$XDG_DATA_HOME/oyama-hrt/pgdata"
socket_dir="$XDG_RUNTIME_DIR/oyama-hrt"
config="$XDG_CONFIG_HOME/oyama-hrt/config"

cleanup() {
    if [[ -f "$data_dir/postmaster.pid" ]]; then
        "$bindir/pg_ctl" -D "$data_dir" -m immediate -w stop >/dev/null 2>&1 || true
    fi
    rm -rf "$root"
}
trap cleanup EXIT

run_cli() {
    "$cli" --migrations "$migrations" "$@"
}

help_output="$(run_cli db setup --help)"
grep -q 'db setup' <<<"$help_output"

setup_output="$(run_cli --port "$port" --dbname "$dbname" \
    --pg-bin-dir "$bindir" db setup --managed --yes)"
grep -q $'backend\tmanaged' <<<"$setup_output"
grep -q $'cluster\trunning' <<<"$setup_output"
grep -q $'schema_version\t2' <<<"$setup_output"
grep -q $'applied\t2' <<<"$setup_output"

ping_output="$(run_cli db ping)"
grep -q $'database\t'"$dbname" <<<"$ping_output"

status_output="$(run_cli db cluster-status)"
grep -q $'backend\tmanaged' <<<"$status_output"
grep -q $'initialized\tyes' <<<"$status_output"
grep -q $'running\tyes' <<<"$status_output"

settings_output="$(run_cli settings set --weight 70 --birth-year 1990)"
grep -q $'body_weight_kg\t70' <<<"$settings_output"
grep -q $'birth_year\t1990' <<<"$settings_output"

repo_mode="$(stat -c %a "$config" 2>/dev/null || stat -f %Lp "$config")"
if [[ "$repo_mode" != "600" ]]; then
    echo "config file mode is $repo_mode, expected 600" >&2
    exit 1
fi
data_mode="$(stat -c %a "$data_dir" 2>/dev/null || stat -f %Lp "$data_dir")"
if [[ "$data_mode" != "700" ]]; then
    echo "data directory mode is $data_mode, expected 700" >&2
    exit 1
fi
socket_mode="$(stat -c %a "$socket_dir" 2>/dev/null || stat -f %Lp "$socket_dir")"
if [[ "$socket_mode" != "700" ]]; then
    echo "socket directory mode is $socket_mode, expected 700" >&2
    exit 1
fi
socket_file="$socket_dir/.s.PGSQL.$port"
if [[ ! -S "$socket_file" ]]; then
    echo "PostgreSQL socket not found: $socket_file" >&2
    exit 1
fi
socket_file_mode="$(stat -c %a "$socket_file" 2>/dev/null || stat -f %Lp "$socket_file")"
if [[ "$socket_file_mode" != "700" ]]; then
    echo "socket file mode is $socket_file_mode, expected 700" >&2
    exit 1
fi

run_cli db stop >/dev/null
status_output="$(run_cli db cluster-status)"
grep -q $'running\tno' <<<"$status_output"
if run_cli db ping >/dev/null 2>&1; then
    echo "db ping unexpectedly succeeded while the cluster was stopped" >&2
    exit 1
fi

run_cli db start >/dev/null
ping_output="$(run_cli db ping)"
grep -q $'database\t'"$dbname" <<<"$ping_output"

init_output="$(run_cli db init)"
grep -q 'database '"$dbname"' already exists' <<<"$init_output"
grep -q 'applied 0 migration(s)' <<<"$init_output"

second_setup="$(run_cli db setup --managed --yes)"
grep -q 'using existing configuration' <<<"$second_setup"
grep -q $'schema_version\t2' <<<"$second_setup"
grep -q $'applied\t0' <<<"$second_setup"

existing_config="$root/existing-config"
existing_setup="$(OYAMA_CONFIG_FILE="$existing_config" run_cli --port "$port" \
    --dbname "$dbname" --socket-dir "$socket_dir" db setup --existing --yes)"
grep -q $'backend\texisting' <<<"$existing_setup"
grep -q $'cluster\texternal' <<<"$existing_setup"
OYAMA_CONFIG_FILE="$existing_config" run_cli db ping >/dev/null

if OYAMA_CONFIG_FILE="$existing_config" run_cli db start >/dev/null 2>&1; then
    echo "db start unexpectedly managed an existing cluster" >&2
    exit 1
fi

missing_tools="$root/missing-tools"
if PG_BINDIR= PATH="$PATH" run_cli --port "$port" --dbname "$dbname" \
    --pg-bin-dir "$missing_tools" --data-dir "$root/should-not-init" \
    db setup --managed --yes --force >/dev/null 2>"$root/missing.err"; then
    echo "db setup unexpectedly succeeded without server tools" >&2
    exit 1
fi
grep -q 'PostgreSQL server tools were not found' "$root/missing.err"

run_cli db stop >/dev/null

echo "cli_managed_integration: ok"
