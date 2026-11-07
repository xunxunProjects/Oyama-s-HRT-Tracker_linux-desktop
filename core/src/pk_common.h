/*
 * Shared PK helpers: analytical kernels, age/parameter helpers, and patch
 * pairing.  Internal to the C core; not part of oyama/pk.h.
 */
#ifndef OYAMA_PK_COMMON_H
#define OYAMA_PK_COMMON_H

#include "pk_internal.h"
#include "oyama/pk.h"

/* Resolved-parameter struct (shared by pk.c and the route modules). */
typedef struct pk_params {
    double frac_fast;
    double k1_fast;
    double k1_slow;
    double k2;
    double k2_slow;
    double k3;
    double F;
    double rate_mg_h;
    double F_fast;
    double F_slow;
} pk_params;

double oyama_pk_common_exp_div_diff1(double x, double y, double tau);
double oyama_pk_common_exp_div_diff2(double a, double b, double c, double tau);
double oyama_pk_common_analytic2c(double tau, double dose_mg, double F,
                                  double ka, double ke);
double oyama_pk_common_analytic3c(double tau, double dose_mg, double F,
                                  double k1, double k2, double k3);
double oyama_pk_common_one_comp_amount(double tau, double dose_mg,
                                       const pk_params *p);
double oyama_pk_common_cpa_wslow(void);

void oyama_pk_common_patch_removal_times(const oyama_pk_dose *events, size_t n,
                                         double *remove_time);
double oyama_pk_common_patch_wear_h(size_t index, const oyama_pk_dose *events,
                                    const double *remove_time);

double oyama_pk_common_max_lifetime_h(const pk_params *p, double wear_h,
                                      int is_patch);

#endif /* OYAMA_PK_COMMON_H */

void oyama_pk_common_sort_events_by_time(size_t *order, size_t n,
                                         const oyama_pk_dose *events);
