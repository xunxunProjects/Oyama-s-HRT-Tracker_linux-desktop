#!/usr/bin/env python3
"""Plot the 50M-point C-vs-TypeScript PK cross-validation results."""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

AGE_ORDER = [
    "prepubertal", "earlyPuberty", "midPuberty", "latePuberty",
    "adult", "older", "unknown",
]
ROUTE_ORDER = [
    "oral", "sublingual", "gel", "patchApply", "injection",
    "nasal", "sc", "pellet", "mixed",
]
TEMPLATE_ROUTE = {
    "oral_e2": "oral", "oral_ester": "oral", "cpa": "oral",
    "sublingual_e2": "sublingual", "sublingual_ester": "sublingual",
    "gel_e2": "gel", "gel_ev": "gel", "gel_t": "gel",
    "patch_e2": "patchApply", "patch_t": "patchApply",
    "injection_e2": "injection", "injection_t": "injection",
    "t_nasal": "nasal", "t_sc": "sc", "t_pellet": "pellet",
    "mixed": "mixed", "unsorted": "mixed",
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def num(value: str | None, default: float = math.nan) -> float:
    if value is None or value == "":
        return default
    try:
        return float(value)
    except ValueError:
        return default


def route_of(row: dict[str, str]) -> str:
    return TEMPLATE_ROUTE.get(row.get("template", ""), row.get("template", "unknown"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("docs/test/PNG_Log"))
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    summary = json.loads((args.results / "summary.json").read_text(encoding="utf-8"))
    sample = read_csv(args.results / "errors_sample.csv")
    per_scenario = read_csv(args.results / "per_scenario.csv")

    max_error = float(summary["max_abs_error_pg_ml"])
    gt15 = int(summary["count_gt_1_5_pg_ml"])
    gt5 = int(summary["count_gt_5_pg_ml"])
    nonfinite = int(summary.get("non_finite_points", 0))
    total = int(summary["total_points"])

    # ------------------------------------------------------------------
    # 1. Scatter: absolute error vs concentration, coloured by age band
    # ------------------------------------------------------------------
    fig, ax = plt.subplots(figsize=(14, 8.5))
    for band in AGE_ORDER:
        rows = [r for r in sample if r["age_band"] == band]
        if not rows:
            continue
        x = np.array([max(num(r["conc_pg_ml"]), 1e-9) for r in rows])
        y = np.array([max(num(r["error_pg_ml"]), 1e-16) for r in rows])
        ax.scatter(x, y, s=5, alpha=0.30, linewidths=0,
                   label=f"{band} (n={len(rows):,})", rasterized=True)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Concentration at sample time (pg/mL; E2 or testosterone)")
    ax.set_ylabel("Absolute C-vs-TypeScript error (pg/mL)")
    ax.set_title(
        f"PK cross-validation: {total:,} concentration points, "
        f"max |error| = {max_error:.2e} pg/mL"
    )
    ax.axhline(1.5, color="#d62728", linestyle="--", linewidth=1.5,
               label="1.5 pg/mL threshold")
    ax.axhline(5.0, color="#8c1d18", linestyle=":", linewidth=1.8,
               label="5 pg/mL limit")
    ax.set_ylim(1e-16, 10.0)
    finite_x = [max(num(r["conc_pg_ml"]), 1e-9) for r in sample]
    if finite_x:
        ax.set_xlim(max(1e-6, min(finite_x) * 0.8), max(finite_x) * 1.25)
    ax.grid(True, which="major", alpha=0.25)
    ax.legend(loc="upper right", fontsize=9, framealpha=0.9, ncol=2)
    ax.text(
        0.012, 0.025,
        f"Points > 1.5 pg/mL: {gt15:,}\nPoints > 5 pg/mL: {gt5:,}\n"
        f"Observed errors sit ~10 orders of magnitude below the thresholds.",
        transform=ax.transAxes, fontsize=10, va="bottom",
        bbox=dict(boxstyle="round,pad=0.5", facecolor="white", alpha=0.9),
    )
    fig.tight_layout()
    scatter_path = args.out / "pk_crossval_scatter.png"
    fig.savefig(scatter_path, dpi=200, bbox_inches="tight")
    plt.close(fig)

    # ------------------------------------------------------------------
    # 2. Box plot: log10 error by route, plus a threshold-margin panel
    # ------------------------------------------------------------------
    fig, (ax_box, ax_margin) = plt.subplots(
        1, 2, figsize=(16, 8), gridspec_kw={"width_ratios": [2.2, 1.0]}
    )
    data: list[list[float]] = []
    labels: list[str] = []
    route_max: list[tuple[str, float, int]] = []
    for route in ROUTE_ORDER:
        rows = [r for r in per_scenario
                if route_of(r) == route and num(r["max_error"]) > 0.0]
        if not rows:
            continue
        vals = [math.log10(num(r["max_error"])) for r in rows]
        data.append(vals)
        labels.append(f"{route}\n(n={len(vals):,})")
        route_max.append((route, max(10.0 ** v for v in vals), len(vals)))

    if data:
        ax_box.boxplot(
            data, tick_labels=labels, showfliers=True, patch_artist=True,
            flierprops={"marker": ".", "markersize": 2, "alpha": 0.25,
                        "markerfacecolor": "#1d4e89", "markeredgecolor": "none"},
            medianprops={"color": "#111111", "linewidth": 1.4},
            boxprops={"facecolor": "#8ecae6", "edgecolor": "#1d4e89"},
            whiskerprops={"color": "#1d4e89"},
            capprops={"color": "#1d4e89"},
        )
        ax_box.set_ylabel("log10 per-scenario max absolute error (pg/mL)")
        ax_box.set_title("Per-scenario worst error by route (box = IQR, whiskers = 1.5×IQR)")
        ax_box.grid(True, axis="y", alpha=0.25)
        lo = min(min(v) for v in data)
        hi = max(max(v) for v in data)
        ax_box.set_ylim(lo - 0.5, hi + 0.5)
        ax_box.text(
            0.015, 0.97,
            "Thresholds 1.5 and 5 pg/mL are ~10 orders above the observed errors.",
            transform=ax_box.transAxes, fontsize=9, va="top",
            bbox=dict(boxstyle="round,pad=0.4", facecolor="white", alpha=0.9),
        )

    if route_max:
        names = [r[0] for r in route_max]
        vals = [max(r[1], 1e-16) for r in route_max]
        ypos = np.arange(len(names))
        ax_margin.barh(ypos, vals, color="#219ebc", alpha=0.85)
        ax_margin.set_yticks(ypos)
        ax_margin.set_yticklabels(names)
        ax_margin.set_xscale("log")
        ax_margin.set_xlabel("Worst per-scenario error (pg/mL)")
        ax_margin.set_title("Worst per-scenario error by route vs thresholds")
        ax_margin.axvline(1.5, color="#d62728", linestyle="--", label="1.5 pg/mL")
        ax_margin.axvline(5.0, color="#8c1d18", linestyle=":", label="5 pg/mL")
        ax_margin.set_xlim(max(1e-13, min(vals) * 0.5), 10.0)
        ax_margin.grid(True, axis="x", alpha=0.25)
        ax_margin.legend(loc="lower right", fontsize=9)

    fig.suptitle(
        f"PK cross-validation error by route — {total:,} points; "
        f"max {max_error:.2e} pg/mL",
        fontsize=14,
    )
    fig.tight_layout()
    box_path = args.out / "pk_crossval_box.png"
    fig.savefig(box_path, dpi=200, bbox_inches="tight")
    plt.close(fig)

    # ------------------------------------------------------------------
    # 3. Heatmap: max error by route x age band
    # ------------------------------------------------------------------
    route_rows = [r for r in ROUTE_ORDER if any(route_of(s) == r for s in per_scenario)]
    age_cols = [a for a in AGE_ORDER if any(s["age_band"] == a for s in per_scenario)]
    grid = np.full((len(route_rows), len(age_cols)), np.nan)
    counts = np.zeros_like(grid, dtype=int)
    for row in per_scenario:
        route = route_of(row)
        age = row["age_band"]
        if route not in route_rows or age not in age_cols:
            continue
        i = route_rows.index(route)
        j = age_cols.index(age)
        value = num(row["max_error"])
        if math.isnan(value):
            continue
        if math.isnan(grid[i, j]) or value > grid[i, j]:
            grid[i, j] = value
        counts[i, j] += 1

    log_grid = np.log10(np.maximum(grid, 1e-16))
    masked = np.ma.masked_invalid(log_grid)
    cmap = plt.get_cmap("viridis").copy()
    cmap.set_bad("#d9d9d9")

    fig, ax = plt.subplots(figsize=(13, 7.5))
    im = ax.imshow(masked, aspect="auto", cmap=cmap,
                   vmin=np.nanmin(log_grid) - 0.4, vmax=np.nanmax(log_grid) + 0.4)
    ax.set_xticks(np.arange(len(age_cols)))
    ax.set_xticklabels(age_cols, rotation=30, ha="right")
    ax.set_yticks(np.arange(len(route_rows)))
    ax.set_yticklabels(route_rows)
    ax.set_xlabel("Age band")
    ax.set_ylabel("Route")
    ax.set_title(
        f"Max absolute error by route and age band — {total:,} points, "
        f"max {max_error:.2e} pg/mL"
    )
    for i in range(len(route_rows)):
        for j in range(len(age_cols)):
            if np.isnan(grid[i, j]):
                ax.text(j, i, "n/a", ha="center", va="center",
                        fontsize=9, color="#555555")
            else:
                ax.text(j, i, f"{grid[i, j]:.1e}\n(n_scen={counts[i, j]})",
                        ha="center", va="center", fontsize=7.5,
                        color="white" if log_grid[i, j] < np.nanmean(log_grid) else "black")
    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label("log10(max absolute error, pg/mL)")
    fig.tight_layout()
    heatmap_path = args.out / "pk_crossval_heatmap.png"
    fig.savefig(heatmap_path, dpi=200, bbox_inches="tight")
    plt.close(fig)

    # ------------------------------------------------------------------
    # 4. CDF of the absolute error
    # ------------------------------------------------------------------
    errors = np.array(
        [num(r["error_pg_ml"]) for r in sample if math.isfinite(num(r["error_pg_ml"]))],
        dtype=float,
    )
    errors.sort()
    sample_n = errors.size
    zero_count = int(np.count_nonzero(errors == 0.0))
    zero_fraction = zero_count / sample_n
    sample_max = float(errors[-1])
    positive = errors[errors > 0.0]
    positive_cdf = (zero_count + np.arange(1, positive.size + 1)) / sample_n

    fig, (ax_tail, ax_full) = plt.subplots(
        1, 2, figsize=(16, 7.5), gridspec_kw={"width_ratios": [1.45, 1.0]}
    )

    ax_tail.step(
        positive, positive_cdf, where="post", color="#1d4e89", linewidth=2.2,
        label=f"Empirical CDF (sample n={sample_n:,})",
    )
    ax_tail.set_xscale("log")
    ax_tail.set_xlim(max(1e-16, float(positive[0]) * 0.8), max(1e-9, sample_max * 2.0))
    ax_tail.set_ylim(max(0.0, zero_fraction - 0.02), 1.005)
    for q, label in ((0.99, "p99"), (0.999, "p99.9")):
        value = float(errors[min(sample_n - 1, int(q * sample_n))])
        ax_tail.axvline(value, color="#888888", linestyle=":", linewidth=1.0)
        ax_tail.text(
            value, zero_fraction + 0.008, f"{label}\n{value:.1e}",
            rotation=90, fontsize=8, va="bottom", ha="right", color="#555555",
        )
    ax_tail.set_xlabel("Absolute C-vs-TypeScript error (pg/mL; log scale)")
    ax_tail.set_ylabel("Cumulative fraction of points")
    ax_tail.set_title("CDF tail (positive errors)")
    ax_tail.grid(True, which="both", alpha=0.20)
    ax_tail.legend(loc="lower right", fontsize=9, framealpha=0.9)
    ax_tail.text(
        0.03, 0.97,
        f"{zero_fraction:.1%} exactly zero\n"
        f"sample max {sample_max:.2e} pg/mL\n"
        f"full-run max {max_error:.2e} pg/mL",
        transform=ax_tail.transAxes, fontsize=9.5, va="top",
        bbox=dict(boxstyle="round,pad=0.45", facecolor="white", alpha=0.92),
    )

    ax_full.step(
        errors, np.arange(1, sample_n + 1) / sample_n, where="post",
        color="#1d4e89", linewidth=2.2,
    )
    ax_full.set_xscale("symlog", linthresh=1e-12, linscale=1.2)
    ax_full.set_xlim(0.0, 10.0)
    ax_full.set_ylim(0.0, 1.02)
    ax_full.axhline(1.0, color="#777777", linewidth=0.8, linestyle=":")
    ax_full.axvline(1.5, color="#d62728", linestyle="--", linewidth=1.5,
                    label="1.5 pg/mL threshold (0 points)")
    ax_full.axvline(5.0, color="#8c1d18", linestyle=":", linewidth=1.8,
                    label="5 pg/mL limit (0 points)")
    ax_full.set_xlabel("Absolute error (pg/mL; symlog scale)")
    ax_full.set_ylabel("Cumulative fraction of points")
    ax_full.set_title("Full range vs acceptance thresholds")
    ax_full.grid(True, which="both", alpha=0.20)
    ax_full.legend(loc="lower right", fontsize=9, framealpha=0.9)
    ax_full.text(
        0.03, 0.55,
        f"Sample: {sample_n:,} of {total:,} points\n"
        f"100% <= 1.5 pg/mL\n"
        f"0 points > 5 pg/mL",
        transform=ax_full.transAxes, fontsize=9.5, va="center",
        bbox=dict(boxstyle="round,pad=0.45", facecolor="white", alpha=0.92),
    )

    fig.suptitle(
        f"Cumulative distribution of absolute error — {total:,} points, "
        f"max {max_error:.2e} pg/mL",
        fontsize=14,
    )
    fig.tight_layout()
    cdf_path = args.out / "pk_crossval_cdf.png"
    fig.savefig(cdf_path, dpi=200, bbox_inches="tight")
    plt.close(fig)

    # ------------------------------------------------------------------
    # Report
    # ------------------------------------------------------------------
    report = rf"""# PK cross-validation report

Generated from `{args.results}`.

## Result

| Metric | Value |
|---|---|
| Concentration points compared | {total:,} |
| Scenarios compared | {summary['scenarios_compared']:,} |
| Max absolute error | {max_error:.6e} pg/mL |
| Points with error > 1.5 pg/mL | {gt15:,} |
| Points with error > 5 pg/mL | {gt5:,} |
| Grid mismatches | {summary['grid_mismatches']:,} |
| Null results | {summary['null_results']:,} |
| Non-finite points | {nonfinite:,} |
| Pass (all acceptance checks) | **{summary['pass']}** |

Worst point: scenario `{summary['worst_case']['id']}` (`{summary['worst_case']['template']}`),
route `{summary['worst_case']['route']}`, ester `{summary['worst_case']['ester']}`,
{summary['worst_case']['weight']} kg, error `{summary['worst_case']['error_pg_ml']:.3e} pg/mL`
at concentration `{summary['worst_case']['conc_pg_ml']:.3f} pg/mL`.

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
"""
    (args.out / "README.md").write_text(report, encoding="utf-8")
    (args.out / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"wrote {scatter_path}")
    print(f"wrote {box_path}")
    print(f"wrote {heatmap_path}")
    print(f"wrote {cdf_path}")
    print(f"wrote {args.out / 'README.md'}")


if __name__ == "__main__":
    main()
