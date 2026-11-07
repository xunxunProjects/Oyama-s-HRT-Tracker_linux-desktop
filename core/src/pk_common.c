#include "pk_common.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

double oyama_pk_common_exp_div_diff1(double x, double y, double tau)
{
    const double z = fabs(y - x) * tau / 2.0;
    double sinhc;

    if (z > 1.0) return (exp(-x * tau) - exp(-y * tau)) / (x - y);
    sinhc = z == 0.0 ? 1.0 : sinh(z) / z;
    return -tau * exp((-(x + y) / 2.0) * tau) * sinhc;
}

double oyama_pk_common_exp_div_diff2(double a, double b, double c, double tau)
{
    double mean, ua, ub, uc;
    double h1 = 1.0, h2 = 1.0, h3 = 1.0;
    double fact = 2.0, sum = 0.5;
    int j;

    if (a > b) { double t = a; a = b; b = t; }
    if (b > c) { double t = b; b = c; c = t; }
    if (a > b) { double t = a; a = b; b = t; }

    if ((c - a) * tau >= 0.5) {
        return (oyama_pk_common_exp_div_diff1(b, c, tau) -
                oyama_pk_common_exp_div_diff1(a, b, tau)) / (c - a);
    }

    mean = (a + b + c) / 3.0;
    ua = (mean - a) * tau;
    ub = (mean - b) * tau;
    uc = (mean - c) * tau;
    for (j = 1; j < 20; j++) {
        h1 *= ua;
        h2 = h1 + ub * h2;
        h3 = h2 + uc * h3;
        fact *= (double)(j + 2);
        sum += h3 / fact;
    }
    return tau * tau * exp(-mean * tau) * sum;
}

double oyama_pk_common_analytic2c(double tau, double dose_mg, double F,
                                  double ka, double ke)
{
    if (ka <= 0.0 || !(dose_mg > 0.0)) return 0.0;
    return -dose_mg * F * ka * oyama_pk_common_exp_div_diff1(ka, ke, tau);
}

double oyama_pk_common_analytic3c(double tau, double dose_mg, double F,
                                  double k1, double k2, double k3)
{
    if (k1 <= 0.0 || !(dose_mg > 0.0)) return 0.0;
    return dose_mg * F * k1 * k2 * oyama_pk_common_exp_div_diff2(k1, k2, k3, tau);
}

double oyama_pk_common_one_comp_amount(double tau, double dose_mg,
                                       const pk_params *p)
{
    return oyama_pk_common_analytic2c(tau, dose_mg, p->F, p->k1_fast, p->k3);
}

double oyama_pk_common_cpa_wslow(void)
{
    const double beta = log(2.0) / 43.9;
    const double exposure_h = 14.0 / (0.0035 * 60.0);
    return (exposure_h - CPA_ORAL_WFAST / CPA_ORAL_ALPHA) * beta;
}

#define PATCH_MIN_WEAR_H 1.0
#define PATCH_REMOVAL_GRACE_H 24.0
#define PATCH_MAX_OVERWEAR_H 240.0

static const char *ester_code(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_E2:  return "E2";
    case OYAMA_PK_ESTER_EB:  return "EB";
    case OYAMA_PK_ESTER_EV:  return "EV";
    case OYAMA_PK_ESTER_EC:  return "EC";
    case OYAMA_PK_ESTER_EN:  return "EN";
    case OYAMA_PK_ESTER_EU:  return "EU";
    case OYAMA_PK_ESTER_CPA: return "CPA";
    case OYAMA_PK_ESTER_SPIRO: return "SPIRO";
    case OYAMA_PK_ESTER_BICA: return "BICA";
    case OYAMA_PK_ESTER_MPA: return "MPA";
    case OYAMA_PK_ESTER_LEUP: return "LEUP";
    case OYAMA_PK_ESTER_TRIP: return "TRIP";
    case OYAMA_PK_ESTER_RLX: return "RLX";
    case OYAMA_PK_ESTER_T:   return "T";
    case OYAMA_PK_ESTER_TC:  return "TC";
    case OYAMA_PK_ESTER_TE:  return "TE";
    case OYAMA_PK_ESTER_TU:  return "TU";
    default:                 return "UNKNOWN";
    }
}

/* Patch pairing                                                             */
/* ------------------------------------------------------------------------- */

enum patch_hormone { PH_NONE = 0, PH_E2 = 1, PH_T = 2 };

static int patch_hormone(const oyama_pk_dose *e)
{
    if (oyama_pk_ester_is_testosterone(e->ester)) return PH_T;
    if (oyama_pk_ester_is_estradiol(e->ester)) return PH_E2;
    return PH_NONE;
}

