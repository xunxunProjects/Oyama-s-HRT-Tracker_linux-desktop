# Encrypted Vault Design

**Status:** approved for planning, 2026-10-31

**Supersedes:** the plaintext-storage assumptions in
`docs/superpowers/specs/2026-10-29-linux-desktop-design.md` §10.3 and §11.
The rest of that document still applies. The approved
`2026-10-30-desktop-ui-overhaul-design.md` still applies, but its
implementation has not started yet; see §13 for sequencing.

## 1. Intent

Keep the existing local PostgreSQL store, but make every business record
unreadable at rest. The database holds only random row UUIDs in clear;
the record kind, timestamps, mode, and payload are encrypted inside one
row blob. A random data-encryption key (DEK) is wrapped by a key derived
from a paper recovery phrase. The phrase and the DEK never touch disk.
Decryption happens in the application process, never in PostgreSQL.

The same C99 ABI is usable from the Qt/C++ app, the CLI, and future C
tools. The storage layer becomes a thin typed adapter over that ABI.

## 2. Threat model

### In scope

- An attacker who copies the PostgreSQL data directory, WAL, or a backup
  file from a powered-off machine and tries to read it offline.
- An attacker who steals the disk or a backup drive.
- A backdoor or other process that runs **while the vault is locked** and
  can read files but cannot read the application's locked key material.
- Accidental plaintext leakage through the database, routine backups, or
  logs.

### Out of scope, and must be documented in the UI/README

- **Same-user malware while the vault is unlocked.** It can read
  `/proc/<pid>/mem`, attach a debugger if the process is dumpable, inject
  into the process, log keystrokes, or call the unlocked ABI. Application
  encryption is not a sandbox. This is the same limitation the old design
  accepted for a live database.
- **Root, kernel, hypervisor, or physical bus attacks.** `mlock` and
  zeroization reduce exposure; they do not defeat them.
- **Cold-boot or hibernation attacks while the app is running.** The DEK
  is in RAM while unlocked. Suspend-to-disk, swap, and hibernation images
  can contain it. Recommend encrypted swap and disabling hibernation.
- **Full database rollback/replacement.** GCM detects tampering with an
  individual row, but an attacker with write access can replace the whole
  database with an older snapshot. The vault has no external monotonic
  counter in v1.
- **Metadata analysis.** Row count, database size, and access times are
  visible. Row UUIDs are random. Settings and preferences use fixed
  singleton row IDs; this reveals only that those singletons exist.
- **Legacy plaintext residue.** A database that existed before this work
  contains plaintext in heap pages, dead tuples, WAL, and free space.
  Encrypting new writes does not erase it. The migration keeps the old
  cluster intact until verification, then asks the user to delete it.
  Deletion is not guaranteed erasure on SSD, CoW filesystems, snapshots,
  RAID, or backups. Full-disk encryption plus destruction of the old
  medium is the only reliable remedy.

### What this design buys

- A stolen database file or backup is useless without the paper phrase,
  subject to Argon2id's offline-guessing cost.
- A locked application has no usable key in its address space.
- The database process and its shared buffers never see plaintext or the
  DEK.

## 3. Goals, non-goals, anti-goals

### Goals

1. All rows in `vault_records` are AES-256-GCM (or XChaCha20-Poly1305
   fallback) encrypted as one blob per record.
2. The only cleartext column is the row UUID.
3. A recovered database plus the paper phrase restores all data on a new
   machine with the same CPU crypto capabilities.
4. Routine backups are encrypted by default.
5. A managed legacy cluster can be migrated without a JSON round trip,
   verified record by record, and only then retired.
6. The C ABI is small, has no global state, and keeps the DEK inside the
   vault object.
7. Losing the paper phrase means permanent loss. The UI says this before
   setup completes.

### Non-goals

- Full-disk encryption implementation. LUKS2 is still recommended and is
  the correct first line of defense for swap, temp files, and the old
  cluster.
- TPM, PKCS#11, hardware tokens, or OS keyring integration.
- Multi-user or network access to the vault.
- Searchable encryption, encrypted indexes, or SQL predicates over
  encrypted fields. History and LWW are computed in memory.
- Key escrow, recovery codes, or a vendor backdoor.
- Changing the pharmacokinetic math or the upstream backup schema.
- Migrating an existing **external** legacy database automatically by
  creating a new database in that server. See §9.3.

### Anti-goals

- A scheme that stores the DEK or a password-derived key in PostgreSQL,
  a config file, a log, or a shell history.
- "Entropy" scoring of user-chosen passwords.
- Promising that `shred` erases SSD/CoW/snapshot copies.
- Silently reading or writing plaintext because a migration failed.
- Making the app unusable when the vault is locked or the phrase is wrong.

## 4. Architecture

```
Qt/QML app ── AppController ── VaultStore (C++20) ── oyama_vault (C99) ── libpq
CLI ──────── commands ────── repositories (C++20) ──┘                    │
                                                                          ▼
                                                                 PostgreSQL
                                                            vault_meta + vault_records
```

- `oyama_vault` is a C99 static library. It owns the libpq connection,
  the KDF, the DEK, and the AEAD. No C++ or Qt types cross the ABI.
- `VaultStore` is a C++20 RAII wrapper. It throws `VaultError` with the
  C status code and message.
- Repositories keep their existing public method names and value types,
  but their constructors take `VaultStore&` instead of `Connection&`.
  They serialize/deserialize the same JSON shapes the upstream backup
  uses.
- `Database` and `LocalCluster` remain. `Database` runs migrations and
  opens the source connection for legacy migration. Business data goes
  through `VaultStore`.
- `backup.cpp` keeps the upstream LWW/merge logic, but reads and writes
  through repositories backed by `VaultStore`.

