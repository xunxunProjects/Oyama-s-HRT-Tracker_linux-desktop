#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

static double ester_mw(oyama_pk_ester ester)
{
    (void)ester;
    return 288.42;
}

static double get_to_t_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_T) return 1.0;
    return 288.42 / ester_mw(ester);
}

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_NASAL) return 0;
    return ester == OYAMA_PK_ESTER_T;
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)route;
    (void)extras;
    (void)params;
    if (ester != OYAMA_PK_ESTER_T) return 0.0;
    return 0.08 * get_to_t_factor(ester);
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double t_k3 =
        params->t_kClear *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_T, birth_year, event->time_h);

    memset(o, 0, sizeof *o);
    o->k3 = t_k3;

    if (event->ester != OYAMA_PK_ESTER_T) return;

    const double F = bioavailability(event->route, event->ester, &event->extras, params);
    o->frac_fast = 1.0;
    o->k1_fast = 2.0;
    o->k2 = 0.0;
    o->F = F;
    o->F_fast = F;
    o->F_slow = F;
}

const oyama_pk_route_module oyama_pk_module_t_nasal = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
