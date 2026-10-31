#include "oyama/storage/vault_lab_result_repository.h"

#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"
#include "oyama/vault.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace oyama::storage {
namespace {

constexpr const char* kLabKind = "lab_result";

std::uint64_t envelope_stamp(
    const std::optional<std::int64_t>& updated_at) {
    if (!updated_at.has_value() || *updated_at <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(*updated_at);
}

}  // namespace

VaultLabResultRepository::VaultLabResultRepository(VaultStore& vault)
    : vault_(vault) {}

std::vector<LabResult> VaultLabResultRepository::list(Mode mode) {
    std::vector<LabResult> results;
    for (const auto& record : vault_.scan(kLabKind)) {
        auto result = decode_lab_result(record.payload);
        if (result.mode == mode) {
            results.push_back(std::move(result));
        }
    }
    std::sort(results.begin(), results.end(),
              [](const LabResult& left, const LabResult& right) {
                  if (left.time_h != right.time_h) {
                      return left.time_h < right.time_h;
                  }
                  return left.id < right.id;
              });
    return results;
}

std::optional<LabResult> VaultLabResultRepository::get(std::string_view id) {
    const auto record = vault_.get(id);
    if (!record.has_value()) {
        return std::nullopt;
    }
    if (record->kind != kLabKind) {
        throw VaultError(OYAMA_VAULT_ERR_CORRUPT,
                         "lab record kind does not match its row");
    }
    auto result = decode_lab_result(record->payload);
    if (result.id != std::string(id)) {
        throw VaultCodecError(kLabKind, std::string(id), "id",
                              "does not match the row id");
    }
    return result;
}

void VaultLabResultRepository::upsert(const LabResult& result) {
    vault_.put(VaultRecord{result.id, kLabKind,
                           envelope_stamp(result.updated_at),
                           encode_lab_result(result)});
}

bool VaultLabResultRepository::remove(std::string_view id) {
    const auto record = vault_.get(id);
    if (!record.has_value()) {
        return false;
    }
    if (record->kind != kLabKind) {
        return false;
    }
    return vault_.remove(id);
}

}  // namespace oyama::storage
