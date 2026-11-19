/*
 * Binary curve dumper for the 50M-point C-vs-TypeScript cross-validation.
 *
 * The TSV parser is shared with tools/compare/dump.c.  Renaming its main()
 * keeps that file as the single source of truth for the grammar and the
 * scenario structs; this translation unit only replaces the JSON writer with
 * a compact little-endian stream:
 *
 *   char[8]  magic "OYKV1\0\0\0"
 *   uint32   scenario_count
 *   repeat scenario_count times:
 *     uint32 index
 *     uint32 length
 *     double time_h[length]
 *     double conc_pg_ml_e2[length]
 *     double conc_ng_dl_t[length]
 *
 * A scenario that the C core cannot simulate is emitted with length 0; the
 * Node comparator reports it as a failure rather than silently skipping it.
 */
#define main oyama_pk_compare_json_main
#include "../compare/dump.c"
#undef main

#include <stdint.h>

static void write_u32(FILE *out, uint32_t value)
{
    if (fwrite(&value, sizeof value, 1, out) != 1) die("cannot write output");
}

static void write_doubles(FILE *out, const double *values, size_t count)
{
    if (count > 0 && fwrite(values, sizeof *values, count, out) != count) {
        die("cannot write output");
    }
}

static void run_binary(FILE *out, uint32_t index, const scenario_rec *s)
{
    oyama_pk_request request;
    oyama_pk_result result;
    oyama_pk_dose *doses = NULL;
    double *time_h = NULL;
    double *conc = NULL;
    double *e2 = NULL;
    double *cpa = NULL;
    double *t = NULL;
    size_t cap = 0;
    size_t i;
    oyama_pk_status status;

    if (s->event_count > 0) {
        doses = (oyama_pk_dose *)malloc(s->event_count * sizeof *doses);
        if (doses == NULL) die("out of memory");
        for (i = 0; i < s->event_count; i++) doses[i] = s->events[i].dose;
    }

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

    if (status != OYAMA_PK_OK) {
        write_u32(out, index);
        write_u32(out, 0);
    } else {
        if (result.length > UINT32_MAX) die("scenario too long");
        write_u32(out, index);
        write_u32(out, (uint32_t)result.length);
        write_doubles(out, result.time_h, result.length);
        write_doubles(out, result.conc_pg_ml_e2, result.length);
        write_doubles(out, result.conc_ng_dl_t, result.length);
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
    static const unsigned char magic[8] = {'O', 'Y', 'K', 'V', '1', 0, 0, 0};
    FILE *out;
    size_t i;

    if (argc != 3) {
        fprintf(stderr, "usage: %s scenarios.tsv curves.bin\n", argv[0]);
        return 2;
    }

    read_scenarios(argv[1]);
    if (scenario_count > UINT32_MAX) die("too many scenarios");

    out = fopen(argv[2], "wb");
    if (out == NULL) die("cannot open output file");
    if (fwrite(magic, sizeof magic, 1, out) != 1) die("cannot write output");
    write_u32(out, (uint32_t)scenario_count);
    for (i = 0; i < scenario_count; i++) {
        run_binary(out, (uint32_t)i, &scenarios[i]);
    }
    if (fclose(out) != 0) die("cannot close output file");

    fprintf(stderr, "dump_binary: wrote %zu scenarios to %s\n",
            scenario_count, argv[2]);
    return 0;
}
