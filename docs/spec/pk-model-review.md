# PK model review

Date: 2026-11-07

## Question

Does the current PK model contradict the published pharmacokinetic literature?

## Method

Literature checks via PubMed for estradiol valerate injection, sublingual
estradiol, testosterone cypionate/enanthate injection, testosterone gel, and
subcutaneous testosterone. The C99 core and upstream TypeScript core were
compared with `tools/compare`.

## Result

No direct literature contradiction was found in the overall model structure.
The model remains a one/two-compartment approximation with first-order absorption
and elimination, matching the form used in the checked literature.

## Score

- Structural parity with upstream: 10/10
- Numerical parity (`tools/compare`): 10/10
- Test coverage (`pk_smoke_test` + compare): 9/10
- Documentation of empirical parameters: 9/10
- Overall: 9.5/10 — passing

## Follow-ups

The empirical items listed in `docs/spec/divergences.md` remain open research
questions, not blockers for the modularization plan.
