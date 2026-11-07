#include "pk_internal.h"
#include "pk_common.h"
#include "oyama/pk.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CPA_ORAL_F 0.88
#define CPA_ORAL_KA 0.5
#define CPA_ORAL_ALPHA 0.25

static double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static double ester_mw(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_E2:  return 272.38;
    case OYAMA_PK_ESTER_EB:  return 376.50;
    case OYAMA_PK_ESTER_EV:  return 356.50;
    case OYAMA_PK_ESTER_EC:  return 396.58;
    case OYAMA_PK_ESTER_EN:  return 384.56;
    case OYAMA_PK_ESTER_EU:  return 440.66;
    case OYAMA_PK_ESTER_CPA: return 416.94;
    case OYAMA_PK_ESTER_T:   return 288.42;
    case OYAMA_PK_ESTER_TC:  return 412.60;
    case OYAMA_PK_ESTER_TE:  return 400.59;
    case OYAMA_PK_ESTER_TU:  return 456.70;
    default:                 return 0.0;
    }
}

static double get_to_e2_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_E2) return 1.0;
    if (ester == OYAMA_PK_ESTER_CPA || oyama_pk_ester_is_unmodelled(ester)) return 1.0;
    if (oyama_pk_ester_is_testosterone(ester)) {
        return 288.42 / ester_mw(ester);
    }
    return 272.38 / ester_mw(ester);
}


/* ------------------------------------------------------------------------- */
/* Internal resolved parameters                                              */
/* ------------------------------------------------------------------------- */

static int gel_site_index(const oyama_pk_extras *extras)
{
    long site = (long)round((double)extras->gel_site);
    if (site < 0) site = 0;
    if (site > 2) site = 2;
    return (int)site;
}

static double sublingual_theta(const oyama_pk_extras *extras,
                               const oyama_pk_params *p)
{
    double theta = p->e2_sl_standard;

    if (extras->has_sublingual_theta) {
        if (isfinite(extras->sublingual_theta)) {
            theta = clamp01(extras->sublingual_theta);
        }
        return theta;
    }
    if (extras->has_sublingual_tier) {
        long tier = (long)extras->sublingual_tier;
        if (tier < 0) tier = 0;
        if (tier > 3) tier = 3;
        if (tier == 0) theta = p->e2_sl_quick;
        else if (tier == 1) theta = p->e2_sl_casual;
        else if (tier == 2) theta = p->e2_sl_standard;
        else theta = p->e2_sl_strict;
    }
    return theta;
}

static double e2_injection_formation(oyama_pk_ester ester,
                                     const oyama_pk_params *p)
{
    switch (ester) {
    case OYAMA_PK_ESTER_EB: return p->e2_ff_EB;
    case OYAMA_PK_ESTER_EV: return p->e2_ff_EV;
    case OYAMA_PK_ESTER_EC: return p->e2_ff_EC;
    case OYAMA_PK_ESTER_EN: return p->e2_ff_EN;
    case OYAMA_PK_ESTER_EU: return p->e2_ff_EU;
    case OYAMA_PK_ESTER_E2: return 1.0;
    default: return 0.08;
    }
}

static double t_injection_formation(oyama_pk_ester ester,
                                    const oyama_pk_params *p)
{
    switch (ester) {
    case OYAMA_PK_ESTER_TC: return p->t_ff_TC;
    case OYAMA_PK_ESTER_TE: return p->t_ff_TE;
    case OYAMA_PK_ESTER_TU: return p->t_ff_TU;
    default: return p->t_ff_TE;
    }
}

static double bioavailability(oyama_pk_route route,
                               oyama_pk_ester ester,
                               const oyama_pk_extras *extras,
                               const oyama_pk_params *p)
{
    const double mw_factor = get_to_e2_factor(ester);

    if (ester == OYAMA_PK_ESTER_CPA) {
        return route == OYAMA_PK_ROUTE_ORAL ? CPA_ORAL_F : 0.0;
    }

    if (oyama_pk_ester_is_testosterone(ester)) {
        switch (route) {
        case OYAMA_PK_ROUTE_INJECTION:
            if (ester == OYAMA_PK_ESTER_T) return 0.0;
            return t_injection_formation(ester, p) * mw_factor;
        case OYAMA_PK_ROUTE_GEL: {
            const int site = gel_site_index(extras);
            const double bio = site == 0 ? p->t_gel_arm
                             : site == 1 ? p->t_gel_thigh
                                         : p->t_gel_scrotal;
            return bio * mw_factor;
        }
        case OYAMA_PK_ROUTE_PATCH_APPLY:
            return 1.0 * mw_factor;
        default:
            return 0.0;
        }
    }

    switch (route) {
    case OYAMA_PK_ROUTE_INJECTION:
        return e2_injection_formation(ester, p) * mw_factor;
    case OYAMA_PK_ROUTE_ORAL:
        return p->e2_oral_bio * mw_factor;
    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double theta = sublingual_theta(extras, p);
        return (theta + (1.0 - theta) * p->e2_oral_bio) * mw_factor;
    }
    case OYAMA_PK_ROUTE_GEL: {
        const int site = gel_site_index(extras);
        const double bio = site == 0 ? p->e2_gel_arm
                         : site == 1 ? p->e2_gel_thigh
                                     : p->e2_gel_scrotal;
        return bio * mw_factor;
    }
    case OYAMA_PK_ROUTE_PATCH_APPLY:
        return 1.0 * mw_factor;
    default:
        return 0.0;
    }
}

