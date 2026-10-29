#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oyama/pk.h"

typedef struct {
    char *id;
    oyama_pk_dose dose;
} event_rec;

typedef struct {
    char *id;
    double weight;
    int birth_year; /* 0 = unset */
    double now_h;
    event_rec *events;
    size_t event_count;
    size_t event_cap;
} scenario_rec;

static scenario_rec *scenarios = NULL;
static size_t scenario_count = 0;
static size_t scenario_cap = 0;

static char *dup_text(const char *s)
{
    size_t n;
    char *p;
    if (s == NULL) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p != NULL) memcpy(p, s, n);
    return p;
}

static void die(const char *message)
{
    fprintf(stderr, "dump: %s\n", message);
    exit(2);
}

static double parse_double(const char *text, const char *what)
{
    char *end = NULL;
    double v;
    errno = 0;
    v = strtod(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE) {
        fprintf(stderr, "dump: bad %s: %s\n", what, text);
        exit(2);
    }
    return v;
}

static int parse_year(const char *text)
{
    if (strcmp(text, "none") == 0) return 0;
    return (int)parse_double(text, "birth year");
}

static oyama_pk_route parse_route(const char *text)
{
    if (strcmp(text, "sublingual") == 0) return OYAMA_PK_ROUTE_SUBLINGUAL;
    if (strcmp(text, "injection") == 0) return OYAMA_PK_ROUTE_INJECTION;
    if (strcmp(text, "patchApply") == 0) return OYAMA_PK_ROUTE_PATCH_APPLY;
    if (strcmp(text, "patchRemove") == 0) return OYAMA_PK_ROUTE_PATCH_REMOVE;
    if (strcmp(text, "gel") == 0) return OYAMA_PK_ROUTE_GEL;
    if (strcmp(text, "oral") == 0) return OYAMA_PK_ROUTE_ORAL;
    fprintf(stderr, "dump: bad route: %s\n", text);
    exit(2);
}

static oyama_pk_ester parse_ester(const char *text)
{
    static const char *const names[] = {
        "E2", "EB", "EV", "EC", "EN", "EU", "CPA", "SPIRO", "BICA", "MPA",
        "LEUP", "TRIP", "RLX", "T", "TC", "TE", "TU"
    };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (strcmp(text, names[i]) == 0) return (oyama_pk_ester)i;
    }
    return OYAMA_PK_ESTER_UNKNOWN;
}

static void parse_extras(const char *text, oyama_pk_extras *extras)
{
    char *copy;
    char *part;
    char *save = NULL;
    memset(extras, 0, sizeof *extras);
    if (text == NULL || strcmp(text, "-") == 0) return;
    copy = dup_text(text);
    if (copy == NULL) die("out of memory");
    for (part = strtok_r(copy, ";", &save); part != NULL;
         part = strtok_r(NULL, ";", &save)) {
        char *eq = strchr(part, '=');
        char *value;
        if (eq == NULL) continue;
        *eq = '\0';
        value = eq + 1;
        if (strcmp(part, "releaseRate") == 0) {
            extras->has_release_rate_ug_per_day = 1;
            extras->release_rate_ug_per_day = parse_double(value, "release rate");
        } else if (strcmp(part, "theta") == 0) {
            extras->has_sublingual_theta = 1;
            extras->sublingual_theta = parse_double(value, "theta");
        } else if (strcmp(part, "tier") == 0) {
            extras->has_sublingual_tier = 1;
            extras->sublingual_tier = (int)parse_double(value, "tier");
        } else if (strcmp(part, "gelSite") == 0) {
            extras->gel_site = (int)parse_double(value, "gel site");
        } else if (strcmp(part, "patchWear") == 0) {
            extras->has_patch_wear_h = 1;
            extras->patch_wear_h = parse_double(value, "patch wear");
        }
    }
    free(copy);
}