static int planned_wear_h(const oyama_pk_dose *e, double *out)
{
    if (e->extras.has_patch_wear_h &&
        isfinite(e->extras.patch_wear_h) &&
        e->extras.patch_wear_h > 0.0) {
        *out = e->extras.patch_wear_h;
        return 1;
    }
    return 0;
}

static double release_rate_value(const oyama_pk_dose *e)
{
    if (e->extras.has_release_rate_ug_per_day &&
        isfinite(e->extras.release_rate_ug_per_day)) {
        return e->extras.release_rate_ug_per_day;
    }
    return 0.0;
}

static const char *event_id_string(const oyama_pk_dose *e)
{
    return e->id != NULL ? e->id : "undefined";
}

static void stable_sort_indices(size_t *a, size_t n,
                                int (*cmp)(size_t, size_t, void *),
                                void *ctx)
{
    size_t *tmp;
    size_t width;

    if (n < 2) return;
    tmp = (size_t *)malloc(n * sizeof *tmp);
    if (tmp == NULL) return; /* caller still has the original order */

    for (width = 1; width < n; width *= 2) {
        size_t i;
        for (i = 0; i < n; i += 2 * width) {
            size_t left = i;
            size_t mid = i + width < n ? i + width : n;
            size_t right = mid;
            size_t end = i + 2 * width < n ? i + 2 * width : n;
            size_t k = i;

            while (left < mid && right < end) {
                if (cmp(a[left], a[right], ctx) <= 0) tmp[k++] = a[left++];
                else tmp[k++] = a[right++];
            }
            while (left < mid) tmp[k++] = a[left++];
            while (right < end) tmp[k++] = a[right++];
        }
        memcpy(a, tmp, n * sizeof *a);
        if (width > n / 2) break;
    }
    free(tmp);
}

typedef struct {
    const oyama_pk_dose *events;
} event_sort_ctx;

static int cmp_event_time(size_t ia, size_t ib, void *ctx)
{
    const event_sort_ctx *c = (const event_sort_ctx *)ctx;
    const double ta = c->events[ia].time_h;
    const double tb = c->events[ib].time_h;
    return (ta > tb) - (ta < tb);
}

static int cmp_patch_apply(size_t ia, size_t ib, void *ctx)
{
    const event_sort_ctx *c = (const event_sort_ctx *)ctx;
    const oyama_pk_dose *a = &c->events[ia];
    const oyama_pk_dose *b = &c->events[ib];
    double wa, wb;
    int ha, hb;
    int cmp;

    if (a->time_h != b->time_h) return (a->time_h > b->time_h) - (a->time_h < b->time_h);
    ha = planned_wear_h(a, &wa);
    hb = planned_wear_h(b, &wb);
    if (ha && hb) {
        if (wa != wb) return (wa > wb) - (wa < wb);
    } else if (ha != hb) {
        return ha ? -1 : 1;
    }
    if (a->dose_mg != b->dose_mg) {
        return (a->dose_mg > b->dose_mg) - (a->dose_mg < b->dose_mg);
    }
    {
        const double ra = release_rate_value(a);
        const double rb = release_rate_value(b);
        if (ra != rb) return (ra > rb) - (ra < rb);
    }
    cmp = strcmp(ester_code(a->ester), ester_code(b->ester));
    if (cmp != 0) return cmp;
    return strcmp(event_id_string(a), event_id_string(b));
}

static int cmp_patch_remove(size_t ia, size_t ib, void *ctx)
{
    const event_sort_ctx *c = (const event_sort_ctx *)ctx;
    const oyama_pk_dose *a = &c->events[ia];
    const oyama_pk_dose *b = &c->events[ib];
    int ra, rb;

    if (a->time_h != b->time_h) return (a->time_h > b->time_h) - (a->time_h < b->time_h);
    ra = patch_hormone(a);
    rb = patch_hormone(b);
    if (ra == PH_E2) ra = 0; else if (ra == PH_T) ra = 1; else ra = 2;
    if (rb == PH_E2) rb = 0; else if (rb == PH_T) rb = 1; else rb = 2;
    return (ra > rb) - (ra < rb);
}

/*
 * Fill remove_time[i] with the removal time paired to sorted event i, or NAN.
 * Mirrors patchRemovalTimes() in the upstream TypeScript model.
 */
