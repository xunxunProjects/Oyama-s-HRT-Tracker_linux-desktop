#!/usr/bin/env bash
set -euo pipefail

# End-to-end test of `db migrate-vault` against a temporary application-managed
# legacy cluster.  The legacy cluster starts with migrations 0001-0002 and seed
# data, then the command must build a new vault cluster and promote it.

cli="${1:?usage: run_cli_migration.sh /path/to/oyama-hrt-cli}"
migrations="${2:?usage: run_cli_migration.sh CLI MIGRATIONS_DIR}"

if [[ -n "${PG_BINDIR:-}" ]]; then
    bindir="$PG_BINDIR"
elif command -v pg_config >/dev/null 2>&1; then
    bindir="$(pg_config --bindir)"
else
    bindir=""
fi

for tool in initdb pg_ctl psql; do
    if [[ -z "$bindir" || ! -x "$bindir/$tool" ]]; then
        echo "SKIP: PostgreSQL server binaries not found (set PG_BINDIR)" >&2
        exit 77
    fi
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
seed="$repo_root/storage/tests/legacy_seed.sql"

workdir="$(mktemp -d "${TMPDIR:-/tmp}/oyama-cli-migration.XXXXXX")"
export XDG_CONFIG_HOME="$workdir/config"
export XDG_DATA_HOME="$workdir/data"
export XDG_RUNTIME_DIR="$workdir/runtime"
export PG_BINDIR="$bindir"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$workdir" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"

dbname="oyama_hrt_migration_test"
port=$((59000 + RANDOM % 1000))
data_dir="$XDG_DATA_HOME/oyama-hrt/pgdata"
socket_dir="$XDG_RUNTIME_DIR/oyama-hrt"
legacy_migrations="$workdir/legacy-migrations"
mkdir -p "$legacy_migrations"
cp "$migrations/0001_initial_schema.sql" "$migrations/0002_settings_timestamps.sql" \
    "$legacy_migrations/"

cleanup() {
    "$cli" --migrations "$migrations" --port "$port" --dbname "$dbname" \
        db stop >/dev/null 2>&1 || true
    for directory in "$data_dir" "$data_dir-migrate" "$data_dir"-plaintext-*; do
        if [[ -d "$directory" && -f "$directory/postmaster.pid" ]]; then
            "$bindir/pg_ctl" -D "$directory" -m immediate -w stop \
                >/dev/null 2>&1 || true
        fi
    done
    rm -rf "$workdir"
}
trap cleanup EXIT

run_cli_with_migrations() {
    local migrations_dir="$1"
    shift
    "$cli" --port "$port" --dbname "$dbname" --migrations "$migrations_dir" "$@"
}

run_cli() {
    run_cli_with_migrations "$migrations" "$@"
}

psql_query() {
    "$bindir/psql" -X -h "$socket_dir" -p "$port" -U "$(id -un)" -d "$dbname" \
        -v ON_ERROR_STOP=1 -tAc "$1"
}

setup_output="$(run_cli_with_migrations "$legacy_migrations" \
    db setup --managed --yes)"
grep -q $'backend\tmanaged' <<<"$setup_output"
grep -q $'schema_version\t2' <<<"$setup_output"

"$bindir/psql" -X -h "$socket_dir" -p "$port" -U "$(id -un)" -d "$dbname" \
    -v ON_ERROR_STOP=1 -f "$seed" >/dev/null

legacy_counts="$(psql_query "SELECT count(*) FROM dose_events")"
test "$legacy_counts" = "2"

phrase="correct horse battery staple"
migrate_output="$(printf '%s\n' "$phrase" | \
    run_cli db migrate-vault --yes --phrase-stdin)"

grep -q $'verified\tyes' <<<"$migrate_output"
grep -q $'count\tdose_events\t2' <<<"$migrate_output"
grep -q '^plaintext path: ' <<<"$migrate_output"
grep -q 'does not guarantee erasure' <<<"$migrate_output"

plaintext="$(sed -n 's/^plaintext path: //p' <<<"$migrate_output" | head -n 1)"
if [[ -z "$plaintext" || ! -d "$plaintext" ]]; then
    echo "plaintext sibling was not reported or does not exist" >&2
    exit 1
fi
case "$plaintext" in
    "$data_dir"-plaintext-[0-9]*T*Z) ;;
    *)
        echo "unexpected plaintext sibling name: $plaintext" >&2
        exit 1
        ;;
esac

run_cli db start >/dev/null
meta_count="$(psql_query "SELECT count(*) FROM vault_meta")"
test "$meta_count" = "1"
record_count="$(psql_query "SELECT count(*) FROM vault_records")"
test "$record_count" = "10"

if run_cli db migrate-vault --yes --purge-old "$workdir/wrong-plaintext" \
    >"$workdir/purge-wrong.out" 2>"$workdir/purge-wrong.err"; then
    echo "wrong --purge-old path was accepted" >&2
    exit 1
fi
grep -q 'does not exactly match' "$workdir/purge-wrong.err"
test -d "$plaintext"

if run_cli db migrate-vault --purge-old "$plaintext" \
    >"$workdir/purge-no-yes.out" 2>"$workdir/purge-no-yes.err"; then
    echo "--purge-old without --yes was accepted" >&2
    exit 1
fi
grep -q -- '--yes' "$workdir/purge-no-yes.err"
test -d "$plaintext"

existing_config="$workdir/existing-config"
OYAMA_CONFIG_FILE="$existing_config" run_cli --socket-dir "$socket_dir" \
    db setup --existing --yes >/dev/null
if OYAMA_CONFIG_FILE="$existing_config" run_cli db migrate-vault --yes \
    --phrase-stdin <<<"$phrase" >"$workdir/existing.out" \
    2>"$workdir/existing.err"; then
    echo "db migrate-vault accepted an existing backend" >&2
    exit 1
fi
grep -q 'JSON export' "$workdir/existing.err"

purge_output="$(run_cli db migrate-vault --yes --purge-old "$plaintext")"
grep -q $'purged\t'"$plaintext" <<<"$purge_output"
grep -q 'does not guarantee erasure' <<<"$purge_output"
test ! -e "$plaintext"

echo "cli_migration: ok"
