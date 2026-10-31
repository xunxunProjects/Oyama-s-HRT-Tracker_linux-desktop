#pragma once

#include "connection.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

struct BackupCategoryCounts {
    std::size_t events{0};
    std::size_t lab_results{0};
    std::size_t dose_templates{0};
    std::size_t quick_doses{0};
    std::size_t supplies{0};
    std::size_t plan{0};
    std::size_t deletions{0};
};

struct BackupStats {
    std::size_t added{0};
    std::size_t updated{0};
    std::size_t skipped{0};
    std::size_t removed{0};
};

struct BackupPreview {
    bool ok{false};
    std::vector<std::string> errors;
    std::vector<std::string> issues;
    std::vector<std::string> missing;
    std::vector<std::string> blocked;
    std::map<std::string, BackupCategoryCounts> counts;
    BackupStats stats;
    std::vector<std::string> removed_ids;
};

struct BackupImportResult {
    BackupPreview preview;
    bool applied{false};
};

/// Parses and validates a backup and computes the LWW merge without writing.
/// Root-envelope errors are returned in `errors` with `ok == false`; category
/// validation errors block only that category and are returned in `blocked`.
[[nodiscard]] BackupPreview preview_backup(Connection& connection,
                                           const std::filesystem::path& path,
                                           std::int64_t now_ms);

/// Applies the same merge in one transaction.  If the envelope is invalid this
/// throws; blocked categories are left untouched and the rest is applied.
[[nodiscard]] BackupImportResult import_backup(Connection& connection,
                                               const std::filesystem::path& path,
                                               std::int64_t now_ms);

/// Exports a complete upstream-compatible bundle.  The file is written with
/// mode 0600.  Invalid rows are omitted and reported through the thrown error
/// only when the bundle cannot be represented at all.
void export_backup(Connection& connection, const std::filesystem::path& path,
                   std::int64_t now_ms);

/// Vault-backed variants.  They read and write the exact same upstream
/// `oyama-hrt-complete-backup` v1 JSON through the encrypted vault.
/// Encrypted files are detected by the magic and opened with `backup_phrase`.
[[nodiscard]] std::string export_backup_json(VaultStore& vault,
                                             std::int64_t now_ms);

[[nodiscard]] BackupPreview preview_backup(
    VaultStore& vault, const std::filesystem::path& path, std::int64_t now_ms,
    std::string_view backup_phrase = {});

[[nodiscard]] BackupImportResult import_backup(
    VaultStore& vault, const std::filesystem::path& path, std::int64_t now_ms,
    std::string_view backup_phrase = {});

/// Writes the upstream-compatible bundle with mode 0600.
void export_backup(VaultStore& vault, const std::filesystem::path& path,
                   std::int64_t now_ms);

/// Writes an encrypted `OYAMAEBK` v1 envelope around the same upstream JSON.
/// The plaintext bundle exists only in memory; the file is written with mode
/// 0600.
void export_encrypted_backup(VaultStore& vault,
                             const std::filesystem::path& path,
                             std::int64_t now_ms,
                             std::string_view backup_phrase);

}  // namespace oyama::storage
