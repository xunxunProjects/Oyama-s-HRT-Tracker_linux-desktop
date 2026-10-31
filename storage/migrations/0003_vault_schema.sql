CREATE TABLE vault_meta (
    id               boolean PRIMARY KEY DEFAULT true CHECK (id),
    format_version   integer NOT NULL,
    kdf_algorithm    text    NOT NULL,
    kdf_salt         bytea   NOT NULL,
    kdf_opslimit     bigint  NOT NULL,
    kdf_memlimit     bigint  NOT NULL,
    record_algorithm text    NOT NULL,
    wrap_nonce       bytea   NOT NULL,
    wrapped_dek      bytea   NOT NULL,
    created_at       bigint  NOT NULL,
    updated_at       bigint  NOT NULL
);

CREATE TABLE vault_records (
    id   uuid  PRIMARY KEY,
    blob bytea NOT NULL
);
