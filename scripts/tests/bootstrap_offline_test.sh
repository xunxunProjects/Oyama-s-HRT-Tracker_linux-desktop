#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/oyama-bootstrap.XXXXXX")"
trap 'rm -rf "$work"' EXIT

if "$root/scripts/bootstrap-third-party.sh" \
    --offline --only json --cache "$work/cache" --prefix "$work/prefix" \
    >"$work/out" 2>&1; then
    echo "expected offline bootstrap with an empty cache to fail" >&2
    exit 1
fi
grep -q "json.tar.xz" "$work/out"
grep -q "offline" "$work/out"
echo "bootstrap_offline_test: PASS"
