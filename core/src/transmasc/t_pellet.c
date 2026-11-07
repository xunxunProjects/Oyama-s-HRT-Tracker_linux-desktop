#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

static double ester_mw(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_T:   return 288.42;
    case OYAMA_PK_ESTER_TC:  return 412.60;
    case OYAMA_PK_ESTER_TE:  return 400.59;
    case OYAMA_PK_ESTER_TU:  return 456.70;
    default:                 return 288.42;
    }
}

static double get_to_t_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_T) return 1.0;
    return 288.42 / ester_mw(ester);
}

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_PELLET) return 0;
    return oyama_pk_ester_is_testosterone(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)route;
    (void)extras;
    (void)params;
    return get_to_t_factor(ester);
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double t_k3 =
        params->t_kClear *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_T, birth_year, event->time_h);
    const double F = bioavailability(event->route, event->ester, &event->extras, params);

    double rate = 0.0;
    if (event->extras.has_release_rate_ug_per_day &&
        isfinite(event->extras.release_rate_ug_per_day) &&
        event->extras.release_rate_ug_per_day > 0.0) {
        rate = (event->extras.release_rate_ug_per_day / 24.0 / 1000.0) * F;
    } else {
        rate = 0.05 * F;
    }

    memset(o, 0, sizeof *o);
    o->k3 = t_k3;
    o->frac_fast = 1.0;
    o->F = F;
    o->F_fast = F;
    o->F_slow = F;
    if (rate > 0.0) {
        o->rate_mg_h = rate;
    } else {
        o->k1_fast = 0.001;
    }
}

const oyama_pk_route_module oyama_pk_module_t_pellet = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