static scenario_rec *new_scenario(const char *id)
{
    scenario_rec *s;
    if (scenario_count == scenario_cap) {
        size_t cap = scenario_cap == 0 ? 16 : scenario_cap * 2;
        scenario_rec *grown = (scenario_rec *)realloc(scenarios, cap * sizeof *grown);
        if (grown == NULL) die("out of memory");
        scenarios = grown;
        scenario_cap = cap;
    }
    s = &scenarios[scenario_count++];
    memset(s, 0, sizeof *s);
    s->id = dup_text(id);
    if (s->id == NULL) die("out of memory");
    return s;
}

static void add_event(scenario_rec *s, const char *route, const char *ester,
                      const char *dose, const char *time_h, const char *id,
                      const char *extras)
{
    event_rec *e;
    if (s == NULL) die("EVENT before SCENARIO");
    if (s->event_count == s->event_cap) {
        size_t cap = s->event_cap == 0 ? 8 : s->event_cap * 2;
        event_rec *grown = (event_rec *)realloc(s->events, cap * sizeof *grown);
        if (grown == NULL) die("out of memory");
        s->events = grown;
        s->event_cap = cap;
    }
    e = &s->events[s->event_count++];
    memset(e, 0, sizeof *e);
    if (strcmp(id, "-") != 0) {
        e->id = dup_text(id);
        if (e->id == NULL) die("out of memory");
    }
    e->dose.id = e->id;
    e->dose.route = parse_route(route);
    e->dose.ester = parse_ester(ester);
    e->dose.dose_mg = parse_double(dose, "dose");
    e->dose.time_h = parse_double(time_h, "time");
    parse_extras(extras, &e->dose.extras);
}

static void trim_line(char *line)
{
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
}

static void read_scenarios(const char *path)
{
    FILE *f = fopen(path, "r");
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    scenario_rec *current = NULL;
    if (f == NULL) die("cannot open scenarios file");
    while ((len = getline(&line, &cap, f)) >= 0) {
        char *field[8];
        int n = 0;
        char *save = NULL;
        char *p;
        (void)len;
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        p = strtok_r(line, "\t", &save);
        while (p != NULL && n < (int)(sizeof field / sizeof field[0])) {
            field[n++] = p;
            p = strtok_r(NULL, "\t", &save);
        }
        if (n == 0) continue;
        if (strcmp(field[0], "SCENARIO") == 0 && n >= 5) {
            current = new_scenario(field[1]);
            current->weight = parse_double(field[2], "weight");
            current->birth_year = parse_year(field[3]);
            current->now_h = parse_double(field[4], "now");
        } else if (strcmp(field[0], "EVENT") == 0 && n >= 7) {
            add_event(current, field[1], field[2], field[3], field[4], field[5], field[6]);
        }
    }
    free(line);
    fclose(f);
}

static void print_number(double v)
{
    if (!isfinite(v)) {
        fputs("null", stdout);
        return;
    }
    printf("%.17g", v);
}

static void print_array(const double *values, size_t n)
{
    size_t i;
    putchar('[');
    for (i = 0; i < n; i++) {
        if (i > 0) putchar(',');
        print_number(values[i]);
    }
    putchar(']');
}

