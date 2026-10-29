#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace oyama::storage {

enum class Mode {
    Transfem,
    Transmasc,
};

[[nodiscard]] std::string to_string(Mode mode);
[[nodiscard]] std::optional<Mode> parse_mode(std::string_view text);

/// Global settings.  There is exactly one row; weight, birth year, PK params and
/// calibration are shared by both modes, matching the upstream application.
struct Settings {
    std::optional<double> body_weight_kg;
    std::optional<std::int64_t> weight_updated_at;
    std::optional<int> birth_year;
    std::optional<std::int64_t> birth_year_updated_at;
    std::optional<std::string> pk_params_json;
    std::optional<std::int64_t> pk_params_updated_at;
    std::string calibration_method{"mipd"};
    std::string calibration_history_mode{"retrospective"};
    std::int64_t updated_at{0};
};

struct DoseEvent {
    std::string id;
    Mode mode{Mode::Transfem};
    std::string route;
    std::string ester;
    double time_h{0.0};
    double dose_mg{0.0};
    std::string extras_json{"{}"};
    std::optional<std::int64_t> updated_at;
    std::int64_t created_at{0};
};

struct LabResult {
    std::string id;
    Mode mode{Mode::Transfem};
    double time_h{0.0};
    double value{0.0};
    std::string unit;
    std::string assay;
    std::optional<std::int64_t> updated_at;
    std::int64_t created_at{0};
};

enum class JsonRecordKind {
    DoseTemplate,
    QuickDose,
    Supply,
    PlanItem,
};

[[nodiscard]] std::string table_name(JsonRecordKind kind);

struct JsonRecord {
    std::string id;
    Mode mode{Mode::Transfem};
    std::string payload_json{"{}"};
    std::optional<std::int64_t> updated_at;
};

struct Preference {
    std::string key;
    std::string value_json;
    std::int64_t updated_at{0};
};

struct Tombstone {
    std::string kind;
    std::string record_id;
    std::int64_t deleted_at{0};
};

}  // namespace oyama::storage
