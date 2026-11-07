#include "oyama/pk.h"
#include "pk_internal.h"
#include "pk_common.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HOURS_PER_YEAR (365.2425 * 24.0)
#define AGE_REF_YEARS 54.0
#define AGE_MIN_YEARS 19.0
#define AGE_MAX_YEARS 75.0

#define PATCH_MIN_WEAR_H 1.0
#define PATCH_REMOVAL_GRACE_H 24.0
#define PATCH_MAX_OVERWEAR_H 240.0

#define CPA_ORAL_F 0.88
#define CPA_ORAL_KA 0.5
#define CPA_ORAL_ALPHA 0.25
#define CPA_ORAL_WFAST 5.2786

static double clamp(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double clamp01(double v)
{
    return clamp(v, 0.0, 1.0);
}

/* ------------------------------------------------------------------------- */
/* Static drug data                                                          */
/* ------------------------------------------------------------------------- */

int oyama_pk_ester_is_testosterone(oyama_pk_ester ester)
{
    return ester == OYAMA_PK_ESTER_T || ester == OYAMA_PK_ESTER_TC ||
           ester == OYAMA_PK_ESTER_TE || ester == OYAMA_PK_ESTER_TU;
}

int oyama_pk_ester_is_unmodelled(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_E2:
    case OYAMA_PK_ESTER_EB:
    case OYAMA_PK_ESTER_EV:
    case OYAMA_PK_ESTER_EC:
    case OYAMA_PK_ESTER_EN:
    case OYAMA_PK_ESTER_EU:
    case OYAMA_PK_ESTER_CPA:
    case OYAMA_PK_ESTER_T:
    case OYAMA_PK_ESTER_TC:
    case OYAMA_PK_ESTER_TE:
    case OYAMA_PK_ESTER_TU:
        return 0;
    default:
        return 1;
    }
}

