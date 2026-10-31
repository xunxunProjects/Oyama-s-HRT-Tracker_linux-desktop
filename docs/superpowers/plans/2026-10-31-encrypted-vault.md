# Encrypted Vault Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every PostgreSQL business row unreadable without the user's paper recovery phrase, expose the vault through a small C99 ABI, migrate managed legacy clusters into a fresh encrypted cluster, and encrypt backups by default.

**Architecture:** A C99 `oyama_vault` library (libpq + libsodium) owns the data-encryption key and stores `vault_records(id uuid, blob bytea)`. A C++ `VaultStore` wraps that ABI; new `Vault*Repository` classes scan, decrypt, decode, filter, and sort in memory. The Qt app and CLI gate all data access behind setup/unlock. Managed legacy clusters are read with a separate source connection and imported into a new `<data_dir>-migrate` cluster before an atomic directory promotion. Backups are the existing upstream JSON wrapped in an Argon2id + AEAD envelope.

**Tech Stack:** C99, C++20, PostgreSQL 14+, libpq, libsodium 1.0.18+ (Argon2id, AES-256-GCM, XChaCha20-Poly1305), nlohmann/json, Qt 6.4+ QML, CMake/Ninja.

**Spec:** `docs/superpowers/specs/2026-10-31-encrypted-vault-design.md`

## Global Constraints

- PostgreSQL **14+**, Unix sockets only, `listen_addresses=''`, peer/trust auth in tests. No TCP, no cloud, no telemetry.
- `vault_records` has exactly two columns: `id uuid PRIMARY KEY`, `blob bytea NOT NULL`. No business field, kind, timestamp, or mode may appear outside the encrypted blob.
- Production KDF is fixed: Argon2id13, `opslimit=3`, `memlimit=67108864` (64 MiB). `setup_with_params` and `--kdf-ops/--kdf-mem` exist only for tests and advanced provisioning. Never lower the parameters for an existing vault.
- AES-256-GCM when `crypto_aead_aes256gcm_is_available()`, otherwise XChaCha20-Poly1305. Store the chosen record algorithm in `vault_meta`; fail with `OYAMA_VAULT_ERR_UNSUPPORTED` rather than silently downgrading an AES vault on a CPU without AES.
- The only display of a phrase is the one-time setup screen (or the one-time setup output of `vault setup` when it generates a new phrase). No other path logs it, prints it, places it on the clipboard, passes it through argv, or writes it to disk. CLI reads existing phrases only from stdin (`--phrase-stdin`, `--new-phrase-stdin`, `--backup-phrase-stdin`). Zero every phrase buffer with `sodium_memzero` as soon as it is no longer needed.
- The DEK never crosses the `oyama_vault` API boundary. `VaultStore` and repositories only see payload bytes.
- All data operations require an unlocked vault. `AppController` and CLI data commands must refuse with a clear message when locked or uninitialized.
- Legacy managed clusters are **never** migrated with `Database::migrate()`. Detect legacy data before applying `0003`; read the old cluster with `SELECT` only.
- Existing (non-managed) legacy databases are refused in v1 with JSON-export instructions. Only managed clusters get automatic promotion.
- Default `backup export` is encrypted. Plaintext upstream export requires explicit `--plaintext` and must print the unencrypted-health-data warning.
- Tests use `PG_BINDIR=/tmp/opencode/pg18/bin`. Full suite: `cmake --build build-release -j && PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure`. ASan: `cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure`.
- Every task ends with a commit. Do not combine task commits.

## Review Focus

1. **Wrong phrase, tampered blob, swapped row** — unlock fails with `BAD_PASSPHRASE`; a modified or moved ciphertext fails with `CORRUPT`; no plaintext or partial write escapes. Tests in Tasks 1–4 and 12.
2. **Interrupted legacy migration or promotion** — the old cluster is untouched before promotion; a crash between the two renames is resumable from the directory state table. Tests in Task 10.
3. **Backup wrong phrase, tamper, or plaintext fallback** — encrypted backup is rejected before any database write; upstream plaintext still imports when explicitly requested. Tests in Tasks 11–12.
4. **Lock, idle timeout, quit** — models and the setup phrase are cleared, the DEK is zeroed, and data commands are refused immediately. Tests in Tasks 14–15.
5. **First-run setup cancel or wrong challenge** — no `vault_meta` row is written until acknowledgement and all three challenge words match; the phrase is displayed only during setup. Tests in Tasks 14–15.

---

### Task 1: Vault crypto core and public C ABI