## 5. Database schema

New migration `storage/migrations/0003_vault_schema.sql`:

```sql
CREATE TABLE vault_meta (
    id              boolean PRIMARY KEY DEFAULT true CHECK (id),
    format_version  integer NOT NULL,
    kdf_algorithm   text    NOT NULL,
    kdf_salt        bytea   NOT NULL,
    kdf_opslimit    bigint  NOT NULL,
    kdf_memlimit    bigint  NOT NULL,
    record_algorithm text   NOT NULL,
    wrap_nonce      bytea   NOT NULL,
    wrapped_dek     bytea   NOT NULL,
    created_at      bigint  NOT NULL,
    updated_at      bigint  NOT NULL
);

CREATE TABLE vault_records (
    id   uuid  PRIMARY KEY,
    blob bytea NOT NULL
);
```

`vault_meta` is a singleton. It contains no secret in clear: the salt is
public, and `wrapped_dek` is AEAD-encrypted with the phrase-derived KEK.
The old tables from migrations 0001/0002 remain in new databases for
compatibility with the migration reader and the upstream backup tests,
but the vault-backed app never reads or writes them. A legacy managed
cluster is read from its old tables and imported into a **new** cluster;
the old cluster is never upgraded in place.

Migration 0001 inserts a default `settings` singleton (`updated_at = 0`,
all optional fields NULL, `calibration_method = 'mipd'`,
`calibration_history_mode = 'retrospective'`). That row is **not** legacy
data. It exists in every database, including a fresh one. Legacy
detection must ignore it and only count it when at least one field differs
from those defaults.

The vault schema deliberately has no indexes other than the primary key.
A scan is `SELECT id, blob FROM vault_records ORDER BY id`; filtering by
kind, mode, or time happens after decryption in memory. Personal data
volumes make this acceptable.

### 5.1 Record IDs

- Dose events, lab results, templates, quick doses, supplies, plan items,
  and tombstones use their existing random UUIDs.
- Settings uses `00000000-0000-0000-0000-000000000001`.
- Preferences uses `00000000-0000-0000-0000-000000000002`.
  All preference keys live in one JSON object; per-key `updatedAt` is
  preserved inside that object, so LWW still works per key.
- A tombstone row has its own random UUID; the deleted record's kind and
  UUID are inside the encrypted payload.

### 5.2 Record kinds

`kind` is one of:

```
settings
preferences
dose_event
lab_result
dose_template
quick_dose
supply
plan_item
tombstone
```

The payload is UTF-8 JSON. It uses the upstream backup field names:
`timeH`, `doseMG`, `concValue`, `updatedAt`, `createdAt`, etc. A
`mode` field (`transfem` or `transmasc`) is added to every mode-scoped
payload so repositories can filter after decryption. The backup exporter
removes the internal `mode` field when writing the upstream bundle.

### 5.3 Timestamps

The envelope's `updated_at_ms` is a mandatory `u64` used only for coarse
scan ordering. A source row with a NULL `updated_at` stores `0` in the
envelope and **omits** `updatedAt` from the payload. The payload's
optional `updatedAt` remains authoritative for LWW merges and backup
round-trips: absent is distinct from `0`. This preserves the existing
`merge_stamped` behavior, which skips a comparison when either side has
no stamp.

For settings the envelope timestamp is `settings.updated_at` (non-null,
default 0). For preferences the envelope timestamp is the maximum key
stamp; each key's `updatedAt` lives in the aggregate payload. For
tombstones the envelope timestamp is `deleted_at`.

## 6. Cryptography

### 6.1 Key hierarchy

```
paper phrase ──Argon2id(salt, ops, mem)──> 32-byte KEK
KEK ──AES-256-GCM or XChaCha20-Poly1305──> 32-byte random DEK
DEK ──AES-256-GCM or XChaCha20-Poly1305──> each record blob
```

- The DEK is 32 random bytes from `randombytes_buf`.
- The KEK is 32 bytes from `crypto_pwhash` with
  `crypto_pwhash_ALG_ARGON2ID13`.
- Production KDF parameters are fixed: `opslimit = 3`,
  `memlimit = 64 * 1024 * 1024` (64 MiB). Setup does not benchmark or
  choose per-machine parameters; it shows a busy indicator while
  Argon2id runs. The constants live in one header and are stored in
  `vault_meta`; changing them later must not make old vaults unreadable.
  Tests may use `setup_with_params` with `opslimit = 1`,
  `memlimit = 8 MiB`.
- The phrase is normalized before derivation: ASCII lowercase, all
  whitespace runs collapsed to one space, leading/trailing whitespace
  removed. The canonical bytes are the KDF input. The C API receives
  already-normalized bytes; `VaultStore` and the CLI/app normalize before
  calling it.
- Setup generates a 16-byte random salt, a random wrap nonce, and wraps
  the DEK. The meta write is one transaction and fails if a meta row
  already exists.
- Changing the phrase generates a new salt and nonce and re-wraps the
  same DEK. Records are not re-encrypted.
- The phrase is never stored. There is no recovery path.

### 6.2 Paper phrase

- Seven words from the EFF long Diceware list (7776 words), chosen with
  `randombytes_uniform(7776)`. Seven words is about 90.5 bits.
- The wordlist is vendored at `vault/data/eff_long_wordlist.txt` with its
  CC BY 3.0 attribution, and compiled into the C++ passphrase module so
  the app and CLI do not depend on a runtime data path.
- The setup UI displays the numbered phrase once, in a monospace,
  non-selectable field, with the warning that anyone who reads it owns the
  data and that losing it means permanent loss. Clipboard actions are
  absent.
