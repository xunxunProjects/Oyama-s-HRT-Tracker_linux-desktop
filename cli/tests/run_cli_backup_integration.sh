#!/usr/bin/env bash
set -euo pipefail

# The caller may have exported a config from manual testing; the test must use
# its own throwaway cluster.
unset OYAMA_CONFIG_FILE

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
phrase="correct horse battery staple"
printf '%s\n' "$phrase" | run_cli vault setup --phrase-stdin \
    --kdf-ops 1 --kdf-mem 8388608 >/dev/null

run_unlocked() {
    printf '%s\n' "$phrase" | run_cli --phrase-stdin "$@"
}

bundle="$workdir/backup.oyamaebk"
export_output="$(run_unlocked backup export "$bundle")"
grep -q "exported $bundle" <<<"$export_output"
if grep -q "plaintext health data" <<<"$export_output"; then
    echo "encrypted export printed the plaintext warning" >&2
    exit 1
fi
test -f "$bundle"
test "$(dd if="$bundle" bs=1 count=8 2>/dev/null)" = "OYAMAEBK"
mode="$(stat -c %a "$bundle" 2>/dev/null || stat -f %Lp "$bundle")"
test "$mode" = "600"

preview_output="$(run_unlocked backup preview "$bundle")"
grep -q $'ok\tyes' <<<"$preview_output"
grep -q $'count\ttransfem\tevents\t0' <<<"$preview_output"

run_unlocked backup import "$bundle" --dry-run | grep -q $'ok\tyes'
run_unlocked backup import --dry-run "$bundle" | grep -q $'ok\tyes'

dose_output="$(run_unlocked doses add --mode transfem --route oral --ester E2 \
    --dose-mg 2 --time-h 480000)"
grep -q '^added ' <<<"$dose_output"
dose_id="${dose_output#added }"

import_output="$(run_unlocked backup import "$bundle")"
grep -q $'applied\tyes' <<<"$import_output"
run_unlocked doses list --mode transfem | grep -q "$dose_id"

plain="$workdir/backup.json"
plain_output="$(run_unlocked backup export "$plain" --plaintext)"
grep -q "plaintext health data" <<<"$plain_output"
grep -q '"format":"oyama-hrt-complete-backup"' "$plain"
plain_mode="$(stat -c %a "$plain" 2>/dev/null || stat -f %Lp "$plain")"
test "$plain_mode" = "600"

if run_unlocked backup import "$plain" >"$workdir/plain.out" \
    2>"$workdir/plain.err"; then
    echo "plaintext backup was imported without --plaintext" >&2
    exit 1
fi
grep -q -- '--plaintext' "$workdir/plain.err"
run_unlocked backup import "$plain" --plaintext | grep -q $'applied\tyes'

printf '{"format":"wrong"}\n' >"$workdir/bad.json"
if run_unlocked backup preview --plaintext "$workdir/bad.json" >/dev/null 2>&1; then
    echo "invalid backup was accepted" >&2
    exit 1
fi

run_cli db stop >/dev/null
echo "cli_backup_integration: ok"
