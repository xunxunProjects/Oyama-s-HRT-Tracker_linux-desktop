# PK Core Modularization Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Reorganize the C99 pharmacokinetic core in `core/` so its source tree mirrors the upstream route-module layout (`transfem/*` and `transmasc/*`, one module per drug × route), while preserving the public `oyama/pk.h` ABI and the numerical results validated by `tools/compare`.

**Architecture:** Keep `oyama/pk.h` as the stable public umbrella. Move implementation details behind an internal header, `core/src/pk_internal.h`, that declares a `oyama_pk_route_module` vtable (`owns`, `bioavailability`, `resolve_params`, `amount`). Split `core/src/pk.c` into:
- `core/src/pk_common.c` — shared constants, analytical kernels, age helpers, patch pairing, simulation loop, interpolation.
- `core/src/pk_dispatch.c` — module registry and dispatch.
- `core/src/transfem/*.c` — `e2-injection`, `e2-sublingual`, `e2-gel`, `e2-patch`, `e2-oral`, `cpa-oral`, `bica-oral`.
- `core/src/transmasc/*.c` — `t-injection`, `t-sc`, `t-gel`, `t-patch`, `t-oral`, `t-nasal`, `t-pellet`.
Each task ends with a build, `pk_smoke_test`, the compare harness, a dev diary entry, and a commit.

**Tech Stack:** C99, CMake/Ninja, Qt 6.4+/5.15.2 consumers, Python comparison harness, vitest upstream dumper.

**Spec:** `docs/superpowers/specs/2026-10-29-linux-desktop-design.md` (upstream parity, §PK core); upstream commit `f6e68cd` (route-module refactor).

**Baseline:** `tools/compare` passes on the current tree — 413,323 field comparisons, 0 above `1e-6`, worst relative error `~7.5e-16`.

## Global Constraints

- Public ABI in `core/include/oyama/pk.h` must remain source- and binary-compatible for desktop and CLI callers unless a task explicitly changes it and updates all callers.
- Numerical output must remain within the compare harness tolerance (`1e-6`) after every task; a regression is a task failure.
- Unmodelled drugs stay unmodelled and are filtered exactly as upstream filters them.
- Every task ends with a commit. Do not combine task commits.
- A dev diary entry is added under `docs/dev-diary/` for each completed task, referencing the plan task number and commit SHA.

## Review Focus

1. **ABI stability** — `oyama_pk_simulate`, interpolation, helpers, and enums keep their signatures and values.
2. **Numerical parity** — compare harness stays at 0 fields above `1e-6`.
3. **Module ownership** — every modelled route/ester combination has exactly one owner, matching upstream `moduleFor()`.
4. **Build hygiene** — no new warnings under `-Wall -Wextra -Wpedantic`; ASan build remains green.

---

### Task 1: Baseline capture and plan (done)

**Files:**
- Create: `docs/superpowers/plans/2026-11-07-pk-modularization.md`
- Create: `docs/dev-diary/2026-11-07-pk-modularization-plan.md`

- [x] Run the compare harness on the current tree and record the result.
- [x] Write this plan.
- [x] Add the dev diary entry and commit.

**Verification:** `git status` shows only the plan and diary; `ctest -R pk_smoke_test` passes.

---

### Task 2: Internal module ABI scaffolding

**Files:**
- Create: `core/src/pk_internal.h`
- Modify: `core/src/pk.c` (include the new header; no behavior change)
- Modify: `CMakeLists.txt` (no new sources yet)

**Interfaces:**
```c
typedef struct oyama_pk_route_module {
    int (*owns)(oyama_pk_route route, oyama_pk_ester ester);
    double (*bioavailability)(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params);
    void (*resolve_params)(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out);
    double (*amount)(const oyama_pk_dose *event, const void *params,
                     double wear_h, double t);
} oyama_pk_route_module;
```

- [x] Add the internal header with the vtable and a registry declaration.
- [x] Include it from `pk.c` without changing behavior.
- [x] Build and run `pk_smoke_test`.

**Verification:** build clean; smoke test passes.

---

### Task 3: Extract shared analytical kernels and patch pairing

**Files:**
- Create: `core/src/pk_common.h`
- Create: `core/src/pk_common.c`
- Modify: `core/src/pk.c` (call into common; remove duplicated statics)
- Modify: `CMakeLists.txt`

- [x] Move `exp_div_diff1`, `exp_div_diff2`, `analytic2c`, `analytic3c`, `one_comp_amount`, `cpa_wslow`, `patch_removal_times`, `max_lifetime_h`, age helpers into `pk_common.c`.
- [x] Expose them through `pk_common.h`.
- [x] Build, smoke test, compare harness.

**Verification:** 0 fields above `1e-6`.

---

### Task 4: Dispatch registry

**Files:**
- Create: `core/src/pk_dispatch.c`
- Modify: `core/src/pk_internal.h`
- Modify: `core/src/pk.c`

- [x] Implement `oyama_pk_module_for(route, ester)` mirroring upstream `moduleFor()`.
- [x] Replace the inline `bioavailability()` and `resolve_params()` switches with dispatch calls, keeping the zeroed fallback.
- [x] Build, smoke test, compare harness.

**Verification:** module ownership table matches upstream; 0 fields above `1e-6`.

---

### Task 5: Transfeminine route modules

**Files:**
- Create: `core/src/transfem/e2_injection.c`, `e2_sublingual.c`, `e2_gel.c`, `e2_patch.c`, `e2_oral.c`, `cpa_oral.c`, `bica_oral.c`
- Modify: `CMakeLists.txt`, `pk_dispatch.c`

- [x] Move each route's bioavailability, resolve_params, and amount logic into its module file.
- [x] Register modules in the dispatch table.
- [x] Build, smoke test, compare harness.

**Verification:** transfeminine scenarios unchanged.

---

### Task 6: Transmasculine route modules

**Files:**
- Create: `core/src/transmasc/t_injection.c`, `t_sc.c`, `t_gel.c`, `t_patch.c`, `t_oral.c`, `t_nasal.c`, `t_pellet.c`
- Modify: `CMakeLists.txt`, `pk_dispatch.c`

- [x] Move each route's logic into its module file.
- [x] Register modules in the dispatch table.
- [x] Build, smoke test, compare harness.

**Verification:** transmasculine scenarios unchanged.

---

### Task 7: Header split

**Files:**
- Modify: `core/include/oyama/pk.h`
- Create: `core/include/oyama/pk/transfem.h`, `transmasc.h`, and per-route headers if needed

- [x] Split declarations/documentation by sex and route while keeping `pk.h` as an umbrella include.
- [x] Update consumers if include paths change.
- [x] Build desktop and CLI.

**Verification:** all targets build; smoke test passes.

---

### Task 8: New-route parity (nasal, sc, pellet, bicalutamide)

**Files:**
- Modify: upstream dumper and `tools/compare` scenarios
- Modify: `core/src/transmasc/*.c`, `core/src/transfem/bica_oral.c`

- [x] Add scenarios for nasal, subcutaneous, pellet, and bicalutamide (enabled) routes.
- [x] Implement any missing behavior in the C modules.
- [x] Regenerate compare outputs and verify 0 fields above `1e-6`.

**Verification:** expanded scenario set passes.

---

### Task 9: Final verification and documentation

**Files:**
- Modify: `docs/spec/divergences.md` if any intentional deviation remains
- Modify: `CHANGELOG.md`

- [x] Run full test suite and ASan build.
- [x] Run compare harness on expanded scenarios.
- [x] Update documentation and changelog.
- [x] Commit.

**Verification:** full suite green; compare harness green.