static void zero_params(pk_params *out, double k3)
{
    memset(out, 0, sizeof *out);
    out->k3 = k3;
}

void oyama_pk_resolve_params(const oyama_pk_dose *event,
                           int birth_year,
                           const oyama_pk_params *p,
                           pk_params *out)
{
    const oyama_pk_route_module *mod = oyama_pk_module_for(event->route, event->ester);
    if (mod != NULL) {
        mod->resolve_params(event, birth_year, p, out);
        return;
    }
    /* Every route sets only the fields it uses; k2_slow is optional upstream.
     * Clear first so a previous event's optional rate cannot leak into this
     * one's max-lifetime calculation. */
    memset(out, 0, sizeof *out);

    const double default_k3 =
        (event->route == OYAMA_PK_ROUTE_INJECTION ? p->e2_kClearInj : p->e2_kClear) *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year,
                                           event->time_h);
    const double to_e2 = get_to_e2_factor(event->ester);

    if (oyama_pk_ester_is_testosterone(event->ester)) {
        const double t_k3 =
            (event->route == OYAMA_PK_ROUTE_INJECTION ? p->t_kClearInj : p->t_kClear) *
            oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_T, birth_year,
                                               event->time_h);
        switch (event->route) {
        case OYAMA_PK_ROUTE_INJECTION: {
            double frac_fast, k1_fast, k1_slow, k2, F;
            if (event->ester == OYAMA_PK_ESTER_T) {
                zero_params(out, t_k3);
                return;
            }
            frac_fast = 0.35;
            k1_fast = 0.025;
            k1_slow = 0.005;
            k2 = 0.2;
            if (event->ester == OYAMA_PK_ESTER_TE) {
                frac_fast = 0.4;
                k1_fast = 0.035;
                k1_slow = 0.008;
            } else if (event->ester == OYAMA_PK_ESTER_TU) {
                frac_fast = 0.0;
                k1_fast = log(2.0) / (50.0 * 24.0);
                k1_slow = log(2.0) / (50.0 * 24.0);
                k2 = 0.036;
            }
            F = bioavailability(event->route, event->ester, &event->extras, p);
            out->frac_fast = frac_fast;
            out->k1_fast = k1_fast;
            out->k1_slow = k1_slow;
            out->k2 = k2;
            out->k3 = t_k3;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            return;
        }
        case OYAMA_PK_ROUTE_GEL: {
            const double F = bioavailability(event->route, event->ester,
                                             &event->extras, p);
            zero_params(out, t_k3);
            out->frac_fast = 1.0;
            out->k1_fast = 0.05;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            return;
        }
        case OYAMA_PK_ROUTE_PATCH_APPLY: {
            const double F = bioavailability(event->route, event->ester,
                                             &event->extras, p);
            double rate = 0.0;
            if (event->extras.has_release_rate_ug_per_day &&
                isfinite(event->extras.release_rate_ug_per_day) &&
                event->extras.release_rate_ug_per_day > 0.0) {
                rate = (event->extras.release_rate_ug_per_day / 24.0 / 1000.0) * F;
            }
            zero_params(out, t_k3);
            out->frac_fast = 1.0;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            if (rate > 0.0) {
                out->rate_mg_h = rate;
            } else {
                out->k1_fast = 0.03;
            }
            return;
        }
        case OYAMA_PK_ROUTE_PATCH_REMOVE:
        default:
            zero_params(out, t_k3);
            return;
        }
    }

    if (event->ester == OYAMA_PK_ESTER_CPA &&
        event->route != OYAMA_PK_ROUTE_ORAL) {
        zero_params(out, default_k3);
        return;
    }

    switch (event->route) {
    case OYAMA_PK_ROUTE_INJECTION: {
        double frac_fast = 0.5;
        double k1_fast = 0.1;
        double k1_slow = 0.01;
        double k2 = 0.0;
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
        switch (event->ester) {
        case OYAMA_PK_ESTER_EB:
            frac_fast = 0.9; k1_fast = 0.144; k1_slow = 0.114; k2 = 0.09; break;
        case OYAMA_PK_ESTER_EV:
            frac_fast = 0.4; k1_fast = 0.0216; k1_slow = 0.0138; k2 = 0.07; break;
        case OYAMA_PK_ESTER_EC:
            frac_fast = 0.229164549; k1_fast = 0.005035046;
            k1_slow = 0.004510574; k2 = 0.045; break;
        case OYAMA_PK_ESTER_EN:
            frac_fast = 0.05; k1_fast = 0.001; k1_slow = 0.005; k2 = 0.015; break;
        case OYAMA_PK_ESTER_EU:
            frac_fast = 0.08; k1_fast = 0.006; k1_slow = 0.0022; k2 = 0.012; break;
        case OYAMA_PK_ESTER_E2:
            frac_fast = 1.0; k1_fast = 0.5; k1_slow = 0.0; k2 = 0.0; break;
        default:
            break;
        }
        out->frac_fast = frac_fast;
        out->k1_fast = k1_fast;
        out->k1_slow = k1_slow;
        out->k2 = k2;
        out->k3 = default_k3;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        return;
    }

    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double theta = sublingual_theta(&event->extras, p);
        double k2 = 0.0;
        switch (event->ester) {
        case OYAMA_PK_ESTER_EB: k2 = 0.09; break;
        case OYAMA_PK_ESTER_EV: k2 = 0.07; break;
        case OYAMA_PK_ESTER_EC: k2 = 0.045; break;
        case OYAMA_PK_ESTER_EN: k2 = 0.015; break;
        case OYAMA_PK_ESTER_EU: k2 = 0.012; break;
        default: k2 = 0.0; break;
        }
        out->frac_fast = theta;
        out->k1_fast = 1.8;
        out->k1_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
        out->k2 = k2;
        out->k2_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
        out->k3 = default_k3;
        out->F_fast = to_e2;
        out->F_slow = p->e2_oral_bio * to_e2;
        out->F = theta * out->F_fast + (1.0 - theta) * out->F_slow;
        return;
    }

    case OYAMA_PK_ROUTE_GEL: {
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
        zero_params(out, default_k3);
        out->frac_fast = 1.0;
        out->k1_fast = 0.0193;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        return;
    }

    case OYAMA_PK_ROUTE_PATCH_APPLY: {
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
        double rate = 0.0;
        if (event->extras.has_release_rate_ug_per_day &&
            isfinite(event->extras.release_rate_ug_per_day) &&
            event->extras.release_rate_ug_per_day > 0.0) {
            rate = (event->extras.release_rate_ug_per_day / 24.0 / 1000.0) * F;
        }
        zero_params(out, default_k3);
        out->frac_fast = 1.0;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        if (rate > 0.0) {
            out->rate_mg_h = rate;
        } else {
            out->k1_fast = 0.0075;
        }
        return;
    }

    case OYAMA_PK_ROUTE_PATCH_REMOVE:
        zero_params(out, default_k3);
        return;

    case OYAMA_PK_ROUTE_ORAL: {
        if (event->ester == OYAMA_PK_ESTER_CPA) {
            const double beta = log(2.0) / 43.9;
            zero_params(out, beta);
            out->frac_fast = 1.0;
            out->k1_fast = CPA_ORAL_KA;
            out->F = CPA_ORAL_F;
            out->F_fast = CPA_ORAL_F;
            out->F_slow = CPA_ORAL_F;
            return;
        }
        out->frac_fast = 1.0;
        out->k1_fast = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
        out->k2 = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
        out->k3 = default_k3;
        out->F = p->e2_oral_bio * to_e2;
        out->F_fast = out->F;
        out->F_slow = out->F;
        return;
    }
    }

    zero_params(out, default_k3);
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

static const oyama_pk_route_module *const OYAMA_PK_MODULES[] = {
    &oyama_pk_module_e2_injection,
    &oyama_pk_module_e2_sublingual,
    &oyama_pk_module_e2_gel,
    &oyama_pk_module_e2_patch,
    &oyama_pk_module_e2_oral,
    &oyama_pk_module_cpa_oral,
    &oyama_pk_module_t_injection,
    &oyama_pk_module_t_sc,
    &oyama_pk_module_t_gel,
    &oyama_pk_module_t_patch,
    &oyama_pk_module_t_oral,
    &oyama_pk_module_t_nasal,
    &oyama_pk_module_t_pellet,
    NULL
};
const oyama_pk_route_module *oyama_pk_module_for(oyama_pk_route route,
                                                oyama_pk_ester ester)
{
    size_t i;
    for (i = 0; OYAMA_PK_MODULES[i] != NULL; i++) {
        if (OYAMA_PK_MODULES[i]->owns(route, ester)) {
            return OYAMA_PK_MODULES[i];
        }
    }
    return NULL;
}
