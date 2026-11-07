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

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_GEL) return 0;
    if (oyama_pk_ester_is_testosterone(ester)) return 0;
    if (ester == OYAMA_PK_ESTER_CPA) return 0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 0;
    return oyama_pk_ester_is_estradiol(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    const int site = gel_site_index(extras);
    const double bio = site == 0 ? params->e2_gel_arm
                     : site == 1 ? params->e2_gel_thigh
                                 : params->e2_gel_scrotal;
    (void)route;
    return bio * get_to_e2_factor(ester);
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double F = bioavailability(event->route, event->ester,
                                     &event->extras, params);
    const double default_k3 =
        params->e2_kClear *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year, event->time_h);

    memset(o, 0, sizeof *o);
    o->k3 = default_k3;    o->frac_fast = 1.0;
    o->k1_fast = 0.0193;
    o->F = F;
    o->F_fast = F;
    o->F_slow = F;
}

const oyama_pk_route_module oyama_pk_module_e2_gel = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