static void run_scenario(const scenario_rec *s)
{
    oyama_pk_dose *doses;
    oyama_pk_request request;
    oyama_pk_result result;
    double *time_h = NULL, *conc = NULL, *e2 = NULL, *cpa = NULL, *t = NULL;
    size_t i, cap = 0;
    oyama_pk_status status;

    doses = s->event_count == 0 ? NULL : (oyama_pk_dose *)malloc(s->event_count * sizeof *doses);
    if (s->event_count > 0 && doses == NULL) die("out of memory");
    for (i = 0; i < s->event_count; i++) doses[i] = s->events[i].dose;

    memset(&request, 0, sizeof request);
    request.events = doses;
    request.event_count = s->event_count;
    request.body_weight_kg = s->weight;
    request.birth_year = s->birth_year;
    request.now_h = s->now_h;
    request.params = NULL;

    oyama_pk_result_init(&result, NULL, NULL, NULL, NULL, NULL, 0);
    status = oyama_pk_simulate(&request, &result);
    if (status == OYAMA_PK_ERR_BUFFER_TOO_SMALL) {
        cap = result.length;
        time_h = (double *)malloc(cap * sizeof *time_h);
        conc = (double *)malloc(cap * sizeof *conc);
        e2 = (double *)malloc(cap * sizeof *e2);
        cpa = (double *)malloc(cap * sizeof *cpa);
        t = (double *)malloc(cap * sizeof *t);
        if (time_h == NULL || conc == NULL || e2 == NULL || cpa == NULL || t == NULL) {
            die("out of memory");
        }
        oyama_pk_result_init(&result, time_h, conc, e2, cpa, t, cap);
        status = oyama_pk_simulate(&request, &result);
    }

    printf("\"%s\":", s->id);
    if (status == OYAMA_PK_NO_RESULT) {
        fputs("null", stdout);
    } else if (status != OYAMA_PK_OK) {
        printf("{\"error\":%d}", (int)status);
    } else {
        printf("{\"auc\":");
        print_number(result.auc);
        fputs(",\"timeH\":", stdout);
        print_array(result.time_h, result.length);
        fputs(",\"concPGmL\":", stdout);
        print_array(result.conc_pg_ml, result.length);
        fputs(",\"concPGmL_E2\":", stdout);
        print_array(result.conc_pg_ml_e2, result.length);
        fputs(",\"concPGmL_CPA\":", stdout);
        print_array(result.conc_ng_ml_cpa, result.length);
        fputs(",\"concNGdL_T\":", stdout);
        print_array(result.conc_ng_dl_t, result.length);

        {
            static const double offsets[] = {-12.0, -6.0, -1.0, 0.0, 24.0};
            size_t k;
            fputs(",\"interp\":{\"e2\":[", stdout);
            for (k = 0; k < sizeof offsets / sizeof offsets[0]; k++) {
                if (k > 0) putchar(',');
                print_number(oyama_pk_interpolate_e2(&result, s->now_h + offsets[k]));
            }
            fputs("],\"cpa\":[", stdout);
            for (k = 0; k < sizeof offsets / sizeof offsets[0]; k++) {
                if (k > 0) putchar(',');
                print_number(oyama_pk_interpolate_cpa(&result, s->now_h + offsets[k]));
            }
            fputs("],\"t\":[", stdout);
            for (k = 0; k < sizeof offsets / sizeof offsets[0]; k++) {
                if (k > 0) putchar(',');
                print_number(oyama_pk_interpolate_t(&result, s->now_h + offsets[k]));
            }
            fputs("]}", stdout);
        }
        putchar('}');
    }

    free(time_h);
    free(conc);
    free(e2);
    free(cpa);
    free(t);
    free(doses);
}

int main(int argc, char **argv)
{
    size_t i;
    if (argc != 2) {
        fprintf(stderr, "usage: %s scenarios.txt\n", argv[0]);
        return 2;
    }
    read_scenarios(argv[1]);
    fputs("{\"theta\":[", stdout);
    {
        static const double holds[] = {0, 1, 2, 3, 5, 7.5, 10, 12, 15, 20, 30, 60};
        size_t k;
        for (k = 0; k < sizeof holds / sizeof holds[0]; k++) {
            if (k > 0) putchar(',');
            printf("{\"hold\":");
            print_number(holds[k]);
            fputs(",\"theta\":", stdout);
            print_number(oyama_pk_sublingual_theta_from_hold(holds[k], NULL));
            putchar('}');
        }
    }
    fputs("],\"scenarios\":{", stdout);
    for (i = 0; i < scenario_count; i++) {
        if (i > 0) putchar(',');
        run_scenario(&scenarios[i]);
    }
    fputs("}}\n", stdout);
    return 0;
}
