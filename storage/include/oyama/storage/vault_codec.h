#pragma once
#include <oyama/compat/all.h>

#include "types.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

class VaultCodecError : public std::runtime_error {
public:
    VaultCodecError(std::string kind, std::string id, std::string field,
                    const std::string& message);

    OYAMA_NODISCARD const std::string& kind() const noexcept { return kind_; }
    OYAMA_NODISCARD const std::string& id() const noexcept { return id_; }
    OYAMA_NODISCARD const std::string& field() const noexcept { return field_; }

private:
    std::string kind_;
    std::string id_;
    std::string field_;
};

OYAMA_NODISCARD std::string encode_settings(const Settings& settings);
OYAMA_NODISCARD Settings decode_settings(oyama::string_view payload);

OYAMA_NODISCARD std::string encode_preferences(
    const std::vector<Preference>& preferences);
OYAMA_NODISCARD std::vector<Preference> decode_preferences(
    oyama::string_view payload);

OYAMA_NODISCARD std::string encode_dose_event(const DoseEvent& event);
OYAMA_NODISCARD DoseEvent decode_dose_event(oyama::string_view payload);

OYAMA_NODISCARD std::string encode_lab_result(const LabResult& result);
OYAMA_NODISCARD LabResult decode_lab_result(oyama::string_view payload);

OYAMA_NODISCARD std::string encode_json_record(const JsonRecord& record,
                                             JsonRecordKind kind);
OYAMA_NODISCARD JsonRecord decode_json_record(oyama::string_view payload,
                                            JsonRecordKind kind);

OYAMA_NODISCARD std::string encode_tombstone(const Tombstone& tombstone);
OYAMA_NODISCARD Tombstone decode_tombstone(oyama::string_view payload);

} }  // namespace oyama::storage