int oyama_pk_ester_is_estradiol(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_E2:
    case OYAMA_PK_ESTER_EB:
    case OYAMA_PK_ESTER_EV:
    case OYAMA_PK_ESTER_EC:
    case OYAMA_PK_ESTER_EN:
    case OYAMA_PK_ESTER_EU:
        return 1;
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Parameters                                                                */
/* ------------------------------------------------------------------------- */

void oyama_pk_params_defaults(oyama_pk_params *out)
{
    if (out == NULL) return;
    out->e2_kClear = 0.41;
    out->e2_kClearInj = 0.041;
    out->e2_ff_EB = 0.1092;
    out->e2_ff_EV = 0.0623;
    out->e2_ff_EC = 0.1173;
    out->e2_ff_EN = 0.12;
    out->e2_ff_EU = 0.04;
    out->e2_oral_bio = 0.03;
    out->e2_sl_quick = 0.01;
    out->e2_sl_casual = 0.04;
    out->e2_sl_standard = 0.11;
    out->e2_sl_strict = 0.18;
    out->e2_gel_arm = 0.05;
    out->e2_gel_thigh = 0.05;
    out->e2_gel_scrotal = 0.25;
    out->t_kClear = 0.5;
    out->t_kClearInj = 0.035;
    out->t_ff_TC = 0.043;
    out->t_ff_TE = 0.043;
    out->t_ff_TU = 0.043;
    out->t_gel_arm = 0.1;
    out->t_gel_thigh = 0.1;
    out->t_gel_scrotal = 0.5;
}

static double sanitize_value(double value, double fallback, double lo, double hi)
{
    if (!isfinite(value)) return fallback;
    return clamp(value, lo, hi);
}

static double sanitize_t_ff(double value, double fallback)
{
    if (!isfinite(value)) return fallback;
    /* Values written by older builds were never a deliberate choice. */
    if (value == 0.025) return fallback;
    return clamp(value, 0.0, 1.0);
}

void oyama_pk_params_sanitize(const oyama_pk_params *in, oyama_pk_params *out)
{
    oyama_pk_params defaults;

    if (out == NULL) return;
    oyama_pk_params_defaults(&defaults);
    if (in == NULL) {
        *out = defaults;
        return;
    }

    out->e2_kClear = sanitize_value(in->e2_kClear, defaults.e2_kClear, 0.001, 5.0);
    out->e2_kClearInj = sanitize_value(in->e2_kClearInj, defaults.e2_kClearInj, 0.001, 1.0);
    out->e2_ff_EB = sanitize_value(in->e2_ff_EB, defaults.e2_ff_EB, 0.0, 1.0);
    out->e2_ff_EV = sanitize_value(in->e2_ff_EV, defaults.e2_ff_EV, 0.0, 1.0);
    out->e2_ff_EC = sanitize_value(in->e2_ff_EC, defaults.e2_ff_EC, 0.0, 1.0);
    out->e2_ff_EN = sanitize_value(in->e2_ff_EN, defaults.e2_ff_EN, 0.0, 1.0);
    out->e2_ff_EU = sanitize_value(in->e2_ff_EU, defaults.e2_ff_EU, 0.0, 1.0);
    out->e2_oral_bio = sanitize_value(in->e2_oral_bio, defaults.e2_oral_bio, 0.0, 1.0);
    out->e2_sl_quick = sanitize_value(in->e2_sl_quick, defaults.e2_sl_quick, 0.0, 1.0);
    out->e2_sl_casual = sanitize_value(in->e2_sl_casual, defaults.e2_sl_casual, 0.0, 1.0);
    out->e2_sl_standard = sanitize_value(in->e2_sl_standard, defaults.e2_sl_standard, 0.0, 1.0);
    out->e2_sl_strict = sanitize_value(in->e2_sl_strict, defaults.e2_sl_strict, 0.0, 1.0);
    out->e2_gel_arm = sanitize_value(in->e2_gel_arm, defaults.e2_gel_arm, 0.0, 1.0);
    out->e2_gel_thigh = sanitize_value(in->e2_gel_thigh, defaults.e2_gel_thigh, 0.0, 1.0);
    out->e2_gel_scrotal = sanitize_value(in->e2_gel_scrotal, defaults.e2_gel_scrotal, 0.0, 1.0);
    out->t_kClear = sanitize_value(in->t_kClear, defaults.t_kClear, 0.001, 5.0);
    out->t_kClearInj = sanitize_value(in->t_kClearInj, defaults.t_kClearInj, 0.001, 1.0);
    out->t_ff_TC = sanitize_t_ff(in->t_ff_TC, defaults.t_ff_TC);
    out->t_ff_TE = sanitize_t_ff(in->t_ff_TE, defaults.t_ff_TE);
    out->t_ff_TU = sanitize_t_ff(in->t_ff_TU, defaults.t_ff_TU);
    out->t_gel_arm = sanitize_value(in->t_gel_arm, defaults.t_gel_arm, 0.0, 1.0);
    out->t_gel_thigh = sanitize_value(in->t_gel_thigh, defaults.t_gel_thigh, 0.0, 1.0);
    out->t_gel_scrotal = sanitize_value(in->t_gel_scrotal, defaults.t_gel_scrotal, 0.0, 1.0);
}

/* ------------------------------------------------------------------------- */
/* Age                                                                       */
/* ------------------------------------------------------------------------- */

double oyama_pk_age_at_h(int birth_year, double time_h)
{
    return 1970.0 + time_h / HOURS_PER_YEAR - ((double)birth_year + 0.5);
}

double oyama_pk_age_clearance_multiplier(oyama_pk_hormone hormone,
                                          int birth_year,
                                          double time_h)
{
    double age;
    double slope;

    if (birth_year == 0) return 1.0;
    age = clamp(oyama_pk_age_at_h(birth_year, time_h), AGE_MIN_YEARS, AGE_MAX_YEARS);
    slope = hormone == OYAMA_PK_HORMONE_T
                ? log(1821.0 / 1390.0) / (67.0 - 27.0)
                : log(1.0 / 0.75) / (79.0 - 27.0);
    return exp(-slope * (age - AGE_REF_YEARS));
}

/* ------------------------------------------------------------------------- */
/* Sublingual hold -> mucosal fraction                                       */
/* ------------------------------------------------------------------------- */

static double sl_theta_model(double hold_min)
{
    const double k_diss = log(2.0) / (5.0 / 60.0);
    const double k_out = 1.8 + 1.8;
    const double t = hold_min / 60.0;

    return (1.8 * k_diss / (k_diss - k_out)) *
           ((1.0 - exp(-k_out * t)) / k_out -
            (1.0 - exp(-k_diss * t)) / k_diss);
}

static double sl_theta_past_tiers(double hold_min, double last_theta)
{
    const double last_hold = 15.0;
    const double base = sl_theta_model(last_hold);
    if (base == 0.0) return 0.0;
    return clamp01(last_theta * sl_theta_model(hold_min) / base);
}

double oyama_pk_sublingual_theta_from_hold(double hold_min,
                                           const oyama_pk_params *params)
{
    oyama_pk_params p;
    double holds[4] = {2.0, 5.0, 10.0, 15.0};
    double thetas[4];
    double h;
    int i;

    if (!(hold_min > 0.0)) return 0.0;
    oyama_pk_params_sanitize(params, &p);
    thetas[0] = p.e2_sl_quick;
    thetas[1] = p.e2_sl_casual;
    thetas[2] = p.e2_sl_standard;
    thetas[3] = p.e2_sl_strict;

    h = hold_min > 1.0 ? hold_min : 1.0;
    if (h >= holds[3]) return sl_theta_past_tiers(h, thetas[3]);

    for (i = 0; i < 3; i++) {
        if (h >= holds[i] && h <= holds[i + 1]) {
            const double t = (h - holds[i]) / (holds[i + 1] - holds[i]);
            return clamp01(thetas[i] + (thetas[i + 1] - thetas[i]) * t);
        }
    }

    {
        const double slope = (thetas[1] - thetas[0]) / (holds[1] - holds[0]);
        return clamp01(thetas[0] + (h - holds[0]) * slope);
    }
}
/* Event amount (uses shared kernels)                                       */
/* ------------------------------------------------------------------------- */

static double event_amount(const oyama_pk_dose *event,
                           const pk_params *p,
                           double wear_h,
                           double t)
{
    const double tau = t - event->time_h;
    if (tau < 0.0) return 0.0;

    switch (event->route) {
    case OYAMA_PK_ROUTE_INJECTION:
    case OYAMA_PK_ROUTE_SC: {
        const double dose_fast = event->dose_mg * p->frac_fast;
        const double dose_slow = event->dose_mg * (1.0 - p->frac_fast);
        if (p->k2 > 0.0) {
            return oyama_pk_common_analytic3c(tau, dose_fast, p->F, p->k1_fast, p->k2, p->k3) +
                   oyama_pk_common_analytic3c(tau, dose_slow, p->F, p->k1_slow, p->k2, p->k3);
        }
        return oyama_pk_common_analytic2c(tau, dose_fast, p->F, p->k1_fast, p->k3) +
               oyama_pk_common_analytic2c(tau, dose_slow, p->F, p->k1_slow, p->k3);
    }

    case OYAMA_PK_ROUTE_GEL:
    case OYAMA_PK_ROUTE_ORAL:
    case OYAMA_PK_ROUTE_NASAL:
        if (event->route == OYAMA_PK_ROUTE_ORAL &&
            event->ester == OYAMA_PK_ESTER_CPA) {
            const double beta = log(2.0) / 43.9;
            return oyama_pk_common_analytic2c(tau, event->dose_mg, CPA_ORAL_F * CPA_ORAL_WFAST,
                              CPA_ORAL_KA, CPA_ORAL_ALPHA) +
                   oyama_pk_common_analytic2c(tau, event->dose_mg, CPA_ORAL_F * oyama_pk_common_cpa_wslow(),
                              CPA_ORAL_KA, beta);
        }
        if (p->k2 > 0.0) {
            return oyama_pk_common_analytic3c(tau, event->dose_mg, p->F, p->k1_fast,
                              p->k2, p->k3);
        }
        return oyama_pk_common_one_comp_amount(tau, event->dose_mg, p);

    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double dose_fast = event->dose_mg * p->frac_fast;
        const double dose_slow = event->dose_mg * (1.0 - p->frac_fast);
        const double fast = p->k2 > 0.0
            ? oyama_pk_common_analytic3c(tau, dose_fast, p->F_fast, p->k1_fast, p->k2, p->k3)
            : oyama_pk_common_analytic2c(tau, dose_fast, p->F_fast, p->k1_fast, p->k3);
        const double slow = p->k2_slow > 0.0
            ? oyama_pk_common_analytic3c(tau, dose_slow, p->F_slow, p->k1_slow, p->k2_slow, p->k3)
            : oyama_pk_common_analytic2c(tau, dose_slow, p->F_slow, p->k1_slow, p->k3);
        return fast + slow;
    }

    case OYAMA_PK_ROUTE_PATCH_APPLY:
    case OYAMA_PK_ROUTE_PELLET:
        if (p->rate_mg_h > 0.0) {
            if (tau <= wear_h) {
                return (p->rate_mg_h / p->k3) * (1.0 - exp(-p->k3 * tau));
            } else {
                const double amt_removal =
                    (p->rate_mg_h / p->k3) * (1.0 - exp(-p->k3 * wear_h));
                return amt_removal * exp(-p->k3 * (tau - wear_h));
            }
        } else {
            const double under = oyama_pk_common_one_comp_amount(tau, event->dose_mg, p);
            if (tau > wear_h) {
                const double at_removal =
                    oyama_pk_common_one_comp_amount(wear_h, event->dose_mg, p);
                return at_removal * exp(-p->k3 * (tau - wear_h));
            }
            return under;
        }

    default:
        return 0.0;
    }
}

