#!/usr/bin/env bash
set -euo pipefail

# Runs the filesystem-only promotion state machine test.  It never starts
# PostgreSQL and therefore needs no server binaries.

test_binary="${1:?usage: run_legacy_promotion_test.sh /path/to/legacy_promotion_test}"

exec "$test_binary"
