/*
 * Oyama's HRT Tracker - C99 pharmacokinetic core
 *
 * This module is pure computation: no I/O, no network, no database, no global
 * mutable state.  It is the single source of truth for dose -> concentration
 * curves used by the desktop application and by the command-line tools.
 * Internal working memory is allocated and freed by oyama_pk_simulate(); the
 * caller owns only the output buffers described below.
 *
 * Time is measured in hours since 1970-01-01T00:00:00Z, matching the upstream
 * TypeScript model (`timeH`).  Doses are in milligrams of the ester or
 * compound as logged, never in estradiol-equivalent milligrams.  Molecular
 * weight conversion happens inside the model, exactly as upstream does.
 *
 * Concentrations:
 *   - estradiol: pg/mL
 *   - cyproterone acetate: ng/mL (the upstream field is named concPGmL_CPA
 *     even though it stores ng/mL; this API uses the honest name)
 *   - testosterone: ng/dL
 */
#ifndef OYAMA_PK_H
#define OYAMA_PK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OYAMA_PK_VERSION_MAJOR 1
#define OYAMA_PK_VERSION_MINOR 0
#define OYAMA_PK_VERSION_PATCH 0

/* Hard cap on the number of base grid points, matching upstream. */
#define OYAMA_PK_MAX_STEPS 100000

/*
 * Resource limits for oyama_pk_simulate().  They bound CPU work, not the
 * numerical model: a normal history is unaffected, while a crafted request
 * (for example 20,000 patch applications that never end) is rejected with
 * OYAMA_PK_ERR_TOO_MUCH_DATA instead of freezing the caller.
 *
 * OYAMA_PK_MAX_EVENTS is the largest accepted request->event_count, before
 * unmodelled drugs are filtered; it matches the backup importer's per-mode
 * event cap.  OYAMA_PK_MAX_WORK is the approximate number of model/time-point
 * evaluations allowed (patches are re-evaluated at every grid and offset
 * point).  The budget is data-independent and is normally reached in about a
 * second on a desktop CPU.
 */
#define OYAMA_PK_MAX_EVENTS 20000
#define OYAMA_PK_MAX_WORK 100000000ULL

typedef enum {
    OYAMA_PK_OK = 0,
    OYAMA_PK_NO_RESULT = 1,
    OYAMA_PK_ERR_INVALID_ARGUMENT = 2,
    OYAMA_PK_ERR_BUFFER_TOO_SMALL = 3,
    OYAMA_PK_ERR_INTERNAL = 4,
    /* The request is valid but exceeds the documented resource budget.  The
     * caller may show a "history too large to simulate" error; the model does
     * not silently truncate the history. */
    OYAMA_PK_ERR_TOO_MUCH_DATA = 5
} oyama_pk_status;

typedef enum {
    OYAMA_PK_ROUTE_SUBLINGUAL = 0,
    OYAMA_PK_ROUTE_INJECTION = 1,
    OYAMA_PK_ROUTE_PATCH_APPLY = 2,
    OYAMA_PK_ROUTE_PATCH_REMOVE = 3,
    OYAMA_PK_ROUTE_GEL = 4,
    OYAMA_PK_ROUTE_ORAL = 5
} oyama_pk_route;

/* Values are stable; keep them in sync with the upstream `Ester` enum. */
typedef enum {
    OYAMA_PK_ESTER_E2 = 0,
    OYAMA_PK_ESTER_EB = 1,
    OYAMA_PK_ESTER_EV = 2,
    OYAMA_PK_ESTER_EC = 3,
    OYAMA_PK_ESTER_EN = 4,
    OYAMA_PK_ESTER_EU = 5,
    OYAMA_PK_ESTER_CPA = 6,
    OYAMA_PK_ESTER_SPIRO = 7,
    OYAMA_PK_ESTER_BICA = 8,
    OYAMA_PK_ESTER_MPA = 9,
    OYAMA_PK_ESTER_LEUP = 10,
    OYAMA_PK_ESTER_TRIP = 11,
    OYAMA_PK_ESTER_RLX = 12,
    OYAMA_PK_ESTER_T = 13,
    OYAMA_PK_ESTER_TC = 14,
    OYAMA_PK_ESTER_TE = 15,
    OYAMA_PK_ESTER_TU = 16,
    /* A code from a newer version, or any value the model does not know. */
    OYAMA_PK_ESTER_UNKNOWN = 255
} oyama_pk_ester;

typedef enum {
    OYAMA_PK_GEL_ARM = 0,
    OYAMA_PK_GEL_THIGH = 1,
    OYAMA_PK_GEL_SCROTAL = 2
} oyama_pk_gel_site;

typedef enum {
    OYAMA_PK_HORMONE_E2 = 0,
    OYAMA_PK_HORMONE_T = 1
} oyama_pk_hormone;

/*
 * Optional per-dose inputs.  A field is used only when its `has_*` flag is
 * non-zero, so an explicit zero (for example theta = 0) is distinguishable
 * from "not supplied".
 */
