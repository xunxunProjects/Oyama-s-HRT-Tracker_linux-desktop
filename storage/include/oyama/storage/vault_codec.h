#pragma once

#include "types.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultCodecError : public std::runtime_error {
public:
    VaultCodecError(std::string kind, std::string id, std::string field,
                    const std::string& message);

    [[nodiscard]] const std::string& kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& field() const noexcept { return field_; }

private:
    std::string kind_;
    std::string id_;
    std::string field_;
};

[[nodiscard]] std::string encode_settings(const Settings& settings);
[[nodiscard]] Settings decode_settings(std::string_view payload);

[[nodiscard]] std::string encode_preferences(
    const std::vector<Preference>& preferences);
[[nodiscard]] std::vector<Preference> decode_preferences(
    std::string_view payload);

[[nodiscard]] std::string encode_dose_event(const DoseEvent& event);
[[nodiscard]] DoseEvent decode_dose_event(std::string_view payload);

[[nodiscard]] std::string encode_lab_result(const LabResult& result);
[[nodiscard]] LabResult decode_lab_result(std::string_view payload);

[[nodiscard]] std::string encode_json_record(const JsonRecord& record,
                                             JsonRecordKind kind);
[[nodiscard]] JsonRecord decode_json_record(std::string_view payload,
                                            JsonRecordKind kind);

[[nodiscard]] std::string encode_tombstone(const Tombstone& tombstone);
[[nodiscard]] Tombstone decode_tombstone(std::string_view payload);

}  // namespace oyama::storage