- The user must copy it by hand and then type three randomly chosen word
  positions. The challenge is generated in memory. The meta row is not
  written until the challenge passes and the user checks the
  "I understand there is no recovery" box.
- Setup may re-display the phrase until it completes. After the meta row
  is written, the phrase is cleared and never shown again.
- Unlock accepts the phrase in any whitespace layout; normalization is
  the same as setup.
- A wrong phrase returns `OYAMA_VAULT_ERR_BAD_PASSPHRASE`; the UI shows a
  generic error and lets the user retry. Argon2id is the rate limit. No
  lockout that can brick a legitimate user.

### 6.3 AEAD choice and blob format

At setup, choose the record algorithm once and store it in
`vault_meta.record_algorithm`:

- `aes256gcm` if `crypto_aead_aes256gcm_is_available()` is true;
- otherwise `xchacha20poly1305`.

The same algorithm is used for the DEK wrap and record blobs. On a machine
that cannot run the stored algorithm, unlock returns
`OYAMA_VAULT_ERR_UNSUPPORTED` with a clear message; it never silently
degrades.

Record blob layout (all integers big-endian):

```
u8  blob_version = 1
u8  algorithm     (1 = AES-256-GCM, 2 = XChaCha20-Poly1305)
u8  nonce_len     (12 or 24)
u8  reserved      = 0
nonce[nonce_len]
ciphertext[...]
tag[16]
```

The plaintext inside the AEAD is the canonical record envelope:

```
u32 kind_len
kind[kind_len]        UTF-8, no NUL
u64 updated_at_ms
u32 payload_len
payload[payload_len]  UTF-8 JSON
```

AAD is the ASCII string `oyama-vault-record-v1`, followed by the 16-byte
binary UUID of the row, the blob version byte, and the algorithm byte.
This binds the ciphertext to its row and format; swapping two rows or
changing the algorithm/version makes decryption fail.

The nonce is fresh `randombytes_buf` for every encryption. AES-GCM uses
12 bytes; XChaCha20-Poly1305 uses 24 bytes. Nonces are never derived from
the record ID and never reused.

The meta wrap plaintext is the 32-byte DEK. Its AAD is the ASCII string
`oyama-vault-meta-v1`, followed by a canonical encoding of
`format_version`, `kdf_algorithm`, `kdf_salt`, `kdf_opslimit`,
`kdf_memlimit`, and `record_algorithm`. Changing any stored KDF parameter
or algorithm makes unwrapping fail.

### 6.4 Memory hygiene

- The DEK, KEK, and plaintext buffers use `sodium_malloc`/`sodium_free`;
  the DEK and KEK pages are `sodium_mlock`ed.
- `oyama_vault_lock` calls `sodium_memzero` on the DEK and KEK, frees
  them, and marks the vault locked. `close` locks first.
- The app calls `prctl(PR_SET_DUMPABLE, 0)` and sets `RLIMIT_CORE` to zero
  at startup on Linux. On FreeBSD it sets `RLIMIT_CORE` to zero and uses
  the platform's no-dump facility when available. Failure to set these is
  logged but does not abort startup.
- Qt `QString` and QML text can retain copies of the phrase and decrypted
  values. The app minimizes their lifetime and clears models on lock, but
  the design does not claim that every copy is wiped. This is part of the
  residual-risk documentation.
- No phrase, DEK, or plaintext is logged. libpq error messages are
  surfaced only after the vault layer strips query parameters.

## 7. C ABI

Public header: `include/oyama/vault.h`, C99, `extern "C"`, installed with
the CLI. No C++ types, no Qt, no callbacks that throw.

```c
#ifndef OYAMA_VAULT_H
#define OYAMA_VAULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oyama_vault oyama_vault;

typedef struct oyama_error {
    int  code;
    char message[256];
} oyama_error;

typedef enum oyama_vault_status {
    OYAMA_VAULT_OK = 0,
    OYAMA_VAULT_ERR_INVALID_ARGUMENT = 1,
    OYAMA_VAULT_ERR_NOT_INITIALIZED = 2,
    OYAMA_VAULT_ERR_ALREADY_INITIALIZED = 3,
    OYAMA_VAULT_ERR_LOCKED = 4,
    OYAMA_VAULT_ERR_BAD_PASSPHRASE = 5,
    OYAMA_VAULT_ERR_CRYPTO = 6,
    OYAMA_VAULT_ERR_DATABASE = 7,
    OYAMA_VAULT_ERR_CORRUPT = 8,
    OYAMA_VAULT_ERR_UNSUPPORTED = 9,
    OYAMA_VAULT_ERR_NOT_FOUND = 10,
    OYAMA_VAULT_ERR_NOMEM = 11,
    OYAMA_VAULT_ERR_CALLBACK = 12
} oyama_vault_status;

typedef enum oyama_vault_state {
    OYAMA_VAULT_UNINITIALIZED = 0,
    OYAMA_VAULT_LOCKED = 1,
    OYAMA_VAULT_UNLOCKED = 2
} oyama_vault_state;

typedef struct oyama_vault_kdf_params {
    uint64_t opslimit;
    uint64_t memlimit;
} oyama_vault_kdf_params;

typedef struct oyama_buf {
    unsigned char *data;
    size_t         len;
} oyama_buf;

typedef struct oyama_record {
    char       id[37];
    char       kind[64];
    uint64_t   updated_at_ms;
    oyama_buf  payload;
} oyama_record;

typedef int (*oyama_vault_scan_cb)(
    void *user_data,
    const char *id,
    const char *kind,
    uint64_t updated_at_ms,
    const unsigned char *payload,
    size_t payload_len);

int oyama_vault_open(const char *conninfo, oyama_vault **out,
                     oyama_error *err);
void oyama_vault_close(oyama_vault *vault);

int oyama_vault_get_state(const oyama_vault *vault, oyama_vault_state *out,
                          oyama_error *err);

int oyama_vault_setup(oyama_vault *vault, const char *phrase,
                      size_t phrase_len, oyama_error *err);
int oyama_vault_setup_with_params(oyama_vault *vault, const char *phrase,
                                  size_t phrase_len,
                                  const oyama_vault_kdf_params *params,
                                  oyama_error *err);
int oyama_vault_unlock(oyama_vault *vault, const char *phrase,
                       size_t phrase_len, oyama_error *err);
int oyama_vault_change_phrase(oyama_vault *vault, const char *new_phrase,
                              size_t new_len, oyama_error *err);
void oyama_vault_lock(oyama_vault *vault);

int oyama_vault_put(oyama_vault *vault, const char *id, const char *kind,
                    uint64_t updated_at_ms, const void *payload,
                    size_t payload_len, oyama_error *err);
int oyama_vault_get(oyama_vault *vault, const char *id, oyama_record *out,
                    oyama_error *err);
int oyama_vault_remove(oyama_vault *vault, const char *id,
                       oyama_error *err);
int oyama_vault_scan(oyama_vault *vault, const char *kind_or_null,
                     oyama_vault_scan_cb callback, void *user_data,
                     oyama_error *err);
int oyama_vault_clear(oyama_vault *vault, oyama_error *err);

int oyama_vault_begin(oyama_vault *vault, oyama_error *err);
int oyama_vault_commit(oyama_vault *vault, oyama_error *err);
int oyama_vault_rollback(oyama_vault *vault, oyama_error *err);

void oyama_buf_free(oyama_buf *buf);
void oyama_record_free(oyama_record *record);
const char *oyama_vault_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif
```

