#include "oyama/pk.h"

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

static double ester_mw(oyama_pk_ester ester)
{
    switch (ester) {
    case OYAMA_PK_ESTER_E2:  return 272.38;
    case OYAMA_PK_ESTER_EB:  return 376.50;
    case OYAMA_PK_ESTER_EV:  return 356.50;
    case OYAMA_PK_ESTER_EC:  return 396.58;
    case OYAMA_PK_ESTER_EN:  return 384.56;
    case OYAMA_PK_ESTER_EU:  return 440.66;
    case OYAMA_PK_ESTER_CPA: return 416.94;
    case OYAMA_PK_ESTER_T:   return 288.42;
    case OYAMA_PK_ESTER_TC:  return 412.60;
    case OYAMA_PK_ESTER_TE:  return 400.59;
    case OYAMA_PK_ESTER_TU:  return 456.70;
    default:                 return 0.0;
    }
}

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

/* mg of estradiol (or testosterone) per mg of the logged compound. */
static double get_to_e2_factor(oyama_pk_ester ester)
{
    if (ester == OYAMA_PK_ESTER_E2) return 1.0;
    if (ester == OYAMA_PK_ESTER_CPA || oyama_pk_ester_is_unmodelled(ester)) return 1.0;
    if (oyama_pk_ester_is_testosterone(ester)) {
        return 288.42 / ester_mw(ester);
    }
    return 272.38 / ester_mw(ester);
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

/* ------------------------------------------------------------------------- */
/* Internal resolved parameters                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
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

static int gel_site_index(const oyama_pk_extras *extras)
{
    long site = (long)round((double)extras->gel_site);
    if (site < 0) site = 0;
    if (site > 2) site = 2;
    return (int)site;
}

static double sublingual_theta(const oyama_pk_extras *extras,
                               const oyama_pk_params *p)
{
    double theta = p->e2_sl_standard;

    if (extras->has_sublingual_theta) {
        if (isfinite(extras->sublingual_theta)) {
            theta = clamp01(extras->sublingual_theta);
        }
        return theta;
    }
    if (extras->has_sublingual_tier) {
        long tier = (long)extras->sublingual_tier;
        if (tier < 0) tier = 0;
        if (tier > 3) tier = 3;
        if (tier == 0) theta = p->e2_sl_quick;
        else if (tier == 1) theta = p->e2_sl_casual;
        else if (tier == 2) theta = p->e2_sl_standard;
        else theta = p->e2_sl_strict;
    }
    return theta;
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

static double bioavailability(oyama_pk_route route,
                               oyama_pk_ester ester,
                               const oyama_pk_extras *extras,
                               const oyama_pk_params *p)
{
    const double mw_factor = get_to_e2_factor(ester);

    if (ester == OYAMA_PK_ESTER_CPA) {
        return route == OYAMA_PK_ROUTE_ORAL ? CPA_ORAL_F : 0.0;
    }

    if (oyama_pk_ester_is_testosterone(ester)) {
        switch (route) {
        case OYAMA_PK_ROUTE_INJECTION:
            if (ester == OYAMA_PK_ESTER_T) return 0.0;
            return t_injection_formation(ester, p) * mw_factor;
        case OYAMA_PK_ROUTE_GEL: {
            const int site = gel_site_index(extras);
            const double bio = site == 0 ? p->t_gel_arm
                             : site == 1 ? p->t_gel_thigh
                                         : p->t_gel_scrotal;
            return bio * mw_factor;
        }
        case OYAMA_PK_ROUTE_PATCH_APPLY:
            return 1.0 * mw_factor;
        default:
            return 0.0;
        }
    }

    switch (route) {
    case OYAMA_PK_ROUTE_INJECTION:
        return e2_injection_formation(ester, p) * mw_factor;
    case OYAMA_PK_ROUTE_ORAL:
        return p->e2_oral_bio * mw_factor;
    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double theta = sublingual_theta(extras, p);
        return (theta + (1.0 - theta) * p->e2_oral_bio) * mw_factor;
    }
    case OYAMA_PK_ROUTE_GEL: {
        const int site = gel_site_index(extras);
        const double bio = site == 0 ? p->e2_gel_arm
                         : site == 1 ? p->e2_gel_thigh
                                     : p->e2_gel_scrotal;
        return bio * mw_factor;
    }
    case OYAMA_PK_ROUTE_PATCH_APPLY:
        return 1.0 * mw_factor;
    default:
        return 0.0;
    }
}

static void zero_params(pk_params *out, double k3)
{
    memset(out, 0, sizeof *out);
    out->k3 = k3;
}

static void resolve_params(const oyama_pk_dose *event,
                           int birth_year,
                           const oyama_pk_params *p,
                           pk_params *out)
{
    /* Every route sets only the fields it uses; k2_slow is optional upstream.
     * Clear first so a previous event's optional rate cannot leak into this
     * one's max-lifetime calculation. */
    memset(out, 0, sizeof *out);

    const double default_k3 =
        (event->route == OYAMA_PK_ROUTE_INJECTION ? p->e2_kClearInj : p->e2_kClear) *
        oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_E2, birth_year,
                                           event->time_h);
    const double to_e2 = get_to_e2_factor(event->ester);

    if (oyama_pk_ester_is_testosterone(event->ester)) {
        const double t_k3 =
            (event->route == OYAMA_PK_ROUTE_INJECTION ? p->t_kClearInj : p->t_kClear) *
            oyama_pk_age_clearance_multiplier(OYAMA_PK_HORMONE_T, birth_year,
                                               event->time_h);
        switch (event->route) {
        case OYAMA_PK_ROUTE_INJECTION: {
            double frac_fast, k1_fast, k1_slow, k2, F;
            if (event->ester == OYAMA_PK_ESTER_T) {
                zero_params(out, t_k3);
                return;
            }
            frac_fast = 0.35;
            k1_fast = 0.025;
            k1_slow = 0.005;
            k2 = 0.2;
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
            F = bioavailability(event->route, event->ester, &event->extras, p);
            out->frac_fast = frac_fast;
            out->k1_fast = k1_fast;
            out->k1_slow = k1_slow;
            out->k2 = k2;
            out->k3 = t_k3;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            return;
        }
        case OYAMA_PK_ROUTE_GEL: {
            const double F = bioavailability(event->route, event->ester,
                                             &event->extras, p);
            zero_params(out, t_k3);
            out->frac_fast = 1.0;
            out->k1_fast = 0.05;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            return;
        }
        case OYAMA_PK_ROUTE_PATCH_APPLY: {
            const double F = bioavailability(event->route, event->ester,
                                             &event->extras, p);
            double rate = 0.0;
            if (event->extras.has_release_rate_ug_per_day &&
                isfinite(event->extras.release_rate_ug_per_day) &&
                event->extras.release_rate_ug_per_day > 0.0) {
                rate = (event->extras.release_rate_ug_per_day / 24.0 / 1000.0) * F;
            }
            zero_params(out, t_k3);
            out->frac_fast = 1.0;
            out->F = F;
            out->F_fast = F;
            out->F_slow = F;
            if (rate > 0.0) {
                out->rate_mg_h = rate;
            } else {
                out->k1_fast = 0.03;
            }
            return;
        }
        case OYAMA_PK_ROUTE_PATCH_REMOVE:
        default:
            zero_params(out, t_k3);
            return;
        }
    }

    if (event->ester == OYAMA_PK_ESTER_CPA &&
        event->route != OYAMA_PK_ROUTE_ORAL) {
        zero_params(out, default_k3);
        return;
    }

    switch (event->route) {
    case OYAMA_PK_ROUTE_INJECTION: {
        double frac_fast = 0.5;
        double k1_fast = 0.1;
        double k1_slow = 0.01;
        double k2 = 0.0;
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
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
        out->frac_fast = frac_fast;
        out->k1_fast = k1_fast;
        out->k1_slow = k1_slow;
        out->k2 = k2;
        out->k3 = default_k3;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        return;
    }

    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double theta = sublingual_theta(&event->extras, p);
        double k2 = 0.0;
        switch (event->ester) {
        case OYAMA_PK_ESTER_EB: k2 = 0.09; break;
        case OYAMA_PK_ESTER_EV: k2 = 0.07; break;
        case OYAMA_PK_ESTER_EC: k2 = 0.045; break;
        case OYAMA_PK_ESTER_EN: k2 = 0.015; break;
        case OYAMA_PK_ESTER_EU: k2 = 0.012; break;
        default: k2 = 0.0; break;
        }
        out->frac_fast = theta;
        out->k1_fast = 1.8;
        out->k1_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
        out->k2 = k2;
        out->k2_slow = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
        out->k3 = default_k3;
        out->F_fast = to_e2;
        out->F_slow = p->e2_oral_bio * to_e2;
        out->F = theta * out->F_fast + (1.0 - theta) * out->F_slow;
        return;
    }

    case OYAMA_PK_ROUTE_GEL: {
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
        zero_params(out, default_k3);
        out->frac_fast = 1.0;
        out->k1_fast = 0.0193;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        return;
    }

    case OYAMA_PK_ROUTE_PATCH_APPLY: {
        const double F = bioavailability(event->route, event->ester,
                                         &event->extras, p);
        double rate = 0.0;
        if (event->extras.has_release_rate_ug_per_day &&
            isfinite(event->extras.release_rate_ug_per_day) &&
            event->extras.release_rate_ug_per_day > 0.0) {
            rate = (event->extras.release_rate_ug_per_day / 24.0 / 1000.0) * F;
        }
        zero_params(out, default_k3);
        out->frac_fast = 1.0;
        out->F = F;
        out->F_fast = F;
        out->F_slow = F;
        if (rate > 0.0) {
            out->rate_mg_h = rate;
        } else {
            out->k1_fast = 0.0075;
        }
        return;
    }

    case OYAMA_PK_ROUTE_PATCH_REMOVE:
        zero_params(out, default_k3);
        return;

    case OYAMA_PK_ROUTE_ORAL: {
        if (event->ester == OYAMA_PK_ESTER_CPA) {
            const double beta = log(2.0) / 43.9;
            zero_params(out, beta);
            out->frac_fast = 1.0;
            out->k1_fast = CPA_ORAL_KA;
            out->F = CPA_ORAL_F;
            out->F_fast = CPA_ORAL_F;
            out->F_slow = CPA_ORAL_F;
            return;
        }
        out->frac_fast = 1.0;
        out->k1_fast = event->ester == OYAMA_PK_ESTER_EV ? 0.05 : 0.32;
        out->k2 = event->ester == OYAMA_PK_ESTER_EV ? 0.0 : (log(2.0) / 14.36);
        out->k3 = default_k3;
        out->F = p->e2_oral_bio * to_e2;
        out->F_fast = out->F;
        out->F_slow = out->F;
        return;
    }
    }

    zero_params(out, default_k3);
}

