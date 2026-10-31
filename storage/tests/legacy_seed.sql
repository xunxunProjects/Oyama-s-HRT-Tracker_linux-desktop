UPDATE settings
SET body_weight_kg = 60.0,
    weight_updated_at = 100,
    birth_year = 1990,
    birth_year_updated_at = 101,
    pk_params = '{"e2_kClear": 0.4}'::jsonb,
    pk_params_updated_at = 102,
    calibration_method = 'mipd',
    calibration_history_mode = 'retrospective',
    updated_at = 103
WHERE id = true;

INSERT INTO dose_events
    (id, mode, route, ester, time_h, dose_mg, extras, updated_at, created_at)
VALUES
    ('00000000-0000-0000-0000-000000000101', 'transfem', 'oral', 'E2',
     10.0, 2.0, '{"site": "oral"}'::jsonb, 110, 100),
    ('00000000-0000-0000-0000-000000000102', 'transmasc', 'injection', 'EN',
     20.0, 5.0, '{}'::jsonb, NULL, 101);

INSERT INTO lab_results
    (id, mode, time_h, value, unit, assay, updated_at, created_at)
VALUES
    ('00000000-0000-0000-0000-000000000103', 'transfem', 30.0, 100.0,
     'pg/ml', 'immuno', 120, 100);

INSERT INTO dose_templates (id, mode, payload, updated_at)
VALUES
    ('00000000-0000-0000-0000-000000000104', 'transfem',
     '{"id":"00000000-0000-0000-0000-000000000104","name":"weekly","route":"injection","ester":"EV","doseMG":2.0,"extras":{},"createdAt":1}'::jsonb,
     130);

INSERT INTO quick_doses (id, mode, payload, updated_at)
VALUES
    ('00000000-0000-0000-0000-000000000105', 'transfem',
     '{"id":"00000000-0000-0000-0000-000000000105","route":"oral","ester":"E2","value":1.0,"createdAt":1}'::jsonb,
     NULL);

INSERT INTO supplies (id, mode, payload, updated_at)
VALUES
    ('00000000-0000-0000-0000-000000000106', 'transfem',
     '{"id":"00000000-0000-0000-0000-000000000106","route":"injection","ester":"EV","pack":{"kind":"vial","ml":10.0,"mgPerMl":40.0},"count":1.0,"startedAt":1}'::jsonb,
     NULL);

INSERT INTO plan_items (id, mode, payload, updated_at)
VALUES
    ('00000000-0000-0000-0000-000000000107', 'transfem',
     '{"id":"00000000-0000-0000-0000-000000000107","route":"injection","ester":"EV","doseMG":2.0,"extras":{},"everyDays":7,"timesPerDay":1}'::jsonb,
     NULL);

INSERT INTO preferences (key, value, updated_at)
VALUES
    ('app-theme', '"dark"'::jsonb, 140),
    ('hrt-lang', '"zh"'::jsonb, 141);

INSERT INTO tombstones (kind, record_id, deleted_at)
VALUES
    ('dose_events', '00000000-0000-0000-0000-000000000199', 150);
