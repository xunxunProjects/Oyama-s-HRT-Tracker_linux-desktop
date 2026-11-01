#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

/// Largest accepted backup file, ciphertext or plaintext, in bytes.  Preview
/// and import require a regular file no larger than this and never read more
/// than this many bytes.
inline constexpr std::size_t kMaxBackupFileBytes = 64U * 1024U * 1024U;

/// Maximum JSON nesting depth accepted by preview and import.  The check runs
/// before nlohmann::json's recursive parser sees the text.
inline constexpr std::size_t kMaxBackupJsonDepth = 64;

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

/// Vault-backed backup operations.  They read and write the exact same upstream
/// `oyama-hrt-complete-backup` v1 JSON through the encrypted vault.
/// Encrypted files are detected by the magic and opened with `backup_phrase`.
[[nodiscard]] std::string export_backup_json(VaultStore& vault,
                                             std::int64_t now_ms);

/// Applies the same regular-file, symlink, size, and read bounds as preview
/// and returns true when the file starts with the encrypted-backup magic.
/// Throws Error for a missing, symlinked, non-regular, oversize, or unreadable
/// file.
[[nodiscard]] bool backup_file_is_encrypted(
    const std::filesystem::path& path);

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
