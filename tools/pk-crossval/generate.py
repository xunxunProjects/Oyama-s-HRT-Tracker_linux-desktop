#!/usr/bin/env python3
"""Generate the deterministic scenario set for the 50M-point PK cross-validation.

The TSV file is consumed by the C dumper (same grammar as tools/compare).
The JSON file carries the same scenarios for the TypeScript/Node comparator.

Every scenario keeps its events within roughly the last 1,500 hours and uses
the 2,000-step floor of runSimulation, so each scenario contributes about
2,000-2,100 concentration points.  The comparator truncates the concatenated
stream at exactly 50,000,000 points.
"""

from __future__ import annotations

import argparse
import json
import random
from pathlib import Path

NOW_H = 480000.0
DEFAULT_COUNT = 26000
DEFAULT_SEED = 20261120

E2_ESTERS = ["E2", "EB", "EV", "EC", "EN", "EU"]
T_ESTERS = ["T", "TC", "TE", "TU"]


def event(route: str, ester: str, dose: float, time_h: float, eid: str,
          extras: dict | None = None) -> dict:
    return {
        "route": route,
        "ester": ester,
        "dose_mg": round(float(dose), 4),
        "time_h": round(float(time_h), 4),
        "id": eid,
        "extras": extras or {},
    }


def pick_weight(rng: random.Random) -> float:
    r = rng.random()
    if r < 0.10:
        return round(rng.uniform(35.0, 55.0), 1)
    if r < 0.20:
        return round(rng.uniform(120.0, 160.0), 1)
    return round(rng.uniform(55.0, 120.0), 1)


def pick_birth_year(rng: random.Random) -> int:
    if rng.random() < 0.15:
        return 0  # unknown; age multiplier is 1
    r = rng.random()
    if r < 0.20:
        return rng.randint(2006, 2018)  # puberty / under 19
    if r < 0.30:
        return rng.randint(1930, 1949)  # over 75 / cap
    return rng.randint(1950, 2005)


def sublingual_extras(rng: random.Random) -> dict:
    r = rng.random()
    if r < 0.55:
        return {"theta": round(rng.uniform(0.005, 0.30), 4)}
    if r < 0.80:
        return {"tier": rng.randint(0, 3)}
    return {}


