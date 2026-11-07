# Dev Diary — PK Task 7: public header split

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md` (Task 7)

Changes:
- Split `core/include/oyama/pk.h` into `core/include/oyama/pk/types.h` and
  `core/include/oyama/pk/api.h`.
- `pk.h` is now an umbrella header that includes both.

Verification:
- `pk_smoke_test`: PASS
- compare harness: 413,323 field comparisons, 0 above `1e-6`.
