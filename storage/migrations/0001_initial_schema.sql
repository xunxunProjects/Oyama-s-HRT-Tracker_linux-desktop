-- Initial local schema for Oyama's HRT Tracker.
-- The migration runner owns schema_migrations; this file owns application data.

CREATE TABLE settings (
    id boolean PRIMARY KEY DEFAULT true CHECK (id),
    body_weight_kg double precision
        CHECK (body_weight_kg IS NULL OR body_weight_kg > 0),
    birth_year integer
        CHECK (birth_year IS NULL OR birth_year BETWEEN 1900 AND 2200),
    pk_params jsonb
        CHECK (pk_params IS NULL OR jsonb_typeof(pk_params) = 'object'),
    pk_params_updated_at bigint,
    calibration_method text NOT NULL DEFAULT 'mipd'
        CHECK (calibration_method IN ('off', 'ekf', 'ou_kalman', 'mipd')),
    calibration_history_mode text NOT NULL DEFAULT 'retrospective'
        CHECK (calibration_history_mode IN ('forward', 'retrospective')),
    updated_at bigint NOT NULL
);

INSERT INTO settings (id, updated_at) VALUES (true, 0)
ON CONFLICT (id) DO NOTHING;

CREATE TABLE dose_events (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    route text NOT NULL,
    ester text NOT NULL,
    time_h double precision NOT NULL,
    dose_mg double precision NOT NULL,
    extras jsonb NOT NULL DEFAULT '{}'::jsonb
        CHECK (jsonb_typeof(extras) = 'object'),
    updated_at bigint,
    created_at bigint NOT NULL
);

CREATE INDEX dose_events_mode_time_idx ON dose_events (mode, time_h);
CREATE INDEX dose_events_updated_at_idx ON dose_events (updated_at);

CREATE TABLE lab_results (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    time_h double precision NOT NULL,
    value double precision NOT NULL,
    unit text NOT NULL,
    assay text NOT NULL CHECK (assay IN ('immuno', 'lcms')),
    updated_at bigint,
    created_at bigint NOT NULL
);

CREATE INDEX lab_results_mode_time_idx ON lab_results (mode, time_h);
CREATE INDEX lab_results_updated_at_idx ON lab_results (updated_at);

CREATE TABLE dose_templates (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    updated_at bigint
);

CREATE INDEX dose_templates_mode_idx ON dose_templates (mode);

CREATE TABLE quick_doses (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    updated_at bigint
);

CREATE INDEX quick_doses_mode_idx ON quick_doses (mode);

CREATE TABLE supplies (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    updated_at bigint
);

CREATE INDEX supplies_mode_idx ON supplies (mode);

CREATE TABLE plan_items (
    id uuid PRIMARY KEY,
    mode text NOT NULL CHECK (mode IN ('transfem', 'transmasc')),
    payload jsonb NOT NULL CHECK (jsonb_typeof(payload) = 'object'),
    updated_at bigint
);

CREATE INDEX plan_items_mode_idx ON plan_items (mode);

CREATE TABLE tombstones (
    kind text NOT NULL,
    record_id uuid NOT NULL,
    deleted_at bigint NOT NULL,
    PRIMARY KEY (kind, record_id)
);

CREATE INDEX tombstones_deleted_at_idx ON tombstones (deleted_at);

CREATE TABLE preferences (
    key text PRIMARY KEY,
    value jsonb NOT NULL,
    updated_at bigint NOT NULL
);
