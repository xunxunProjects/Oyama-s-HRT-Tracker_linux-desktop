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
    if (route != OYAMA_PK_ROUTE_ORAL) return 0;
    return oyama_pk_ester_is_testosterone(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)extras;
    (void)params;
    (void)route;
    (void)ester;
    return 0.0;
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double t_k3 =
        (event->route == OYAMA_PK_ROUTE_INJECTION ? params->t_kClearInj : params->t_kClear) *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_T, birth_year, event->time_h);

    memset(o, 0, sizeof *o);
    o->k3 = t_k3;
    o->frac_fast = 1.0;
    o->k1_fast = 0.0; /* oral T not modelled beyond zero */
}

const oyama_pk_route_module oyama_pk_module_t_oral = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
