#include "oyama/storage/vault_codec.h"

#include "oyama/storage/uuid.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oyama { namespace storage {
namespace {

using json = nlohmann::json;

constexpr double kTimeMax = 1500000.0;
constexpr double kDoseMax = 10000.0;
constexpr double kWeightMin = 20.0;
constexpr double kWeightMax = 400.0;

const std::array<oyama::string_view, 6> kRoutes = {
    "oral",     "sublingual", "injection",
    "patchApply", "patchRemove", "gel",
};
const std::array<oyama::string_view, 17> kEsters = {
    "E2",   "EB",   "EV",   "EC",   "EN",   "EU",   "CPA",  "T",    "TC",
    "TE",   "TU",   "SPIRO", "BICA", "MPA",  "LEUP", "TRIP", "RLX",
};
const std::array<oyama::string_view, 4> kUnits = {
    "pg/ml", "pmol/l", "ng/dl", "nmol/l",
};
const std::array<oyama::string_view, 2> kAssays = {"immuno", "lcms"};
const std::array<oyama::string_view, 4> kCalibrationMethods = {
    "off", "ekf", "ou_kalman", "mipd",
};
const std::array<oyama::string_view, 2> kHistoryModes = {
    "forward", "retrospective",
};

template <typename Values>
bool contains(const Values& values, oyama::string_view value) {
    for (const auto& candidate : values) {
        if (candidate == value) {
            return true;
        }
    }
    return false;
}

[[noreturn]] void fail(const std::string& kind, const std::string& id,
                       const std::string& field, const std::string& message) {
    throw VaultCodecError(kind, id, field, message);
}

std::string canonical(const json& value) {
    return value.dump(2, ' ', false, json::error_handler_t::strict);
}

std::string compact(const json& value) {
    return value.dump();
}

json parse_object(oyama::string_view text, const std::string& kind,
                  const std::string& id) {
    json value = json::parse(text.begin(), text.end(), nullptr, false);
    if (value.is_discarded() || !value.is_object()) {
        fail(kind, id, "payload", "must be a JSON object");
    }
    return value;
}

void check_allowed(const json& value, std::initializer_list<oyama::string_view> allowed,
                   const std::string& kind, const std::string& id) {
    for (auto it = value.begin(); it != value.end(); ++it) {
        bool ok = false;
        for (oyama::string_view key : allowed) {
            if (it.key() == key) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            fail(kind, id, it.key(), "unknown field");
        }
    }
}

std::string required_string(const json& value, const char* key,
                            const std::string& kind, const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || !it->is_string()) {
        fail(kind, id, key, "must be a string");
    }
    return it->get<std::string>();
}

double required_number(const json& value, const char* key,
                       const std::string& kind, const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || !it->is_number() ||
        !std::isfinite(it->get<double>())) {
        fail(kind, id, key, "must be a finite number");
    }
    return it->get<double>();
}

std::int64_t required_integer(const json& value, const char* key,
                              const std::string& kind, const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || (!it->is_number_integer() &&
                              !it->is_number_unsigned())) {
        fail(kind, id, key, "must be an integer");
    }
    const auto number = it->get<std::int64_t>();
    if (number < 0) {
        fail(kind, id, key, "must not be negative");
    }
    return number;
}

oyama::optional<std::int64_t> optional_integer(const json& value, const char* key,
                                             const std::string& kind,
                                             const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || it->is_null()) {
        return oyama::nullopt;
    }
    if (!it->is_number_integer() && !it->is_number_unsigned()) {
        fail(kind, id, key, "must be an integer");
    }
    const auto number = it->get<std::int64_t>();
    if (number < 0) {
        fail(kind, id, key, "must not be negative");
    }
    return number;
}

oyama::optional<double> optional_number(const json& value, const char* key,
                                      const std::string& kind,
                                      const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || it->is_null()) {
        return oyama::nullopt;
    }
    if (!it->is_number() || !std::isfinite(it->get<double>())) {
        fail(kind, id, key, "must be a finite number");
    }
    return it->get<double>();
}

