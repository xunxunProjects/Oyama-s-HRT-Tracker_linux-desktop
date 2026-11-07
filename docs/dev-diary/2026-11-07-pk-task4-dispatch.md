# Dev Diary — PK Task 4: dispatch registry

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md` (Task 4)

Changes:
- Created `core/src/pk_dispatch.c` containing `bioavailability()`, `resolve_params()`, and helpers.
- Exposed `oyama_pk_resolve_params()` via `pk_common.h`.
- `pk.c` now calls `oyama_pk_resolve_params()` instead of the inlined resolver.

Verification:
- `pk_smoke_test`: PASS
- compare harness: 413,323 field comparisons, 0 above `1e-6`.
