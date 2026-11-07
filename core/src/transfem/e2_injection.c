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
    default:                 return 0.0;
    }
}

static double get_to_e2_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_E2) return 1.0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 1.0;
    return 272.38 / ester_mw(ester);
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

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_INJECTION) return 0;
    if (oyama_pk_ester_is_testosterone(ester)) return 0;
    if (ester == OYAMA_PK_ESTER_CPA) return 0;
    if (oyama_pk_ester_is_unmodelled(ester)) return 0;
    return oyama_pk_ester_is_estradiol(ester);
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)route;
    (void)extras;
    return e2_injection_formation(ester, params) * get_to_e2_factor(ester);}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    double frac_fast = 0.5;
    double k1_fast = 0.1;
    double k1_slow = 0.01;
    double k2 = 0.0;
    const double default_k3 =
        params->e2_kClearInj *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year,
                                           event->time_h);
    const double F = bioavailability(event->route, event->ester,
                                     &event->extras, params);

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

    memset(o, 0, sizeof *o);
    o->frac_fast = frac_fast;
    o->k1_fast = k1_fast;
    o->k1_slow = k1_slow;
    o->k2 = k2;
    o->k3 = default_k3;
    o->F = F;
    o->F_fast = F;
    o->F_slow = F;
}

const oyama_pk_route_module oyama_pk_module_e2_injection = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
