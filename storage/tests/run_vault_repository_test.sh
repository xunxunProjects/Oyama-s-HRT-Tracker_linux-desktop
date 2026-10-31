#!/usr/bin/env bash
set -euo pipefail
exec bash "$(dirname "$0")/../../vault/tests/run_vault_pg_test.sh" "$@"