### 7.1 ABI rules

- All functions return an `oyama_vault_status` value or a non-negative
  boolean; `err` may be `NULL`. `err->message` is always NUL-terminated
  when `err` is non-NULL.
- `oyama_vault_open` connects but does not require an initialized vault;
  `oyama_vault_get_state` reports `UNINITIALIZED` when `vault_meta` has no row.
- `setup` fails with `ALREADY_INITIALIZED` if a meta row exists. It uses
  production KDF parameters; `setup_with_params` exists for tests and
  advanced provisioning and stores the supplied parameters.
- `unlock` requires a meta row; it returns `BAD_PASSPHRASE` for any
  authentication failure and `UNSUPPORTED` when the stored crypto
  algorithm cannot run on this CPU.
- `put`/`get`/`remove`/`scan`/`clear` require `UNLOCKED`; otherwise they
  return `LOCKED`.
- `scan` decrypts every row, filters by `kind` when non-NULL, and invokes
  the callback. The callback returns 0 to continue or non-zero to stop.
  Stopping is not an error; `scan` returns `OK`. The callback must not call
  back into the vault, retain the pointers, or throw (it is C).
- `get` fills `oyama_record`; the caller must call `oyama_record_free`.
  `oyama_buf_free` is safe on a zeroed or already-freed struct.
- `begin`/`commit`/`rollback` are explicit; the vault does not nest
  transactions. Closing with an open transaction rolls it back. Callers
  must not call `scan` from inside a callback.
- The vault object is single-threaded. Concurrent use is undefined.
- `close(NULL)`, `lock(NULL)`, and the free functions are no-ops on NULL.
- The C library never writes the phrase, DEK, or plaintext to disk, stderr,
  or PostgreSQL. PostgreSQL sees only UUIDs and ciphertext.

## 8. C++ storage layer

### 8.1 `VaultStore`

`storage/include/oyama/storage/vault_store.h`:

```cpp
namespace oyama::storage {

enum class VaultState { Uninitialized, Locked, Unlocked };

struct VaultRecord {
    std::string id;
    std::string kind;
    std::uint64_t updated_at_ms{0};
    std::string payload;
};

class VaultError : public std::runtime_error {
public:
    VaultError(int code, const std::string& message);
    [[nodiscard]] int code() const noexcept;
};

class VaultStore {
public:
    explicit VaultStore(const ConnectionConfig& config);
    ~VaultStore();
    VaultStore(const VaultStore&) = delete;
    VaultStore& operator=(const VaultStore&) = delete;
    VaultStore(VaultStore&&) noexcept;
    VaultStore& operator=(VaultStore&&) noexcept;

    void open();
    [[nodiscard]] VaultState state() const;
    void setup(std::string_view phrase);
    void setup(std::string_view phrase, const oyama_vault_kdf_params& params);
    void unlock(std::string_view phrase);
    void change_phrase(std::string_view new_phrase);
    void lock();
    [[nodiscard]] bool is_unlocked() const noexcept;

    void put(const VaultRecord& record);
    [[nodiscard]] std::optional<VaultRecord> get(std::string_view id);
    void remove(std::string_view id);
    [[nodiscard]] std::vector<VaultRecord> scan(std::string_view kind = {});
    void clear();
    void begin();
    void commit();
    void rollback();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace oyama::storage
```

`ConnectionConfig` gains `std::string to_conninfo() const`, used by both
`Connection` and `VaultStore`. It quotes socket paths and values so a
home directory containing spaces or quotes still works.

### 8.2 Passphrase module

`storage/include/oyama/storage/passphrase.h`:

```cpp
class Passphrase {
public:
    static std::vector<std::string> generate(std::size_t words = 7);
    static std::string join(const std::vector<std::string>& words);
    static std::string normalize(std::string_view phrase);
    static std::vector<std::size_t> challenge(std::size_t word_count,
                                              std::size_t count = 3);
    static bool verify_challenge(const std::vector<std::string>& words,
                                 const std::vector<std::size_t>& positions,
                                 const std::vector<std::string>& answers);
};
```

