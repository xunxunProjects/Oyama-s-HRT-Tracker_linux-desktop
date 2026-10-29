/*
 * Upstream-side dumper for the C99 PK comparison harness.
 *
 * This file is a template: copy it into the upstream repository's tests/
 * directory as `_dump_pk.test.ts`, run it with vitest, then remove it.  It
 * imports `../logic` and `./helpers`, so it must live next to the upstream
 * tests.  The output directory can be overridden with OYAMA_PK_COMPARE_DIR.
 */
import { mkdirSync, writeFileSync } from 'node:fs';
import { test } from 'vitest';
import {
    DoseEvent,
    Ester,
    ExtraKey,
    Route,
    interpolateConcentration_CPA,
    interpolateConcentration_E2,
    interpolateConcentration_T,
    runSimulation,
    sublingualThetaFromHold,
} from '../logic';
import { NOW_H } from './helpers';

type Extras = Partial<Record<ExtraKey, number>>;

interface Scenario {
    id: string;
    weight: number;
    birthYear: number | null;
    events: DoseEvent[];
}

let seq = 0;
const ev = (
    route: Route,
    ester: Ester,
    doseMG: number,
    timeH: number,
    extras: Extras = {},
    id?: string,
): DoseEvent => {
    seq += 1;
    return { id: id ?? `ev-${seq}`, route, ester, doseMG, timeH, extras };
};
const at = (offsetH: number) => NOW_H + offsetH;

const scenarios: Scenario[] = [];
const scenario = (id: string, events: DoseEvent[], weight = 70, birthYear: number | null = null) => {
    scenarios.push({ id, weight, birthYear, events });
};

scenario('oral_e2_single', [ev(Route.oral, Ester.E2, 2, at(-24))]);
scenario('oral_ev_repeat', [
    ev(Route.oral, Ester.EV, 4, at(-96)),
    ev(Route.oral, Ester.EV, 4, at(-72)),
    ev(Route.oral, Ester.EV, 4, at(-48)),
    ev(Route.oral, Ester.EV, 4, at(-24)),
]);
scenario('oral_e2_cpa', [
    ev(Route.oral, Ester.E2, 2, at(-24)),
    ev(Route.oral, Ester.CPA, 50, at(-24)),
]);
scenario('sublingual_default', [ev(Route.sublingual, Ester.E2, 2, at(-6))]);
scenario('sublingual_custom_theta', [
    ev(Route.sublingual, Ester.E2, 2, at(-6), { [ExtraKey.sublingualTheta]: 0.6 }),
]);
scenario('sublingual_tier_quick', [
    ev(Route.sublingual, Ester.EV, 2, at(-6), { [ExtraKey.sublingualTier]: 0 }),
]);
scenario('sublingual_ev', [ev(Route.sublingual, Ester.EV, 2, at(-6))]);
scenario('sublingual_eb', [ev(Route.sublingual, Ester.EB, 2, at(-6))]);
scenario('oral_eb', [ev(Route.oral, Ester.EB, 2, at(-24))]);
scenario('gel_ev', [ev(Route.gel, Ester.EV, 1.5, at(-12), { [ExtraKey.gelSite]: 0 })]);
scenario('gel_arm', [ev(Route.gel, Ester.E2, 1.5, at(-12), { [ExtraKey.gelSite]: 0 })]);
scenario('gel_thigh', [ev(Route.gel, Ester.E2, 1.5, at(-12), { [ExtraKey.gelSite]: 1 })]);
scenario('gel_scrotal', [ev(Route.gel, Ester.E2, 1.5, at(-12), { [ExtraKey.gelSite]: 2 })]);
scenario('patch_zero_order', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-48), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 84,
    }),
]);
scenario('patch_first_order', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-48), { [ExtraKey.patchWearH]: 84 }),
]);
scenario('patch_remove', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 168,
    }),
    ev(Route.patchRemove, Ester.E2, 0, at(-24)),
]);
scenario('patch_overlap', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-240), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 168,
    }),
    ev(Route.patchApply, Ester.E2, 1.56, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 168,
    }),
    ev(Route.patchRemove, Ester.E2, 0, at(-24)),
]);
scenario('patch_fallback', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-300), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 84,
    }),
    ev(Route.patchRemove, Ester.E2, 0, at(-100)),
]);
scenario('patch_mismatch', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 168,
    }),
    ev(Route.patchRemove, Ester.T, 0, at(-24)),
]);
scenario('injection_eb', [ev(Route.injection, Ester.EB, 5, at(-480))]);
scenario('injection_ev', [ev(Route.injection, Ester.EV, 5, at(-480))]);
scenario('injection_ec', [ev(Route.injection, Ester.EC, 5, at(-480))]);
scenario('injection_en', [ev(Route.injection, Ester.EN, 5, at(-480))]);
scenario('injection_eu', [ev(Route.injection, Ester.EU, 5, at(-480))]);
scenario('injection_e2', [ev(Route.injection, Ester.E2, 2, at(-80))]);
scenario('t_gel_arm', [ev(Route.gel, Ester.T, 50, at(-12), { [ExtraKey.gelSite]: 0 })]);
scenario('t_gel_scrotal', [ev(Route.gel, Ester.T, 50, at(-12), { [ExtraKey.gelSite]: 2 })]);
scenario('t_gel_thigh', [ev(Route.gel, Ester.T, 50, at(-12), { [ExtraKey.gelSite]: 1 })]);
scenario('t_patch', [
    ev(Route.patchApply, Ester.T, 4, at(-48), {
        [ExtraKey.releaseRateUGPerDay]: 4000,
        [ExtraKey.patchWearH]: 24,
    }),
]);
scenario('t_patch_first_order', [
    ev(Route.patchApply, Ester.T, 4, at(-48), { [ExtraKey.patchWearH]: 24 }),
]);
scenario('t_patch_remove', [
    ev(Route.patchApply, Ester.T, 4, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 4000,
        [ExtraKey.patchWearH]: 168,
    }),
    ev(Route.patchRemove, Ester.T, 0, at(-24)),
]);
scenario('patch_same_time_tiebreak', [
    ev(Route.patchApply, Ester.E2, 1.56, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 168,
    }, 'zzz'),
    ev(Route.patchApply, Ester.E2, 0.1, at(-120), {
        [ExtraKey.releaseRateUGPerDay]: 100,
        [ExtraKey.patchWearH]: 84,
    }, 'aaa'),
    ev(Route.patchRemove, Ester.E2, 0, at(-24)),
]);
scenario('t_injection_tc', [ev(Route.injection, Ester.TC, 250, at(-240))]);
scenario('t_injection_te', [ev(Route.injection, Ester.TE, 250, at(-240))]);
scenario('t_injection_tu', [ev(Route.injection, Ester.TU, 1000, at(-240))]);
scenario('age_e2_young', [ev(Route.oral, Ester.E2, 2, at(-24))], 70, 1990);
scenario('age_e2_older', [ev(Route.oral, Ester.E2, 2, at(-24))], 70, 1950);
scenario('age_t_older', [ev(Route.injection, Ester.TC, 250, at(-240))], 70, 1950);
scenario('mixed_unsorted', [
    ev(Route.gel, Ester.T, 50, at(-12), { [ExtraKey.gelSite]: 0 }),
    ev(Route.injection, Ester.EV, 5, at(-240)),
    ev(Route.oral, Ester.CPA, 50, at(-24)),
    ev(Route.oral, Ester.E2, 2, at(-24)),
]);
scenario('unmodelled_filtered', [
    ev(Route.oral, Ester.E2, 2, at(-24)),
    ev(Route.oral, Ester.SPIRO, 100, at(-24)),
    ev(Route.oral, Ester.BICA, 50, at(-24)),
]);
scenario('empty', []);
scenario('zero_weight', [ev(Route.oral, Ester.E2, 2, at(-24))], 0);
scenario('invalid_birth_year', [ev(Route.oral, Ester.E2, 2, at(-24))], 70, 1800);

