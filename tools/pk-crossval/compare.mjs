#!/usr/bin/env node
/*
 * Compare the C binary curves with the upstream TypeScript model.
 *
 * Usage:
 *   node compare.mjs --scenarios DIR/scenarios.json --curves DIR/curves.bin \
 *     --module /tmp/opencode/pk-crossval/runSimulation.mjs --out DIR \
 *     [--limit 50000000] [--sample-every 251]
 */

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const NOW_H = 480000.0;
const HOURS_PER_YEAR = 365.2425 * 24.0;

function arg(name, fallback = null) {
    const i = process.argv.indexOf(`--${name}`);
    if (i < 0) return fallback;
    if (i + 1 >= process.argv.length) throw new Error(`missing value for --${name}`);
    return process.argv[i + 1];
}

const scenariosPath = arg('scenarios');
const curvesPath = arg('curves');
const modulePath = arg('module');
const outDir = arg('out');
const limit = Number(arg('limit', '50000000'));
const sampleEvery = Number(arg('sample-every', '251'));

if (!scenariosPath || !curvesPath || !modulePath || !outDir) {
    console.error('usage: compare.mjs --scenarios FILE --curves FILE --module FILE --out DIR');
    process.exit(2);
}

fs.mkdirSync(outDir, { recursive: true });
Date.now = () => NOW_H * 3600 * 1000;

const { runSimulation } = await import(pathToFileURL(modulePath).href);

const payload = JSON.parse(fs.readFileSync(scenariosPath, 'utf8'));
const scenarios = payload.scenarios;

const EXTRA_KEYS = {
    releaseRate: 'releaseRateUGPerDay',
    theta: 'sublingualTheta',
    tier: 'sublingualTier',
    gelSite: 'gelSite',
    patchWear: 'patchWearH',
};

function toDoseEvent(e) {
    const extras = {};
    for (const [k, v] of Object.entries(e.extras || {})) {
        extras[EXTRA_KEYS[k] || k] = v;
    }
    return {
        id: e.id,
        route: e.route,
        timeH: e.time_h,
        doseMG: e.dose_mg,
        ester: e.ester,
        extras,
    };
}

function ageYears(birthYear, nowH) {
    if (!birthYear) return null;
    return 1970 + nowH / HOURS_PER_YEAR - (birthYear + 0.5);
}

function ageBand(age) {
    if (age === null) return 'unknown';
    if (age < 10) return 'prepubertal';
    if (age < 13) return 'earlyPuberty';
    if (age < 16) return 'midPuberty';
    if (age < 19) return 'latePuberty';
    if (age <= 75) return 'adult';
    return 'older';
}

function weightBand(weight) {
    if (weight < 50) return '<50';
    if (weight < 70) return '50-70';
    if (weight < 90) return '70-90';
    if (weight < 120) return '90-120';
    return '120+';
}

function readExact(fd, buf) {
    let off = 0;
    while (off < buf.length) {
        const n = fs.readSync(fd, buf, off, buf.length - off, null);
        if (n === 0) throw new Error('unexpected end of curves file');
        off += n;
    }
}

function readU32(fd) {
    const b = Buffer.alloc(4);
    readExact(fd, b);
    return b.readUInt32LE(0);
}

function readDoubles(fd, count) {
    const b = Buffer.alloc(count * 8);
    readExact(fd, b);
    return new Float64Array(b.buffer, b.byteOffset, count);
}

const fd = fs.openSync(curvesPath, 'r');
const magic = Buffer.alloc(8);
readExact(fd, magic);
if (magic.toString('latin1') !== 'OYKV1\u0000\u0000\u0000') {
    throw new Error(`bad curves magic: ${JSON.stringify(magic.toString('latin1'))}`);
}
const cCount = readU32(fd);
if (cCount !== scenarios.length) {
    throw new Error(`scenario count mismatch: C ${cCount}, JSON ${scenarios.length}`);
}

const perScenario = [];
const sample = [];
const histogram = new Array(181).fill(0); // log10(error) -16.0 .. 2.0, 0.1
const routeStats = new Map();
const ageStats = new Map();
const allSampleErrors = [];

let totalPoints = 0;
let comparedScenarios = 0;
let gridMismatches = 0;
let nullResults = 0;
let maxError = 0;
let countGt15 = 0;
let countGt5 = 0;
let sumError = 0;
let sumSqError = 0;
let worst = null;

function statsFor(map, key) {
    let s = map.get(key);
    if (!s) {
        s = { points: 0, max: 0, sum: 0, gt15: 0, gt5: 0 };
        map.set(key, s);
    }
    return s;
}