The 7776-word list is a generated C++ source file in
`storage/src/passphrase_wordlist.cpp`, with the EFF attribution and a
generator script `storage/tools/gen_wordlist.py`. The generated file is
checked in; building does not require Python.

### 8.3 Repositories

The following keep their existing public methods and value types but
replace `Connection&` with `VaultStore&`:

- `DoseEventRepository`
- `LabResultRepository`
- `SettingsRepository`
- `PreferencesRepository`
- `TombstoneRepository`
- `JsonRecordRepository`

They use shared JSON codecs in
`storage/include/oyama/storage/vault_codec.h` /
`storage/src/vault_codec.cpp`. The codecs are strict: malformed or
out-of-range payloads throw `VaultError`/`std::runtime_error`, never
silently drop records. `DoseEventRepository::list(mode)` scans all
`dose_event` rows, decodes, filters by mode, and sorts by `(time_h, id)`
in memory. `count(mode)` uses the same path. `SettingsRepository::get()`
returns defaults if the singleton is missing and creates it on the next
save. `PreferencesRepository` loads/saves one JSON object and preserves
per-key stamps. `TombstoneRepository` scans `tombstone` records and
applies the same prune rules.

### 8.4 Backup changes

`backup.h` changes to:

```cpp
[[nodiscard]] BackupPreview preview_backup(
    VaultStore& vault, const std::filesystem::path& path,
    std::int64_t now_ms, std::string_view backup_phrase = {});

[[nodiscard]] BackupImportResult import_backup(
    VaultStore& vault, const std::filesystem::path& path,
    std::int64_t now_ms, std::string_view backup_phrase = {});

void export_backup(VaultStore& vault, const std::filesystem::path& path,
                   std::int64_t now_ms);  // legacy plaintext, explicit

void export_encrypted_backup(VaultStore& vault,
                             const std::filesystem::path& path,
                             std::int64_t now_ms,
                             std::string_view backup_phrase);
```

The LWW/tombstone logic is unchanged, but `LocalState` is loaded through
the vault repositories and applied with one `VaultStore` transaction.
`export_encrypted_backup` serializes the exact upstream JSON in memory
and encrypts it; it never writes a plaintext temporary file.

Encrypted backup format `oyama-hrt-encrypted-backup` v1:

```
magic "OYAMAEBK" (8 bytes)
u8  format_version = 1
u8  algorithm
u8  reserved[2]
u32 kdf_opslimit
u32 kdf_memlimit
u8  salt[16]
u8  nonce[12 or 24]
ciphertext (the upstream JSON)
tag[16]
```

The backup key is Argon2id(phrase, salt, ops, mem), independent of the
vault meta salt. The AAD is the header bytes through the nonce. Import
auto-detects the magic; a plaintext file is accepted only with the
explicit `--plaintext` flag. A wrong phrase or tampered file is rejected
before any database write. The backup phrase defaults to the vault phrase
but can be supplied separately; a restore into a different vault therefore
needs both phrases.

## 9. Legacy migration

### 9.1 Detection

Detection happens **before** the new migration 0003 is applied to a
legacy database. It runs against the configured database after the old
cluster is started, and it only `SELECT`s.

Legacy data is present when any of these is true:

- `dose_events`, `lab_results`, `dose_templates`, `quick_doses`,
  `supplies`, `plan_items`, `tombstones`, or `preferences` has at least
  one row; or
- the `settings` singleton differs from the migration-0001 default:
  `body_weight_kg IS NOT NULL OR birth_year IS NOT NULL OR
  pk_params IS NOT NULL OR calibration_method <> 'mipd' OR
  calibration_history_mode <> 'retrospective' OR updated_at <> 0`.

The default `settings` row inserted by 0001 is ignored. An empty legacy
schema is therefore not a migration trigger; the app can apply 0003 and
go straight to vault setup.

State table:

| `vault_meta` exists | legacy data exists | action |
|---|---|---|
| no | no | apply 0003, then normal vault setup |
| no | yes | managed: migrate to a new cluster; existing: refuse and offer the JSON path |
| yes | no | normal vault unlock/setup flow |
| yes | yes | **mixed state**: refuse all writes and show recovery instructions |

The mixed state can only arise if someone applies 0003 (or runs vault
setup) and then writes legacy rows with an old binary. The app must not
guess which copy is authoritative. It reports both table sets and tells
the user to export the legacy rows with the old binary and import them
into a fresh vault, or restore a pre-migration backup. No automatic
merge or deletion is performed.

For a managed cluster with legacy data, `Database::migrate()` is **not**
called on the old database. The new cluster gets migrations 0001–0003 and
the old cluster stays at its existing schema version. For an
existing/external legacy database, the app refuses and says to use the
old binary's JSON export plus the new encrypted import path. This is
deliberate: the new app must not create databases or drop tables in a
server it does not own.

### 9.2 Managed cluster migration

Path derivation:

- Let `old` be `AppConfig.data_dir` when explicitly set, otherwise
  `default_data_dir()` (`$XDG_DATA_HOME/oyama-hrt/pgdata`).
- Let `target` be the sibling path `<old>-migrate` (for example
  `$XDG_DATA_HOME/oyama-hrt/pgdata-migrate`). Resolve both to absolute
  lexical paths. Reject symlinks, non-directories, `target == old`, and
  any target that is not exactly the expected sibling. The same rule
  applies to a custom `--data-dir`.
- The old socket directory is the configured one (or the default). The
  target socket directory is `<old socket dir>-migrate`; when the old
  socket directory was the `data_dir/run` fallback, use `target/run`.
  Both clusters may run at the same time because their socket directories
  differ; no TCP is opened.
