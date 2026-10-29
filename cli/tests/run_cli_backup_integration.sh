#!/usr/bin/env bash
set -euo pipefail

# End-to-end backup CLI test with a managed, socket-only PostgreSQL cluster.

cli="${1:?usage: run_cli_backup_integration.sh CLI MIGRATIONS_DIR}"
migrations="${2:?usage: run_cli_backup_integration.sh CLI MIGRATIONS_DIR}"

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

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-backup-cli.XXXXXX")"
export XDG_CONFIG_HOME="$workdir/config"
export XDG_DATA_HOME="$workdir/data"
export XDG_RUNTIME_DIR="$workdir/runtime"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$workdir" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
export PG_BINDIR="$bindir"

port=$((58000 + RANDOM % 1000))
run_cli() {
    "$cli" --port "$port" --dbname oyama_hrt_backup_test \
        --migrations "$migrations" "$@"
}

cleanup() {
    run_cli db stop >/dev/null 2>&1 || true
    rm -rf "$workdir"
}
trap cleanup EXIT

run_cli db setup --managed --yes >/dev/null

bundle="$workdir/backup.json"
export_output="$(run_cli backup export "$bundle")"
grep -q "exported $bundle" <<<"$export_output"
grep -q "plaintext health data" <<<"$export_output"
test -f "$bundle"
grep -q '"format": "oyama-hrt-complete-backup"' "$bundle"
mode="$(stat -c %a "$bundle" 2>/dev/null || stat -f %Lp "$bundle")"
test "$mode" = "600"

preview_output="$(run_cli backup preview "$bundle")"
grep -q $'ok\tyes' <<<"$preview_output"
grep -q $'count\ttransfem\tevents\t0' <<<"$preview_output"

run_cli backup import "$bundle" --dry-run | grep -q $'ok\tyes'
run_cli backup import --dry-run "$bundle" | grep -q $'ok\tyes'

dose_output="$(run_cli doses add --mode transfem --route oral --ester E2 \
    --dose-mg 2 --time-h 480000)"
grep -q '^added ' <<<"$dose_output"
dose_id="${dose_output#added }"

import_output="$(run_cli backup import "$bundle")"
grep -q $'applied\tyes' <<<"$import_output"
run_cli doses list --mode transfem | grep -q "$dose_id"

printf '{"format":"wrong"}\n' >"$workdir/bad.json"
if run_cli backup preview "$workdir/bad.json" >/dev/null 2>&1; then
    echo "invalid backup was accepted" >&2
    exit 1
fi

run_cli db stop >/dev/null
echo "cli_backup_integration: ok"