const startedNs = process.hrtime.bigint();
outer:
for (let si = 0; si < scenarios.length; si++) {
    const scenario = scenarios[si];
    const cIndex = readU32(fd);
    if (cIndex !== si) throw new Error(`scenario index mismatch at ${si}: ${cIndex}`);
    const len = readU32(fd);
    if (len === 0) {
        nullResults++;
        perScenario.push({
            index: si, id: scenario.id, template: scenario.template,
            weight_kg: scenario.weight_kg, birth_year: scenario.birth_year,
            age_years: ageYears(scenario.birth_year, scenario.now_h),
            age_band: ageBand(ageYears(scenario.birth_year, scenario.now_h)),
            points: 0, max_error: null, mean_error: null, count_gt_1_5: 0,
            count_gt_5: 0, grid_ok: false, time_of_max: null, conc_at_max: null,
        });
        continue;
    }

    const cTime = readDoubles(fd, len);
    const cE2 = readDoubles(fd, len);
    const cT = readDoubles(fd, len);

    const events = scenario.events.map(toDoseEvent);
    const ts = runSimulation(
        events,
        scenario.weight_kg,
        scenario.birth_year === 0 ? null : scenario.birth_year,
    );
    if (!ts) {
        nullResults++;
        continue;
    }

    const age = ageYears(scenario.birth_year, scenario.now_h);
    const aBand = ageBand(age);
    const wBand = weightBand(scenario.weight_kg);
    const route = scenario.events.length ? scenario.events[0].route : 'none';
    const ester = scenario.events.length ? scenario.events[0].ester : 'none';
    const n = Math.min(len, ts.timeH.length);
    let gridOk = len === ts.timeH.length;
    if (!gridOk) gridMismatches++;
    for (let j = 0; j < n; j++) {
        if (Math.abs(cTime[j] - ts.timeH[j]) > 1e-9) {
            gridOk = false;
            gridMismatches++;
            break;
        }
    }

    let sMax = 0;
    let sSum = 0;
    let sGt15 = 0;
    let sGt5 = 0;
    let sTime = null;
    let sConc = null;
    let used = 0;

    for (let j = 0; j < n && totalPoints < limit; j++) {
        const e2 = Math.abs(cE2[j] - ts.concPGmL_E2[j]);
        // C and TS both report total testosterone in ng/dL; convert the
        // difference to pg/mL (1 ng/dL = 10 pg/mL) for the common threshold.
        const t = Math.abs(cT[j] - ts.concNGdL_T[j]) * 10.0;
        const err = Math.max(e2, t);
        const conc = Math.max(cE2[j], cT[j] * 10.0);
        const idx = totalPoints;

        totalPoints++;
        used++;
        sumError += err;
        sumSqError += err * err;
        if (err > maxError) {
            maxError = err;
            worst = {
                scenario: si, id: scenario.id, template: scenario.template,
                time_h: cTime[j], conc_pg_ml: conc, error_pg_ml: err,
                error_e2: e2, error_t: t, route, ester, age, weight: scenario.weight_kg,
            };
        }
        if (err > 1.5) countGt15++;
        if (err > 5.0) countGt5++;
        if (err > sMax) {
            sMax = err;
            sTime = cTime[j];
            sConc = conc;
        }
        sSum += err;
        if (err > 1.5) sGt15++;
        if (err > 5.0) sGt5++;

        const h = Math.max(0, Math.min(180, Math.floor((Math.log10(Math.max(err, 1e-16)) + 16.0) / 0.1)));
        histogram[h]++;

        const rs = statsFor(routeStats, route);
        rs.points++; rs.sum += err;
        if (err > rs.max) rs.max = err;
        if (err > 1.5) rs.gt15++;
        if (err > 5.0) rs.gt5++;

        const as = statsFor(ageStats, aBand);
        as.points++; as.sum += err;
        if (err > as.max) as.max = err;
        if (err > 1.5) as.gt15++;
        if (err > 5.0) as.gt5++;

        if (idx % sampleEvery === 0) {
            const row = {
                scenario: si, template: scenario.template, route, ester,
                age_years: age, age_band: aBand, weight_kg: scenario.weight_kg,
                weight_band: wBand, time_h: cTime[j], conc_pg_ml: conc,
                error_pg_ml: err, error_e2: e2, error_t: t,
            };
            sample.push(row);
            allSampleErrors.push(err);
        }

        if (totalPoints >= limit) break;
    }

    comparedScenarios++;
    perScenario.push({
        index: si, id: scenario.id, template: scenario.template,
        weight_kg: scenario.weight_kg, birth_year: scenario.birth_year,
        age_years: age, age_band: aBand, points: used,
        max_error: used ? sMax : null, mean_error: used ? sSum / used : null,
        count_gt_1_5: sGt15, count_gt_5: sGt5, grid_ok: gridOk,
        time_of_max: sTime, conc_at_max: sConc,
    });

    if (totalPoints >= limit) break outer;
}

