# PK model divergences

## Status

After the PK modularization work (plan
`docs/superpowers/plans/2026-11-07-pk-modularization.md`), the C99 core has **no
intentional numerical deviations** from the upstream TypeScript model. The
`tools/compare` harness is the authoritative check and currently reports:

- 453,612 field comparisons across 40 scenarios
- 0 fields above `1e-6`
- worst relative error `~1.12e-15`

## Empirical / uncertain parameters

The following are not divergences from upstream, but are places where the shared
model relies on empirical choices or on sparse literature. They are recorded here
so future work can revisit them without changing the parity contract silently.

1. **E2 gel absorption rate.** `k1_fast = 0.0193 h^-1` (about 36 h) is between
   conflicting label values (EstroGel ~36 h vs. Divigel ~10 h). Product-specific
   absorption is not modelled.
2. **Oral testosterone undecanoate.** Uses a one-compartment approximation with
   `frac_fast = 0`, even though a transit-compartment helper exists upstream.
   This is a deliberate simplification shared with upstream.
3. **E2 injection depot parameters.** `ff_EB/EV/EC/EN/EU`, `k1_fast`, `k1_slow`,
   and `k2` are empirical and have no per-ester PK validation in the literature
   search performed for this review.
4. **Sublingual estradiol.** The theta/tier model (quick/casual/standard/strict)
   is empirical.
5. **Testosterone pellet.** The default release rate `0.05 * F mg/h` is a
   placeholder default when no explicit release rate is supplied.
6. **Bicalutamide.** The optional bicalutamide submodel is implemented upstream
   but is not represented in the C result struct; in the C core it is treated as
   unmodelled unless explicitly enabled, and it does not contribute to the
   compared E2/T/CPA curves.

## Policy

Any future change to one of the above must update both implementations and keep
`tools/compare` at 0 fields above `1e-6`, or be recorded here as an intentional
deviation before merging.
