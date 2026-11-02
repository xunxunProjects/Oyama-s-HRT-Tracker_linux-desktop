#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/oyama-verify.XXXXXX")"
trap 'rm -rf "$work"' EXIT

printf 'payload\n' >"$work/test.tar.gz"
sha="$(sha256sum "$work/test.tar.gz" | awk '{print $1}')"

cat >"$work/versions.cmake" <<EOF
set(OYAMA_TEST_VERSION "1.0")
set(OYAMA_TEST_URL "https://example.invalid/test-1.0.tar.gz")
set(OYAMA_TEST_SHA256 "$sha")
set(OYAMA_TEST_PATCHES "")
EOF

mkdir -p "$work/cache"
cp "$work/test.tar.gz" "$work/cache/test-1.0.tar.gz"

# Match: exit 0.
"$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test

# Mismatch: exit 1 and name both hashes.
printf 'tampered\n' >"$work/cache/test-1.0.tar.gz"
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test \
    >"$work/out" 2>&1; then
    echo "expected mismatch to fail" >&2
    exit 1
fi
grep -q "$sha" "$work/out"
grep -q "MISMATCH" "$work/out"

# Missing archive: exit 1. Keep an unrelated file so the cache is not empty.
rm "$work/cache/test-1.0.tar.gz"
printf 'other\n' >"$work/cache/other-1.0.tar.gz"
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/cache" test \
    >"$work/out" 2>&1; then
    echo "expected missing archive to fail" >&2
    exit 1
fi
grep -q "MISSING" "$work/out"

# Empty cache: exit 77.
mkdir -p "$work/empty-cache"
if "$root/scripts/verify-third-party.sh" \
    --versions "$work/versions.cmake" --cache "$work/empty-cache" test \
    >"$work/out" 2>&1; then
    echo "expected empty cache to skip" >&2
    exit 1
else
    status=$?
fi
test "$status" -eq 77
grep -q "SKIP" "$work/out"

echo "verify_third_party_test: PASS"