fs.closeSync(fd);

function percentile(sorted, p) {
    if (sorted.length === 0) return null;
    const i = Math.min(sorted.length - 1, Math.max(0, Math.ceil(p * sorted.length) - 1));
    return sorted[i];
}
allSampleErrors.sort((a, b) => a - b);

const summary = {
    generated_at: new Date().toISOString(),
    limit,
    scenario_count: scenarios.length,
    scenarios_compared: comparedScenarios,
    total_points: totalPoints,
    max_abs_error_pg_ml: maxError,
    count_gt_1_5_pg_ml: countGt15,
    count_gt_5_pg_ml: countGt5,
    pass: maxError <= 5.0 && countGt15 <= 5 && gridMismatches === 0 && nullResults === 0,
    grid_mismatches: gridMismatches,
    null_results: nullResults,
    mean_abs_error_pg_ml: totalPoints ? sumError / totalPoints : null,
    rmse_pg_ml: totalPoints ? Math.sqrt(sumSqError / totalPoints) : null,
    percentiles_pg_ml: {
        p50: percentile(allSampleErrors, 0.50),
        p90: percentile(allSampleErrors, 0.90),
        p99: percentile(allSampleErrors, 0.99),
        p999: percentile(allSampleErrors, 0.999),
        p9999: percentile(allSampleErrors, 0.9999),
        sample_size: allSampleErrors.length,
    },
    worst_case: worst,
    per_route: Object.fromEntries([...routeStats.entries()].map(([k, v]) => [k, {
        points: v.points, max_abs_error_pg_ml: v.max,
        mean_abs_error_pg_ml: v.points ? v.sum / v.points : null,
        count_gt_1_5: v.gt15, count_gt_5: v.gt5,
    }])),
    per_age_band: Object.fromEntries([...ageStats.entries()].map(([k, v]) => [k, {
        points: v.points, max_abs_error_pg_ml: v.max,
        mean_abs_error_pg_ml: v.points ? v.sum / v.points : null,
        count_gt_1_5: v.gt15, count_gt_5: v.gt5,
    }])),
    elapsed_s: Number(process.hrtime.bigint() - startedNs) / 1e9,
};

fs.writeFileSync(path.join(outDir, 'summary.json'), JSON.stringify(summary, null, 2));

const perHeader = 'index,id,template,weight_kg,birth_year,age_years,age_band,points,max_error,mean_error,count_gt_1_5,count_gt_5,grid_ok,time_of_max,conc_at_max';
const perLines = perScenario.map((s) => [
    s.index, s.id, s.template, s.weight_kg, s.birth_year,
    s.age_years === null ? '' : s.age_years.toFixed(6), s.age_band, s.points,
    s.max_error === null ? '' : s.max_error.toExponential(6),
    s.mean_error === null ? '' : s.mean_error.toExponential(6),
    s.count_gt_1_5, s.count_gt_5, s.grid_ok,
    s.time_of_max === null ? '' : s.time_of_max.toFixed(6),
    s.conc_at_max === null ? '' : s.conc_at_max.toExponential(6),
].join(','));
fs.writeFileSync(path.join(outDir, 'per_scenario.csv'), [perHeader, ...perLines].join('\n') + '\n');

const sampleHeader = 'scenario,template,route,ester,age_years,age_band,weight_kg,weight_band,time_h,conc_pg_ml,error_pg_ml,error_e2,error_t';
const sampleLines = sample.map((r) => [
    r.scenario, r.template, r.route, r.ester,
    r.age_years === null ? '' : r.age_years.toFixed(6), r.age_band,
    r.weight_kg, r.weight_band, r.time_h.toFixed(6),
    r.conc_pg_ml.toExponential(6), r.error_pg_ml.toExponential(6),
    r.error_e2.toExponential(6), r.error_t.toExponential(6),
].join(','));
fs.writeFileSync(path.join(outDir, 'errors_sample.csv'), [sampleHeader, ...sampleLines].join('\n') + '\n');

fs.writeFileSync(path.join(outDir, 'histogram.json'), JSON.stringify({
    log10_start: -16.0, log10_step: 0.1, counts: histogram,
}, null, 2));

console.log(JSON.stringify({
    pass: summary.pass,
    total_points: summary.total_points,
    scenarios_compared: summary.scenarios_compared,
    max_abs_error_pg_ml: summary.max_abs_error_pg_ml,
    count_gt_1_5_pg_ml: summary.count_gt_1_5_pg_ml,
    count_gt_5_pg_ml: summary.count_gt_5_pg_ml,
    grid_mismatches: summary.grid_mismatches,
    null_results: summary.null_results,
    elapsed_s: summary.elapsed_s,
    worst_case: summary.worst_case,
}, null, 2));

if (!summary.pass) process.exitCode = 1;
