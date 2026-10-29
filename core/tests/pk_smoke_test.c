/*
 * Small behavioural tests for the C99 PK core.
 *
 * These are intentionally independent of the TypeScript implementation. The
 * numerical cross-check against the upstream model lives in tools/compare.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "oyama/pk.h"

#define CHECK(cond)                                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            return 1;                                                                   \
        }                                                                               \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                         \
    do {                                                                                \
        const double _a = (actual);                                                     \
        const double _e = (expected);                                                   \
        const double _t = (tolerance);                                                  \
        const double _d = fabs(_a - _e);                                                \
        const double _limit = _t * (fabs(_e) > 1.0 ? fabs(_e) : 1.0);                   \
        if (!(_d <= _limit)) {                                                          \
            fprintf(stderr, "FAIL %s:%d: %s = %.17g, expected %.17g (tol %.3g)\n",      \
                    __FILE__, __LINE__, #actual, _a, _e, _t);                           \
            return 1;                                                                   \
        }                                                                               \
    } while (0)

static const double NOW_H = 480000.0;
static const double HOURS_PER_YEAR = 365.2425 * 24.0;

static oyama_pk_dose oral_e2(double dose_mg, double time_h)
{
    oyama_pk_dose d;
    memset(&d, 0, sizeof d);
    d.id = "oral-e2";
    d.route = OYAMA_PK_ROUTE_ORAL;
    d.ester = OYAMA_PK_ESTER_E2;
    d.time_h = time_h;
    d.dose_mg = dose_mg;
    return d;
}

static int test_empty_history(void)
{
    oyama_pk_request req;
    oyama_pk_result out;
    double t[8], total[8], e2[8], cpa[8], tt[8];

    memset(&req, 0, sizeof req);
    req.body_weight_kg = 70.0;
    req.now_h = NOW_H;

    memset(&out, 0, sizeof out);
    out.time_h = t;
    out.conc_pg_ml = total;
    out.conc_pg_ml_e2 = e2;
    out.conc_ng_ml_cpa = cpa;
    out.conc_ng_dl_t = tt;
    out.capacity = 8;

    CHECK(oyama_pk_simulate(&req, &out) == OYAMA_PK_NO_RESULT);
    CHECK(out.length == 0);
    return 0;
}

static int test_oral_e2_exposure(void)
{
    const double dose_mg = 2.0;
    const double t0 = NOW_H - 200.0;
    const oyama_pk_dose dose = oral_e2(dose_mg, t0);
    oyama_pk_request req;
    oyama_pk_result out;
    double t[8192], total[8192], e2[8192], cpa[8192], tt[8192];
    const double expected_auc =
        (dose_mg * 0.03 / 0.41) * 1e9 / (2.0 * 70.0 * 1000.0);

    memset(&req, 0, sizeof req);
    req.events = &dose;
    req.event_count = 1;
    req.body_weight_kg = 70.0;
    req.now_h = NOW_H;

    memset(&out, 0, sizeof out);
    out.time_h = t;
    out.conc_pg_ml = total;
    out.conc_pg_ml_e2 = e2;
    out.conc_ng_ml_cpa = cpa;
    out.conc_ng_dl_t = tt;
    out.capacity = 8192;

    CHECK(oyama_pk_simulate(&req, &out) == OYAMA_PK_OK);
    CHECK(out.length > 2000);
    CHECK(out.time_h[0] == t0 - 24.0);
    CHECK(out.conc_pg_ml_e2[out.length - 1] < out.conc_pg_ml_e2[out.length / 2]);
    CHECK(out.auc > 0.0);
    CHECK_NEAR(out.auc, expected_auc, 0.002);
    CHECK(oyama_pk_interpolate_e2(&out, t0 + 12.0) > 0.0);
    CHECK(oyama_pk_interpolate_e2(&out, t0 - 1.0) == 0.0);
    return 0;
}

static int test_oral_decays_out_of_window(void)
{
    const oyama_pk_dose dose = oral_e2(2.0, NOW_H - 1000.0);
    oyama_pk_request req;
    oyama_pk_result out;
    double t[8192], total[8192], e2[8192], cpa[8192], tt[8192];
    size_t i;

    memset(&req, 0, sizeof req);
    req.events = &dose;
    req.event_count = 1;
    req.body_weight_kg = 70.0;
    req.now_h = NOW_H;

    memset(&out, 0, sizeof out);
    out.time_h = t;
    out.conc_pg_ml = total;
    out.conc_pg_ml_e2 = e2;
    out.conc_ng_ml_cpa = cpa;
    out.conc_ng_dl_t = tt;
    out.capacity = 8192;

    CHECK(oyama_pk_simulate(&req, &out) == OYAMA_PK_OK);
    /* The engine drops an event once its own decay window is over.  A stale
     * k2_slow from an earlier event must not stretch that window. */
    for (i = 0; i < out.length; i++) {
        if (out.time_h[i] > dose.time_h + 1000.0) {
            CHECK(out.conc_pg_ml_e2[i] == 0.0);
        }
    }
    CHECK(oyama_pk_interpolate_e2(&out, NOW_H) == 0.0);
    return 0;
}

