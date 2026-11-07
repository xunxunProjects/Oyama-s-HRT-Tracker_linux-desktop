#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

static int gel_site_index(const oyama_pk_extras *extras)
{
    long site = (long)round((double)extras->gel_site);
    if (site < 0) site = 0;
    if (site > 2) site = 2;
    return (int)site;
}

static double sublingual_theta(const oyama_pk_extras *extras, const oyama_pk_params *p)
{
    double theta = p->e2_sl_standard;
    if (extras->has_sublingual_theta) {
        if (isfinite(extras->sublingual_theta)) {
            theta = extras->sublingual_theta;
            if (theta < 0.0) theta = 0.0;
            if (theta > 1.0) theta = 1.0;
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

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_SUBLINGUAL) return 0;
    if (oyama_pk_ester_is_testosterone(ester)) return 0;
    if (ester == OYAMA_PK_ESTER_CPA) return 0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 0;
    return oyama_pk_ester_is_estradiol(ester);
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
    default:                 return 272.38;
    }
}

static double get_to_e2_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_E2) return 1.0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 1.0;
    return 272.38 / ester_mw(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    const double theta = sublingual_theta(extras, params);
    (void)route;
    return (theta + (1.0 - theta) * params->e2_oral_bio) * get_to_e2_factor(ester);
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double theta = sublingual_theta(&event->extras, params);
    double k2 = 0.0;
    switch (event->ester) {
    case OYAMA_PK_ESTER_EB: k2 = 0.09; break;
    case OYAMA_PK_ESTER_EV: k2 = 0.07; break;
    case OYAMA_PK_ESTER_EC: k2 = 0.045; break;
    case OYAMA_PK_ESTER_EN: k2 = 0.015; break;
    case OYAMA_PK_ESTER_EU: k2 = 0.012; break;
    default: k2 = 0.0; break;
    }
    const double default_k3 =
        params->e2_kClear *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year, event->time_h);
    const double to_e2 = get_to_e2_factor(event->ester);

    memset(o, 0, sizeof *o);
    o->frac_fast = theta;
    o->k1_fast = 1.8;
    o->k1_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
    o->k2 = k2;
    o->k2_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
    o->k3 = default_k3;
    o->F_fast = to_e2;
    o->F_slow = params->e2_oral_bio * to_e2;
    o->F = theta * o->F_fast + (1.0 - theta) * o->F_slow;
}

const oyama_pk_route_module oyama_pk_module_e2_sublingual = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
