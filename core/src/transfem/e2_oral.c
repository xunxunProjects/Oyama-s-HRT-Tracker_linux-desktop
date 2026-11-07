#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

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
    if (route != OYAMA_PK_ROUTE_ORAL) return 0;
    if (oyama_pk_ester_is_testosterone(ester)) return 0;
    if (ester == OYAMA_PK_ESTER_CPA) return 0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 0;
    return oyama_pk_ester_is_estradiol(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)extras;
    (void)route;
    return params->e2_oral_bio * get_to_e2_factor(ester);
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double default_k3 =
        params->e2_kClear *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year, event->time_h);
    const double to_e2 = get_to_e2_factor(event->ester);

    memset(o, 0, sizeof *o);
    o->frac_fast = 1.0;
    o->k1_fast = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
    o->k2 = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
    o->k3 = default_k3;
    o->F = params->e2_oral_bio * to_e2;
    o->F_fast = o->F;
    o->F_slow = o->F;
}

const oyama_pk_route_module oyama_pk_module_e2_oral = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
