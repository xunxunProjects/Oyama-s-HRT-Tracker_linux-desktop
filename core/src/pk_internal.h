/*
 * Internal PK module ABI.  Not part of the public oyama/pk.h API.
 *
 * This mirrors the upstream `PkRouteModule` interface: one module owns a set
 * of (route, ester) combinations and is responsible for bioavailability,
 * resolved parameters, and the amount curve for that combination.
 */
#ifndef OYAMA_PK_INTERNAL_H
#define OYAMA_PK_INTERNAL_H

#include "oyama/pk.h"

typedef struct oyama_pk_route_module {
    int (*owns)(oyama_pk_route route, oyama_pk_ester ester);
    double (*bioavailability)(oyama_pk_route route, oyama_pk_ester ester,
                              const oyama_pk_extras *extras,
                              const oyama_pk_params *params);
    void (*resolve_params)(const oyama_pk_dose *event, int birth_year,
                           const oyama_pk_params *params, void *out);
    double (*amount)(const oyama_pk_dose *event, const void *params,
                     double wear_h, double t);
} oyama_pk_route_module;

/* Registry lookup; returns NULL when no module owns the combination. */
const oyama_pk_route_module *oyama_pk_module_for(oyama_pk_route route,
                                                oyama_pk_ester ester);

extern const oyama_pk_route_module oyama_pk_module_e2_injection;
extern const oyama_pk_route_module oyama_pk_module_e2_sublingual;
extern const oyama_pk_route_module oyama_pk_module_e2_gel;
extern const oyama_pk_route_module oyama_pk_module_e2_patch;
extern const oyama_pk_route_module oyama_pk_module_e2_oral;
extern const oyama_pk_route_module oyama_pk_module_cpa_oral;
extern const oyama_pk_route_module oyama_pk_module_t_injection;
extern const oyama_pk_route_module oyama_pk_module_t_gel;
extern const oyama_pk_route_module oyama_pk_module_t_patch;
extern const oyama_pk_route_module oyama_pk_module_t_oral;/* Resolved parameters for one event are declared in pk_common.h. */
#endif /* OYAMA_PK_INTERNAL_H */