- `AppConfig.data_dir` is never rewritten. Promotion swaps directories at
  the same path, so an explicit configuration keeps pointing at the same
  path.

Promotion uses directory names as its only journal. No extra state file
is written.

| old | target | `<old>-plaintext-*` | meaning / action |
|---|---|---|---|
| yes | no | none | normal start |
| yes | yes | none | failed target; offer resume or exact-path delete |
| no | yes | exactly one | crashed between the two renames; verify target, then finish `target -> old` |
| yes | no | exactly one | promoted; `old` is the new vault; offer purge of the plaintext sibling |
| yes | yes | any | ambiguous; refuse and require manual inspection |
| no | no | any | refuse; the target may have been deleted |
| any | any | more than one | refuse and require manual inspection |

All renames happen only after both clusters are stopped. The old
directory is renamed to `<old>-plaintext-<UTC timestamp>` in
`YYYYMMDDTHHMMSSZ` form; the target is then renamed to `<old>`. If the
process dies between the renames, the table above defines recovery. The
parent directory is flushed after each rename where the platform supports
it.

Steps:

1. Detect legacy data before applying 0003 (see §9.1). Start the old
   cluster if needed. The importer only `SELECT`s; the old cluster is
   never migrated.
2. Resolve the directory state above. A leftover target is resumed only
   after the user confirms it is from this migration; deleting it
   requires its exact path and typed confirmation.
3. Initialize `target` with `LocalCluster`, apply migrations 0001–0003,
   and open a `VaultStore`.
4. If the target has no vault meta, run setup (phrase display + challenge)
   and write the meta only after the challenge passes. If meta already
   exists, unlock it with the paper phrase instead of generating a new
   one.
5. Import in one target transaction. Begin the transaction, clear
   `vault_records`, read the old tables in batches, strictly encode each
   row, and `put` it. Settings, preferences, and tombstones become their
   singleton/aggregate records. Any validation error rolls back the whole
   import and reports `kind`, source id, and field.
6. Verify logical records, not physical row ids:
   - ordinary records: `(kind, id, envelope updated_at, canonical payload)`;
   - settings: the singleton projection `("settings", canonical payload)`;
   - preferences: one projection per key
     `("preferences", key, key updated_at, canonical value)`, ignoring the
     aggregate row's physical id;
   - tombstones: `(tombstone, target kind + ":" + deleted record id,
     deleted_at, canonical payload)`, ignoring the tombstone's own random
     vault id.
   Canonical payloads use sorted object keys and the §5.3 `updatedAt`
   rule. Compare per-kind counts, every projected record, and a SHA-256
   over the sorted canonical projections. Only when all match is the
   migration successful.
7. Stop the old and target clusters. Rename `old` to
   `<old>-plaintext-<timestamp>`, then `target` to `old`. Start `old`,
   unlock the new vault, and load data. On a rename failure, report the
   state from the table; do not start a half-promoted path.
8. Show the plaintext sibling path and the §10.4 warning. A "Delete old
   plaintext data" action accepts the exact path, refuses symlinks,
   recursively removes it, and still states that deletion is not
   guaranteed erasure on SSD/CoW/snapshots/backups.
9. Failures before step 7 leave the old cluster untouched. The target may
   remain for resume; no configuration is changed until verification
   succeeds.

The import is idempotent on retry: `oyama_vault_clear` and all inserts
run in one transaction, and a crash rolls it back. Vault setup is not
repeated when meta already exists; the user unlocks the existing target.
Strict codec failure is deliberate: managed migration aborts rather than
dropping a row. The JSON fallback (§9.3) inherits the old exporter's
"invalid rows may be omitted" behavior and must say so; it cannot
guarantee zero loss.

The migration runs on a worker thread in the app. The worker owns its
`LocalCluster`, `Database`, and `VaultStore`; the GUI thread only receives
queued progress/result signals. Data access and idle auto-lock are
disabled while the state is `migrating`.

### 9.3 JSON fallback

For cross-machine moves, an unreadable old cluster, or an external legacy
database:

- Old binary: `oyama-hrt-cli backup export --out old.json` (plaintext,
  0600, loud warning).
- New binary: set up a fresh managed vault, then
  `oyama-hrt-cli backup import old.json --plaintext --phrase-stdin`.
- Best path: export directly into a LUKS/VeraCrypt container or tmpfs, or
  stream it: `ssh old 'oyama-hrt-cli backup export --stdout' |
  oyama-hrt-cli backup import --stdin --plaintext --phrase-stdin`.
- If a plaintext JSON file must touch a normal disk, verify the import
  first, then delete it. `shred -n 7 -u -z -x -v` is offered as an
  optional best-effort command, with the explicit caveat that ext4
  journaling, CoW filesystems, snapshots, RAID, SSD wear leveling, and
  sync/backup tools may retain copies. It is not a security guarantee.
- The import buffer uses `sodium_malloc` and is zeroed after use. Pause
  Dropbox/Time Machine/Tracker indexing/sync during migration.

## 10. Application integration

### 10.1 `AppController` states

Add:

```cpp
Q_PROPERTY(QString vaultState READ vaultState NOTIFY changed)
Q_PROPERTY(QString setupPhrase READ setupPhrase NOTIFY changed)
Q_PROPERTY(QVariantList setupChallenge READ setupChallenge NOTIFY changed)
Q_PROPERTY(bool vaultBusy READ vaultBusy NOTIFY changed)
Q_PROPERTY(bool vaultRecoveryAcknowledged READ ... WRITE ... NOTIFY changed)
```

`vaultState` is one of `uninitialized`, `locked`, `unlocked`,
`migrating`, `error`.

Flow:

