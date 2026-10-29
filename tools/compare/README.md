# PK comparison harness

This directory checks the C99 model against the upstream TypeScript model in
`/home/haiyan/git/Oyama-s-HRT-Tracker` (upstream commit `f727a4d` when the port
was verified). The C core and upstream are independent implementations of the
same equations, so agreement is evidence that the port is faithful.

The harness has three parts:

1. `upstream-dumper.test.ts` — a vitest test that runs a fixed set of scenarios
   through the upstream `runSimulation()` and writes `scenarios.txt` and
   `main.json`.
2. `dump.c` — reads the same `scenarios.txt`, runs `oyama_pk_simulate()`, and
   writes `c.json`.
3. `compare.py` — compares the arrays, AUC, interpolation samples, and
   sublingual theta values with a relative tolerance (default `1e-6`).

## Run it

Build the C side:

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Generate the upstream output (from the upstream repository):

```sh
cp /path/to/Oyama-s-HRT-Tracker_linux-desktop/tools/compare/upstream-dumper.test.ts \
   tests/_dump_pk.test.ts
npx vitest run tests/_dump_pk.test.ts
rm tests/_dump_pk.test.ts
```

The default output directory is `/tmp/opencode/pk-compare`. Override it with
`OYAMA_PK_COMPARE_DIR=/some/dir` for both the vitest run and the C dumper.

Run the C dumper and compare:

```sh
./build/oyama_pk_dump /tmp/opencode/pk-compare/scenarios.txt \
    > /tmp/opencode/pk-compare/c.json
python3 tools/compare/compare.py \
    /tmp/opencode/pk-compare/main.json \
    /tmp/opencode/pk-compare/c.json
```

A successful run ends with `0 above 1e-06`. The last verified run compared
413,323 fields across 36 scenarios; the worst relative error was
`1.119e-15` (concentration values) with zero fields above `1e-6`.

## What is covered

The scenarios exercise both hormone modes and every modelled route/ester:
oral E2/EV/EB and CPA, sublingual E2/EV/EB with default and custom theta,
gel E2/EV and T at all three sites, E2 and T patches including removal,
overlap, fallback pairing, and same-time tie-breaks, all E2 and T injection
esters, age adjustment, unsorted input, unmodelled drugs, empty history,
zero body weight, and an invalid birth year.

Calibration (`off`/`ekf`/`ou_kalman`/`mipd`) is not part of the core simulator
and is not covered here; it is a later milestone.

## Model contract

The intended contract is a JSON vector file under `docs/spec/`. Until that is
generated, this harness is the authoritative comparison. Any intentional
deviation from upstream must be recorded in `docs/spec/divergences.md`.
