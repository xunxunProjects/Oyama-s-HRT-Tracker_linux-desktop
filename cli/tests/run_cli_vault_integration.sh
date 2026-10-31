#!/usr/bin/env bash
set -euo pipefail

# The caller may have exported a config from manual testing; the test must use
# its own throwaway cluster.
unset OYAMA_CONFIG_FILE

# End-to-end CLI test for vault setup/unlock, encrypted data commands, and
# encrypted/plaintext backup handling against a throwaway PostgreSQL cluster.

cli="${1:?usage: run_cli_vault_integration.sh CLI MIGRATIONS_DIR}"
migrations="${2:?usage: run_cli_vault_integration.sh CLI MIGRATIONS_DIR}"

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

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-cli-vault.XXXXXX")"
export XDG_CONFIG_HOME="$workdir/config"
export XDG_DATA_HOME="$workdir/data"
export XDG_RUNTIME_DIR="$workdir/runtime"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR" \
    "$workdir/socket"
chmod 700 "$workdir" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR" \
    "$workdir/socket"

data="$workdir/data/pg"
socket_dir="$workdir/socket"
port=$((54000 + RANDOM % 1000))
dbname="oyama_hrt_vault_test"
phrase="correct horse battery staple"
new_phrase="new correct horse battery staple"

cleanup() {
    if [[ -f "$data/postmaster.pid" ]]; then
        "$bindir/pg_ctl" -D "$data" -m immediate -w stop >/dev/null 2>&1 || true
    fi
    rm -rf "$workdir"
}
trap cleanup EXIT

"$bindir/initdb" -D "$data" -A trust --no-locale -E UTF8 --no-sync \
    >"$workdir/initdb.log" 2>&1
"$bindir/pg_ctl" -D "$data" -w \
    -o "-k $socket_dir -p $port -c listen_addresses= -c fsync=off -c synchronous_commit=off" \
    -l "$workdir/postgres.log" start >/dev/null

run_cli() {
    "$cli" --socket-dir "$socket_dir" --port "$port" --user "$(id -un)" \
        --dbname "$dbname" --migrations "$migrations" "$@"
}

run_unlocked() {
    printf '%s\n' "$phrase" | run_cli --phrase-stdin "$@"
}

fail() {
    echo "$1" >&2
    exit 1
}

setup_output="$(run_cli db setup --existing --yes)"
grep -q $'backend\texisting' <<<"$setup_output"
grep -q $'schema_version\t3' <<<"$setup_output"

init_output="$(run_cli db init)"
grep -q 'database '"$dbname"' already exists' <<<"$init_output"
grep -q 'schema version 3' <<<"$init_output"

status_output="$(run_cli vault status)"
grep -qx 'uninitialized' <<<"$status_output"

if run_cli doses list --mode transfem >"$workdir/uninitialized.out" \
    2>"$workdir/uninitialized.err"; then
    fail "data command succeeded before vault setup"
fi
grep -q 'vault setup' "$workdir/uninitialized.err"

printf '%s\n' "$phrase" | run_cli vault setup --phrase-stdin \
    --kdf-ops 1 --kdf-mem 8388608 >"$workdir/setup.out"
if grep -q "$phrase" "$workdir/setup.out"; then
    fail "vault setup echoed the supplied phrase"
fi

status_output="$(run_cli vault status)"
grep -qx 'locked' <<<"$status_output"

if run_cli doses list --mode transfem >"$workdir/locked.out" \
    2>"$workdir/locked.err"; then
    fail "data command succeeded while the vault was locked"
fi
grep -q -- '--phrase-stdin' "$workdir/locked.err"

if printf 'wrong\n' | run_cli --phrase-stdin vault unlock-check \
    >"$workdir/wrong-unlock.out" 2>"$workdir/wrong-unlock.err"; then
    fail "unlock-check accepted a wrong phrase"
fi

unlock_output="$(printf '%s\n' "$phrase" | \
    run_cli --phrase-stdin vault unlock-check)"
grep -qx 'unlocked' <<<"$unlock_output"

add_output="$(run_unlocked doses add --mode transfem --route oral --ester E2 \
    --dose-mg 2 --time-h 10)"
grep -q '^added ' <<<"$add_output"
dose_a="${add_output#added }"

run_unlocked doses list --mode transfem | grep -q "$dose_a"

if printf 'wrong\n' | run_cli --phrase-stdin doses list --mode transfem \
    >"$workdir/wrong-list.out" 2>"$workdir/wrong-list.err"; then
    fail "doses list accepted a wrong phrase"
fi