1. `reload()` loads config and starts the managed cluster. It detects
   legacy data **before** applying 0003. If legacy data exists and the
   backend is managed, it starts the background migration worker
   (§9.2) instead of migrating in place. If legacy data exists and the
   backend is existing, it refuses with the JSON-path instructions. If
   there is no legacy data, it applies migrations and opens `VaultStore`.
2. If no meta: state `uninitialized`; `beginVaultSetup()` generates the
   seven-word phrase and three challenge positions. `setupPhrase` and the
   challenge live only in memory. `confirmVaultSetup(QVariantList answers)`
   verifies the challenge and the acknowledgement box, calls
   `VaultStore::setup`, unlocks, and loads data. Cancel clears both.
3. If meta exists: state `locked`; `unlockVault(QString phrase)` calls
   `VaultStore::unlock` and loads data on success. Wrong phrase leaves the
   state locked and shows a generic error.
4. `lockVault()` clears `doses_`, `curve_`, settings values, phrase
   properties, and calls `VaultStore::lock()`; state becomes `locked`.
5. A 15-minute inactivity `QTimer` calls `lockVault()`. The timer resets
   on user interaction and is disabled while the vault is locked or
   migrating.
6. `main.cpp` installs dump/core limits; `aboutToQuit` calls `lockVault`.
7. No data command is accepted while not `unlocked`; the methods return
   false and set a status message.

Threading: setup, unlock, lock, scans, and CRUD run synchronously on the
GUI thread. Argon2id at 3 ops / 64 MiB takes roughly a second; the QML
gate shows `vaultBusy` and disables its controls. Only legacy migration
runs on a worker thread; the worker owns its own `LocalCluster`,
`Database`, and `VaultStore` and reports progress through queued signals.
The GUI thread does not touch those objects while `vaultState ==
"migrating"`.

The existing `status`, `mode`, `backend`, `doses`, `curve`, settings
properties, and add/update/remove/save invokables keep their names and
semantics. They now read/write through vault-backed repositories.

### 10.2 QML

Add to the current shell (before the pending UI overhaul):

- `app/qml/VaultGate.qml`: full-window overlay, visible when
  `vaultState !== "unlocked"`.
- `app/qml/UnlockVaultPage.qml`: phrase field, unlock button, error text,
  lockout-free retry.
- `app/qml/SetupVaultPage.qml`: numbered phrase display with no selection
  or copy, "I wrote it down" checkbox, three word inputs labeled with the
  challenge positions, "I understand there is no recovery" checkbox,
  confirm and cancel.
- A "Lock" button in the current header, and a migration progress view
  for `migrating`.

The gate is a modal overlay, not a new navigation item. The rest of the
UI cannot be interacted with while locked. The phrase field uses
`echoMode: TextInput.Password`; setup phrase uses `Text` with
`selectByMouse: false`. No clipboard calls exist on either page.

### 10.3 CLI

Add:

```
oyama-hrt-cli vault status
oyama-hrt-cli vault setup [--phrase-stdin] [--kdf-ops N --kdf-mem BYTES]
oyama-hrt-cli vault unlock-check --phrase-stdin
oyama-hrt-cli vault change-phrase --phrase-stdin --new-phrase-stdin
oyama-hrt-cli db migrate-vault [--yes] [--phrase-stdin] [--purge-old PATH]
```

`--kdf-ops`/`--kdf-mem` select the test/advanced provisioning path
(`setup_with_params`); normal setup uses the fixed 3 ops / 64 MiB
parameters. `db migrate-vault` runs the same `LegacyMigrator` as the app,
uses the configured data directory, and never deletes the old cluster by
default: it prints the plaintext sibling path. `--purge-old` accepts only
that exact path and additionally requires `--yes`. The command reports
progress, counts, and the verification digest.

Data commands (`settings`, `doses`, `backup`) gain a global
`--phrase-stdin` option. When the vault is initialized and the command
needs data, the CLI reads one line from stdin, normalizes it, unlocks, and
runs the command. `vault setup` without `--phrase-stdin` prints the
generated phrase, prompts for three challenge words, and reads them from
the terminal; with `--phrase-stdin` it reads a supplied phrase (tests and
advanced provisioning only) and does not echo it.

`db setup` remains responsible for cluster/database creation and
migrations. It does not create the vault meta; `vault setup` does. A
fresh install therefore is: `db setup --managed --yes`, then
`vault setup`, then normal commands with `--phrase-stdin`.

`backup export` defaults to the encrypted format. `--plaintext` selects
the upstream-compatible plaintext format and prints the loud warning.
`backup import` auto-detects; `--plaintext` is required for a plaintext
file. `--backup-phrase-stdin` supplies a separate backup phrase when it
differs from the target vault phrase.

### 10.4 UI copy requirements

Every surface that mentions deletion/erasure must say:

> Deleting the old database directory removes the files this application
> knows about. It does not guarantee erasure on SSDs, copy-on-write
> filesystems, snapshots, RAID, or backups. Full-disk encryption and
> destruction of the old medium are the only reliable remedies.

Every backup warning must say that plaintext backups contain health data
and must be stored only in an encrypted container or tmpfs.

## 11. Failure behavior

