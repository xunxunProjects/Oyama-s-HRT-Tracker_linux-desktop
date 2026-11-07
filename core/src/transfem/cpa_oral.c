#include "../pk_internal.h"
#include "../pk_common.h"
#include "oyama/pk.h"

#include <math.h>
#include <string.h>

#define CPA_ORAL_F 0.88
#define CPA_ORAL_KA 0.5
#define CPA_ORAL_ALPHA 0.25
#define CPA_ORAL_WFAST 5.2786

static int owns(oyama_pk_route route, oyama_pk_ester ester)
{
    if (route != OYAMA_PK_ROUTE_ORAL) return 0;
    return ester == OYAMA_PK_ESTER_CPA;
}

static double bioavailability(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params)
{
    (void)extras;
    (void)params;
    (void)route;
    (void)ester;
    return CPA_ORAL_F;
}

static void resolve_params(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out)
{
    pk_params *o = (pk_params *)out;
    const double beta = log(2.0) / 43.9;
    const double default_k3 = beta;

    (void)birth_year;
    (void)params;
    (void)event;

    memset(o, 0, sizeof *o);
    o->k3 = default_k3;
    o->frac_fast = 1.0;
    o->k1_fast = CPA_ORAL_KA;
    o->F = CPA_ORAL_F;
    o->F_fast = CPA_ORAL_F;
    o->F_slow = CPA_ORAL_F;
    /* CPA uses a two-term absorption; store the fast term's rate in k1_fast and
     * the slow term's in k2? The monolithic model handles CPA specially in
     * event_amount. Keep k2 = 0 here; event_amount detects CPA oral. */
}

const oyama_pk_route_module oyama_pk_module_cpa_oral = {
    owns,
    bioavailability,
    resolve_params,
    NULL
};
