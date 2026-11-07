#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

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
    if (route != OYAMA_PK_ROUTE_INJECTION) return 0;
    return oyama_pk_ester_is_testosterone(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)extras;
    (void)route;
    if (ester == OYAMA_PK_ESTER_T) return 0.0;
    return t_injection_formation(ester, params) * get_to_t_factor(ester);
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

    if (event->ester == OYAMA_PK_ESTER_T) {
        return;
    }

    double frac_fast = 0.35;
    double k1_fast = 0.025;
    double k1_slow = 0.005;
    double k2 = 0.2;
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
    const double F = bioavailability(event->route, event->ester, &event->extras, params);

    o->frac_fast = frac_fast;
    o->k1_fast = k1_fast;
    o->k1_slow = k1_slow;
    o->k2 = k2;
    o->k3 = t_k3;
    o->F = F;
    o->F_fast = F;
    o->F_slow = F;
}

const oyama_pk_route_module oyama_pk_module_t_injection = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