**Files:**
- Create: `include/oyama/vault.h`
- Create: `vault/src/vault_crypto.h`
- Create: `vault/src/vault_crypto.c`
- Create: `vault/tests/vault_crypto_test.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: libsodium via `pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)`, CMake 3.21.
- Produces:
  - `include/oyama/vault.h` with the complete C99 declarations from spec §7: `oyama_vault`, `oyama_error`, `oyama_vault_status`, `oyama_vault_state`, `oyama_vault_kdf_params`, `oyama_buf`, `oyama_record`, `oyama_vault_scan_cb`; functions `oyama_vault_open`, `oyama_vault_close`, `oyama_vault_get_state`, `oyama_vault_setup`, `oyama_vault_setup_with_params`, `oyama_vault_unlock`, `oyama_vault_change_phrase`, `oyama_vault_lock`, `oyama_vault_put`, `oyama_vault_get`, `oyama_vault_remove`, `oyama_vault_scan`, `oyama_vault_clear`, `oyama_vault_begin`, `oyama_vault_commit`, `oyama_vault_rollback`, `oyama_buf_free`, `oyama_record_free`, `oyama_vault_status_name`.
  - Internal `vault/src/vault_crypto.h`:
    ```c
    #define OYAMA_VAULT_META_VERSION 1
    #define OYAMA_VAULT_RECORD_VERSION 1
    #define OYAMA_VAULT_ALG_AES256GCM 1
    #define OYAMA_VAULT_ALG_XCHACHA20POLY1305 2
    int oyama_vault_crypto_init(void);
    int oyama_vault_derive_key(const char *phrase, size_t phrase_len,
                               const unsigned char salt[16],
                               uint64_t opslimit, uint64_t memlimit,
                               unsigned char out[32], oyama_error *err);
    int oyama_vault_wrap_dek(const unsigned char dek[32],
                             const unsigned char kek[32],
                             const unsigned char *aad, size_t aad_len,
                             unsigned char alg,
                             unsigned char nonce[24], size_t *nonce_len,
                             unsigned char *out, size_t *out_len,
                             oyama_error *err);
    int oyama_vault_unwrap_dek(const unsigned char *wrapped, size_t wrapped_len,
                               const unsigned char kek[32],
                               const unsigned char *aad, size_t aad_len,
                               unsigned char alg,
                               const unsigned char *nonce, size_t nonce_len,
                               unsigned char dek[32], oyama_error *err);
    int oyama_vault_seal_record(const unsigned char uuid[16],
                                const unsigned char *envelope, size_t envelope_len,
                                const unsigned char dek[32], unsigned char alg,
                                unsigned char **out, size_t *out_len,
                                oyama_error *err);
    int oyama_vault_open_record(const unsigned char uuid[16],
                                const unsigned char *blob, size_t blob_len,
                                const unsigned char dek[32],
                                unsigned char alg, unsigned char **envelope,
                                size_t *envelope_len, oyama_error *err);
    void oyama_vault_secure_free(void *ptr, size_t len);
    ```
- CMake creates `oyama_vault` as a C99 static library with `vault/src/vault_crypto.c`, links `PkgConfig::SODIUM` and `PostgreSQL::PostgreSQL`, and adds `vault_crypto_test`.

- [ ] **Step 1: Write the failing crypto test**

Create `vault/tests/vault_crypto_test.c` with `main` returning nonzero on failure and these assertions:

```c
const unsigned char salt[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
unsigned char k1[32], k2[32];
check(oyama_vault_derive_key("alpha beta gamma", 16, salt, 1, 8388608, k1, &err) == OYAMA_VAULT_OK, "derive");
check(oyama_vault_derive_key("alpha beta gamma", 16, salt, 1, 8388608, k2, &err) == OYAMA_VAULT_OK, "derive again");
check(sodium_memcmp(k1, k2, 32) == 0, "KDF deterministic");
check(oyama_vault_derive_key("alpha beta delta", 16, salt, 1, 8388608, k2, &err) == OYAMA_VAULT_OK, "derive wrong phrase");
check(sodium_memcmp(k1, k2, 32) != 0, "different phrase differs");
```

Add checks for: wrap/unwrap round-trip; wrong KEK returns `OYAMA_VAULT_ERR_BAD_PASSPHRASE`; wrong AAD returns `OYAMA_VAULT_ERR_BAD_PASSPHRASE`; record seal/open round-trip; UUID byte flip fails; each byte of nonce/ciphertext/tag flip fails; AES and XChaCha paths both round-trip; `oyama_vault_status_name` returns non-null for every enum.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release --target vault_crypto_test -j`
Expected: configure fails on missing `vault/src/vault_crypto.c` or the test fails to link.

- [ ] **Step 3: Implement the header and crypto module**

Use `sodium_init()` once in `oyama_vault_crypto_init`. Derive keys with `crypto_pwhash(..., crypto_pwhash_ALG_ARGON2ID13)`. Use `crypto_aead_aes256gcm_*` when available and `crypto_aead_xchacha20poly1305_ietf_*` otherwise. Allocate key material with `sodium_malloc`; zero with `sodium_memzero`; free with `sodium_free`. The record AAD is exactly `"oyama-vault-record-v1"` (21 bytes) followed by the 16 raw UUID bytes, one version byte, and one algorithm byte. The meta AAD is `"oyama-vault-meta-v1"` followed by canonical big-endian format version, KDF id, salt, opslimit, memlimit, and algorithm. Write big-endian integers explicitly, never `memcpy` host structs.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_crypto_test -j && ./build-release/vault_crypto_test`
Expected: `vault_crypto_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add include/oyama/vault.h vault/src/vault_crypto.c vault/src/vault_crypto.h vault/tests/vault_crypto_test.c CMakeLists.txt
git commit -m "feat(vault): add C ABI and authenticated crypto core"
```

---

### Task 2: Vault metadata, setup, unlock, and phrase change

**Files:**
- Create: `storage/migrations/0003_vault_schema.sql`
- Create: `vault/src/vault.c`
- Create: `vault/tests/vault_meta_test.c`
- Create: `vault/tests/run_vault_pg_test.sh`
- Modify: `CMakeLists.txt`
- Modify: `storage/tests/storage_test.cpp` (schema version expectations only)
- Modify: `cli/tests/run_cli_integration.sh`, `cli/tests/run_cli_managed_integration.sh` (schema version expectations only)

**Interfaces:**
- Consumes: `vault/src/vault_crypto.h`, libpq.
- Produces:
  - `0003_vault_schema.sql` exactly as spec §5: `vault_meta` and `vault_records`.
  - `oyama_vault_open/close/state/setup/setup_with_params/unlock/change_phrase/lock` implementations.
  - `run_vault_pg_test.sh <test-binary>` creates a temp cluster, applies `0003_vault_schema.sql` with `psql -v ON_ERROR_STOP=1 -f`, and exports `OYAMA_TEST_PG_SOCKET/DB/USER/PORT`.

- [ ] **Step 1: Write the failing metadata test**

Create `vault/tests/vault_meta_test.c` that reads the four `OYAMA_TEST_PG_*` variables, builds a libpq conninfo, and asserts:

```c
oyama_vault *v = NULL;
check(oyama_vault_open(conninfo, &v, &err) == OYAMA_VAULT_OK, "open");
oyama_vault_state st;
check(oyama_vault_get_state(v, &st, &err) == OYAMA_VAULT_OK && st == OYAMA_VAULT_UNINITIALIZED, "fresh");
oyama_vault_kdf_params low = {1, 8388608};
check(oyama_vault_setup_with_params(v, "correct horse battery staple", 28, &low, &err) == OYAMA_VAULT_OK, "setup");
check(oyama_vault_get_state(v, &st, &err) == OYAMA_VAULT_OK && st == OYAMA_VAULT_LOCKED, "setup locks");
check(oyama_vault_setup(v, "again", 5, &err) == OYAMA_VAULT_ERR_ALREADY_INITIALIZED, "duplicate setup");
check(oyama_vault_unlock(v, "wrong", 5, &err) == OYAMA_VAULT_ERR_BAD_PASSPHRASE, "wrong phrase");
check(oyama_vault_unlock(v, "correct horse battery staple", 28, &err) == OYAMA_VAULT_OK, "unlock");
check(oyama_vault_get_state(v, &st, &err) == OYAMA_VAULT_OK && st == OYAMA_VAULT_UNLOCKED, "unlocked");
```

Add checks for: `lock` returns to locked; reopen returns locked; `change_phrase` while unlocked with `"new phrase"` makes the old phrase fail and the new one succeed; updating `wrapped_dek` or `kdf_salt` in SQL makes unlock fail; `oyama_vault_close(NULL)` and `oyama_vault_lock(NULL)` are no-ops.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_meta_test -j` and then `bash vault/tests/run_vault_pg_test.sh ./build-release/vault_meta_test`
Expected: link failure for missing `vault.c` or `OYAMA_VAULT_ERR_ALREADY_INITIALIZED`.

- [ ] **Step 3: Implement metadata and state**

Use `PQconnectdb`; reject a null/empty conninfo and a null out pointer. `setup_with_params` inserts exactly one `vault_meta` row in a single `BEGIN/COMMIT`; if a row exists, return `ALREADY_INITIALIZED`. `setup` delegates with `{3, 67108864}`. `unlock` selects the row, derives the KEK, unwraps the DEK, and keeps it only in `sodium_malloc` memory. `change_phrase` reuses the current DEK with a fresh salt and nonce and updates the row in a transaction. `state` returns `UNINITIALIZED` when no row exists, otherwise `UNLOCKED` if the in-memory DEK is present, else `LOCKED`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_meta_test -j && bash vault/tests/run_vault_pg_test.sh ./build-release/vault_meta_test`
Expected: `vault_meta_test: ok`.

- [ ] **Step 5: Update schema version assertions and commit**

Change the two `storage_test.cpp` expectations from schema version 2 / two migrations to 3 / three migrations. Also change the `schema version 2` and `applied 2 migration(s)` assertions in `cli/tests/run_cli_integration.sh` and `cli/tests/run_cli_managed_integration.sh` to 3. Run `bash storage/tests/run_with_temp_postgres.sh ./build-release/storage_test` and the two CLI integration scripts.
Expected: pass; the old repositories still operate on the legacy tables and ignore `0003`.

```bash
git add storage/migrations/0003_vault_schema.sql vault/src/vault.c vault/tests/vault_meta_test.c vault/tests/run_vault_pg_test.sh storage/tests/storage_test.cpp cli/tests/run_cli_integration.sh cli/tests/run_cli_managed_integration.sh CMakeLists.txt
git commit -m "feat(vault): add metadata, setup, unlock, and phrase rotation"
```

---

### Task 3: Vault record storage, scan, and transactions

**Files:**
- Modify: `vault/src/vault.c`
- Create: `vault/tests/vault_records_test.c`
- Modify: `vault/tests/run_vault_pg_test.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 vault open/state/meta.
- Produces: `oyama_vault_put/get/remove/scan/clear/begin/commit/rollback`, `oyama_buf_free`, `oyama_record_free`; record blob and plaintext envelope exactly as spec §6.3.

- [ ] **Step 1: Write the failing record test**

```c
setup_low(v); unlock(v, PHRASE);
check(oyama_vault_put(v, id1, "dose_event", 1234, "{\"mode\":\"transfem\"}", 19, &err) == OYAMA_VAULT_OK, "put");
oyama_record rec;
check(oyama_vault_get(v, id1, &rec, &err) == OYAMA_VAULT_OK, "get");
check(strcmp(rec.kind, "dose_event") == 0 && rec.updated_at_ms == 1234 && rec.payload.len == 19, "record fields");
oyama_record_free(&rec);
```

Add checks for: scan with `"dose_event"` returns only that kind; scan with `NULL` returns all; callback returning 1 stops cleanly; put twice updates; remove existing returns OK and missing returns `NOT_FOUND`; clear empties; transaction rollback discards puts; commit persists; locked put/get/scan return `LOCKED`; get missing returns `NOT_FOUND`; invalid UUID/kind/payload length return `INVALID_ARGUMENT`; swap two records' blobs in SQL and assert `CORRUPT`; flip a ciphertext byte and assert `CORRUPT`; empty payload round-trips; 1 MiB payload round-trips.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_records_test -j && bash vault/tests/run_vault_pg_test.sh ./build-release/vault_records_test`
Expected: link failure or `OYAMA_VAULT_ERR_UNSUPPORTED`.

- [ ] **Step 3: Implement record CRUD and scan**

`put` seals the envelope and executes `INSERT ... ON CONFLICT(id) DO UPDATE SET blob = EXCLUDED.blob`. `get` selects the blob, decrypts, parses the envelope, and returns a `sodium_malloc` payload. `scan` executes `SELECT id, blob FROM vault_records ORDER BY id`, decrypts each row, compares the envelope kind when a filter is given, and invokes the callback. `begin` executes `BEGIN`; `commit`/`rollback` execute the matching statement; a second `begin` returns `INVALID_ARGUMENT`; `close` with an open transaction rolls back. Use `PQexecParams` with binary format for `blob` and text for the UUID. Never build SQL by concatenating user values.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_records_test -j && bash vault/tests/run_vault_pg_test.sh ./build-release/vault_records_test`
Expected: `vault_records_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add vault/src/vault.c vault/tests/vault_records_test.c vault/tests/run_vault_pg_test.sh CMakeLists.txt
git commit -m "feat(vault): add encrypted record CRUD, scan, and transactions"
```

---

### Task 4: C++ `VaultStore`, conninfo helper, and paper passphrase

**Files:**
- Create: `storage/include/oyama/storage/vault_store.h`
- Create: `storage/src/vault_store.cpp`
- Create: `storage/include/oyama/storage/passphrase.h`
- Create: `storage/src/passphrase.cpp`
- Create: `storage/src/passphrase_wordlist.cpp`
- Create: `storage/tools/gen_wordlist.py`
- Create: `vault/data/eff_long_wordlist.txt`
- Create: `storage/tests/vault_store_test.cpp`
- Create: `storage/tests/run_vault_store_test.sh`
- Modify: `storage/include/oyama/storage/connection.h`
- Modify: `storage/src/connection.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1–3; `ConnectionConfig`.
- Produces:
  - `std::string ConnectionConfig::to_conninfo() const` in `connection.h`.
  - `enum class VaultState { Uninitialized, Locked, Unlocked };`
  - `struct VaultKdfParams { std::uint64_t opslimit{3}; std::uint64_t memlimit{64 * 1024 * 1024}; };`
  - `struct VaultRecord { std::string id; std::string kind; std::uint64_t updated_at_ms{0}; std::string payload; };`
  - `class VaultError : public std::runtime_error { public: int code() const noexcept; };`
  - `class VaultStore` with `explicit VaultStore(const ConnectionConfig&)`, `open()`, `state()`, `setup(std::string_view, const VaultKdfParams& = {})`, `unlock(std::string_view)`, `change_phrase(std::string_view)`, `lock()`, `is_unlocked()`, `put(const VaultRecord&)`, `get(std::string_view)`, `remove(std::string_view)`, `scan(std::string_view kind = {})`, `clear()`, `begin()`, `commit()`, `rollback()`.
  - `class Passphrase` with `generate(std::size_t words = 7)`, `join`, `normalize`, `challenge(std::size_t word_count, std::size_t count = 3)`, `verify_challenge`.
  - `vault/data/eff_long_wordlist.txt` has exactly 7776 lines, one EFF long-list word per line; `storage/src/passphrase_wordlist.cpp` is generated by `storage/tools/gen_wordlist.py` into a `std::array<std::string_view, 7776>`.
- `VaultStore` normalizes every phrase with `Passphrase::normalize` before calling the C ABI.

- [ ] **Step 1: Write the failing test**

```cpp
VaultStore store(config);
store.open();
check(store.state() == VaultState::Uninitialized, "fresh vault");
store.setup("correct horse battery staple", {1, 8 * 1024 * 1024});
check(store.state() == VaultState::Locked, "setup locks");
store.unlock("correct horse battery staple");
check(store.state() == VaultState::Unlocked, "unlock");
store.put({"00000000-0000-0000-0000-000000000001", "settings", 7, R"({"weight":60})"});
auto got = store.get("00000000-0000-0000-0000-000000000001");
check(got && got->kind == "settings" && got->payload == R"({"weight":60})", "round-trip");
```

Add checks for: wrong phrase throws `VaultError` with `BAD_PASSPHRASE`; scan filters kind; lock then get throws `LOCKED`; `Passphrase::generate(7)` returns 7 entries all in the wordlist; `normalize("  Correct\tHORSE  battery ") == "correct horse battery"`; `challenge(7, 3)` returns three distinct indices in `[0,7)`; `verify_challenge` accepts the right words and rejects a wrong one; `to_conninfo()` quotes a socket path containing a space.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_store_test -j && bash storage/tests/run_vault_store_test.sh ./build-release/vault_store_test`
Expected: missing `vault_store.h` or link error.

- [ ] **Step 3: Implement the wrapper and generator**

`VaultStore` is an RAII owner of `oyama_vault*`; copy is deleted, move is noexcept. Map C status codes to `VaultError` with the C message. `to_conninfo()` uses libpq keyword escaping for single quotes and backslashes. The generator uses `randombytes_uniform(7776)` per word and `sodium_memzero` on temporary buffers. `normalize` lowercases ASCII, splits on any whitespace, and joins with one space. `challenge` uses `randombytes_uniform` with rejection of duplicates. The wordlist script validates 7776 unique lowercase words and emits a C++ header with a license comment.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_store_test -j && bash storage/tests/run_vault_store_test.sh ./build-release/vault_store_test`
Expected: `vault_store_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/vault_store.h storage/src/vault_store.cpp storage/include/oyama/storage/passphrase.h storage/src/passphrase.cpp storage/src/passphrase_wordlist.cpp storage/tools/gen_wordlist.py vault/data/eff_long_wordlist.txt storage/tests/vault_store_test.cpp storage/tests/run_vault_store_test.sh storage/include/oyama/storage/connection.h storage/src/connection.cpp CMakeLists.txt
git commit -m "feat(storage): add C++ vault wrapper and paper passphrase"
```

---

### Task 5: Strict vault codecs

**Files:**
- Create: `storage/include/oyama/storage/vault_codec.h`
- Create: `storage/src/vault_codec.cpp`
- Create: `storage/tests/vault_codec_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `types.h`, nlohmann/json.
- Produces:
  ```cpp
  std::string encode_settings(const Settings&);
  Settings decode_settings(std::string_view);
  std::string encode_preferences(const std::vector<Preference>&);
  std::vector<Preference> decode_preferences(std::string_view);
  std::string encode_dose_event(const DoseEvent&);
  DoseEvent decode_dose_event(std::string_view);
  std::string encode_lab_result(const LabResult&);
  LabResult decode_lab_result(std::string_view);
  std::string encode_json_record(const JsonRecord&, JsonRecordKind);
  JsonRecord decode_json_record(std::string_view, JsonRecordKind);
  std::string encode_tombstone(const Tombstone&);
  Tombstone decode_tombstone(std::string_view);
  ```
- Throws `VaultCodecError : std::runtime_error` with `kind`, `id`, and the failing field in the message. Missing optional `updatedAt` is preserved as `std::nullopt`; an explicit `updatedAt: 0` decodes as `0`.

- [ ] **Step 1: Write the failing codec test**

```cpp
Settings s;
s.body_weight_kg = 61.5;
s.weight_updated_at = 111;
s.birth_year = 1990;
s.birth_year_updated_at = 222;
s.pk_params_json = R"({"e2_kClear":0.4})";
s.pk_params_updated_at = 333;
s.calibration_method = "mipd";
s.calibration_history_mode = "retrospective";
s.updated_at = 444;
check(decode_settings(encode_settings(s)).body_weight_kg == 61.5, "settings weight");
check(decode_settings(encode_settings(s)).updated_at == 444, "settings stamp");

DoseEvent d;
d.id = "00000000-0000-0000-0000-000000000010";
d.mode = Mode::Transmasc;
d.route = "injection";
d.ester = "EN";
d.time_h = 12.5;
d.dose_mg = 4.0;
d.extras_json = R"({"site":"thigh"})";
d.updated_at = 555;
check(decode_dose_event(encode_dose_event(d)).extras_json == d.extras_json, "dose extras");

bool bad_id_threw = false;
try { (void)decode_dose_event(R"({"id":"bad","mode":"transfem"})"); }
catch (const VaultCodecError& e) { bad_id_threw = std::string(e.what()).find("id") != std::string::npos; }
check(bad_id_threw, "invalid id is rejected with the field name");

auto pref_json = encode_preferences({{"theme", R"("dark")", 10}, {"lang", R"("zh")", 20}});
check(decode_preferences(pref_json).size() == 2, "preferences round-trip");
```

Add strict-failure checks for a bad route, ester, negative time, non-finite dose, and unknown typed field. Add round-trips for all four `JsonRecordKind` values, tombstones, labs, and a settings object whose optional fields are all null. Add a check that a dose with `updated_at = std::nullopt` encodes without an `updatedAt` key and decodes back to `nullopt`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_codec_test -j`
Expected: missing `vault_codec.h`.

- [ ] **Step 3: Implement the codecs**

Mirror the field names and validation constants used by `storage/src/backup.cpp`: `timeH`, `doseMG`, `concValue`, `updatedAt`, `createdAt`, `extras`, `mode`, `route`, `ester`, `everyDays`, `timesPerDay`, `pack`, `startedAt`. Reuse `is_uuid`, `parse_mode`, `known_route`/`known_ester` semantics from the existing backup validation. Reject unknown top-level fields only for the typed records; preserve arbitrary objects inside `extras` and `pack`. `encode_preferences` emits `{"items":{key:{"value":<json>,"updatedAt":N}}}`; `decode_preferences` accepts that exact shape and rejects duplicate/unknown keys. Canonical encoding uses `json::dump()` with sorted keys (`json::dump(2, ' ', false, json::error_handler_t::strict)` plus `std::map` ordering) so migration hashes are stable.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_codec_test -j && ./build-release/vault_codec_test`
Expected: `vault_codec_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/vault_codec.h storage/src/vault_codec.cpp storage/tests/vault_codec_test.cpp CMakeLists.txt
git commit -m "feat(storage): add strict vault record codecs"
```

---

### Task 6: Vault settings, preferences, and tombstone repositories

**Files:**
- Create: `storage/include/oyama/storage/vault_settings_repository.h`
- Create: `storage/src/vault_settings_repository.cpp`
- Create: `storage/include/oyama/storage/vault_preferences_repository.h`
- Create: `storage/src/vault_preferences_repository.cpp`
- Create: `storage/include/oyama/storage/vault_tombstone_repository.h`
- Create: `storage/src/vault_tombstone_repository.cpp`
- Create: `storage/tests/vault_repository_test.cpp`
- Create: `storage/tests/run_vault_repository_test.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `vault_codec`, `types.h`, `uuid.h`.
- Produces:
  ```cpp
  class VaultSettingsRepository {
  public:
      explicit VaultSettingsRepository(VaultStore& vault);
      Settings get();
      void save(const Settings&);
  private:
      VaultStore& vault_;
  };

  class VaultPreferencesRepository {
  public:
      explicit VaultPreferencesRepository(VaultStore& vault);
      std::optional<Preference> get(std::string_view key);
      std::vector<Preference> list();
      void set(const Preference&);
      bool remove(std::string_view key);
  private:
      VaultStore& vault_;
  };

  class VaultTombstoneRepository {
  public:
      explicit VaultTombstoneRepository(VaultStore& vault);
      void record(const Tombstone&);
      bool remove(std::string_view kind, std::string_view record_id);
      std::vector<Tombstone> list();
      std::size_t prune_before(std::int64_t cutoff);
  private:
      VaultStore& vault_;
  };
  ```
- IDs: settings `00000000-0000-0000-0000-000000000001`; preferences `...0002` holding one JSON object `{"items":{key:{"value":<json>,"updatedAt":N}}}`; tombstones use `generate_uuid_v4()`.

- [ ] **Step 1: Write the failing repository test**

```cpp
VaultStore vault(config);
vault.open();
vault.setup(PHRASE, {1, 8 * 1024 * 1024});
vault.unlock(PHRASE);
VaultSettingsRepository settings(vault);
Settings s = settings.get();
check(s.calibration_method == "mipd" && !s.body_weight_kg.has_value(), "defaults");
s.body_weight_kg = 60.0;
s.updated_at = 123;
settings.save(s);
check(settings.get().body_weight_kg == 60.0, "settings round-trip");

VaultPreferencesRepository prefs(vault);
prefs.set({"theme", R"("dark")", 10});
prefs.set({"lang", R"("zh")", 20});
check(prefs.list().size() == 2, "preference list");
check(prefs.get("theme")->value_json == R"("dark")", "preference get");
check(prefs.remove("theme"), "preference remove");
check(!prefs.get("theme").has_value(), "preference gone");

VaultTombstoneRepository tombs(vault);
tombs.record({"dose_event", "00000000-0000-0000-0000-000000000010", 99});
check(tombs.list().size() == 1, "tombstone list");
check(tombs.remove("dose_event", "00000000-0000-0000-0000-000000000010"), "tombstone remove");
```

Add checks for: `get()` before any save returns default settings without writing a record; two preference keys survive a lock/unlock; `prune_before(100)` removes only older tombstones; raw `vault_records.blob` does not contain `"dark"` or `"60.0"`; `get` after lock throws `LOCKED`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_repository_test -j && bash storage/tests/run_vault_repository_test.sh ./build-release/vault_repository_test`
Expected: missing `vault_settings_repository.h`.

- [ ] **Step 3: Implement the repositories**

All reads scan the vault kind and decode with the strict codecs. Writes happen through `VaultStore::put`; the caller owns transaction boundaries. Preferences load the single `preferences` record, mutate the in-memory map, bump only the changed key's `updatedAt`, and save with an envelope timestamp equal to the maximum key stamp; `list` returns keys sorted ascending. Tombstones are one vault record each; `list` sorts by `(kind, record_id)`; `remove` scans and deletes the matching record.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_repository_test -j && bash storage/tests/run_vault_repository_test.sh ./build-release/vault_repository_test`
Expected: `vault_repository_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/vault_settings_repository.h storage/src/vault_settings_repository.cpp storage/include/oyama/storage/vault_preferences_repository.h storage/src/vault_preferences_repository.cpp storage/include/oyama/storage/vault_tombstone_repository.h storage/src/vault_tombstone_repository.cpp storage/tests/vault_repository_test.cpp storage/tests/run_vault_repository_test.sh CMakeLists.txt
git commit -m "feat(storage): add vault settings, preferences, and tombstone repositories"
```

---

### Task 7: Vault dose and lab repositories

**Files:**
- Create: `storage/include/oyama/storage/vault_dose_event_repository.h`
- Create: `storage/src/vault_dose_event_repository.cpp`
- Create: `storage/include/oyama/storage/vault_lab_result_repository.h`
- Create: `storage/src/vault_lab_result_repository.cpp`
- Modify: `storage/tests/vault_repository_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `vault_codec`, `types.h`.
- Produces:
  ```cpp
  class VaultDoseEventRepository {
  public:
      explicit VaultDoseEventRepository(VaultStore& vault);
      std::vector<DoseEvent> list(Mode mode);
      std::optional<DoseEvent> get(std::string_view id);
      void upsert(const DoseEvent&);
      bool remove(std::string_view id);
      std::size_t count(Mode mode);
  private:
      VaultStore& vault_;
  };

  class VaultLabResultRepository {
  public:
      explicit VaultLabResultRepository(VaultStore& vault);
      std::vector<LabResult> list(Mode mode);
      std::optional<LabResult> get(std::string_view id);
      void upsert(const LabResult&);
      bool remove(std::string_view id);
  private:
      VaultStore& vault_;
  };
  ```

- [ ] **Step 1: Write the failing test**

```cpp
VaultDoseEventRepository doses(vault);
DoseEvent b{"00000000-0000-0000-0000-000000000020", Mode::Transfem, "oral", "E2", 20.0, 2.0, "{}", 3, 1};
DoseEvent a{"00000000-0000-0000-0000-000000000021", Mode::Transfem, "oral", "E2", 10.0, 1.0, "{}", 2, 1};
doses.upsert(b);
doses.upsert(a);
auto list = doses.list(Mode::Transfem);
check(list.size() == 2 && list[0].id == a.id && list[1].id == b.id, "sorted by time then id");
check(doses.list(Mode::Transmasc).empty(), "mode isolation");
check(doses.get(a.id)->dose_mg == 1.0, "dose get");
check(doses.remove(a.id), "dose remove");
check(doses.count(Mode::Transfem) == 1, "dose count");
```

Add the same round-trip/isolation/ordering/removal checks for `VaultLabResultRepository`; add a check that a dose with `updated_at = std::nullopt` round-trips without an `updatedAt` key in the decrypted payload.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_repository_test -j`
Expected: missing headers.

- [ ] **Step 3: Implement the repositories**

`list(mode)` scans kind `"dose_event"` or `"lab_result"`, decodes, filters `mode`, sorts doses by `(time_h, id)` and labs by `(time_h, id)` for deterministic output. `get` uses the vault ID and verifies the decoded kind. `upsert` writes envelope `updated_at_ms = value_or(0)` and the payload from the codec. `remove` deletes the vault row.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_repository_test -j && bash storage/tests/run_vault_repository_test.sh ./build-release/vault_repository_test`
Expected: `vault_repository_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/vault_dose_event_repository.h storage/src/vault_dose_event_repository.cpp storage/include/oyama/storage/vault_lab_result_repository.h storage/src/vault_lab_result_repository.cpp storage/tests/vault_repository_test.cpp CMakeLists.txt
git commit -m "feat(storage): add vault dose and lab repositories"
```

---

### Task 8: Vault JSON record repository

**Files:**
- Create: `storage/include/oyama/storage/vault_json_record_repository.h`
- Create: `storage/src/vault_json_record_repository.cpp`
- Modify: `storage/tests/vault_repository_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `vault_codec`, `types.h`.
- Produces:
  ```cpp
  class VaultJsonRecordRepository {
  public:
      VaultJsonRecordRepository(VaultStore& vault, JsonRecordKind kind);
      JsonRecordKind kind() const noexcept;
      std::vector<JsonRecord> list(Mode mode);
      std::optional<JsonRecord> get(std::string_view id);
      void upsert(const JsonRecord&);
      bool remove(std::string_view id);
  private:
      VaultStore& vault_;
      JsonRecordKind kind_;
      std::string vault_kind_;
  };
  ```
- Kind strings: `dose_template`, `quick_dose`, `supply`, `plan_item`.

- [ ] **Step 1: Write the failing test**

```cpp
VaultJsonRecordRepository templates(vault, JsonRecordKind::DoseTemplate);
JsonRecord rec{"00000000-0000-0000-0000-000000000030", Mode::Transmasc,
               R"({"id":"00000000-0000-0000-0000-000000000030","name":"weekly","route":"injection","ester":"EN","doseMG":5,"extras":{},"createdAt":1})",
               std::nullopt};
templates.upsert(rec);
check(templates.list(Mode::Transmasc).size() == 1, "template list");
check(templates.get(rec.id)->id == rec.id, "template id");
check(nlohmann::json::parse(templates.get(rec.id)->payload_json) ==
          nlohmann::json::parse(rec.payload_json),
      "template payload");
VaultJsonRecordRepository quick(vault, JsonRecordKind::QuickDose);
check(quick.list(Mode::Transmasc).empty(), "kind isolation");
check(templates.remove(rec.id), "template remove");
```

Add a test for each of the four kinds, and a check that a payload with an invalid route throws `VaultCodecError` on upsert.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target vault_repository_test -j`
Expected: missing `vault_json_record_repository.h`.

- [ ] **Step 3: Implement the repository**

Map `JsonRecordKind` to the vault kind string. `list` scans, filters mode, and sorts by `id`. `upsert` uses the strict codec; `get` verifies the decoded kind; `remove` deletes by ID.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target vault_repository_test -j && bash storage/tests/run_vault_repository_test.sh ./build-release/vault_repository_test`
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/vault_json_record_repository.h storage/src/vault_json_record_repository.cpp storage/tests/vault_repository_test.cpp CMakeLists.txt
git commit -m "feat(storage): add vault JSON record repository"
```

---

### Task 9: Legacy detection, import, and canonical verification

**Files:**
- Create: `storage/include/oyama/storage/legacy_migration.h`
- Create: `storage/src/legacy_migration.cpp`
- Create: `storage/tests/legacy_migration_test.cpp`
- Create: `storage/tests/legacy_seed.sql`
- Create: `storage/tests/run_legacy_migration_test.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Connection` (source), `VaultStore` (target), `vault_codec`, all `Vault*Repository` classes.
- Produces:
  ```cpp
  struct LegacyDetection { bool has_meta{false}; bool has_legacy{false}; };
  LegacyDetection detect_legacy(Connection& source);
  struct LegacyCounts {
      std::size_t settings{0}, preferences{0}, dose_events{0}, lab_results{0},
                    dose_templates{0}, quick_doses{0}, supplies{0}, plan_items{0}, tombstones{0};
  };
  struct LegacyImportResult { LegacyCounts counts; std::string source_sha256; std::string target_sha256; };
  class LegacyMigrationError : public std::runtime_error {
  public:
      LegacyMigrationError(std::string kind, std::string id, std::string field, std::string message);
      const std::string& kind() const noexcept;
      const std::string& id() const noexcept;
      const std::string& field() const noexcept;
  };
  class LegacyMigrator {
  public:
      using Progress = std::function<void(std::string_view kind, std::size_t done, std::size_t total)>;
      LegacyMigrator(Connection& source, VaultStore& target, Progress progress = {});
      LegacyImportResult import_all();
  };
  ```
- `detect_legacy` ignores the default settings row: `updated_at=0`, all optional columns NULL, `calibration_method='mipd'`, `calibration_history_mode='retrospective'`. It reports legacy if any of `dose_events`, `lab_results`, `dose_templates`, `quick_doses`, `supplies`, `plan_items`, `preferences`, `tombstones` has a row, or if the settings row differs from that default. `has_meta` is true only when `vault_meta` exists and contains a row.

- [ ] **Step 1: Write the failing migration test**

`run_legacy_migration_test.sh` creates one temp PostgreSQL server, two databases: `oyama_legacy` with `0001`+`0002` and `legacy_seed.sql`, and `oyama_vault` with `0003`. The test connects a `Connection` to the legacy DB and a `VaultStore` to the vault DB (low KDF), imports, and asserts:

```cpp
auto detection = detect_legacy(source);
check(detection.has_legacy && !detection.has_meta, "legacy detected");
LegacyMigrator migrator(source, vault);
auto result = migrator.import_all();
check(result.counts.dose_events == 2 && result.counts.lab_results == 1, "counts");
check(result.source_sha256 == result.target_sha256, "canonical hashes match");
check(detect_legacy(source).has_legacy, "source unchanged");
```

Add checks for: settings/preferences/tombstones canonical projection; a second import after `vault.clear()` yields the same hashes; a strict invalid legacy row causes `import_all` to throw and leaves `vault_records` empty; `detect_legacy` on a fresh 0001–0003 DB with only the default settings row returns `has_legacy=false`; `has_meta=true` after setup.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target legacy_migration_test -j && bash storage/tests/run_legacy_migration_test.sh ./build-release/legacy_migration_test`
Expected: missing `legacy_migration.h`.

- [ ] **Step 3: Implement detection, import, and verification**

Read each legacy table with `SELECT ... ORDER BY id` (settings by its boolean key). Import the settings row even when it equals the default; the default is ignored only by `detect_legacy`, not by the copy. Encode each row with the strict codecs, put it in the target inside one transaction, and count logical records. Start the transaction with `target.clear()` so a retry replaces any partial prior attempt. For verification, read both sides, project to canonical logical records, sort by `(kind, logical_id)`, serialize deterministically, and hash with `crypto_hash_sha256`. Settings use the singleton ID; preferences project per key; tombstones use `kind:record_id` and ignore their own vault UUID. Any decode error throws `LegacyMigrationError` with kind, id, and field; the transaction rolls back.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target legacy_migration_test -j && bash storage/tests/run_legacy_migration_test.sh ./build-release/legacy_migration_test`
Expected: `legacy_migration_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/legacy_migration.h storage/src/legacy_migration.cpp storage/tests/legacy_migration_test.cpp storage/tests/legacy_seed.sql storage/tests/run_legacy_migration_test.sh CMakeLists.txt
git commit -m "feat(storage): import and verify legacy plaintext data"
```

---

### Task 10: Managed cluster promotion and CLI migration command

**Files:**
- Create: `storage/include/oyama/storage/legacy_promotion.h`
- Create: `storage/src/legacy_promotion.cpp`
- Create: `storage/tests/legacy_promotion_test.cpp`
- Create: `storage/tests/run_legacy_promotion_test.sh`
- Modify: `cli/main.cpp`
- Create: `cli/tests/run_cli_migration.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `LegacyMigrator`, `LocalCluster`, `AppConfig`, `Database`, `VaultStore`.
- Produces:
  ```cpp
  struct LegacyPaths {
      std::filesystem::path data_dir;
      std::filesystem::path target_data_dir;
      std::filesystem::path socket_dir;
      std::filesystem::path target_socket_dir;
  };
  enum class PromotionState { Clean, TargetPending, Promoted, ManualReview };
  LegacyPaths make_legacy_paths(const std::filesystem::path& data_dir,
                                const std::filesystem::path& socket_dir);
  PromotionState inspect_promotion(const LegacyPaths& paths);
  void promote_after_verification(LocalCluster& old_cluster,
                                  LocalCluster& target_cluster,
                                  const LegacyPaths& paths);
  ```
- `make_legacy_paths` derives `<data_dir>-migrate` and `<socket_dir>-migrate` (or `<target>/run` when the socket dir is empty). It rejects symlinks, non-directories, `target == data_dir`, and any target that is not a sibling of `data_dir`.
- `promote_after_verification` stops both clusters, renames `data_dir` to `<data_dir>-plaintext-YYYYMMDDTHHMMSSZ`, renames `target_data_dir` to `data_dir`, and flushes the parent directory. It never updates `AppConfig.data_dir`.
- CLI `db migrate-vault [--yes] [--phrase-stdin] [--purge-old PATH]`: reuses the same code, prints the plaintext path, never deletes by default, and accepts `--purge-old` only when it exactly matches the created plaintext path and `--yes` is present.

- [ ] **Step 1: Write the failing promotion test**

```cpp
auto paths = make_legacy_paths(tmp / "pgdata", tmp / "run");
std::filesystem::create_directories(paths.data_dir);
check(inspect_promotion(paths) == PromotionState::Clean, "clean");
std::filesystem::create_directories(paths.target_data_dir);
check(inspect_promotion(paths) == PromotionState::TargetPending, "target pending");
const auto plaintext = std::filesystem::path(paths.data_dir.string() + "-plaintext-20260101T000000Z");
std::filesystem::rename(paths.data_dir, plaintext);
check(inspect_promotion(paths) == PromotionState::TargetPending, "target ready to promote");
std::filesystem::rename(paths.target_data_dir, paths.data_dir);
check(inspect_promotion(paths) == PromotionState::Promoted, "promoted");
```

Add checks for: two plaintext dirs → `ManualReview`; target with symlink → throw; target equal to old path → throw; custom data dir; idempotent second `inspect`; CLI `db migrate-vault --yes --phrase-stdin` against a temp managed legacy cluster imports and promotes; `--purge-old` with a wrong path is rejected.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target legacy_promotion_test -j && bash storage/tests/run_legacy_promotion_test.sh ./build-release/legacy_promotion_test`
Expected: missing `legacy_promotion.h`.

- [ ] **Step 3: Implement the state machine and CLI command**

`inspect_promotion` reads only directory names and existence; it never starts PostgreSQL. Implement the four states exactly as spec §9.2. The CLI command: load config, refuse `backend=existing` with the JSON-export instructions, detect legacy before `Database::migrate()`, start old cluster read-only, prepare target with `LocalCluster::initialize`, apply migrations, set up or unlock the target vault using the stdin phrase, import, verify hashes, then promote. On any error before promotion, stop target and leave old untouched. Print `plaintext path: <path>` and the §10.4 warning after success.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target legacy_promotion_test -j && bash storage/tests/run_legacy_promotion_test.sh ./build-release/legacy_promotion_test`
Expected: `legacy_promotion_test: ok`; CLI migration script exits 0.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/legacy_promotion.h storage/src/legacy_promotion.cpp storage/tests/legacy_promotion_test.cpp storage/tests/run_legacy_promotion_test.sh cli/main.cpp cli/tests/run_cli_migration.sh CMakeLists.txt
git commit -m "feat(storage): promote migrated managed clusters atomically"
```

---

### Task 11: Vault-backed backup export, preview, and import

**Files:**
- Modify: `storage/include/oyama/storage/backup.h`
- Modify: `storage/src/backup.cpp`
- Modify: `storage/tests/backup_test.cpp`
- Modify: `storage/tests/run_with_temp_postgres.sh` only if a new fixture is needed
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `Vault*Repository`, `vault_codec`.
- Produces overloads (keep the old `Connection&` overloads and `connection.h` include until Task 17; add a `class VaultStore;` forward declaration in `backup.h`):
  ```cpp
  std::string export_backup_json(VaultStore& vault, std::int64_t now_ms);
  BackupPreview preview_backup(VaultStore& vault, const std::filesystem::path& path,
                               std::int64_t now_ms, std::string_view backup_phrase = {});
  BackupImportResult import_backup(VaultStore& vault, const std::filesystem::path& path,
                                   std::int64_t now_ms, std::string_view backup_phrase = {});
  void export_backup(VaultStore& vault, const std::filesystem::path& path, std::int64_t now_ms);
  ```
- The VaultStore overloads read/write the exact upstream `oyama-hrt-complete-backup` v1 JSON. `import_backup` runs inside one `VaultStore` transaction and calls `vault.clear()` before writing the merged state.

- [ ] **Step 1: Write the failing vault backup test**

Create a `VaultStore` fixture, setup/unlock, insert a dose and a lab via `Vault*Repository`, export to a temp file, assert mode 0600, preview, delete all records, import, and assert the records return with the same IDs and payloads. Add checks for: LWW with an older incoming record; tombstone removal; invalid JSON leaves the vault unchanged; `export_backup_json` contains `"format":"oyama-hrt-complete-backup"` and does not contain the phrase.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target backup_test -j && bash storage/tests/run_with_temp_postgres.sh ./build-release/backup_test`
Expected: no `VaultStore&` overload.

- [ ] **Step 3: Implement the vault overloads**

Refactor `LocalState` loading/saving to take a `VaultStore&` and use the `Vault*Repository` classes. Keep the merge logic unchanged. Replace `DELETE FROM ...` with `vault.clear()` inside the import transaction. `export_backup_json` writes to an in-memory string; `export_backup` writes it with mode 0600. `preview_backup` never writes. The `backup_phrase` parameter is accepted but unused until Task 12; if the file is encrypted, throw a clear "encrypted backup support not built" error for now.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target backup_test -j && bash storage/tests/run_with_temp_postgres.sh ./build-release/backup_test`
Expected: `backup_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/backup.h storage/src/backup.cpp storage/tests/backup_test.cpp CMakeLists.txt
git commit -m "feat(storage): run backup merge over the encrypted vault"
```

---

### Task 12: Encrypted backup envelope

**Files:**
- Create: `storage/include/oyama/storage/backup_crypto.h`
- Create: `storage/src/backup_crypto.cpp`
- Create: `storage/tests/backup_crypto_test.cpp`
- Modify: `storage/src/backup.cpp`
- Modify: `storage/tests/backup_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: libsodium directly (`crypto_pwhash`, `crypto_aead_aes256gcm_*`, `crypto_aead_xchacha20poly1305_ietf_*`, `randombytes_buf`). Do not include or call private `vault/src/*` helpers; the backup envelope is independent of the vault record format.
- Produces:
  ```cpp
  bool is_encrypted_backup(std::string_view bytes);
  std::string encrypt_backup_json(std::string_view json, std::string_view phrase);
  std::string decrypt_backup_json(std::string_view bytes, std::string_view phrase);
  ```
- Header exactly `OYAMAEBK` (8 bytes), then `u8 version=1`, `u8 alg`, `u16 reserved=0`, `u32 opslimit`, `u32 memlimit`, `salt[16]`, nonce, ciphertext, tag. `alg` selects the nonce length: 12 bytes for AES-256-GCM, 24 bytes for XChaCha20-Poly1305. AAD is the full header through the nonce; ciphertext length is implicit from the file length. Use Argon2id13 with a fresh 16-byte salt; use AES-256-GCM if available, else XChaCha20-Poly1305.

- [ ] **Step 1: Write the failing encryption test**

```cpp
const std::string json = R"({"format":"oyama-hrt-complete-backup","version":1})";
const std::string phrase = "correct horse battery staple";
auto sealed = encrypt_backup_json(json, phrase);
check(is_encrypted_backup(sealed), "magic detected");
check(decrypt_backup_json(sealed, phrase) == json, "round-trip");
check(sealed.find("oyama-hrt-complete-backup") == std::string::npos, "plaintext absent");
```

Add checks for: wrong phrase throws; each header byte and each ciphertext byte flip throws; truncated file throws; non-magic input is not considered encrypted; two seals of the same JSON differ (random salt/nonce); an `opslimit` above 10 or a `memlimit` above 256 MiB is rejected **before** `crypto_pwhash`, and both below their lower bounds (1 op, 8 MiB) are rejected too.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target backup_crypto_test -j`
Expected: missing `backup_crypto.h`.

- [ ] **Step 3: Implement encryption and wire it into backup**

`encrypt_backup_json` normalizes the phrase, derives a 32-byte key with `crypto_pwhash`, and seals the JSON. `decrypt_backup_json` validates the magic, version, algorithm, reserved bytes, nonce length, and the stored `opslimit`/`memlimit` **before** calling `crypto_pwhash`; accept only `1..10` ops and `8 MiB..256 MiB` memory and throw on anything else. Then derive with the stored salt/parameters and open. In `backup.cpp`, `preview_backup`/`import_backup` read the file bytes, auto-detect encryption, require a non-empty `backup_phrase` when encrypted, and decrypt before parsing. `export_encrypted_backup(VaultStore&, path, now_ms, phrase)` writes `encrypt_backup_json(export_backup_json(...))` with mode 0600. Add the declaration to `backup.h`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build-release --target backup_crypto_test backup_test -j && ./build-release/backup_crypto_test && bash storage/tests/run_with_temp_postgres.sh ./build-release/backup_test`
Expected: both pass.

- [ ] **Step 5: Commit**

```bash
git add storage/include/oyama/storage/backup_crypto.h storage/src/backup_crypto.cpp storage/tests/backup_crypto_test.cpp storage/include/oyama/storage/backup.h storage/src/backup.cpp storage/tests/backup_test.cpp CMakeLists.txt
git commit -m "feat(storage): encrypt backups with Argon2id and AEAD"
```

---

### Task 13: CLI vault commands and unlocked data commands

**Files:**
- Modify: `cli/main.cpp`
- Create: `cli/tests/run_cli_vault_integration.sh`
- Modify: `cli/tests/run_cli_integration.sh`
- Modify: `cli/tests/run_cli_backup_integration.sh`
- Modify: `cli/tests/run_cli_managed_integration.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `Passphrase`, `Vault*Repository`, backup functions.
- Produces CLI behavior:
  ```text
  oyama-hrt-cli vault status
  oyama-hrt-cli vault setup [--phrase-stdin] [--kdf-ops N --kdf-mem BYTES]
  oyama-hrt-cli vault unlock-check --phrase-stdin
  oyama-hrt-cli vault change-phrase --phrase-stdin --new-phrase-stdin
  oyama-hrt-cli db migrate-vault [--yes] [--phrase-stdin] [--purge-old PATH]
  ```
  Global `--phrase-stdin` and `--backup-phrase-stdin`; data commands (`settings`, `doses`, `backup export/preview/import`) open a `VaultStore` and unlock before touching data. `backup export` calls `export_encrypted_backup` by default and **reuses the already-read unlock phrase**; it must not read stdin a second time. `--plaintext` calls `export_backup` and prints `warning\tbackup contains plaintext health data`. `backup import` auto-detects; `--plaintext` is required for an upstream JSON file; it uses `--backup-phrase-stdin` as a separate backup phrase when supplied, otherwise it reuses the vault phrase. Every phrase buffer is cleared with `sodium_memzero` before the command returns.

- [ ] **Step 1: Write the failing CLI integration test**

`run_cli_vault_integration.sh` creates a temp PG, runs `db setup --existing`, `db init`, `vault setup --phrase-stdin --kdf-ops 1 --kdf-mem 8388608`, then:

```bash
"$cli" --socket-dir "$socket" --dbname "$db" vault status
printf 'correct horse battery staple\n' | "$cli" --socket-dir "$socket" --dbname "$db" --phrase-stdin doses add --mode transfem --route oral --ester E2 --dose 2 --time 10
printf 'wrong\n' | "$cli" ... --phrase-stdin doses list; test $? -ne 0
printf 'correct horse battery staple\n' | "$cli" ... --phrase-stdin doses list | grep -q 'E2'
```

Add checks for: `vault status` before setup prints `uninitialized`; after setup prints `locked`; `unlock-check` accepts the phrase; `change-phrase` makes the old phrase fail; `backup export` creates a file starting with `OYAMAEBK` while reading the phrase only once; `backup import` with the wrong backup phrase fails before changing the DB; `backup import` with a separate `--backup-phrase-stdin` succeeds; `--plaintext` export prints the warning; data commands without `--phrase-stdin` fail with a clear locked/usage error.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target oyama-hrt-cli -j && bash cli/tests/run_cli_vault_integration.sh ./build-release/oyama-hrt-cli ./storage/migrations`
Expected: `vault` is not a known command.

- [ ] **Step 3: Implement CLI phrase handling and commands**

Add a `read_secret_from_stdin()` helper that reads at most 4096 bytes, strips one trailing newline, normalizes, and `sodium_memzero`s the raw buffer. Add `open_unlocked_vault(GlobalOptions&)` that builds `VaultStore`, opens it, requires `--phrase-stdin`, unlocks, and returns it together with the normalized phrase; `backup export` passes that same phrase to `export_encrypted_backup` rather than reading stdin again. Keep `db` commands on `Database`/`Connection`. Switch `settings`, `doses`, and `backup` to the `Vault*Repository` classes. For `backup import`, read `--backup-phrase-stdin` once when present and use it for the file; otherwise use the vault phrase. Clear all phrase buffers with `sodium_memzero` before returning from the command.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build-release --target oyama-hrt-cli -j && bash cli/tests/run_cli_vault_integration.sh ./build-release/oyama-hrt-cli ./storage/migrations`
Expected: pass. Then update the existing CLI scripts for schema version 3 and encrypted backup defaults and run all three.

- [ ] **Step 5: Commit**

```bash
git add cli/main.cpp cli/tests/run_cli_vault_integration.sh cli/tests/run_cli_integration.sh cli/tests/run_cli_backup_integration.sh cli/tests/run_cli_managed_integration.sh CMakeLists.txt
git commit -m "feat(cli): add vault commands and unlock data access"
```

---

### Task 14: AppController vault lifecycle

**Files:**
- Modify: `app/src/app_controller.h`
- Modify: `app/src/app_controller.cpp`
- Modify: `app/tests/app_controller_test.cpp`
- Modify: `app/tests/run_app_controller_test.sh`
- Modify: `app/src/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `VaultStore`, `Passphrase`, `Vault*Repository`.
- Produces:
  - `Q_PROPERTY(QString vaultState READ vaultState NOTIFY changed)` with values `unavailable`, `uninitialized`, `locked`, `unlocked`, `migrating`.
  - `Q_PROPERTY(bool vaultBusy READ vaultBusy NOTIFY changed)`.
  - `Q_PROPERTY(QString setupPhrase READ setupPhrase NOTIFY changed)`.
  - `Q_PROPERTY(QVariantList setupChallenge READ setupChallenge NOTIFY changed)`.
  - `Q_INVOKABLE bool beginVaultSetup();`
  - `Q_INVOKABLE bool confirmVaultSetup(const QVariantList& answers, bool acknowledged);`
  - `Q_INVOKABLE bool unlockVault(const QString& phrase);`
  - `Q_INVOKABLE void cancelVaultSetup();`
  - `Q_INVOKABLE void lockVault();`
- `reload()` flow: load config → start managed cluster → detect legacy before `migrate()` → if managed legacy, set `vaultState="unavailable"` and status `"Legacy data requires migration; run oyama-hrt-cli db migrate-vault"`; do not migrate the old cluster, do not open a vault, and do not generate a phrase. Task 16 replaces this interim branch with the setup/worker flow. Otherwise `Database::migrate()` → open `VaultStore` → state `uninitialized`/`locked` → do **not** load data.
- `lockVault()` clears `settings_`, `doses_`, `curve_`, phrase/challenge strings, then `vault_->lock()`. A 15-minute `QTimer` calls it; the timer is stopped when locked/migrating.
- `main.cpp` calls `prctl(PR_SET_DUMPABLE, 0)` on Linux, sets `RLIMIT_CORE` to zero, and connects `aboutToQuit` to lock. No phrase is logged.

- [ ] **Step 1: Write the failing app controller test**

```cpp
AppController app;
app.reload();
check(app.vaultState() == "uninitialized", "fresh vault needs setup");
check(app.beginVaultSetup(), "begin setup");
const QString phrase = app.setupPhrase();
const QStringList words = phrase.split(' ');
check(words.size() == 7 && app.setupChallenge().size() == 3, "7 words and 3 challenges");
QVariantList answers;
for (const auto& position : app.setupChallenge()) {
    answers << words.at(position.toInt());
}
check(!app.confirmVaultSetup(answers, false), "acknowledgement required");
check(app.confirmVaultSetup(answers, true), "setup completes");
check(app.vaultState() == "unlocked", "unlocked after setup");
check(app.addDose("oral", "E2", 2.0, 10.0), "add dose");
app.lockVault();
check(app.vaultState() == "locked" && app.doses().isEmpty(), "lock clears models");
check(!app.unlockVault("wrong"), "wrong phrase rejected");
check(app.unlockVault(phrase), "right phrase unlocks");
```

Keep the `phrase` copy taken from `setupPhrase()` before confirmation; do not add a test-only accessor. Add checks for: a wrong challenge leaves state `uninitialized` and writes no `vault_meta`; cancel clears the phrase and state; `reload()` on an initialized vault returns `locked`; data commands before unlock return false; the idle timer is stopped while locked; `aboutToQuit` locks; a managed legacy fixture returns `unavailable` and does not create `vault_meta` (Task 16 replaces this with the worker flow).

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target app_controller_test -j && bash app/tests/run_app_controller_test.sh ./build-release/app_controller_test`
Expected: no `vaultState` property.

- [ ] **Step 3: Implement the lifecycle**

Replace the direct repository construction with a `std::unique_ptr<VaultStore>`. Keep the existing `reload/addDose/updateDose/saveSettings/removeDose` public signatures; they return false with `status_ = "Vault is locked"` when not unlocked. `beginVaultSetup` generates the phrase with `Passphrase::generate(7)`, stores it in a `QString` (cleared on cancel/confirm), and chooses three challenge positions. `confirmVaultSetup` normalizes answers, verifies, checks acknowledgement, then calls `vault_->setup(phrase.toStdString())`, unlocks, loads data, and clears the phrase. In this task, if `legacy_pending_` is set, `confirmVaultSetup` must not write meta yet: return false and set status to "run `oyama-hrt-cli db migrate-vault`". Task 16 replaces that branch with the worker handoff. `unlockVault` normalizes the input, calls `vault_->unlock`, and loads data. `load_data` uses the `Vault*Repository` classes.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target app_controller_test -j && bash app/tests/run_app_controller_test.sh ./build-release/app_controller_test`
Expected: `app_controller_test: ok`.

- [ ] **Step 5: Commit**

```bash
git add app/src/app_controller.h app/src/app_controller.cpp app/tests/app_controller_test.cpp app/tests/run_app_controller_test.sh app/src/main.cpp CMakeLists.txt
git commit -m "feat(app): gate data access behind vault setup and unlock"
```

---

### Task 15: QML gate, setup, unlock, and lock UI

**Files:**
- Create: `app/qml/VaultGate.qml`
- Create: `app/qml/SetupVaultPage.qml`
- Create: `app/qml/UnlockVaultPage.qml`
- Modify: `app/qml/Main.qml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `AppController` properties/invokables from Task 14.
- Produces: a full-window gate over the existing shell; the shell is visible only when `vaultState === "unlocked"`. Setup shows the numbered phrase, three challenge fields, an acknowledgement checkbox, a busy indicator, and a cancel button. Unlock shows a password field, unlock button, busy indicator, and error text. No copy button and no clipboard API anywhere.

- [ ] **Step 1: Write the failing QML smoke check**

Modify `app/src/main.cpp` so `--self-test` creates the engine, loads `Main.qml`, checks `rootObjects().isEmpty()` is false, finds the child named `vaultGate`, and checks it is visible when the controller has no database (`vaultState == "unavailable"`). Then exit 0. Run with `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software OYAMA_CONFIG_FILE=<temp nonexistent>`. Before the QML files exist, the engine reports `VaultGate is not a type`.

- [ ] **Step 2: Run the smoke check to verify it fails**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software OYAMA_CONFIG_FILE=/tmp/opencode/oyama-no-config ./build-release/oyama-hrt-app --self-test`
Expected: QML type error for `VaultGate`.

- [ ] **Step 3: Implement the gate and pages**

Wrap the existing `Main.qml` shell in an `Item`; add `VaultGate { objectName: "vaultGate"; anchors.fill: parent; visible: app.vaultState !== "unlocked" }` and keep the header, nav, pages, and footer unchanged underneath. Disable the shell (`enabled: app.vaultState === "unlocked"`) so tab focus and shortcuts cannot reach data controls behind the gate. `VaultGate` switches on `vaultState`: `uninitialized` → `SetupVaultPage`; `locked` → `UnlockVaultPage`; `migrating` → a progress panel; `unavailable` → the existing status text. Setup splits `app.setupPhrase` on spaces and displays `1. word` … `7. word`; challenge fields are labeled with `position + 1`; the confirm button calls `app.confirmVaultSetup(answers, acknowledged.checked)`. Add a lock button to the header that calls `app.lockVault()`. Clear challenge fields and the phrase display when the gate leaves setup.

- [ ] **Step 4: Run the smoke check and the full Qt test**

Run: `cmake --build build-release -j && QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software OYAMA_CONFIG_FILE=/tmp/opencode/oyama-no-config ./build-release/oyama-hrt-app --self-test && PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release -R 'qt_shell_smoke|app_controller_test' --output-on-failure`
Expected: pass; no QML warnings.

- [ ] **Step 5: Commit**

```bash
git add app/qml/VaultGate.qml app/qml/SetupVaultPage.qml app/qml/UnlockVaultPage.qml app/qml/Main.qml app/src/main.cpp CMakeLists.txt
git commit -m "feat(app): add vault setup, unlock, and lock UI"
```

---

### Task 16: App legacy migration worker and progress UI

**Files:**
- Create: `app/src/legacy_migration_worker.h`
- Create: `app/src/legacy_migration_worker.cpp`
- Modify: `app/src/app_controller.h`
- Modify: `app/src/app_controller.cpp`
- Modify: `app/qml/VaultGate.qml`
- Create: `app/tests/legacy_migration_worker_test.cpp`
- Create: `app/tests/run_legacy_migration_worker_test.sh`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `LegacyMigrator`, `LegacyPromotion`, `LocalCluster`, `Database`, `VaultStore`.
- Produces:
  ```cpp
  class LegacyMigrationWorker : public QObject {
      Q_OBJECT
  public:
      explicit LegacyMigrationWorker(QObject* parent = nullptr);
  public slots:
      void run(const QString& data_dir, const QString& socket_dir, const QString& phrase);
  signals:
      void progress(const QString& kind, int done, int total);
      void finished();
      void failed(const QString& message, const QString& plaintext_path);
  };
  ```
  The worker creates its own `LocalCluster`, `Database`, `Connection`, and `VaultStore`; it never touches AppController models. In this task, `reload()` changes the interim managed-legacy branch from Task 14 to `vaultState="uninitialized"` with status "Set up a recovery phrase to migrate"; `beginVaultSetup()` is allowed. `confirmVaultSetup()` verifies the phrase and acknowledgement, then starts the worker with that phrase, sets `vaultState="migrating"`, and does **not** call `vault_->setup` in the controller. The phrase is held in a `std::string pending_migration_phrase_`; call `sodium_memzero` on its storage when clearing it after success/failure/cancel. The `QString` received from QML is an unavoidable copy; Qt's implicit-sharing copy is best-effort and is cleared when the invokable returns. The worker does target init + 0001–0003 + `VaultStore::setup` + import + verify + promotion. On `finished`, AppController calls `reload()` and then `unlockVault(QString::fromStdString(pending_migration_phrase_))` (new data dir); on failure it stays `migrating` with `vaultBusy=false`, shows the error and old path, and offers retry or cancel.

- [ ] **Step 1: Write the failing worker test**

The test script creates a temp managed legacy cluster (0001/0002 + seed), writes an `AppConfig` pointing to it, and runs a test binary that constructs `LegacyMigrationWorker`, connects signals to a local event loop, calls `run(data_dir, socket_dir, phrase)`, and asserts `finished` fires, the new `data_dir` has `vault_meta`, the plaintext dir exists, and the new vault unlocks with the phrase. Add a failure case with an invalid legacy row: `failed` fires, the old `data_dir` still exists, and no promotion happened.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build-release --target legacy_migration_worker_test -j && bash app/tests/run_legacy_migration_worker_test.sh ./build-release/legacy_migration_worker_test`
Expected: missing worker class.

- [ ] **Step 3: Implement the worker and controller wiring**

The worker uses the same orchestration as `db migrate-vault`: detect, start old, prepare target, migrate target, setup/unlock, import, verify, promote. Emit progress after each kind and after verification. AppController starts the thread only from `confirmVaultSetup()` after phrase verification + acknowledgement when legacy is pending; never start it from `reload()` (no phrase exists there). Store the phrase as `std::string pending_migration_phrase_` and `sodium_memzero` it when cleared. On success call `reload()`, then `unlockVault(QString::fromStdString(pending_migration_phrase_))`; show `unlocked` only if that unlock succeeds. Do not run the idle timer while `migrating`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build-release --target legacy_migration_worker_test -j && bash app/tests/run_legacy_migration_worker_test.sh ./build-release/legacy_migration_worker_test`
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add app/src/legacy_migration_worker.h app/src/legacy_migration_worker.cpp app/src/app_controller.h app/src/app_controller.cpp app/qml/VaultGate.qml app/tests/legacy_migration_worker_test.cpp app/tests/run_legacy_migration_worker_test.sh CMakeLists.txt
git commit -m "feat(app): migrate legacy managed clusters in a worker"
```

---

### Task 17: Remove plaintext paths, update docs, and run full verification

**Files:**
- Delete: `storage/include/oyama/storage/dose_event_repository.h`, `storage/src/dose_event_repository.cpp`
- Delete: `storage/include/oyama/storage/lab_result_repository.h`, `storage/src/lab_result_repository.cpp`
- Delete: `storage/include/oyama/storage/settings_repository.h`, `storage/src/settings_repository.cpp`
- Delete: `storage/include/oyama/storage/preferences_repository.h`, `storage/src/preferences_repository.cpp`
- Delete: `storage/include/oyama/storage/tombstone_repository.h`, `storage/src/tombstone_repository.cpp`
- Delete: `storage/include/oyama/storage/json_record_repository.h`, `storage/src/json_record_repository.cpp`
- Modify: `storage/src/backup.cpp` (remove `Connection&` overloads)
- Modify: `storage/include/oyama/storage/backup.h`
- Modify: `storage/include/oyama/storage/storage.h`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `storage/README.md`
- Modify: `docs/superpowers/specs/2026-10-29-linux-desktop-design.md` (add a superseded note for §10.3/§11)
- Modify: `cli/tests/run_cli_integration.sh` if any legacy references remain

**Interfaces:**
- Final repository names are the `Vault*` classes. `backup.h` exposes only the `VaultStore&` overloads plus `export_encrypted_backup`.
- Documentation states: libsodium dependency, vault setup phrase, no recovery, encrypted backups, plaintext migration caveat, shred is best-effort, old plaintext cluster path, and full-disk encryption recommendation.

- [ ] **Step 1: Remove the legacy repository and backup overloads**

Delete the old files and their CMake entries. Remove the `Connection&` backup overloads and any `SettingsRepository`/`DoseEventRepository` includes from `storage.h`, `backup.cpp`, `cli/main.cpp`, and tests. Build to find stragglers: `cmake --build build-release -j`.

- [ ] **Step 2: Update documentation**

`README.md`: add `libsodium` to dependencies; document `vault setup`, paper phrase, unlock, `db migrate-vault`, encrypted backup default, `--plaintext` warning, and the fact that deleting the old cluster is not guaranteed erasure. `storage/README.md`: describe `vault_records`, the C ABI boundary, and in-memory filtering. Add a note at the top of the 2026-10-29 design doc that §10.3/§11 are superseded by the 2026-10-31 encrypted vault design.

- [ ] **Step 3: Run the full test suite**

Run: `cmake --build build-release -j && PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure`
Expected: all tests pass or skip with 77 when PostgreSQL tools are unavailable.

- [ ] **Step 4: Run sanitizers and a manual smoke test**

Run: `cmake --build build-asan -j && ctest --test-dir build-asan --output-on-failure` and `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software OYAMA_CONFIG_FILE=/tmp/opencode/oyama-no-config ./build-release/oyama-hrt-app --self-test`.
Expected: no ASan/UBSan reports; the app loads with the gate.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor(storage): remove plaintext repositories and document the vault"
```

---

## Self-Review

- **Spec coverage:** §5 schema → Task 2; §6 crypto/envelope → Tasks 1–3; §7 C ABI → Tasks 1–3; §8 C++ layer → Tasks 4–8; §9 migration → Tasks 9–10, 16; §10 app/CLI → Tasks 13–16; §11 failures → tasks above; §12 tests → each task; §13 backup → Tasks 11–12; §14 builder decisions → remaining UI/layout choices.
- **Type consistency:** `VaultRecord` fields, `VaultKdfParams`, `VaultState`, `LegacyDetection`, `LegacyCounts`, `LegacyImportResult`, `LegacyPaths`, and `PromotionState` are used with the same names in every task.
- **Review Focus:** each of the five failure modes has an owning task and a named test assertion.
- **Security edges checked:** encrypted-backup KDF parameters are bounded before Argon2id; `backup export` reads the unlock phrase once; migration phrases use `std::string` + `sodium_memzero` with Qt copies documented as best-effort; the gate covers `unavailable` and `migrating` and disables the shell underneath.
- **Proportion:** this plan defines interfaces and tests, not full function bodies; the largest code blocks are the C ABI and crypto boundary, which the implementer cannot guess.