/* Simulation                                                                */
/* ------------------------------------------------------------------------- */

typedef struct {
    size_t event_index;
    pk_params params;
    double wear_h;
    double max_lifetime_h;
} event_model;

static int cmp_double(const void *pa, const void *pb)
{
    const double a = *(const double *)pa;
    const double b = *(const double *)pb;
    return (a > b) - (a < b);
}

static int plausible_birth_year(int birth_year, double now_h)
{
    int current_year;
    if (birth_year < 1900) return 0;
    current_year = (int)floor(1970.0 + now_h / HOURS_PER_YEAR);
    return birth_year <= current_year;
}

void oyama_pk_result_init(oyama_pk_result *out,
                          double *time_h,
                          double *conc_pg_ml,
                          double *conc_pg_ml_e2,
                          double *conc_ng_ml_cpa,
                          double *conc_ng_dl_t,
                          size_t capacity)
{
    if (out == NULL) return;
    out->time_h = time_h;
    out->conc_pg_ml = conc_pg_ml;
    out->conc_pg_ml_e2 = conc_pg_ml_e2;
    out->conc_ng_ml_cpa = conc_ng_ml_cpa;
    out->conc_ng_dl_t = conc_ng_dl_t;
    out->capacity = capacity;
    out->length = 0;
    out->auc = 0.0;
}

