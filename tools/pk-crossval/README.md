# 50M-point PK cross-validation

This harness compares the C99 PK core against the upstream TypeScript model at
the curve level. It is intentionally larger and more adversarial than
`tools/compare/`: 26,000 randomized scenarios are run through both
implementations, and the comparison stops after exactly 50,000,000 concentration
time points.

## Result

Last run against upstream commit `766e3cb`:

| Metric | Value |
|---|---|
| Concentration points compared | 50,000,000 |
| Scenarios compared | 24,802 |
| Max absolute error | `1.091394e-10 pg/mL` |
| Points with error > 1.5 pg/mL | 0 |
| Points with error > 5 pg/mL | 0 |
| Grid mismatches | 0 |
| Null results | 0 |
| Pass | **yes** |

Worst point: `s24643` (`injection_t`), route `injection`, ester `TC`,
38.5 kg, age 64.26, concentration `137760.372 pg/mL`, error
`1.091e-10 pg/mL`.

The acceptance rule is: maximum absolute error <= 5 pg/mL and at most 5 points
with error > 1.5 pg/mL.

## Files

- `generate.py` — deterministic scenario generator (seed `20261120`). Writes
  `scenarios.tsv` (the format consumed by the C dumper) and `scenarios.json`
  (metadata for the Node comparator).
- `dump_binary.c` — C dumper. It reuses the parser in `tools/compare/dump.c` and
  writes a compact binary stream: magic `OYKV1\0\0\0`, scenario count, then per
  scenario `uint32 index`, `uint32 length`, `double time_h[]`,
  `double conc_e2[]`, `double conc_t_ng_dl[]`.
- `compare.mjs` — runs the bundled upstream `runSimulation()` on the same
  scenarios, verifies the time grid, compares E2 (pg/mL) and T (ng/dL converted
  to pg/mL), and writes `summary.json`, `per_scenario.csv`,
  `errors_sample.csv`, and `histogram.json`.
- `plot.py` — reads those results and writes the scatter, box, and heatmap PNGs
  plus a report into `docs/test/PNG_Log/`.

## Build

```sh
cmake --build build2 --target oyama_pk_crossval_dump -j4
```

The target is only defined when `OYAMA_BUILD_COMPARE_TOOLS=ON` (the default).

## Bundle the upstream model

From `/home/haiyan/git/Oyama-s-HRT-Tracker`:

```sh
npx esbuild src/domain/simulation/runSimulation.ts \
    --bundle --platform=node --format=esm \
    --outfile=/tmp/opencode/pk-crossval/runSimulation.mjs
```

The upstream test setup freezes `Date.now()` at `NOW_H = 480000` hours; the
comparator does the same.

## Run

```sh
python3 tools/pk-crossval/generate.py \
    --out-dir /tmp/opencode/pk-crossval/50m --count 26000

./build2/oyama_pk_crossval_dump \
    /tmp/opencode/pk-crossval/50m/scenarios.tsv \
    /tmp/opencode/pk-crossval/50m/curves.bin

node tools/pk-crossval/compare.mjs \
    --scenarios /tmp/opencode/pk-crossval/50m/scenarios.json \
    --curves /tmp/opencode/pk-crossval/50m/curves.bin \
    --module /tmp/opencode/pk-crossval/runSimulation.mjs \
    --out /tmp/opencode/pk-crossval/50m/out \
    --limit 50000000 --sample-every 251

python3 tools/pk-crossval/plot.py \
    --results /tmp/opencode/pk-crossval/50m/out \
    --out docs/test/PNG_Log
```

The C binary is about 1.2 GB for 50M points. The comparator streams it one
scenario at a time, so memory use stays small.

## Coverage

The generator cycles through oral E2/EV/EB/EC/EN/EU, sublingual E2 and esters
with custom theta/tier, gel E2/EV/T at all three sites, E2 and T patches with
wear and removal, E2/EB/EV/EC/EN/EU injections, T/TC/TE/TU injections, nasal T,
SC TC/TE/TU, T pellets with explicit release rates, CPA with E2, mixed E2+T
scenarios, and unsorted event lists. Body weight ranges from 35 to 160 kg;
birth years cover ages 6-94 plus an unknown-birth-year bucket.

## Plotting

`plot.py` requires `matplotlib` and `numpy`. If the system Python does not have
them, create a virtual environment:

```sh
python3 -m venv --system-site-packages /tmp/opencode/pk-venv
/tmp/opencode/pk-venv/bin/pip install --no-input matplotlib
/tmp/opencode/pk-venv/bin/python tools/pk-crossval/plot.py ...
```
