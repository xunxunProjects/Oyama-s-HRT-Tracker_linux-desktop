#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="$(mktemp "${TMPDIR:-/tmp}/oyama-sbom.XXXXXX.json")"
trap 'rm -f "$out"' EXIT

cmake -DOUT="$out" -P "$root/scripts/generate-sbom.cmake"
test -s "$out"

python3 - "$out" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
assert data["bomFormat"] == "CycloneDX"
assert data["specVersion"] == "1.5"
assert data["version"] == 1
names = {c["name"] for c in data["components"]}
for expected in ("qt6", "qt5", "postgresql", "libsodium", "nlohmann-json", "xen"):
    assert expected in names, expected
for component in data["components"]:
    hashes = component.get("hashes", [])
    assert any(h["alg"] == "SHA-256" and len(h["content"]) == 64 for h in hashes), component["name"]
print("sbom_test: PASS")
PY
