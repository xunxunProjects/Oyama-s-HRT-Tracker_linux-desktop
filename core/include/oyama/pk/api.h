/* PK API (split from oyama/pk.h) */
#ifndef OYAMA_PK_API_H
#define OYAMA_PK_API_H

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


#endif /* OYAMA_PK_API_H */