typedef struct {
    int has_sublingual_theta;
    double sublingual_theta;

    int has_sublingual_tier;
    int sublingual_tier; /* 0 quick, 1 casual, 2 standard, 3 strict */

    int gel_site; /* oyama_pk_gel_site; ignored outside the gel route */

    int has_release_rate_ug_per_day;
    double release_rate_ug_per_day;

    int has_patch_wear_h;
    double patch_wear_h;
} oyama_pk_extras;

typedef struct {
    /* Optional stable identifier, used only to break exact tie-breaks in patch
     * pairing.  May be NULL. */
    const char *id;
    oyama_pk_route route;
    oyama_pk_ester ester;
    double time_h;
    double dose_mg;
    oyama_pk_extras extras;
} oyama_pk_dose;

typedef struct {
    double e2_kClear;
    double e2_kClearInj;
    double e2_ff_EB;
    double e2_ff_EV;
    double e2_ff_EC;
    double e2_ff_EN;
    double e2_ff_EU;
    double e2_oral_bio;
    double e2_sl_quick;
    double e2_sl_casual;
    double e2_sl_standard;
    double e2_sl_strict;
    double e2_gel_arm;
    double e2_gel_thigh;
    double e2_gel_scrotal;
    double t_kClear;
    double t_kClearInj;
    double t_ff_TC;
    double t_ff_TE;
    double t_ff_TU;
    double t_gel_arm;
    double t_gel_thigh;
    double t_gel_scrotal;
} oyama_pk_params;

typedef struct {
    const oyama_pk_dose *events;
    size_t event_count;
    double body_weight_kg;
    /* 0 means "no year of birth"; valid years are 1900..current year. */
    int birth_year;
    /* The instant the curve should treat as "now", in hours since 1970. */
    double now_h;
    /* NULL selects oyama_pk_params_defaults(). */
    const oyama_pk_params *params;
} oyama_pk_request;

/*
 * Caller-owned output buffers.  Set each pointer and `capacity` before the
 * first call.  If the buffers are too small, oyama_pk_simulate() returns
 * OYAMA_PK_ERR_BUFFER_TOO_SMALL and writes the required length to `length`;
 * call again with larger buffers.
 */
typedef struct {
    double *time_h;
    double *conc_pg_ml;      /* E2 + CPA converted to pg/mL */
    double *conc_pg_ml_e2;   /* estradiol, pg/mL */
    double *conc_ng_ml_cpa;  /* cyproterone acetate, ng/mL */
    double *conc_ng_dl_t;    /* total testosterone, ng/dL */
    size_t capacity;
    size_t length;
    double auc;              /* trapezoidal AUC of the E2 curve, pg*h/mL */
} oyama_pk_result;

/* Fill `out` with the parameter defaults used by the upstream model. */
void oyama_pk_params_defaults(oyama_pk_params *out);

/*
 * Clamp a parameter block into the range the settings screen accepts.
 * Non-finite values and former defaults fall back to the current default.
 * `in` may be NULL (equivalent to defaults); `out` must not be NULL.
 */
void oyama_pk_params_sanitize(const oyama_pk_params *in, oyama_pk_params *out);

/* Convenience initialiser for caller-owned result buffers. */
void oyama_pk_result_init(oyama_pk_result *out,
                          double *time_h,
                          double *conc_pg_ml,
                          double *conc_pg_ml_e2,
                          double *conc_ng_ml_cpa,
                          double *conc_ng_dl_t,
                          size_t capacity);

/*
 * Run the model.
 *
 * Returns OYAMA_PK_NO_RESULT for an empty event list, a non-positive body
 * weight, or an invalid request.  Unmodelled drugs are ignored exactly as
 * upstream ignores them.  The output arrays are valid only when the return
 * value is OYAMA_PK_OK.
 */
oyama_pk_status oyama_pk_simulate(const oyama_pk_request *request,
                                  oyama_pk_result *out);

/* Linear interpolation; returns NAN for an empty result. */
double oyama_pk_interpolate_e2(const oyama_pk_result *sim, double hour_h);
double oyama_pk_interpolate_cpa(const oyama_pk_result *sim, double hour_h);
double oyama_pk_interpolate_t(const oyama_pk_result *sim, double hour_h);

/* Age helpers, matching the upstream age adjustment. */
double oyama_pk_age_at_h(int birth_year, double time_h);
double oyama_pk_age_clearance_multiplier(oyama_pk_hormone hormone,
                                          int birth_year,
                                          double time_h);

/*
 * Map a sublingual hold time in minutes to theta, using the currently
 * supplied parameter block (NULL means defaults).  Returns 0 for hold <= 0.
 */
double oyama_pk_sublingual_theta_from_hold(double hold_min,
                                           const oyama_pk_params *params);

/* Classification helpers. */
int oyama_pk_ester_is_testosterone(oyama_pk_ester ester);
int oyama_pk_ester_is_unmodelled(oyama_pk_ester ester);
int oyama_pk_ester_is_estradiol(oyama_pk_ester ester);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OYAMA_PK_H */