/* ------------------------------------------------------------------------- */
/* Analytical kernels                                                        */
/* ------------------------------------------------------------------------- */

static double exp_div_diff1(double x, double y, double tau)
{
    const double z = fabs(y - x) * tau / 2.0;
    double sinhc;

    if (z > 1.0) return (exp(-x * tau) - exp(-y * tau)) / (x - y);
    sinhc = z == 0.0 ? 1.0 : sinh(z) / z;
    return -tau * exp((-(x + y) / 2.0) * tau) * sinhc;
}

static double exp_div_diff2(double a, double b, double c, double tau)
{
    double mean, ua, ub, uc;
    double h1 = 1.0, h2 = 1.0, h3 = 1.0;
    double fact = 2.0, sum = 0.5;
    int j;

    if (a > b) { double t = a; a = b; b = t; }
    if (b > c) { double t = b; b = c; c = t; }
    if (a > b) { double t = a; a = b; b = t; }

    if ((c - a) * tau >= 0.5) {
        return (exp_div_diff1(b, c, tau) - exp_div_diff1(a, b, tau)) / (c - a);
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

static double analytic2c(double tau, double dose_mg, double F,
                         double ka, double ke)
{
    if (ka <= 0.0 || !(dose_mg > 0.0)) return 0.0;
    return -dose_mg * F * ka * exp_div_diff1(ka, ke, tau);
}

static double analytic3c(double tau, double dose_mg, double F,
                         double k1, double k2, double k3)
{
    if (k1 <= 0.0 || !(dose_mg > 0.0)) return 0.0;
    return dose_mg * F * k1 * k2 * exp_div_diff2(k1, k2, k3, tau);
}

static double one_comp_amount(double tau, double dose_mg, const pk_params *p)
{
    return analytic2c(tau, dose_mg, p->F, p->k1_fast, p->k3);
}

static double cpa_wslow(void)
{
    const double beta = log(2.0) / 43.9;
    const double exposure_h = 14.0 / (0.0035 * 60.0);
    return (exposure_h - CPA_ORAL_WFAST / CPA_ORAL_ALPHA) * beta;
}

static double event_amount(const oyama_pk_dose *event,
                           const pk_params *p,
                           double wear_h,
                           double t)
{
    const double tau = t - event->time_h;
    if (tau < 0.0) return 0.0;

    switch (event->route) {
    case OYAMA_PK_ROUTE_INJECTION: {
        const double dose_fast = event->dose_mg * p->frac_fast;
        const double dose_slow = event->dose_mg * (1.0 - p->frac_fast);
        if (p->k2 > 0.0) {
            return analytic3c(tau, dose_fast, p->F, p->k1_fast, p->k2, p->k3) +
                   analytic3c(tau, dose_slow, p->F, p->k1_slow, p->k2, p->k3);
        }
        return analytic2c(tau, dose_fast, p->F, p->k1_fast, p->k3) +
               analytic2c(tau, dose_slow, p->F, p->k1_slow, p->k3);
    }

    case OYAMA_PK_ROUTE_GEL:
    case OYAMA_PK_ROUTE_ORAL:
        if (event->route == OYAMA_PK_ROUTE_ORAL &&
            event->ester == OYAMA_PK_ESTER_CPA) {
            const double beta = log(2.0) / 43.9;
            return analytic2c(tau, event->dose_mg, CPA_ORAL_F * CPA_ORAL_WFAST,
                              CPA_ORAL_KA, CPA_ORAL_ALPHA) +
                   analytic2c(tau, event->dose_mg, CPA_ORAL_F * cpa_wslow(),
                              CPA_ORAL_KA, beta);
        }
        if (p->k2 > 0.0) {
            return analytic3c(tau, event->dose_mg, p->F, p->k1_fast,
                              p->k2, p->k3);
        }
        return one_comp_amount(tau, event->dose_mg, p);

    case OYAMA_PK_ROUTE_SUBLINGUAL: {
        const double dose_fast = event->dose_mg * p->frac_fast;
        const double dose_slow = event->dose_mg * (1.0 - p->frac_fast);
        const double fast = p->k2 > 0.0
            ? analytic3c(tau, dose_fast, p->F_fast, p->k1_fast, p->k2, p->k3)
            : analytic2c(tau, dose_fast, p->F_fast, p->k1_fast, p->k3);
        const double slow = p->k2_slow > 0.0
            ? analytic3c(tau, dose_slow, p->F_slow, p->k1_slow, p->k2_slow, p->k3)
            : analytic2c(tau, dose_slow, p->F_slow, p->k1_slow, p->k3);
        return fast + slow;
    }

    case OYAMA_PK_ROUTE_PATCH_APPLY:
        if (p->rate_mg_h > 0.0) {
            if (tau <= wear_h) {
                return (p->rate_mg_h / p->k3) * (1.0 - exp(-p->k3 * tau));
            } else {
                const double amt_removal =
                    (p->rate_mg_h / p->k3) * (1.0 - exp(-p->k3 * wear_h));
                return amt_removal * exp(-p->k3 * (tau - wear_h));
            }
        } else {
            const double under = one_comp_amount(tau, event->dose_mg, p);
            if (tau > wear_h) {
                const double at_removal =
                    one_comp_amount(wear_h, event->dose_mg, p);
                return at_removal * exp(-p->k3 * (tau - wear_h));
            }
            return under;
        }

    default:
        return 0.0;
    }
}

/* ------------------------------------------------------------------------- */
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
static void patch_removal_times(const oyama_pk_dose *events, size_t n,
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

static double patch_wear_h(size_t index, const oyama_pk_dose *events,
                           const double *remove_time)
{
    double planned;
    if (isfinite(remove_time[index]) && remove_time[index] > events[index].time_h) {
        return remove_time[index] - events[index].time_h;
    }
    if (planned_wear_h(&events[index], &planned)) return planned;
    return DBL_MAX;
}

static double max_lifetime_h(const pk_params *p, double wear_h, int is_patch)
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
    double auc = 0.0;
    oyama_pk_status status = OYAMA_PK_ERR_INTERNAL;

    if (out == NULL) return OYAMA_PK_ERR_INVALID_ARGUMENT;
    out->length = 0;
    out->auc = 0.0;
    if (request == NULL) return OYAMA_PK_ERR_INVALID_ARGUMENT;
    if (request->event_count == 0 || request->events == NULL) return OYAMA_PK_NO_RESULT;
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
    {
        event_sort_ctx ctx;
        ctx.events = filtered;
        stable_sort_indices(order, n, cmp_event_time, &ctx);
    }
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
    patch_removal_times(sorted, n, remove_time);

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
            resolve_params(&sorted[i], birth_year, &params, &models[mi].params);
            if (sorted[i].route == OYAMA_PK_ROUTE_PATCH_APPLY) {
                wear = patch_wear_h(i, sorted, remove_time);
            }
            models[mi].wear_h = wear;
            models[mi].max_lifetime_h =
                max_lifetime_h(&models[mi].params, wear,
                               sorted[i].route == OYAMA_PK_ROUTE_PATCH_APPLY);
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
                const double amount = event_amount(e, &m->params, m->wear_h, t);
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