def make_scenario(rng: random.Random, index: int, template: str) -> dict:
    now = NOW_H
    ev: list[dict] = []
    meta: dict = {}

    if template == "oral_e2":
        n = 1 if rng.random() < 0.70 else 3
        for k in range(n):
            ev.append(event("oral", "E2", rng.uniform(0.5, 4.0),
                            now - 24.0 * (n - k), f"e{k}"))
    elif template == "oral_ester":
        ester = rng.choice(["EB", "EV", "EC", "EN", "EU"])
        n = 1 if rng.random() < 0.60 else 2
        for k in range(n):
            ev.append(event("oral", ester, rng.uniform(1.0, 6.0),
                            now - 48.0 + 24.0 * k, f"e{k}"))
    elif template == "sublingual_e2":
        ev.append(event("sublingual", "E2", rng.uniform(0.5, 4.0),
                        now - rng.uniform(1.0, 12.0), "e0",
                        sublingual_extras(rng)))
    elif template == "sublingual_ester":
        ester = rng.choice(["EB", "EV", "EC", "EN", "EU"])
        ev.append(event("sublingual", ester, rng.uniform(0.5, 3.0),
                        now - rng.uniform(1.0, 12.0), "e0",
                        sublingual_extras(rng)))
    elif template == "gel_e2":
        ev.append(event("gel", "E2", rng.uniform(0.25, 3.0),
                        now - rng.uniform(2.0, 24.0), "e0",
                        {"gelSite": rng.randint(0, 2)}))
    elif template == "gel_ev":
        ev.append(event("gel", "EV", rng.uniform(0.5, 3.0),
                        now - rng.uniform(2.0, 24.0), "e0",
                        {"gelSite": rng.randint(0, 2)}))
    elif template == "gel_t":
        ev.append(event("gel", "T", rng.uniform(10.0, 100.0),
                        now - rng.uniform(2.0, 24.0), "e0",
                        {"gelSite": rng.randint(0, 2)}))
    elif template == "patch_e2":
        apply_at = now - rng.uniform(24.0, 240.0)
        extras = {
            "releaseRate": round(rng.uniform(25.0, 100.0), 2),
            "patchWear": rng.choice([24.0, 84.0, 168.0]),
        }
        ev.append(event("patchApply", "E2", rng.choice([0.78, 1.56, 2.0]),
                        apply_at, "e0", extras))
        if rng.random() < 0.50:
            ev.append(event("patchRemove", "E2", 0.0,
                            now - rng.uniform(1.0, 24.0), "e1"))
    elif template == "patch_t":
        apply_at = now - rng.uniform(24.0, 240.0)
        extras = {
            "releaseRate": round(rng.uniform(1000.0, 6000.0), 2),
            "patchWear": rng.choice([24.0, 84.0, 168.0]),
        }
        ev.append(event("patchApply", "T", rng.choice([2.0, 4.0, 6.0]),
                        apply_at, "e0", extras))
        if rng.random() < 0.50:
            ev.append(event("patchRemove", "T", 0.0,
                            now - rng.uniform(1.0, 24.0), "e1"))
    elif template == "injection_e2":
        ester = rng.choice(E2_ESTERS)
        dose = rng.uniform(0.5, 5.0) if ester == "E2" else rng.uniform(2.0, 20.0)
        if rng.random() < 0.50:
            ev.append(event("injection", ester, dose, now - 480.0, "e0"))
        ev.append(event("injection", ester, dose, now - 240.0, "e1"))
    elif template == "injection_t":
        ester = rng.choice(T_ESTERS)
        dose = rng.uniform(50.0, 200.0) if ester == "T" else rng.uniform(50.0, 1000.0)
        if rng.random() < 0.50:
            ev.append(event("injection", ester, dose, now - 480.0, "e0"))
        ev.append(event("injection", ester, dose, now - 240.0, "e1"))
    elif template == "t_nasal":
        ev.append(event("nasal", "T", rng.uniform(2.0, 20.0),
                        now - rng.uniform(1.0, 24.0), "e0"))
    elif template == "t_sc":
        ester = rng.choice(["TC", "TE", "TU"])
        ev.append(event("sc", ester, rng.uniform(50.0, 500.0),
                        now - rng.uniform(24.0, 480.0), "e0"))
    elif template == "t_pellet":
        ev.append(event("pellet", "T", rng.uniform(25.0, 100.0),
                        now - rng.uniform(24.0, 240.0), "e0",
                        {"releaseRate": round(rng.uniform(2000.0, 8000.0), 2)}))
    elif template == "cpa":
        ev.append(event("oral", "E2", rng.uniform(1.0, 4.0), now - 24.0, "e0"))
        ev.append(event("oral", "CPA", rng.uniform(25.0, 100.0), now - 24.0, "e1"))
    elif template == "mixed":
        ev.append(event("oral", "E2", rng.uniform(1.0, 4.0), now - 24.0, "e0"))
        ev.append(event("gel", "T", rng.uniform(10.0, 100.0), now - 12.0, "e1",
                        {"gelSite": rng.randint(0, 2)}))
        if rng.random() < 0.50:
            ev.append(event("injection", "TC", rng.uniform(50.0, 400.0),
                            now - 240.0, "e2"))
    elif template == "unsorted":
        ev = [
            event("gel", "T", rng.uniform(10.0, 80.0), now - 12.0, "e0",
                  {"gelSite": rng.randint(0, 2)}),
            event("oral", "E2", rng.uniform(1.0, 4.0), now - 24.0, "e1"),
            event("sublingual", "E2", rng.uniform(0.5, 3.0), now - 6.0, "e2",
                  sublingual_extras(rng)),
        ]
        rng.shuffle(ev)
    else:
        raise ValueError(template)

    if template != "unsorted":
        ev.sort(key=lambda e: e["time_h"])
    meta.update({
        "index": index,
        "id": f"s{index:05d}",
        "template": template,
        "weight_kg": pick_weight(rng),
        "birth_year": pick_birth_year(rng),
        "now_h": now,
        "events": ev,
    })
    return meta


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--count", type=int, default=DEFAULT_COUNT)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    templates = [
        "oral_e2", "oral_ester", "sublingual_e2", "sublingual_ester",
        "gel_e2", "gel_ev", "gel_t", "patch_e2", "patch_t",
        "injection_e2", "injection_t", "t_nasal", "t_sc", "t_pellet",
        "cpa", "mixed", "unsorted",
    ]
    scenarios = [
        make_scenario(rng, i, templates[i % len(templates)])
        for i in range(args.count)
    ]

    args.out_dir.mkdir(parents=True, exist_ok=True)
    json_path = args.out_dir / "scenarios.json"
    tsv_path = args.out_dir / "scenarios.tsv"

    with json_path.open("w", encoding="utf-8") as f:
        json.dump({"now_h": NOW_H, "count": len(scenarios),
                   "seed": args.seed, "scenarios": scenarios}, f)

    with tsv_path.open("w", encoding="utf-8") as f:
        f.write("# oyama-pk-crossval-v1\n")
        for s in scenarios:
            birth = "none" if s["birth_year"] == 0 else str(s["birth_year"])
            f.write(f"SCENARIO\t{s['id']}\t{s['weight_kg']}\t{birth}\t{s['now_h']}\n")
            for e in s["events"]:
                extras = ";".join(f"{k}={v}" for k, v in e["extras"].items()) or "-"
                f.write("\t".join([
                    "EVENT", e["route"], e["ester"], f"{e['dose_mg']:.17g}",
                    f"{e['time_h']:.17g}", e["id"], extras,
                ]) + "\n")

    print(f"wrote {len(scenarios)} scenarios to {json_path} and {tsv_path}")


if __name__ == "__main__":
    main()
