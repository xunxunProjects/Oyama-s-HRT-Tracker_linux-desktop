# Dev Diary — PK modularization plan

Date: 2026-11-07

Plan: `docs/superpowers/plans/2026-11-07-pk-modularization.md`

Baseline compare harness result before modularization:

- Scenarios: 36
- Field comparisons: 413,323
- Fields above 1e-6: 0
- Worst relative error: ~7.5e-16

The plan splits the monolithic `core/src/pk.c` into upstream-style transfeminine/transmasculine route modules while preserving the public `oyama/pk.h` ABI and numerical parity. Task 1 (baseline + plan) is complete; implementation tasks follow.