std::string id_field(const json& value, const std::string& kind,
                     const std::string& fallback_id = {}) {
    const std::string id = required_string(value, "id", kind, fallback_id);
    if (id.empty() || id == "__proto__" || id == "prototype" ||
        id == "constructor" || !is_uuid(id)) {
        fail(kind, id, "id", "must be a canonical UUID");
    }
    return id;
}

Mode mode_field(const json& value, const std::string& kind,
                const std::string& id) {
    const std::string mode = required_string(value, "mode", kind, id);
    const auto parsed = parse_mode(mode);
    if (!parsed.has_value()) {
        fail(kind, id, "mode", "must be transfem or transmasc");
    }
    return *parsed;
}

void route_field(const json& value, const std::string& kind,
                 const std::string& id) {
    if (!contains(kRoutes, required_string(value, "route", kind, id))) {
        fail(kind, id, "route", "is not a known route");
    }
}

void ester_field(const json& value, const std::string& kind,
                 const std::string& id) {
    if (!contains(kEsters, required_string(value, "ester", kind, id))) {
        fail(kind, id, "ester", "is not a known ester");
    }
}

void time_field(const json& value, const std::string& kind,
                const std::string& id) {
    const double time = required_number(value, "timeH", kind, id);
    if (time < 0.0 || time > kTimeMax) {
        fail(kind, id, "timeH", "is out of range");
    }
}

void dose_field(const json& value, const std::string& kind,
                const std::string& id) {
    const double dose = required_number(value, "doseMG", kind, id);
    if (dose < 0.0 || dose > kDoseMax) {
        fail(kind, id, "doseMG", "is out of range");
    }
}

json object_field(const json& value, const char* key, const std::string& kind,
                  const std::string& id) {
    const auto it = value.find(key);
    if (it == value.end() || it->is_null()) {
        return json::object();
    }
    if (!it->is_object()) {
        fail(kind, id, key, "must be an object");
    }
    return *it;
}

void check_calibration(const std::string& method,
                       const std::string& history_mode,
                       const std::string& kind, const std::string& id) {
    const std::string normalized_method =
        method == "average" ? "mipd"
                             : (method == "adaptive" ? "ou_kalman" : method);
    if (!contains(kCalibrationMethods, normalized_method)) {
        fail(kind, id, "calibration.method", "is not a known method");
    }
    if (!contains(kHistoryModes, history_mode)) {
        fail(kind, id, "calibration.historyMode", "is not a known history mode");
    }
}

std::string normalized_calibration_method(std::string method) {
    if (method == "average") {
        return "mipd";
    }
    if (method == "adaptive") {
        return "ou_kalman";
    }
    return method;
}

void validate_pack(const json& pack, const std::string& kind,
                   const std::string& id) {
    if (!pack.is_object()) {
        fail(kind, id, "pack", "must be an object");
    }
    check_allowed(pack, {"kind", "ml", "mgPerMl", "mg"}, kind, id);
    const std::string pack_kind = required_string(pack, "kind", kind, id);
    if (pack_kind == "vial") {
        const double ml = required_number(pack, "ml", kind, id);
        const double mg_per_ml = required_number(pack, "mgPerMl", kind, id);
        if (ml <= 0.0 || mg_per_ml <= 0.0) {
            fail(kind, id, "pack", "vial values must be positive");
        }
        return;
    }
    if (pack_kind == "pill" || pack_kind == "pump" || pack_kind == "sachet") {
        if (required_number(pack, "mg", kind, id) <= 0.0) {
            fail(kind, id, "pack.mg", "must be positive");
        }
        return;
    }
    if (pack_kind != "patch" && pack_kind != "bulk") {
        fail(kind, id, "pack.kind", "is not a known pack kind");
    }
}

