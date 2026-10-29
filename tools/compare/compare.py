#!/usr/bin/env python3
"""Compare the C99 PK core output with the upstream TypeScript output.

Both inputs are JSON documents produced by the scenario dumper:

  main.json - written by tools/compare/upstream-dumper.test.ts
  c.json    - written by oyama_pk_dump (see tools/compare/README.md)

The comparison is relative: abs(c-main) / max(abs(main), floor).  The default
tolerance is 1e-6, matching the project's model contract; the port currently
matches to roughly machine precision.
"""

import argparse
import json
import math
import sys

FIELDS = ["timeH", "concPGmL", "concPGmL_E2", "concPGmL_CPA", "concNGdL_T"]


def relative_error(actual, expected, floor):
    if actual is None or expected is None:
        return 0.0 if actual is expected else math.inf
    if isinstance(actual, bool) or isinstance(expected, bool):
        return 0.0 if actual is expected else math.inf
    if not isinstance(actual, (int, float)) or not isinstance(expected, (int, float)):
        return 0.0 if actual == expected else math.inf
    if not math.isfinite(actual) or not math.isfinite(expected):
        if math.isnan(actual) and math.isnan(expected):
            return 0.0
        return 0.0 if actual == expected else math.inf
    return abs(actual - expected) / max(abs(expected), floor)


def compare_array(worst, sid, field, actual, expected, floor):
    if len(actual) != len(expected):
        worst.append((math.inf, sid, field, math.inf, -1, len(expected), len(actual)))
        return
    for index, (a, e) in enumerate(zip(actual, expected)):
        rel = relative_error(a, e, floor)
        worst.append((rel, sid, field, abs(a - e) if isinstance(a, (int, float)) and isinstance(e, (int, float)) else math.inf, index, e, a))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("main_json")
    parser.add_argument("c_json")
    parser.add_argument("--tolerance", type=float, default=1e-6)
    parser.add_argument("--floor", type=float, default=1e-12)
    args = parser.parse_args()

    with open(args.main_json, encoding="utf-8") as f:
        main_doc = json.load(f)
    with open(args.c_json, encoding="utf-8") as f:
        c_doc = json.load(f)

    main_scenarios = main_doc["scenarios"]
    c_scenarios = c_doc["scenarios"]
    if set(main_scenarios) != set(c_scenarios):
        print("scenario key mismatch:", sorted(set(main_scenarios) ^ set(c_scenarios)))
        return 1

    worst = []
    for sid in main_scenarios:
        expected = main_scenarios[sid]
        actual = c_scenarios[sid]
        if (expected is None) != (actual is None):
            print(f"{sid}: null mismatch main={expected is None} c={actual is None}")
            continue
        if expected is None:
            continue
        if "error" in actual:
            print(f"{sid}: C error {actual['error']}")
            continue
        for field in FIELDS:
            compare_array(worst, sid, field, actual[field], expected[field], args.floor)
        for kind in ("e2", "cpa", "t"):
            compare_array(worst, sid, f"interp.{kind}", actual["interp"][kind], expected["interp"][kind], args.floor)
        rel = relative_error(actual["auc"], expected["auc"], args.floor)
        worst.append((rel, sid, "auc", abs(actual["auc"] - expected["auc"]), -1, expected["auc"], actual["auc"]))

    main_theta = main_doc.get("theta")
    c_theta = c_doc.get("theta")
    if main_theta is not None and c_theta is not None:
        if len(main_theta) != len(c_theta):
            print(f"theta length mismatch {len(main_theta)} vs {len(c_theta)}")
        for index, (expected, actual) in enumerate(zip(main_theta, c_theta)):
            if expected["hold"] != actual["hold"]:
                print(f"theta[{index}] hold mismatch {expected['hold']} vs {actual['hold']}")
            rel = relative_error(actual["theta"], expected["theta"], args.floor)
            worst.append((rel, "theta", "theta", abs(actual["theta"] - expected["theta"]), index, expected["theta"], actual["theta"]))

    worst.sort(reverse=True)
    print(f'{"field":14} {"max_rel":>12} {"max_abs":>12}  scenario')
    for rel, sid, field, absolute, index, expected, actual in worst[:30]:
        print(f"{field:14} {rel:12.3e} {absolute:12.3e}  {sid} idx={index} main={expected!r} c={actual!r}")

    over = [item for item in worst if item[0] > args.tolerance]
    print(f"\n{len(worst)} field comparisons; {len(over)} above {args.tolerance:g}")
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