static int test_buffer_too_small(void)
{
    const oyama_pk_dose dose = oral_e2(2.0, NOW_H - 50.0);
    oyama_pk_request req;
    oyama_pk_result out;
    double t[10], total[10], e2[10], cpa[10], tt[10];

    memset(&req, 0, sizeof req);
    req.events = &dose;
    req.event_count = 1;
    req.body_weight_kg = 70.0;
    req.now_h = NOW_H;

    memset(&out, 0, sizeof out);
    out.time_h = t;
    out.conc_pg_ml = total;
    out.conc_pg_ml_e2 = e2;
    out.conc_ng_ml_cpa = cpa;
    out.conc_ng_dl_t = tt;
    out.capacity = 10;

    CHECK(oyama_pk_simulate(&req, &out) == OYAMA_PK_ERR_BUFFER_TOO_SMALL);
    CHECK(out.length > 10);
    return 0;
}

static int test_age_multiplier(void)
{
    const int birth_year = 1970;
    const double at_ref = (birth_year + 0.5 + 54.0 - 1970.0) * HOURS_PER_YEAR;

    CHECK_NEAR(oyama_pk_age_clearance_multiplier(0, birth_year, at_ref), 1.0, 1e-12);
    CHECK_NEAR(oyama_pk_age_clearance_multiplier(1, birth_year, at_ref), 1.0, 1e-12);
    CHECK(oyama_pk_age_clearance_multiplier(0, 0, at_ref) == 1.0);
    CHECK(oyama_pk_age_clearance_multiplier(0, birth_year, at_ref + 20.0 * HOURS_PER_YEAR) < 1.0);
    CHECK(oyama_pk_age_clearance_multiplier(0, birth_year, at_ref - 20.0 * HOURS_PER_YEAR) > 1.0);
    return 0;
}

static int test_api_helpers(void)
{
    oyama_pk_params defaults, sanitized;

    oyama_pk_params_defaults(&defaults);
    CHECK(defaults.e2_kClear == 0.41);
    CHECK(defaults.e2_oral_bio == 0.03);
    CHECK(defaults.t_ff_TE == 0.043);
    CHECK(defaults.t_gel_scrotal == 0.5);

    sanitized = defaults;
    sanitized.e2_kClear = 0.0;
    sanitized.e2_oral_bio = 2.0;
    sanitized.e2_sl_quick = NAN;
    sanitized.t_ff_TC = 0.025;
    oyama_pk_params_sanitize(&sanitized, &sanitized);
    CHECK(sanitized.e2_kClear == 0.001);
    CHECK(sanitized.e2_oral_bio == 1.0);
    CHECK(sanitized.e2_sl_quick == defaults.e2_sl_quick);
    CHECK(sanitized.t_ff_TC == defaults.t_ff_TC);

    oyama_pk_params_sanitize(NULL, &sanitized);
    CHECK(sanitized.e2_kClear == defaults.e2_kClear);

    CHECK(oyama_pk_ester_is_testosterone(OYAMA_PK_ESTER_T));
    CHECK(oyama_pk_ester_is_testosterone(OYAMA_PK_ESTER_TU));
    CHECK(!oyama_pk_ester_is_testosterone(OYAMA_PK_ESTER_CPA));
    CHECK(oyama_pk_ester_is_estradiol(OYAMA_PK_ESTER_EV));
    CHECK(!oyama_pk_ester_is_estradiol(OYAMA_PK_ESTER_TE));
    CHECK(oyama_pk_ester_is_unmodelled(OYAMA_PK_ESTER_SPIRO));
    CHECK(!oyama_pk_ester_is_unmodelled(OYAMA_PK_ESTER_CPA));

    CHECK(oyama_pk_sublingual_theta_from_hold(0.0, NULL) == 0.0);
    CHECK_NEAR(oyama_pk_sublingual_theta_from_hold(2.0, NULL), 0.01, 1e-12);
    CHECK_NEAR(oyama_pk_sublingual_theta_from_hold(15.0, NULL), 0.18, 1e-12);
    CHECK(oyama_pk_sublingual_theta_from_hold(30.0, NULL) > 0.18);

    CHECK(isnan(oyama_pk_interpolate_e2(NULL, 0.0)));
    return 0;
}

int main(void)
{
    if (test_empty_history()) return 1;
    if (test_oral_e2_exposure()) return 1;
    if (test_oral_decays_out_of_window()) return 1;
    if (test_buffer_too_small()) return 1;
    if (test_age_multiplier()) return 1;
    if (test_api_helpers()) return 1;
    puts("pk_smoke_test: ok");
    return 0;
}