void validate_json_payload(const json& payload, JsonRecordKind record_kind,
                           const std::string& kind, const std::string& id) {
    switch (record_kind) {
    case JsonRecordKind::DoseTemplate:
        check_allowed(payload,
                      {"id", "mode", "name", "route", "ester", "doseMG",
                       "extras", "createdAt", "updatedAt"},
                      kind, id);
        (void)required_string(payload, "name", kind, id);
        route_field(payload, kind, id);
        ester_field(payload, kind, id);
        dose_field(payload, kind, id);
        (void)required_integer(payload, "createdAt", kind, id);
        (void)object_field(payload, "extras", kind, id);
        break;
    case JsonRecordKind::QuickDose:
        check_allowed(payload,
                      {"id", "mode", "route", "ester", "value", "createdAt",
                       "updatedAt"},
                      kind, id);
        route_field(payload, kind, id);
        ester_field(payload, kind, id);
        if (required_number(payload, "value", kind, id) < 0.0) {
            fail(kind, id, "value", "must not be negative");
        }
        (void)required_integer(payload, "createdAt", kind, id);
        break;
    case JsonRecordKind::Supply: {
        check_allowed(payload,
                      {"id", "mode", "route", "ester", "pack", "count",
                       "startedAt", "updatedAt"},
                      kind, id);
        route_field(payload, kind, id);
        ester_field(payload, kind, id);
        const auto pack = payload.find("pack");
        if (pack == payload.end()) {
            fail(kind, id, "pack", "is required");
        }
        validate_pack(*pack, kind, id);
        if (required_number(payload, "count", kind, id) < 0.0) {
            fail(kind, id, "count", "must not be negative");
        }
        if (required_integer(payload, "startedAt", kind, id) <= 0) {
            fail(kind, id, "startedAt", "must be positive");
        }
        break;
    }
    case JsonRecordKind::PlanItem:
        check_allowed(payload,
                      {"id", "mode", "route", "ester", "doseMG", "extras",
                       "everyDays", "timesPerDay", "updatedAt"},
                      kind, id);
        route_field(payload, kind, id);
        ester_field(payload, kind, id);
        dose_field(payload, kind, id);
        (void)object_field(payload, "extras", kind, id);
        {
            const auto every_days = required_number(payload, "everyDays",
                                                    kind, id);
            if (every_days < 1.0 || every_days > 3650.0) {
                fail(kind, id, "everyDays", "is out of range");
            }
        }
        {
            const auto times = required_integer(payload, "timesPerDay", kind,
                                                id);
            if (times < 1 || times > 24) {
                fail(kind, id, "timesPerDay", "is out of range");
            }
        }
        break;
    }
}

