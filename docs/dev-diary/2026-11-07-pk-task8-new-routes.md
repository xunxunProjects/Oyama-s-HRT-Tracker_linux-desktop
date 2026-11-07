# Dev Diary — PK Task 8: new-route parity (nasal, sc, pellet, bicalutamide)

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md` (Task 8)

Changes:
- Added `OYAMA_PK_ROUTE_NASAL`, `OYAMA_PK_ROUTE_SC`, and `OYAMA_PK_ROUTE_PELLET`
  to `core/include/oyama/pk/types.h`.
- Created route modules `core/src/transmasc/t_nasal.c`, `t_sc.c`, and
  `t_pellet.c`, and registered them in `pk_dispatch.c`.
- Extended `core/src/pk.c` for nasal (one-compartment), sc (two-part depot),
  and pellet (wear-based zero/first-order) amount curves, wear time, and
  max-lifetime handling.
- Updated `tools/compare/dump.c` to parse nasal/sc/pellet routes.
- Extended `tools/compare/upstream-dumper.test.ts` with nasal, sc, pellet, and
  bicalutamide-enabled scenarios.

Verification:
- pk_smoke_test: PASS
- ASan pk_smoke_test: PASS
- compare harness: 453,612 field comparisons, 0 above 1e-6.