void oyama_pk_common_patch_removal_times(const oyama_pk_dose *events, size_t n,
                                double *remove_time)
{
    size_t *applies = NULL;
    size_t *removals = NULL;
    size_t *open = NULL;
    size_t applies_n = 0, removals_n = 0, open_n = 0, next = 0;
    size_t i;
    event_sort_ctx ctx;

    for (i = 0; i < n; i++) {
        remove_time[i] = NAN;
        if (events[i].route == OYAMA_PK_ROUTE_PATCH_APPLY) applies_n++;
        else if (events[i].route == OYAMA_PK_ROUTE_PATCH_REMOVE) removals_n++;
    }
    if (applies_n == 0 || removals_n == 0) return;

    applies = (size_t *)malloc(applies_n * sizeof *applies);
    removals = (size_t *)malloc(removals_n * sizeof *removals);
    open = (size_t *)malloc(applies_n * sizeof *open);
    if (applies == NULL || removals == NULL || open == NULL) goto done;

    {
        size_t ai = 0, ri = 0;
        for (i = 0; i < n; i++) {
            if (events[i].route == OYAMA_PK_ROUTE_PATCH_APPLY) applies[ai++] = i;
            else if (events[i].route == OYAMA_PK_ROUTE_PATCH_REMOVE) removals[ri++] = i;
        }
    }

    ctx.events = events;
    stable_sort_indices(applies, applies_n, cmp_patch_apply, &ctx);
    stable_sort_indices(removals, removals_n, cmp_patch_remove, &ctx);

    for (i = 0; i < removals_n; i++) {
        const size_t r = removals[i];
        const double rtime = events[r].time_h;
        const int rh = patch_hormone(&events[r]);
        size_t j;
        size_t idx = SIZE_MAX;

        while (next < applies_n && events[applies[next]].time_h <= rtime - PATCH_MIN_WEAR_H) {
            open[open_n++] = applies[next++];
        }

        {
            size_t kept = 0;
            for (j = 0; j < open_n; j++) {
                double wear;
                if (!planned_wear_h(&events[open[j]], &wear) ||
                    events[open[j]].time_h + wear + PATCH_REMOVAL_GRACE_H >= rtime) {
                    open[kept++] = open[j];
                }
            }
            open_n = kept;
        }

        for (j = 0; j < open_n; j++) {
            if (rh == PH_NONE || patch_hormone(&events[open[j]]) == rh) {
                idx = j;
                break;
            }
        }
        if (idx != SIZE_MAX) {
            remove_time[open[idx]] = rtime;
            if (open_n - idx > 1) {
                memmove(&open[idx], &open[idx + 1],
                        (open_n - idx - 1) * sizeof *open);
            }
            open_n--;
            continue;
        }

        {
            long last = (long)next - 1;
            long first;
            long k;
            while (last >= 0 && rh != PH_NONE &&
                   patch_hormone(&events[applies[last]]) != rh) {
                last--;
            }
            if (last < 0) continue;
            first = last;
            while (first > 0 &&
                   events[applies[first - 1]].time_h == events[applies[last]].time_h) {
                first--;
            }
            for (k = first; k <= last; k++) {
                const size_t a = applies[k];
                double wear;
                if (rh != PH_NONE && patch_hormone(&events[a]) != rh) continue;
                if (!isnan(remove_time[a])) continue;
                if (rtime > events[a].time_h +
                                (planned_wear_h(&events[a], &wear) ? wear : 0.0) +
                                PATCH_MAX_OVERWEAR_H) {
                    continue;
                }
                remove_time[a] = rtime;
                break;
            }
        }
    }

done:
    free(applies);
    free(removals);
    free(open);
}

double oyama_pk_common_patch_wear_h(size_t index, const oyama_pk_dose *events,
                           const double *remove_time)
{
    double planned;
    if (isfinite(remove_time[index]) && remove_time[index] > events[index].time_h) {
        return remove_time[index] - events[index].time_h;
    }
    if (planned_wear_h(&events[index], &planned)) return planned;
    return DBL_MAX;
}

double oyama_pk_common_max_lifetime_h(const pk_params *p, double wear_h, int is_patch)
{
    double rates[5];
    int n = 0;
    double min_rate;
    int i;

    if (is_patch) {
        const double decay = p->k3 > 0.0 ? ceil(13.816 / p->k3) : 10000.0;
        if (wear_h >= DBL_MAX) return INFINITY;
        return wear_h + decay;
    }

    if (p->k1_fast > 0.0) rates[n++] = p->k1_fast;
    if (p->k1_slow > 0.0) rates[n++] = p->k1_slow;
    if (p->k2 > 0.0) rates[n++] = p->k2;
    if (p->k2_slow > 0.0) rates[n++] = p->k2_slow;
    if (p->k3 > 0.0) rates[n++] = p->k3;
    if (n == 0) return INFINITY;
    min_rate = rates[0];
    for (i = 1; i < n; i++) if (rates[i] < min_rate) min_rate = rates[i];
    return ceil(13.816 / min_rate);
}

/* ------------------------------------------------------------------------- */

void oyama_pk_common_sort_events_by_time(size_t *order, size_t n,
                                         const oyama_pk_dose *events)
{
    event_sort_ctx ctx;
    ctx.events = events;
    stable_sort_indices(order, n, cmp_event_time, &ctx);
}
