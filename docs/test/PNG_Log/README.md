# PK cross-validation report

Generated from `/tmp/opencode/pk-crossval/50m-final/out`.

## Result

| Metric | Value |
|---|---|
| Concentration points compared | 50,000,000 |
| Scenarios compared | 24,802 |
| Max absolute error | 1.091394e-10 pg/mL |
| Points with error > 1.5 pg/mL | 0 |
| Points with error > 5 pg/mL | 0 |
| Grid mismatches | 0 |
| Null results | 0 |
| Non-finite points | 0 |
| Pass (all acceptance checks) | **True** |

Worst point: scenario `s24643` (`injection_t`),
route `injection`, ester `TC`,
38.5 kg, error `1.091e-10 pg/mL`
at concentration `137760.372 pg/mL`.

## Figures

- `pk_crossval_scatter.png` — absolute error vs concentration, coloured by age band.
- `pk_crossval_box.png` — error distribution by route, with a threshold-margin panel.
- `pk_crossval_heatmap.png` — max error by route and age band.
- `pk_crossval_cdf.png` — cumulative distribution of absolute error (log tail + full-range acceptance view).

The 1.5 pg/mL and 5 pg/mL thresholds are far above the observed numerical
difference; the C and TypeScript implementations agree to floating-point
round-off.

## Reproduce

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

The complete `summary.json` from the run is committed next to this report.