std::string kind_name(JsonRecordKind kind) {
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

VaultCodecError::VaultCodecError(std::string kind, std::string id,
                                 std::string field,
                                 const std::string& message)
    : std::runtime_error(
          (kind.empty() ? std::string("vault record") : kind) +
          (id.empty() ? std::string() : " " + id) + " field " + field + ": " +
          message),
      kind_(std::move(kind)),
      id_(std::move(id)),
      field_(std::move(field)) {}

std::string encode_settings(const Settings& settings) {
    json value = json::object();
    if (settings.body_weight_kg.has_value()) {
        if (!std::isfinite(*settings.body_weight_kg) ||
            *settings.body_weight_kg < kWeightMin ||
            *settings.body_weight_kg > kWeightMax) {
            fail("settings", "", "weight", "is out of range");
        }
        value["weight"] = *settings.body_weight_kg;
    }
    if (settings.weight_updated_at.has_value()) {
        value["weightUpdatedAt"] = *settings.weight_updated_at;
    }
    if (settings.birth_year.has_value()) {
        if (*settings.birth_year < 1900 || *settings.birth_year > 2200) {
            fail("settings", "", "birthYear", "is out of range");
        }
        value["birthYear"] = *settings.birth_year;
    }
    if (settings.birth_year_updated_at.has_value()) {
        value["birthYearUpdatedAt"] = *settings.birth_year_updated_at;
    }
    if (settings.pk_params_json.has_value()) {
        const json params =
            parse_object(*settings.pk_params_json, "settings", "");
        value["pkParams"] = params;
    }
    if (settings.pk_params_updated_at.has_value()) {
        value["pkParamsUpdatedAt"] = *settings.pk_params_updated_at;
    }
    check_calibration(settings.calibration_method,
                      settings.calibration_history_mode, "settings", "");
    value["calibration"] = {
        {"method", normalized_calibration_method(settings.calibration_method)},
        {"historyMode", settings.calibration_history_mode},
    };
    value["updatedAt"] = settings.updated_at;
    return canonical(value);
}

Settings decode_settings(oyama::string_view text) {
    const json value = parse_object(text, "settings", "");
    check_allowed(value,
                  {"weight", "weightUpdatedAt", "birthYear",
                   "birthYearUpdatedAt", "pkParams", "pkParamsUpdatedAt",
                   "calibration", "updatedAt"},
                  "settings", "");
    Settings settings;
    if (const auto weight = optional_number(value, "weight", "settings", "")) {
        if (*weight < kWeightMin || *weight > kWeightMax) {
            fail("settings", "", "weight", "is out of range");
        }
        settings.body_weight_kg = *weight;
    }
    settings.weight_updated_at =
        optional_integer(value, "weightUpdatedAt", "settings", "");
    if (const auto birth = optional_integer(value, "birthYear", "settings", "")) {
        if (*birth < 1900 || *birth > 2200) {
            fail("settings", "", "birthYear", "is out of range");
        }
        settings.birth_year = static_cast<int>(*birth);
    }
    settings.birth_year_updated_at =
        optional_integer(value, "birthYearUpdatedAt", "settings", "");
    const auto params = value.find("pkParams");
    if (params != value.end() && !params->is_null()) {
        if (!params->is_object()) {
            fail("settings", "", "pkParams", "must be an object");
        }
        settings.pk_params_json = compact(*params);
    }
    settings.pk_params_updated_at =
        optional_integer(value, "pkParamsUpdatedAt", "settings", "");
    const auto calibration = value.find("calibration");
    if (calibration != value.end() && !calibration->is_null()) {
        if (!calibration->is_object()) {
            fail("settings", "", "calibration", "must be an object");
        }
        check_allowed(*calibration, {"method", "historyMode"}, "settings", "");
        settings.calibration_method =
            normalized_calibration_method(required_string(
                *calibration, "method", "settings", ""));
        settings.calibration_history_mode =
            required_string(*calibration, "historyMode", "settings", "");
        check_calibration(settings.calibration_method,
                          settings.calibration_history_mode, "settings", "");
    }
    if (const auto updated = optional_integer(value, "updatedAt", "settings", "")) {
        settings.updated_at = *updated;
    }
    return settings;
}

std::string encode_preferences(const std::vector<Preference>& preferences) {
    json items = json::object();
    for (const auto& preference : preferences) {
        if (preference.key.empty()) {
            fail("preferences", "", "key", "must not be empty");
        }
        if (items.contains(preference.key)) {
            fail("preferences", "", preference.key, "duplicate preference");
        }
        const json value =
            json::parse(preference.value_json.begin(),
                        preference.value_json.end(), nullptr, false);
        if (value.is_discarded()) {
            fail("preferences", "", preference.key, "value is not valid JSON");
        }
        items[preference.key] = {
            {"value", value},
            {"updatedAt", preference.updated_at},
        };
    }
    return canonical(json{{"items", items}});
}

std::vector<Preference> decode_preferences(oyama::string_view text) {
    const json value = parse_object(text, "preferences", "");
    check_allowed(value, {"items"}, "preferences", "");
    const auto items = value.find("items");
    if (items == value.end() || !items->is_object()) {
        fail("preferences", "", "items", "must be an object");
    }
    std::vector<Preference> preferences;
    for (auto it = items->begin(); it != items->end(); ++it) {
        if (it.key().empty()) {
            fail("preferences", "", "key", "must not be empty");
        }
        if (!it->is_object()) {
            fail("preferences", "", it.key(), "must be an object");
        }
        check_allowed(*it, {"value", "updatedAt"}, "preferences", it.key());
        const auto updated = it->find("updatedAt");
        if (updated == it->end() ||
            (!updated->is_number_integer() && !updated->is_number_unsigned())) {
            fail("preferences", "", it.key() + ".updatedAt",
                 "must be an integer");
        }
        const auto stamp = updated->get<std::int64_t>();
        if (stamp < 0) {
            fail("preferences", "", it.key() + ".updatedAt",
                 "must not be negative");
        }
        const auto pref_value = it->find("value");
        if (pref_value == it->end()) {
            fail("preferences", "", it.key() + ".value", "is required");
        }
        preferences.push_back(
            Preference{it.key(), compact(*pref_value), stamp});
    }
    return preferences;
}

std::string encode_dose_event(const DoseEvent& event) {
    const std::string kind = "dose_event";
    if (event.id.empty() || !is_uuid(event.id)) {
        fail(kind, event.id, "id", "must be a canonical UUID");
    }
    if (!contains(kRoutes, event.route)) {
        fail(kind, event.id, "route", "is not a known route");
    }
    if (!contains(kEsters, event.ester)) {
        fail(kind, event.id, "ester", "is not a known ester");
    }
    if (!std::isfinite(event.time_h) || event.time_h < 0.0 ||
        event.time_h > kTimeMax) {
        fail(kind, event.id, "timeH", "is out of range");
    }
    if (!std::isfinite(event.dose_mg) || event.dose_mg < 0.0 ||
        event.dose_mg > kDoseMax) {
        fail(kind, event.id, "doseMG", "is out of range");
    }
    const json extras = parse_object(event.extras_json, kind, event.id);
    json value = {
        {"id", event.id},
        {"mode", to_string(event.mode)},
        {"route", event.route},
        {"ester", event.ester},
        {"timeH", event.time_h},
        {"doseMG", event.dose_mg},
        {"extras", extras},
        {"createdAt", event.created_at},
    };
    if (event.updated_at.has_value()) {
        value["updatedAt"] = *event.updated_at;
    }
    return canonical(value);
}

DoseEvent decode_dose_event(oyama::string_view text) {
    const std::string kind = "dose_event";
    const json value = parse_object(text, kind, "");
    check_allowed(value,
                  {"id", "mode", "route", "ester", "timeH", "doseMG",
                   "extras", "createdAt", "updatedAt"},
                  kind, "");
    DoseEvent event;
    event.id = id_field(value, kind);
    event.mode = mode_field(value, kind, event.id);
    route_field(value, kind, event.id);
    ester_field(value, kind, event.id);
    time_field(value, kind, event.id);
    dose_field(value, kind, event.id);
    event.route = required_string(value, "route", kind, event.id);
    event.ester = required_string(value, "ester", kind, event.id);
    event.time_h = required_number(value, "timeH", kind, event.id);
    event.dose_mg = required_number(value, "doseMG", kind, event.id);
    const json extras = object_field(value, "extras", kind, event.id);
    event.extras_json = compact(extras);
    event.updated_at = optional_integer(value, "updatedAt", kind, event.id);
    event.created_at = optional_integer(value, "createdAt", kind, event.id)
                           .value_or(0);
    return event;
}

std::string encode_lab_result(const LabResult& result) {
    const std::string kind = "lab_result";
    if (result.id.empty() || !is_uuid(result.id)) {
        fail(kind, result.id, "id", "must be a canonical UUID");
    }
    if (!std::isfinite(result.time_h) || result.time_h < 0.0 ||
        result.time_h > kTimeMax) {
        fail(kind, result.id, "timeH", "is out of range");
    }
    if (!std::isfinite(result.value)) {
        fail(kind, result.id, "concValue", "must be finite");
    }
    if (!contains(kUnits, result.unit)) {
        fail(kind, result.id, "unit", "is not a known unit");
    }
    if (!contains(kAssays, result.assay)) {
        fail(kind, result.id, "assay", "is not a known assay");
    }
    json value = {
        {"id", result.id},
        {"mode", to_string(result.mode)},
        {"timeH", result.time_h},
        {"concValue", result.value},
        {"unit", result.unit},
        {"assay", result.assay},
        {"createdAt", result.created_at},
    };
    if (result.updated_at.has_value()) {
        value["updatedAt"] = *result.updated_at;
    }
    return canonical(value);
}

LabResult decode_lab_result(oyama::string_view text) {
    const std::string kind = "lab_result";
    const json value = parse_object(text, kind, "");
    check_allowed(value,
                  {"id", "mode", "timeH", "concValue", "unit", "assay",
                   "createdAt", "updatedAt"},
                  kind, "");
    LabResult result;
    result.id = id_field(value, kind);
    result.mode = mode_field(value, kind, result.id);
    time_field(value, kind, result.id);
    result.time_h = required_number(value, "timeH", kind, result.id);
    const double concentration =
        required_number(value, "concValue", kind, result.id);
    result.value = concentration;
    result.unit = required_string(value, "unit", kind, result.id);
    if (!contains(kUnits, result.unit)) {
        fail(kind, result.id, "unit", "is not a known unit");
    }
    result.assay = required_string(value, "assay", kind, result.id);
    if (!contains(kAssays, result.assay)) {
        fail(kind, result.id, "assay", "is not a known assay");
    }
    result.updated_at = optional_integer(value, "updatedAt", kind, result.id);
    result.created_at =
        optional_integer(value, "createdAt", kind, result.id).value_or(0);
    return result;
}

std::string encode_json_record(const JsonRecord& record,
                               JsonRecordKind record_kind) {
    const std::string kind = kind_name(record_kind);
    if (record.id.empty() || !is_uuid(record.id)) {
        fail(kind, record.id, "id", "must be a canonical UUID");
    }
    json payload = parse_object(record.payload_json, kind, record.id);
    validate_json_payload(payload, record_kind, kind, record.id);
    payload["id"] = record.id;
    payload["mode"] = to_string(record.mode);
    if (record.updated_at.has_value()) {
        payload["updatedAt"] = *record.updated_at;
    } else {
        payload.erase("updatedAt");
    }
    return canonical(payload);
}

JsonRecord decode_json_record(oyama::string_view text,
                              JsonRecordKind record_kind) {
    const std::string kind = kind_name(record_kind);
    const json payload = parse_object(text, kind, "");
    validate_json_payload(payload, record_kind, kind, "");
    JsonRecord record;
    record.id = id_field(payload, kind);
    record.mode = mode_field(payload, kind, record.id);
    record.updated_at = optional_integer(payload, "updatedAt", kind, record.id);
    record.payload_json = compact(payload);
    return record;
}

std::string encode_tombstone(const Tombstone& tombstone) {
    if (tombstone.kind.empty() || tombstone.kind.size() > 64) {
        fail("tombstone", tombstone.record_id, "kind", "is invalid");
    }
    if (!is_uuid(tombstone.record_id)) {
        fail("tombstone", tombstone.record_id, "recordId",
             "must be a canonical UUID");
    }
    if (tombstone.deleted_at < 0) {
        fail("tombstone", tombstone.record_id, "deletedAt",
             "must not be negative");
    }
    return canonical(json{{"kind", tombstone.kind},
                          {"recordId", tombstone.record_id},
                          {"deletedAt", tombstone.deleted_at}});
}

Tombstone decode_tombstone(oyama::string_view text) {
    const json value = parse_object(text, "tombstone", "");
    check_allowed(value, {"kind", "recordId", "deletedAt"}, "tombstone", "");
    Tombstone tombstone;
    tombstone.kind = required_string(value, "kind", "tombstone", "");
    if (tombstone.kind.empty() || tombstone.kind.size() > 64) {
        fail("tombstone", "", "kind", "is invalid");
    }
    tombstone.record_id =
        required_string(value, "recordId", "tombstone", "");
    if (!is_uuid(tombstone.record_id)) {
        fail("tombstone", tombstone.record_id, "recordId",
             "must be a canonical UUID");
    }
    tombstone.deleted_at =
        required_integer(value, "deletedAt", "tombstone", tombstone.record_id);
    return tombstone;
}

} }  // namespace oyama::storage