| Condition | Behavior |
|---|---|
| No meta row | `uninitialized`; setup gate; data commands refused |
| Wrong phrase | `BAD_PASSPHRASE`; generic UI error; retry allowed |
| Missing/renamed meta | `NOT_INITIALIZED`; setup would overwrite nothing because it refuses if any row exists; user must restore the meta |
| Tampered row / swapped rows | `CORRUPT`; the affected operation fails; no partial write |
| Tampered meta / changed KDF params | unwrap fails; `BAD_PASSPHRASE` or `CORRUPT`; no data write |
| Algorithm unavailable | `UNSUPPORTED`; no fallback decryption |
| Legacy cluster unreadable | stop; keep old cluster; offer JSON path with the old binary |
| Mixed legacy + vault_meta | refuse all writes; show both table sets; JSON export/restore only |
| Legacy row fails strict codec | roll back the whole import; report kind/id/field; old cluster untouched |
| Migration validation mismatch | roll back target import; old cluster untouched; report counts/hash mismatch |
| Migration crash before promotion | target transaction rolls back; old cluster untouched; retry clears target |
| Promotion interrupted between renames | recover from the directory-state table; never start a half-promoted path |
| Target path exists or is a symlink | refuse; offer resume or exact-path delete, never guess |
| Custom data_dir | use the same `-migrate`/`-plaintext-*` sibling rule; never rewrite the config path |
| Backup wrong phrase/tamper | reject before any DB write |
| Disk full during setup/import | transaction rollback; no meta or partial data; clear error |
| Locked/quit | zero DEK/KEK; clear models; no further reads |

## 12. Testing

### C vault tests

- `vault_crypto_test`: Argon2id deterministic vector with fixed params,
  wrap/unwrap, wrong phrase, tampered meta AAD, nonce length, algorithm
  selection, `sodium_memzero` state transitions.
- `vault_store_test` (temp PG): state before/after setup, duplicate
  setup, put/get/scan/remove/clear, kind filtering, lock/unlock, reopen,
  transaction commit/rollback, tampered blob, swapped row IDs, corrupted
  meta, missing tables, wrong phrase, unsupported algorithm simulation.
- A byte-flip matrix test: every byte of a blob is flipped in turn and
  must fail cleanly, never crash.

### C++ storage tests

- `vault_store_test`: C++ wrapper error mapping, conninfo quoting, phrase
  normalization.
- `vault_codec_test`: exact upstream JSON round trips for every record
  kind; malformed JSON/out-of-range values throw.
- Repository tests: settings defaults, dose list/filter/sort, lab, JSON
  records, preferences per-key LWW, tombstones/prune.
- `backup_test`: plaintext and encrypted export/preview/import, LWW and
  tombstones, wrong phrase, tampered header/ciphertext, 0600 mode,
  no-plaintext-temp assertion.

### Migration tests

- Build a temp legacy cluster with migrations 0001/0002 and seed rows in
  every table.
- Run the managed migration against a fresh target cluster.
- Assert per-kind counts, payload equality, SHA-256 equality, old cluster
  still intact on failure, config updated only on success, and the new
  vault unlocks.
- Simulate an import failure at the last record and assert the target is
  empty after rollback.
- Seed a row the strict codec rejects and assert the whole import rolls
  back with kind/id/field in the error; the old cluster is unchanged.
- Exercise the directory-state table: target-only after the first rename,
  promoted state, ambiguous old+target+plaintext, multiple plaintext
  siblings, and a symlinked target. Each must refuse or resume without
  starting the wrong cluster.
- Use a custom `--data-dir` and assert the target/plaintext siblings are
  used and the config path is not rewritten.
- Verify canonical projections for settings, per-key preferences, and
  tombstones; a physical-id-only match must not be accepted.
- Test existing/external backend refusal without touching the server.
- Test mixed legacy + `vault_meta` refusal and the JSON recovery message.

### App tests

- `app_controller_test`: first run shows uninitialized; setup phrase has
  seven words; wrong challenge does not write meta; correct challenge
  unlocks; add dose persists across lock/unlock; wrong unlock stays
  locked; lock clears models; `--self-test` still passes.
- `qt_shell_smoke`: offscreen boot with a configured temp DB reaches the
  setup or unlock gate without crashing; `--self-test` remains
  non-interactive.

### Commands

```
cmake --build build-release -j
PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release \
  --output-on-failure
```

The existing `build-asan` configuration is used for the vault tests; all
new C code is built with ASan/UBSan there. A libFuzzer target for
`oyama_vault_open_blob`/record parsing is added but not required in the
default CTest run; the byte-flip matrix is the always-on check.

## 13. Compatibility and sequencing

- The approved 2026-10-30 UI overhaul is not implemented. This work adds
  the vault gate to the current five-page shell. When the UI overhaul is
  implemented, it must preserve the vault state machine, the setup/unlock
  gate, the no-clipboard rule, the lock button, and the migration
  progress/warning flow. The vault plan's QML files can be restyled, not
  removed.
- The CLI's existing commands keep their names and output formats where
  possible. Tests that assert plaintext backup warning text and schema
  version must be updated for the new default and migration 0003.
- The upstream backup format v1 remains readable and writable. Apps that
  only understand upstream JSON can consume `backup export --plaintext`;
  they cannot read the encrypted default.
- PostgreSQL 14+ remains the floor. libsodium 1.0.18+ is added as a
  dependency (1.0.22 is installed here). `pkg-config` must find
  `libsodium`.
- The old migrations 0001/0002 are not edited. Migration 0003 is
  additive. A database that has already applied 0003 has an empty
  `vault_meta` until setup.

## 14. Decisions left to the builder

These do not change the security or data format:

- Exact QML layout, colors, and wording of the vault gate (subject to the
  copy requirements in §10.4).
- Whether the 15-minute idle timeout is configurable in Settings, and its
  exact UI label.
- Error-message wording that does not reveal whether a phrase was close.
- Whether the CLI's `vault setup` interactive challenge echoes words or
  uses a hidden input; the paper phrase itself is never echoed back after
  setup.
- The generator script's exact implementation, as long as the checked-in
  wordlist has exactly 7776 EFF long words and the attribution remains.
- Whether `vault change-phrase` gets a QML Settings action in this plan or
  a follow-up; the C ABI and CLI support it.
