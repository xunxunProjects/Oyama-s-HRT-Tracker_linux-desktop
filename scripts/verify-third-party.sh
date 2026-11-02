#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
versions="$root/third_party/versions.cmake"
cache="$root/third_party/cache"
deps=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --versions) versions="${2:?}"; shift 2 ;;
        --cache) cache="${2:?}"; shift 2 ;;
        -h|--help)
            echo "usage: verify-third-party.sh [--versions FILE] [--cache DIR] [dep...]"
            exit 0 ;;
        *) deps+=("$1"); shift ;;
    esac
done

if [[ ${#deps[@]} -eq 0 ]]; then
    deps=(qt6 qt5 pg sodium json xen)
fi

if [[ ! -d "$cache" ]]; then
    echo "SKIP: cache directory not present: $cache"
    exit 77
fi

# An existing but empty cache is a skip, not a missing-archive error.
shopt -s nullglob
cache_files=("$cache"/*)
shopt -u nullglob
if [[ ${#cache_files[@]} -eq 0 ]]; then
    echo "SKIP: cache is empty: $cache"
    exit 77
fi

status=0
for dep in "${deps[@]}"; do
    info="$(cmake -DNAME="$dep" -DVERSIONS="$versions" -P "$root/scripts/third-party-info.cmake" 2>&1 || true)"
    url="$(sed -n 's/^URL=//p' <<<"$info")"
    expected="$(sed -n 's/^SHA256=//p' <<<"$info")"
    if [[ -z "$url" ]]; then
        echo "UNKNOWN: no entry for $dep" >&2
        status=1
        continue
    fi
    file="$cache/$(basename "$url")"

    if [[ ! -f "$file" ]]; then
        echo "MISSING: $file" >&2
        status=1
        continue
    fi

    if [[ "$expected" == "<fill>" || -z "$expected" ]]; then
        echo "UNPINNED: $dep has no SHA-256 in versions.cmake" >&2
        status=1
        continue
    fi

    actual="$(sha256sum "$file" | awk '{print $1}')"
    if [[ "$actual" != "$expected" ]]; then
        echo "MISMATCH: $file" >&2
        echo "  expected $expected" >&2
        echo "  actual   $actual" >&2
        status=1
        continue
    fi
    echo "OK: $dep $(basename "$file")"
done

exit "$status"