sealed="$workdir/backup.oyamaebk"
export_output="$(run_unlocked backup export "$sealed")"
grep -q "exported $sealed" <<<"$export_output"
if grep -q 'plaintext health data' <<<"$export_output"; then
    fail "default encrypted export printed the plaintext warning"
fi
test "$(dd if="$sealed" bs=1 count=8 2>/dev/null)" = "OYAMAEBK"
sealed_mode="$(stat -c %a "$sealed" 2>/dev/null || stat -f %Lp "$sealed")"
test "$sealed_mode" = "600"

preview_output="$(run_unlocked backup preview "$sealed")"
grep -q $'ok\tyes' <<<"$preview_output"
grep -q $'count\ttransfem\tevents\t1' <<<"$preview_output"

plain="$workdir/backup.json"
plain_output="$(run_unlocked backup export "$plain" --plaintext)"
grep -q "plaintext health data" <<<"$plain_output"
grep -q '"format":"oyama-hrt-complete-backup"' "$plain"
grep -q '"mode":"transfem"' "$plain"
plain_mode="$(stat -c %a "$plain" 2>/dev/null || stat -f %Lp "$plain")"
test "$plain_mode" = "600"

printf '%s\n%s\n' "$phrase" "$new_phrase" | \
    run_cli vault change-phrase --phrase-stdin --new-phrase-stdin \
    >"$workdir/change.out"

if printf '%s\n' "$phrase" | run_cli --phrase-stdin vault unlock-check \
    >"$workdir/old-phrase.out" 2>"$workdir/old-phrase.err"; then
    fail "old phrase still unlocked the vault after change-phrase"
fi

printf '%s\n' "$new_phrase" | run_cli --phrase-stdin vault unlock-check \
    | grep -qx 'unlocked'

add_b_output="$(printf '%s\n' "$new_phrase" | \
    run_cli --phrase-stdin doses add --mode transfem --route oral --ester E2 \
    --dose-mg 4 --time-h 20)"
dose_b="${add_b_output#added }"

if printf '%s\n%s\n' "$new_phrase" wrong | \
    run_cli --phrase-stdin --backup-phrase-stdin backup import "$sealed" \
    >"$workdir/wrong-backup.out" 2>"$workdir/wrong-backup.err"; then
    fail "backup import accepted a wrong backup phrase"
fi
grep -q 'backup phrase is wrong' "$workdir/wrong-backup.err"
printf '%s\n' "$new_phrase" | run_cli --phrase-stdin doses list --mode transfem \
    | grep -q "$dose_b" || fail "wrong backup phrase changed the database"

separate_preview="$(printf '%s\n%s\n' "$new_phrase" "$phrase" | \
    run_cli --phrase-stdin --backup-phrase-stdin backup preview "$sealed")"
grep -q $'ok\tyes' <<<"$separate_preview"

separate_import="$(printf '%s\n%s\n' "$new_phrase" "$phrase" | \
    run_cli --phrase-stdin --backup-phrase-stdin backup import "$sealed")"
grep -q $'applied\tyes' <<<"$separate_import"
list_output="$(printf '%s\n' "$new_phrase" | \
    run_cli --phrase-stdin doses list --mode transfem)"
grep -q "$dose_a" <<<"$list_output"
test "$(grep -c "$dose_a" <<<"$list_output")" = "1"

add_c_output="$(printf '%s\n' "$new_phrase" | \
    run_cli --phrase-stdin doses add --mode transfem --route oral --ester E2 \
    --dose-mg 6 --time-h 30)"
dose_c="${add_c_output#added }"

if printf '%s\n' "$new_phrase" | run_cli --phrase-stdin backup import "$plain" \
    >"$workdir/plain-no-flag.out" 2>"$workdir/plain-no-flag.err"; then
    fail "plaintext backup import succeeded without --plaintext"
fi
grep -q -- '--plaintext' "$workdir/plain-no-flag.err"
printf '%s\n' "$new_phrase" | run_cli --phrase-stdin doses list --mode transfem \
    | grep -q "$dose_c" || fail "plaintext import without --plaintext changed the database"

plain_import="$(printf '%s\n' "$new_phrase" | \
    run_cli --phrase-stdin backup import "$plain" --plaintext)"
grep -q $'applied\tyes' <<<"$plain_import"
list_output="$(printf '%s\n' "$new_phrase" | \
    run_cli --phrase-stdin doses list --mode transfem)"
grep -q "$dose_a" <<<"$list_output"
test "$(grep -c "$dose_a" <<<"$list_output")" = "1"

echo "cli_vault_integration: ok"
