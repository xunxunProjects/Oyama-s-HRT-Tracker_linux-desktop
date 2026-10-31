#pragma once

#include "connection.h"
#include "types.h"
#include "vault_store.h"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace oyama::storage {

struct LegacyDetection {
    bool has_meta{false};
    bool has_legacy{false};
};

[[nodiscard]] LegacyDetection detect_legacy(Connection& source);

struct LegacyCounts {
    std::size_t settings{0};
    std::size_t preferences{0};
    std::size_t dose_events{0};
    std::size_t lab_results{0};
    std::size_t dose_templates{0};
    std::size_t quick_doses{0};
    std::size_t supplies{0};
    std::size_t plan_items{0};
    std::size_t tombstones{0};
};

struct LegacyImportResult {
    LegacyCounts counts;
    std::string source_sha256;
    std::string target_sha256;
};

class LegacyMigrationError : public std::runtime_error {
public:
    LegacyMigrationError(std::string kind, std::string id, std::string field,
                         std::string message);
    [[nodiscard]] const std::string& kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& field() const noexcept { return field_; }

private:
    std::string kind_;
    std::string id_;
    std::string field_;
};

class LegacyMigrator {
public:
    using Progress = std::function<void(std::string_view kind, std::size_t done,
                                        std::size_t total)>;

    LegacyMigrator(Connection& source, VaultStore& target,
                   Progress progress = {});
    [[nodiscard]] LegacyImportResult import_all();

private:
    Connection& source_;
    VaultStore& target_;
    Progress progress_;
};

}  // namespace oyama::storage
