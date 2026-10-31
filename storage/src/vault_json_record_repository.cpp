#include "oyama/storage/vault_json_record_repository.h"

#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace oyama::storage {
namespace {

[[nodiscard]] const char* vault_kind_name(JsonRecordKind kind) {
    switch (kind) {
    case JsonRecordKind::DoseTemplate:
        return "dose_template";
    case JsonRecordKind::QuickDose:
        return "quick_dose";
    case JsonRecordKind::Supply:
        return "supply";
    case JsonRecordKind::PlanItem:
        return "plan_item";
    }
    return "json_record";
}

}  // namespace

VaultJsonRecordRepository::VaultJsonRecordRepository(VaultStore& vault,
                                                     JsonRecordKind kind)
    : vault_(vault), kind_(kind), vault_kind_(vault_kind_name(kind)) {}

JsonRecordKind VaultJsonRecordRepository::kind() const noexcept { return kind_; }

std::vector<JsonRecord> VaultJsonRecordRepository::list(Mode mode) {
    std::vector<JsonRecord> result;
    for (const auto& record : vault_.scan(vault_kind_)) {
        auto decoded = decode_json_record(record.payload, kind_);
        if (decoded.mode == mode) {
            result.push_back(std::move(decoded));
        }
    }
    std::sort(result.begin(), result.end(),
              [](const JsonRecord& left, const JsonRecord& right) {
                  return left.id < right.id;
              });
    return result;
}

std::optional<JsonRecord> VaultJsonRecordRepository::get(std::string_view id) {
    const auto stored = vault_.get(id);
    if (!stored.has_value()) {
        return std::nullopt;
    }
    auto decoded = decode_json_record(stored->payload, kind_);
    if (decoded.id != id) {
        throw VaultCodecError("json_record", std::string(id), "id",
                              "does not match the requested record");
    }
    return decoded;
}

void VaultJsonRecordRepository::upsert(const JsonRecord& record) {
    const auto stamp =
        record.updated_at.has_value() && *record.updated_at > 0
            ? static_cast<std::uint64_t>(*record.updated_at)
            : std::uint64_t{0};
    vault_.put(VaultRecord{record.id, vault_kind_, stamp,
                           encode_json_record(record, kind_)});
}

bool VaultJsonRecordRepository::remove(std::string_view id) {
    return vault_.remove(id);
}

}  // namespace oyama::storage
