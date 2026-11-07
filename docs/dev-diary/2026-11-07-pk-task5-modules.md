# Dev Diary — PK Task 5: transfeminine and transmasculine route modules

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md` (Task 5/6)

Changes:
- Created route modules under core/src/transfem and core/src/transmasc:
  e2_injection, e2_sublingual, e2_gel, e2_patch, e2_oral, cpa_oral,
  t_injection, t_gel, t_patch, t_oral.
- Registered all modules in pk_dispatch.c via oyama_pk_module_for().
- Each module owns its (route, ester) combinations and provides bioavailability + resolve_params.

Verification:
- pk_smoke_test: PASS
- compare harness: 413,323 field comparisons, 0 above 1e-6.
