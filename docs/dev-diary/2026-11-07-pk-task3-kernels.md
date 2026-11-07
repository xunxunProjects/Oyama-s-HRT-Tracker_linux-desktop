# Dev Diary — PK Task 3: shared analytical kernels and patch pairing

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md` (Task 3)

Changes:
- Created `core/src/pk_common.c` and `core/src/pk_common.h`.
- Moved analytical kernels (`exp_div_diff1/2`, `analytic2c/3c`, `one_comp_amount`, `cpa_wslow`), patch pairing (`patch_removal_times`, `patch_wear_h`, `max_lifetime_h`, helpers), and event sorting into the common module.
- Updated `CMakeLists.txt` to compile `pk_common.c`.

Verification:
- `pk_smoke_test`: PASS
- compare harness: 413,323 field comparisons, 0 above `1e-6`.