const extraText = (extras: Extras): string => {
    const parts: string[] = [];
    const add = (key: string, value: number | undefined) => {
        if (value !== undefined) parts.push(`${key}=${value}`);
    };
    add('releaseRate', extras[ExtraKey.releaseRateUGPerDay]);
    add('theta', extras[ExtraKey.sublingualTheta]);
    add('tier', extras[ExtraKey.sublingualTier]);
    add('gelSite', extras[ExtraKey.gelSite]);
    add('patchWear', extras[ExtraKey.patchWearH]);
    return parts.length ? parts.join(';') : '-';
};

test('dump upstream PK scenarios and results', () => {
    const outDir = process.env.OYAMA_PK_COMPARE_DIR ?? '/tmp/opencode/pk-compare';
    mkdirSync(outDir, { recursive: true });

    const lines: string[] = ['# oyama-pk-compare-v1'];
    const results: Record<string, unknown> = {};
    const sampleHours = [-12, -6, -1, 0, 24].map(offset => NOW_H + offset);
    for (const s of scenarios) {
        lines.push(
            ['SCENARIO', s.id, s.weight, s.birthYear === null ? 'none' : s.birthYear, NOW_H].join('\t'),
        );
        for (const e of s.events) {
            lines.push(
                [
                    'EVENT',
                    e.route,
                    e.ester,
                    e.doseMG,
                    e.timeH,
                    e.id,
                    extraText(e.extras),
                ].join('\t'),
            );
        }
        const sim = runSimulation(s.events, s.weight, s.birthYear);
        results[s.id] = sim
            ? {
                  ...sim,
                  interp: {
                      e2: sampleHours.map(h => interpolateConcentration_E2(sim, h)),
                      cpa: sampleHours.map(h => interpolateConcentration_CPA(sim, h)),
                      t: sampleHours.map(h => interpolateConcentration_T(sim, h)),
                  },
              }
            : null;
    }

    const theta = [0, 1, 2, 3, 5, 7.5, 10, 12, 15, 20, 30, 60].map(h => ({
        hold: h,
        theta: sublingualThetaFromHold(h),
    }));

    writeFileSync(`${outDir}/scenarios.txt`, `${lines.join('\n')}\n`);
    writeFileSync(`${outDir}/main.json`, JSON.stringify({ nowH: NOW_H, theta, scenarios: results }));
});