oyama_pk_status oyama_pk_simulate(const oyama_pk_request *request,
                                  oyama_pk_result *out)
{
    oyama_pk_dose *filtered = NULL;
    oyama_pk_dose *sorted = NULL;
    size_t *order = NULL;
    double *remove_time = NULL;
    event_model *models = NULL;
    size_t *active = NULL;
    double *times = NULL;
    oyama_pk_params params;
    size_t n = 0, n_models = 0;
    size_t i;
    int birth_year = 0;
    double start_time, end_time, total_hours, step_size;
    size_t steps, all_n, unique_n;
    size_t activate_idx = 0, active_n = 0;
    size_t work = 0;
    double auc = 0.0;
    oyama_pk_status status = OYAMA_PK_ERR_INTERNAL;

    if (out == NULL) return OYAMA_PK_ERR_INVALID_ARGUMENT;
    out->length = 0;
    out->auc = 0.0;
    if (request == NULL) return OYAMA_PK_ERR_INVALID_ARGUMENT;
    if (request->event_count == 0 || request->events == NULL) return OYAMA_PK_NO_RESULT;
    if (request->event_count > OYAMA_PK_MAX_EVENTS) {
        return OYAMA_PK_ERR_TOO_MUCH_DATA;
    }
    if (!isfinite(request->body_weight_kg) || request->body_weight_kg <= 0.0) {
        return OYAMA_PK_NO_RESULT;
    }
    if (!isfinite(request->now_h)) return OYAMA_PK_ERR_INVALID_ARGUMENT;

    if (request->event_count > SIZE_MAX / sizeof(oyama_pk_dose)) {
        return OYAMA_PK_ERR_INVALID_ARGUMENT;
    }

    filtered = (oyama_pk_dose *)malloc(request->event_count * sizeof *filtered);
    if (filtered == NULL) return OYAMA_PK_ERR_INTERNAL;

    for (i = 0; i < request->event_count; i++) {
        const oyama_pk_dose *e = &request->events[i];
        if (oyama_pk_ester_is_unmodelled(e->ester)) continue;
        if (!isfinite(e->time_h) || !isfinite(e->dose_mg)) {
            free(filtered);
            return OYAMA_PK_ERR_INVALID_ARGUMENT;
        }
        filtered[n++] = *e;
    }
    if (n == 0) {
        free(filtered);
        return OYAMA_PK_NO_RESULT;
    }

    order = (size_t *)malloc(n * sizeof *order);
    sorted = (oyama_pk_dose *)malloc(n * sizeof *sorted);
    if (order == NULL || sorted == NULL) goto cleanup;
    for (i = 0; i < n; i++) order[i] = i;
    oyama_pk_common_sort_events_by_time(order, n, filtered);
    for (i = 0; i < n; i++) sorted[i] = filtered[order[i]];
    free(filtered);
    filtered = NULL;
    free(order);
    order = NULL;

    oyama_pk_params_sanitize(request->params, &params);
    if (plausible_birth_year(request->birth_year, request->now_h)) {
        birth_year = request->birth_year;
    }

    remove_time = (double *)malloc(n * sizeof *remove_time);
    if (remove_time == NULL) goto cleanup;
    oyama_pk_common_patch_removal_times(sorted, n, remove_time);

    for (i = 0; i < n; i++) {
        if (sorted[i].route != OYAMA_PK_ROUTE_PATCH_REMOVE) n_models++;
    }

    models = (event_model *)malloc((n_models > 0 ? n_models : 1) * sizeof *models);
    if (models == NULL) goto cleanup;

    {
        size_t mi = 0;
        for (i = 0; i < n; i++) {
            double wear = 0.0;
            if (sorted[i].route == OYAMA_PK_ROUTE_PATCH_REMOVE) continue;
            models[mi].event_index = i;
            oyama_pk_resolve_params(&sorted[i], birth_year, &params, &models[mi].params);
            if (sorted[i].route == OYAMA_PK_ROUTE_PATCH_APPLY ||
                sorted[i].route == OYAMA_PK_ROUTE_PELLET) {
                wear = oyama_pk_common_patch_wear_h(i, sorted, remove_time);
            }
            models[mi].wear_h = wear;
            models[mi].max_lifetime_h =
                oyama_pk_common_max_lifetime_h(&models[mi].params, wear,
                               sorted[i].route == OYAMA_PK_ROUTE_PATCH_APPLY ||
                               sorted[i].route == OYAMA_PK_ROUTE_PELLET);
            mi++;
        }
    }

    start_time = sorted[0].time_h - 24.0;
    end_time = sorted[n - 1].time_h + 24.0 * 14.0;
    if (request->now_h + 24.0 > end_time) end_time = request->now_h + 24.0;
    total_hours = end_time - start_time;
    if (!isfinite(total_hours) || total_hours < 0.0) {
        status = OYAMA_PK_ERR_INVALID_ARGUMENT;
        goto cleanup;
    }
    {
        double steps_d = ceil(total_hours);
        if (steps_d < 2000.0) steps_d = 2000.0;
        if (steps_d > (double)OYAMA_PK_MAX_STEPS) steps_d = (double)OYAMA_PK_MAX_STEPS;
        steps = (size_t)steps_d;
    }
    step_size = (end_time - start_time) / (double)(steps - 1);

    if (n_models > (SIZE_MAX - steps - n - 1) / 10) {
        status = OYAMA_PK_ERR_INTERNAL;
        goto cleanup;
    }
    {
        const size_t capacity = steps + n + 10 * n_models + 1;
        if (capacity > SIZE_MAX / sizeof *times) {
            status = OYAMA_PK_ERR_INTERNAL;
            goto cleanup;
        }
        times = (double *)malloc(capacity * sizeof *times);
        if (times == NULL) goto cleanup;
        all_n = 0;
        for (i = 0; i < steps; i++) {
            times[all_n++] = start_time + (double)i * step_size;
        }
        for (i = 0; i < n; i++) times[all_n++] = sorted[i].time_h;
        {
            static const double offsets[10] = {0.25, 0.5, 1, 2, 4, 6, 8, 12, 24, 48};
            size_t mi;
            for (mi = 0; mi < n_models; mi++) {
                size_t oi;
                const double event_time = sorted[models[mi].event_index].time_h;
                for (oi = 0; oi < 10; oi++) {
                    double t;
                    if (offsets[oi] > models[mi].max_lifetime_h) break;
                    t = event_time + offsets[oi];
                    if (t >= start_time && t <= end_time) times[all_n++] = t;
                }
            }
        }
        if (request->now_h >= start_time && request->now_h <= end_time) {
            times[all_n++] = request->now_h;
        }

        qsort(times, all_n, sizeof *times, cmp_double);
        unique_n = 0;
        for (i = 0; i < all_n; i++) {
            if (unique_n == 0 || times[i] != times[unique_n - 1]) {
                times[unique_n++] = times[i];
            }
        }
        all_n = unique_n;

        if (out->capacity < all_n) {
            out->length = all_n;
            status = OYAMA_PK_ERR_BUFFER_TOO_SMALL;
            goto cleanup;
        }
        if (out->time_h == NULL || out->conc_pg_ml == NULL ||
            out->conc_pg_ml_e2 == NULL || out->conc_ng_ml_cpa == NULL ||
            out->conc_ng_dl_t == NULL) {
            status = OYAMA_PK_ERR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }

    active = (size_t *)malloc((n_models > 0 ? n_models : 1) * sizeof *active);
    if (active == NULL) goto cleanup;

    for (i = 0; i < all_n; i++) {
        const double t = times[i];
        size_t j;

        while (activate_idx < n_models &&
               sorted[models[activate_idx].event_index].time_h <= t) {
            active[active_n++] = activate_idx++;
        }
        for (j = 0; j < active_n;) {
            const event_model *m = &models[active[j]];
            if (work >= OYAMA_PK_MAX_WORK) {
                status = OYAMA_PK_ERR_TOO_MUCH_DATA;
                goto cleanup;
            }
            work++;
            if (t - sorted[m->event_index].time_h > m->max_lifetime_h) {
                active[j] = active[active_n - 1];
                active_n--;
            } else {
                j++;
            }
        }

        {
            double total_e2 = 0.0, total_cpa = 0.0, total_t = 0.0;
            for (j = 0; j < active_n; j++) {
                const event_model *m = &models[active[j]];
                const oyama_pk_dose *e = &sorted[m->event_index];
                double amount;
                if (work >= OYAMA_PK_MAX_WORK) {
                    status = OYAMA_PK_ERR_TOO_MUCH_DATA;
                    goto cleanup;
                }
                work++;
                amount = event_amount(e, &m->params, m->wear_h, t);
                if (e->ester == OYAMA_PK_ESTER_CPA) total_cpa += amount;
                else if (oyama_pk_ester_is_testosterone(e->ester)) total_t += amount;
                else total_e2 += amount;
            }

            out->time_h[i] = t;
            out->conc_pg_ml_e2[i] = (total_e2 * 1e9) / (2.0 * request->body_weight_kg * 1000.0);
            out->conc_ng_ml_cpa[i] = (total_cpa * 1e6) / (14.0 * request->body_weight_kg * 1000.0);
            out->conc_ng_dl_t[i] = (total_t * 1e8) / (1.0 * request->body_weight_kg * 1000.0);
            out->conc_pg_ml[i] = out->conc_pg_ml_e2[i] + out->conc_ng_ml_cpa[i] * 1000.0;

            if (i > 0) {
                const double dt = t - times[i - 1];
                auc += 0.5 * (out->conc_pg_ml_e2[i] + out->conc_pg_ml_e2[i - 1]) * dt;
            }
        }
    }

    out->length = all_n;
    out->auc = auc;
    status = OYAMA_PK_OK;

cleanup:
    free(filtered);
    free(order);
    free(sorted);
    free(remove_time);
    free(models);
    free(active);
    free(times);
    if (status == OYAMA_PK_ERR_TOO_MUCH_DATA) {
        out->length = 0;
        out->auc = 0.0;
    }
    return status;
}

/* ------------------------------------------------------------------------- */
/* Interpolation                                                             */
/* ------------------------------------------------------------------------- */

static double interpolate(const double *times, const double *values,
                          size_t n, double hour)
{
    size_t low, high;

    if (n == 0 || times == NULL || values == NULL) return NAN;
    if (hour <= times[0]) return values[0];
    if (hour >= times[n - 1]) return values[n - 1];

    low = 0;
    high = n - 1;
    while (high - low > 1) {
        const size_t mid = low + (high - low) / 2;
        if (times[mid] == hour) return values[mid];
        if (times[mid] < hour) low = mid;
        else high = mid;
    }
    {
        const double t0 = times[low], t1 = times[high];
        const double c0 = values[low], c1 = values[high];
        if (t1 == t0) return c0;
        return c0 + (c1 - c0) * ((hour - t0) / (t1 - t0));
    }
}

double oyama_pk_interpolate_e2(const oyama_pk_result *sim, double hour_h)
{
    if (sim == NULL) return NAN;
    return interpolate(sim->time_h, sim->conc_pg_ml_e2, sim->length, hour_h);
}

double oyama_pk_interpolate_cpa(const oyama_pk_result *sim, double hour_h)
{
    if (sim == NULL) return NAN;
    return interpolate(sim->time_h, sim->conc_ng_ml_cpa, sim->length, hour_h);
}

double oyama_pk_interpolate_t(const oyama_pk_result *sim, double hour_h)
{
    if (sim == NULL) return NAN;
    return interpolate(sim->time_h, sim->conc_ng_dl_t, sim->length, hour_h);
}
