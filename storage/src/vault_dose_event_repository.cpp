#include "oyama/storage/vault_dose_event_repository.h"

#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"
#include "oyama/vault.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace oyama { namespace storage {
namespace {

constexpr const char* kDoseKind = "dose_event";

std::uint64_t envelope_stamp(
    const oyama::optional<std::int64_t>& updated_at) {
    if (!updated_at.has_value() || *updated_at <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(*updated_at);
}

}  // namespace

VaultDoseEventRepository::VaultDoseEventRepository(VaultStore& vault)
    : vault_(vault) {}

std::vector<DoseEvent> VaultDoseEventRepository::list(Mode mode) {
    std::vector<DoseEvent> events;
    for (const auto& record : vault_.scan(kDoseKind)) {
        auto event = decode_dose_event(record.payload);
        if (event.mode == mode) {
            events.push_back(std::move(event));
        }
    }
    std::sort(events.begin(), events.end(),
              [](const DoseEvent& left, const DoseEvent& right) {
                  if (left.time_h != right.time_h) {
                      return left.time_h < right.time_h;
                  }
                  return left.id < right.id;
              });
    return events;
}

oyama::optional<DoseEvent> VaultDoseEventRepository::get(oyama::string_view id) {
    const auto record = vault_.get(id);
    if (!record.has_value()) {
        return oyama::nullopt;
    }
    if (record->kind != kDoseKind) {
        throw VaultError(OYAMA_VAULT_ERR_CORRUPT,
                         "dose record kind does not match its row");
    }
    auto event = decode_dose_event(record->payload);
    if (event.id != std::string(id)) {
        throw VaultCodecError(kDoseKind, std::string(id), "id",
                              "does not match the row id");
    }
    return event;
}

void VaultDoseEventRepository::upsert(const DoseEvent& event) {
    vault_.put(VaultRecord{event.id, kDoseKind,
                           envelope_stamp(event.updated_at),
                           encode_dose_event(event)});
}

bool VaultDoseEventRepository::remove(oyama::string_view id) {
    const auto record = vault_.get(id);
    if (!record.has_value()) {
        return false;
    }
    if (record->kind != kDoseKind) {
        return false;
    }
    return vault_.remove(id);
}

std::size_t VaultDoseEventRepository::count(Mode mode) {
    return list(mode).size();
}

} }  // namespace oyama::storage
