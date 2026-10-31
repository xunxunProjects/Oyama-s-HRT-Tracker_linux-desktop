#include <oyama/storage/backup.h>
#include <oyama/storage/backup_crypto.h>

#include <oyama/storage/storage.h>
#include <oyama/storage/vault_dose_event_repository.h>
#include <oyama/storage/vault_json_record_repository.h>
#include <oyama/storage/vault_lab_result_repository.h>
#include <oyama/storage/vault_preferences_repository.h>
#include <oyama/storage/vault_settings_repository.h>
#include <oyama/storage/vault_store.h>
#include <oyama/storage/vault_tombstone_repository.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace oyama::storage {
namespace {

using json = nlohmann::json;

constexpr const char* kFormat = "oyama-hrt-complete-backup";
constexpr int kFormatVersion = 1;
constexpr std::int64_t kClockSkewMs = std::int64_t{5} * 60 * 1000;
constexpr std::int64_t kTombstoneTtlMs = 180LL * 24 * 60 * 60 * 1000;
constexpr std::size_t kTombstoneMaxPerKind = 5000;
constexpr std::size_t kMaxEntries = 20000;
constexpr double kDoseMax = 10000.0;
constexpr double kTimeMax = 1500000.0;
constexpr double kWeightMin = 20.0;
constexpr double kWeightMax = 400.0;

template <std::size_t N>
bool contains(const std::array<std::string_view, N>& values,
              const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

constexpr std::array<std::string_view, 6> kRoutes = {
    "oral", "sublingual", "injection", "patchApply", "patchRemove", "gel"};
constexpr std::array<std::string_view, 17> kEsters = {
    "E2",  "EB",   "EV",  "EC",   "EN",   "EU",  "CPA", "T",   "TC",
    "TE",  "TU",   "SPIRO", "BICA", "MPA", "LEUP", "TRIP", "RLX"};
constexpr std::array<std::string_view, 4> kUnits = {"pg/ml", "pmol/l", "ng/dl",
                                                    "nmol/l"};
constexpr std::array<std::string_view, 2> kAssays = {"immuno", "lcms"};
constexpr std::array<std::string_view, 4> kCalibrationMethods = {
    "off", "ekf", "ou_kalman", "mipd"};
constexpr std::array<std::string_view, 2> kHistoryModes = {"forward",
                                                           "retrospective"};
constexpr std::array<std::string_view, 5> kPreferences = {
    "app-theme", "hrt-lang", "app-show-prediction", "app-hide-site-label",
    "hrt-supply-lead-days"};
constexpr std::array<std::string_view, 3> kForbiddenIds = {
    "__proto__", "prototype", "constructor"};

std::string mode_key(Mode mode) {
    return to_string(mode);
}

bool finite(double value) {
    return std::isfinite(value);
}

bool known_route(const std::string& value) {
    return contains(kRoutes, value);
}

bool known_ester(const std::string& value) {
    return contains(kEsters, value);
}

std::string iso_utc(std::int64_t now_ms) {
    const std::time_t seconds = static_cast<std::time_t>(now_ms / 1000);
    std::tm tm{};
    gmtime_r(&seconds, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

std::string canonical(const json& value) {
    // nlohmann::json stores objects in sorted key order, so dump() is stable.
    return value.dump();
}

bool id_ok(const json& value, std::string& id) {
    if (!value.is_string()) {
        return false;
    }
    id = value.get<std::string>();
    return !id.empty() && !contains(kForbiddenIds, id) && is_uuid(id);
}

bool read_stamp(const json& object, const char* key, std::int64_t now_ms,
                std::optional<std::int64_t>& out, std::string& error,
                bool allow_missing = true) {
    if (!object.contains(key)) {
        if (allow_missing) {
            return true;
        }
        error = std::string("missing ") + key;
        return false;
    }
    const auto& value = object.at(key);
    if (value.is_null()) {
        return true;
    }
    if (!value.is_number() || !finite(value.get<double>())) {
        error = std::string(key) + " must be a finite number";
        return false;
    }
    const double number = value.get<double>();
    if (number <= 0 || number > static_cast<double>(now_ms + kClockSkewMs)) {
        error = std::string(key) + " is out of range";
        return false;
    }
    out = static_cast<std::int64_t>(number);
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool required_string(const json& object, const char* key, std::string& out,
                     std::string& error) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        error = std::string("missing or invalid ") + key;
        return false;
    }
    out = object.at(key).get<std::string>();
    if (out.empty()) {
        error = std::string(key) + " must not be empty";
        return false;
    }
    return true;
}

bool required_number(const json& object, const char* key, double& out,
                     std::string& error) {
    if (!object.contains(key) || !object.at(key).is_number()) {
        error = std::string("missing or invalid ") + key;
        return false;
    }
    out = object.at(key).get<double>();
    if (!finite(out)) {
        error = std::string(key) + " must be finite";
        return false;
    }
    return true;
}

json extras_object(const json& object, std::vector<std::string>& issues,
                   const std::string& where) {
    if (!object.contains("extras")) {
        return json::object();
    }
    if (!object.at("extras").is_object()) {
        issues.push_back(where + ": extras must be an object; using {}");
        return json::object();
    }
    return object.at("extras");
}

struct ParsedMode {
    std::string key;
    std::vector<DoseEvent> events;
    std::vector<LabResult> labs;
    std::vector<JsonRecord> templates;
    std::vector<JsonRecord> quick;
    std::vector<JsonRecord> supplies;
    std::vector<JsonRecord> plan;
    std::map<std::string, std::int64_t> deletion_events;
    std::map<std::string, std::int64_t> deletion_labs;
    std::map<std::string, std::int64_t> deletion_templates;
    std::set<std::string> blocked;
    bool events_present{false};
    bool labs_present{false};
    bool templates_present{false};
    bool quick_present{false};
    bool supplies_present{false};
    bool plan_present{false};
    bool deletions_present{false};
};

struct ParsedBundle {
    bool valid{false};
    std::vector<std::string> errors;
    std::vector<std::string> issues;
    std::vector<std::string> missing;
    std::map<std::string, ParsedMode> modes;
    bool weight_present{false};
    double weight{0};
    std::optional<std::int64_t> weight_stamp;
    bool birth_present{false};
    int birth_year{0};
    std::optional<std::int64_t> birth_stamp;
    bool pk_present{false};
    bool pk_null{false};
    std::string pk_json{"{}"};
    std::optional<std::int64_t> pk_stamp;
    bool calibration_present{false};
    std::string calibration_method;
    std::string calibration_history;
    std::map<std::string, std::string> preferences;
};

struct LocalState {
    Settings settings;
    std::vector<Preference> preferences;
    std::vector<Tombstone> tombstones;
    std::map<std::string, std::vector<DoseEvent>> events;
    std::map<std::string, std::vector<LabResult>> labs;
    std::map<std::string, std::vector<JsonRecord>> templates;
    std::map<std::string, std::vector<JsonRecord>> quick;
    std::map<std::string, std::vector<JsonRecord>> supplies;
    std::map<std::string, std::vector<JsonRecord>> plan;
};

struct MergedState {
    Settings settings;
    std::map<std::string, std::vector<DoseEvent>> events;
    std::map<std::string, std::vector<LabResult>> labs;
    std::map<std::string, std::vector<JsonRecord>> templates;
    std::map<std::string, std::vector<JsonRecord>> quick;
    std::map<std::string, std::vector<JsonRecord>> supplies;
    std::map<std::string, std::vector<JsonRecord>> plan;
    std::map<std::string, std::map<std::string, std::int64_t>> tombstones;
    std::map<std::string, std::string> preferences;
    BackupPreview preview;
};

bool parse_event(const json& value, Mode mode, std::int64_t now_ms,
                 DoseEvent& event, std::vector<std::string>& issues,
                 std::string& error, const std::string& where) {
    if (!value.is_object()) {
        error = where + ": event must be an object";
        return false;
    }
    std::string route;
    std::string ester;
    if (!id_ok(value.value("id", json()), event.id)) {
        error = where + ": event id must be a UUID";
        return false;
    }
    if (!required_string(value, "route", route, error) || !known_route(route)) {
        error = where + ": invalid route";
        return false;
    }
    if (!required_string(value, "ester", ester, error) || !known_ester(ester)) {
        error = where + ": invalid ester";
        return false;
    }
    if (!required_number(value, "timeH", event.time_h, error) ||
        event.time_h < 0 || event.time_h > kTimeMax) {
        error = where + ": timeH is out of range";
        return false;
    }
    if (!required_number(value, "doseMG", event.dose_mg, error) ||
        event.dose_mg < 0 || event.dose_mg > kDoseMax) {
        error = where + ": doseMG is out of range";
        return false;
    }
    if (!read_stamp(value, "updatedAt", now_ms, event.updated_at, error)) {
        error = where + ": " + error;
        return false;
    }
    event.mode = mode;
    event.route = route;
    event.ester = ester;
    event.extras_json = extras_object(value, issues, where).dump();
    event.created_at = 0;
    return true;
}

bool parse_lab(const json& value, Mode mode, std::int64_t now_ms,
               LabResult& lab, std::vector<std::string>& issues,
               std::string& error, const std::string& where) {
    if (!value.is_object()) {
        error = where + ": lab result must be an object";
        return false;
    }
    if (!id_ok(value.value("id", json()), lab.id)) {
        error = where + ": lab id must be a UUID";
        return false;
    }
    if (!required_number(value, "timeH", lab.time_h, error) || lab.time_h < 0 ||
        lab.time_h > kTimeMax) {
        error = where + ": lab timeH is out of range";
        return false;
    }
    if (!required_number(value, "concValue", lab.value, error) ||
        !finite(lab.value)) {
        error = where + ": lab concValue is invalid";
        return false;
    }
    if (!required_string(value, "unit", lab.unit, error) ||
        !contains(kUnits, lab.unit)) {
        error = where + ": invalid lab unit";
        return false;
    }
    if (value.contains("assay") && !value.at("assay").is_null()) {
        if (!value.at("assay").is_string() ||
            !contains(kAssays, value.at("assay").get<std::string>())) {
            error = where + ": invalid lab assay";
            return false;
        }
        lab.assay = value.at("assay").get<std::string>();
    } else {
        lab.assay = "immuno";
        issues.push_back(where + ": missing lab assay; defaulted to immuno");
    }
    if (!read_stamp(value, "updatedAt", now_ms, lab.updated_at, error)) {
        error = where + ": " + error;
        return false;
    }
    lab.mode = mode;
    lab.created_at = 0;
    return true;
}

bool parse_template(const json& value, Mode mode, std::int64_t now_ms,
                    JsonRecord& record, std::vector<std::string>& issues,
                    std::string& error, const std::string& where) {
    if (!value.is_object()) {
        error = where + ": template must be an object";
        return false;
    }
    if (!id_ok(value.value("id", json()), record.id)) {
        error = where + ": template id must be a UUID";
        return false;
    }
    std::string name;
    std::string route;
    std::string ester;
    double dose = 0;
    double created = 0;
    if (!required_string(value, "name", name, error) ||
        !required_string(value, "route", route, error) || !known_route(route) ||
        !required_string(value, "ester", ester, error) || !known_ester(ester) ||
        !required_number(value, "doseMG", dose, error) || dose < 0 ||
        !required_number(value, "createdAt", created, error)) {
        error = where + ": invalid template";
        return false;
    }
    if (!read_stamp(value, "updatedAt", now_ms, record.updated_at, error)) {
        error = where + ": " + error;
        return false;
    }
    json payload{{"id", record.id},       {"name", name},
                 {"route", route},        {"ester", ester},
                 {"doseMG", dose},        {"extras", extras_object(value, issues, where)},
                 {"createdAt", created}};
    if (record.updated_at.has_value()) {
        payload["updatedAt"] = *record.updated_at;
    }
    record.mode = mode;
    record.payload_json = payload.dump();
    return true;
}

bool parse_quick(const json& value, Mode mode, std::int64_t now_ms,
                 JsonRecord& record, std::vector<std::string>& issues,
                 std::string& error, const std::string& where) {
    (void)now_ms;
    (void)issues;
    if (!value.is_object()) {
        error = where + ": quick dose must be an object";
        return false;
    }
    std::string route;
    std::string ester;
    double amount = 0;
    double created = 0;
    if (!id_ok(value.value("id", json()), record.id) ||
        !required_string(value, "route", route, error) || !known_route(route) ||
        !required_string(value, "ester", ester, error) || !known_ester(ester) ||
        !required_number(value, "value", amount, error) || amount < 0 ||
        !required_number(value, "createdAt", created, error)) {
        error = where + ": invalid quick dose";
        return false;
    }
    record.mode = mode;
    record.updated_at.reset();
    record.payload_json =
        json{{"id", record.id}, {"route", route}, {"ester", ester},
             {"value", amount}, {"createdAt", created}}
            .dump();
    return true;
}

bool parse_supply(const json& value, Mode mode, std::int64_t now_ms,
                  JsonRecord& record, std::vector<std::string>& issues,
                  std::string& error, const std::string& where) {
    (void)now_ms;
    (void)issues;
    if (!value.is_object()) {
        error = where + ": supply must be an object";
        return false;
    }
    std::string route;
    std::string ester;
    double count = 0;
    double started = 0;
    if (!id_ok(value.value("id", json()), record.id) ||
        !required_string(value, "route", route, error) || !known_route(route) ||
        !required_string(value, "ester", ester, error) || !known_ester(ester) ||
        !required_number(value, "count", count, error) || count < 0 ||
        !required_number(value, "startedAt", started, error) || started <= 0) {
        error = where + ": invalid supply";
        return false;
    }
    if (!value.contains("pack") || !value.at("pack").is_object()) {
        error = where + ": supply pack must be an object";
        return false;
    }
    const auto& pack = value.at("pack");
    std::string kind;
    if (!pack.contains("kind") || !pack.at("kind").is_string()) {
        error = where + ": supply pack kind is missing";
        return false;
    }
    kind = pack.at("kind").get<std::string>();
    if (kind == "vial") {
        if (!pack.contains("ml") || !pack.at("ml").is_number() ||
            pack.at("ml").get<double>() <= 0 || !pack.contains("mgPerMl") ||
            !pack.at("mgPerMl").is_number() ||
            pack.at("mgPerMl").get<double>() <= 0) {
            error = where + ": invalid vial pack";
            return false;
        }
    } else if (kind == "pill" || kind == "pump" || kind == "sachet") {
        if (!pack.contains("mg") || !pack.at("mg").is_number() ||
            pack.at("mg").get<double>() <= 0) {
            error = where + ": invalid " + kind + " pack";
            return false;
        }
    } else if (kind != "patch" && kind != "bulk") {
        error = where + ": unknown supply pack kind";
        return false;
    }
    record.mode = mode;
    record.updated_at.reset();
    record.payload_json =
        json{{"id", record.id}, {"route", route}, {"ester", ester},
             {"pack", pack}, {"count", count}, {"startedAt", started}}
            .dump();
    return true;
}

bool parse_plan(const json& value, Mode mode, std::int64_t now_ms,
                JsonRecord& record, std::vector<std::string>& issues,
                std::string& error, const std::string& where) {
    (void)now_ms;
    (void)issues;
    if (!value.is_object()) {
        error = where + ": plan item must be an object";
        return false;
    }
    std::string route;
    std::string ester;
    double dose = 0;
    double every_days = 0;
    double times_per_day = 0;
    if (!id_ok(value.value("id", json()), record.id) ||
        !required_string(value, "route", route, error) || !known_route(route) ||
        !required_string(value, "ester", ester, error) || !known_ester(ester) ||
        !required_number(value, "doseMG", dose, error) || dose < 0 ||
        !required_number(value, "everyDays", every_days, error) ||
        every_days < 1 || every_days > 3650 ||
        !required_number(value, "timesPerDay", times_per_day, error)) {
        error = where + ": invalid plan item";
        return false;
    }
    int times = static_cast<int>(std::llround(times_per_day));
    if (times < 1) {
        times = 1;
    } else if (times > 24) {
        times = 24;
    }
    if (every_days > 1) {
        times = 1;
    }
    record.mode = mode;
    record.updated_at.reset();
    record.payload_json =
        json{{"id", record.id}, {"route", route}, {"ester", ester},
             {"doseMG", dose}, {"extras", extras_object(value, issues, where)},
             {"everyDays", every_days}, {"timesPerDay", times}}
            .dump();
    return true;
}

std::map<std::string, std::int64_t> parse_deletions(
    const json& value, std::int64_t now_ms, std::vector<std::string>& issues,
    std::string& error, const std::string& where) {
    std::map<std::string, std::int64_t> result;
    if (!value.is_object()) {
        error = where + ": deletions must be an object";
        return result;
    }
    for (const auto& [id, stamp_value] : value.items()) {
        if (id.empty() || contains(kForbiddenIds, id) || !is_uuid(id)) {
            error = where + ": deletion id must be a UUID";
            return {};
        }
        if (!stamp_value.is_number() || !finite(stamp_value.get<double>())) {
            error = where + ": deletion stamp must be finite";
            return {};
        }
        const double raw = stamp_value.get<double>();
        if (raw <= 0 || raw > static_cast<double>(now_ms + kClockSkewMs)) {
            issues.push_back(where + ": dropping out-of-range deletion " + id);
            continue;
        }
        const auto stamp = static_cast<std::int64_t>(raw);
        if (now_ms - stamp >= kTombstoneTtlMs) {
            issues.push_back(where + ": dropping expired deletion " + id);
            continue;
        }
        result[id] = std::max(result[id], stamp);
    }
    if (result.size() > kTombstoneMaxPerKind) {
        std::vector<std::pair<std::int64_t, std::string>> ordered;
        ordered.reserve(result.size());
        for (const auto& [id, stamp] : result) {
            ordered.emplace_back(stamp, id);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& left, const auto& right) {
                      return left.first > right.first;
                  });
        result.clear();
        for (std::size_t i = 0; i < kTombstoneMaxPerKind; ++i) {
            result[ordered[i].second] = ordered[i].first;
        }
        issues.push_back(where + ": dropped tombstones beyond the 5000 cap");
    }
    return result;
}

ParsedBundle parse_bundle(const json& root, std::int64_t now_ms) {
    ParsedBundle parsed;
    if (!root.is_object()) {
        parsed.errors.push_back("backup root must be an object");
        return parsed;
    }
    if (root.value("format", std::string()) != kFormat) {
        parsed.errors.push_back("unsupported backup format");
    }
    if (!root.contains("formatVersion") || !root.at("formatVersion").is_number() ||
        root.at("formatVersion").get<int>() != kFormatVersion) {
        parsed.errors.push_back("unsupported backup formatVersion");
    }
    if (!root.contains("createdAt") || !root.at("createdAt").is_string()) {
        parsed.errors.push_back("missing createdAt");
    }
    if (!root.contains("mode") || !root.at("mode").is_string() ||
        !parse_mode(root.at("mode").get<std::string>()).has_value()) {
        parsed.errors.push_back("invalid mode");
    }
    if (!root.contains("modes") || !root.at("modes").is_object()) {
        parsed.errors.push_back("missing modes");
    }
    if (!parsed.errors.empty()) {
        return parsed;
    }

    for (const auto mode : {Mode::Transfem, Mode::Transmasc}) {
        ParsedMode data;
        data.key = mode_key(mode);
        const auto& modes = root.at("modes");
        if (!modes.contains(data.key) || !modes.at(data.key).is_object()) {
            parsed.missing.push_back(data.key);
            parsed.modes[data.key] = std::move(data);
            continue;
        }
        const auto& block = modes.at(data.key);
        const auto parse_array = [&](const char* name, auto&& parse_one,
                                     auto& target, bool& present,
                                     std::set<std::string>& blocked) {
            present = block.contains(name);
            if (!present) {
                parsed.missing.push_back(data.key + "." + name);
                return;
            }
            if (!block.at(name).is_array()) {
                blocked.insert(data.key + "." + name);
                parsed.issues.push_back(data.key + "." + name +
                                        ": category must be an array");
                return;
            }
            if (block.at(name).size() > kMaxEntries) {
                blocked.insert(data.key + "." + name);
                parsed.issues.push_back(data.key + "." + name +
                                        ": more than 20000 entries");
                return;
            }
            std::set<std::string> ids;
            for (std::size_t i = 0; i < block.at(name).size(); ++i) {
                typename std::remove_reference_t<decltype(target)>::value_type
                    record;
                std::string error;
                if (!parse_one(block.at(name).at(i), mode, now_ms, record,
                               parsed.issues, error,
                               data.key + "." + name + "[" +
                                   std::to_string(i) + "]") ||
                    !ids.insert(record.id).second) {
                    blocked.insert(data.key + "." + name);
                    if (!error.empty()) {
                        parsed.issues.push_back(error);
                    } else {
                        parsed.issues.push_back(
                            data.key + "." + name + ": duplicate id");
                    }
                    return;
                }
                target.push_back(std::move(record));
            }
        };

        parse_array("events", parse_event, data.events, data.events_present,
                    data.blocked);
        parse_array("labResults", parse_lab, data.labs, data.labs_present,
                    data.blocked);
        parse_array("doseTemplates", parse_template, data.templates,
                    data.templates_present, data.blocked);
        parse_array("quickDoses", parse_quick, data.quick, data.quick_present,
                    data.blocked);
        parse_array("supplies", parse_supply, data.supplies,
                    data.supplies_present, data.blocked);
        parse_array("plan", parse_plan, data.plan, data.plan_present,
                    data.blocked);

        data.deletions_present = block.contains("deletions");
        if (data.deletions_present && block.at("deletions").is_object()) {
            const auto& deletions = block.at("deletions");
            std::string error;
            if (deletions.contains("events")) {
                data.deletion_events =
                    parse_deletions(deletions.at("events"), now_ms,
                                    parsed.issues, error, data.key + ".deletions.events");
            }
            if (deletions.contains("labResults")) {
                std::string lab_error;
                data.deletion_labs =
                    parse_deletions(deletions.at("labResults"), now_ms,
                                    parsed.issues, lab_error,
                                    data.key + ".deletions.labResults");
                if (!lab_error.empty()) {
                    error = lab_error;
                }
            }
            if (deletions.contains("doseTemplates")) {
                std::string template_error;
                data.deletion_templates =
                    parse_deletions(deletions.at("doseTemplates"), now_ms,
                                    parsed.issues, template_error,
                                    data.key + ".deletions.doseTemplates");
                if (!template_error.empty()) {
                    error = template_error;
                }
            }
            if (!error.empty()) {
                data.blocked.insert(data.key + ".deletions");
            }
        } else if (data.deletions_present) {
            data.blocked.insert(data.key + ".deletions");
        } else {
            parsed.missing.push_back(data.key + ".deletions");
        }

        parsed.modes[data.key] = std::move(data);
    }

    if (root.contains("weight") && !root.at("weight").is_null()) {
        parsed.weight_present = true;
        if (!root.at("weight").is_number()) {
            parsed.errors.push_back("weight must be a number");
        } else {
            parsed.weight = root.at("weight").get<double>();
            if (!finite(parsed.weight) || parsed.weight < kWeightMin ||
                parsed.weight > kWeightMax) {
                parsed.errors.push_back("weight is out of range");
            }
        }
        std::string error;
        if (!read_stamp(root, "weightUpdatedAt", now_ms, parsed.weight_stamp,
                        error)) {
            parsed.errors.push_back(error);
        }
    }
    if (root.contains("birthYear") && !root.at("birthYear").is_null()) {
        parsed.birth_present = true;
        if (!root.at("birthYear").is_number()) {
            parsed.errors.push_back("birthYear must be a number");
        } else {
            parsed.birth_year = root.at("birthYear").get<int>();
            if (parsed.birth_year < 1900 || parsed.birth_year > 2200) {
                parsed.errors.push_back("birthYear is out of range");
            }
        }
        std::string error;
        if (!read_stamp(root, "birthYearUpdatedAt", now_ms, parsed.birth_stamp,
                        error)) {
            parsed.errors.push_back(error);
        }
    }
    if (root.contains("pkParams")) {
        parsed.pk_present = true;
        parsed.pk_null = root.at("pkParams").is_null();
        if (!parsed.pk_null) {
            if (!root.at("pkParams").is_object()) {
                parsed.errors.push_back("pkParams must be an object or null");
            } else {
                parsed.pk_json = root.at("pkParams").dump();
            }
        }
        std::string error;
        if (!read_stamp(root, "pkParamsUpdatedAt", now_ms, parsed.pk_stamp,
                        error)) {
            parsed.errors.push_back(error);
        }
    }
    if (root.contains("calibration")) {
        parsed.calibration_present = true;
        if (!root.at("calibration").is_object()) {
            parsed.errors.push_back("calibration must be an object");
        } else {
            const auto& calibration = root.at("calibration");
            if (!calibration.contains("method") ||
                !calibration.at("method").is_string() ||
                !calibration.contains("historyMode") ||
                !calibration.at("historyMode").is_string()) {
                parsed.errors.push_back("invalid calibration");
            } else {
                parsed.calibration_method =
                    calibration.at("method").get<std::string>();
                parsed.calibration_history =
                    calibration.at("historyMode").get<std::string>();
                if (parsed.calibration_method == "average") {
                    parsed.calibration_method = "mipd";
                } else if (parsed.calibration_method == "adaptive") {
                    parsed.calibration_method = "ou_kalman";
                }
                if (!contains(kCalibrationMethods, parsed.calibration_method) ||
                    !contains(kHistoryModes, parsed.calibration_history)) {
                    parsed.errors.push_back("invalid calibration");
                }
            }
        }
    }
    if (root.contains("preferences") && root.at("preferences").is_object()) {
        for (const auto& [key, value] : root.at("preferences").items()) {
            if (!contains(kPreferences, key)) {
                parsed.issues.push_back("ignoring unknown preference " + key);
                continue;
            }
            parsed.preferences[key] = value.dump();
        }
    }
    parsed.valid = parsed.errors.empty();
    return parsed;
}

struct SqlRepositories {
    using Store = Connection;
    using Settings = SettingsRepository;
    using Preferences = PreferencesRepository;
    using Tombstones = TombstoneRepository;
    using Events = DoseEventRepository;
    using Labs = LabResultRepository;
    using JsonRecords = JsonRecordRepository;
};

struct VaultRepositories {
    using Store = VaultStore;
    using Settings = VaultSettingsRepository;
    using Preferences = VaultPreferencesRepository;
    using Tombstones = VaultTombstoneRepository;
    using Events = VaultDoseEventRepository;
    using Labs = VaultLabResultRepository;
    using JsonRecords = VaultJsonRecordRepository;
};

template <typename Repositories>
LocalState load_local(typename Repositories::Store& store) {
    using SettingsRepo = typename Repositories::Settings;
    using PreferencesRepo = typename Repositories::Preferences;
    using TombstoneRepo = typename Repositories::Tombstones;
    using EventRepo = typename Repositories::Events;
    using LabRepo = typename Repositories::Labs;
    using JsonRepo = typename Repositories::JsonRecords;

    LocalState state;
    state.settings = SettingsRepo(store).get();
    state.preferences = PreferencesRepo(store).list();
    state.tombstones = TombstoneRepo(store).list();
    for (const auto mode : {Mode::Transfem, Mode::Transmasc}) {
        const auto key = mode_key(mode);
        state.events[key] = EventRepo(store).list(mode);
        state.labs[key] = LabRepo(store).list(mode);
        state.templates[key] =
            JsonRepo(store, JsonRecordKind::DoseTemplate).list(mode);
        state.quick[key] =
            JsonRepo(store, JsonRecordKind::QuickDose).list(mode);
        state.supplies[key] =
            JsonRepo(store, JsonRecordKind::Supply).list(mode);
        state.plan[key] =
            JsonRepo(store, JsonRecordKind::PlanItem).list(mode);
    }
    return state;
}

std::string content_fingerprint(const DoseEvent& event) {
    return json{{"route", event.route},
                {"ester", event.ester},
                {"doseMG", event.dose_mg},
                {"timeH", event.time_h},
                {"extras", json::parse(event.extras_json)}}
        .dump();
}

std::string content_fingerprint(const LabResult& lab) {
    return json{{"unit", lab.unit},
                {"concValue", lab.value},
                {"timeH", lab.time_h},
                {"assay", lab.assay}}
        .dump();
}

std::string content_fingerprint(const JsonRecord& record) {
    return canonical(json::parse(record.payload_json));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
template <typename T, typename StampFn, typename FingerprintFn>
void merge_stamped(std::vector<T>& local, const std::vector<T>& incoming,
                   std::map<std::string, std::int64_t>& tombstones,
                   std::map<std::string, T>& by_id, StampFn stamp,
                   FingerprintFn fingerprint, BackupStats& stats,
                   std::vector<std::string>& removed_ids,
                   bool& blocked) {
    for (auto it = local.begin(); it != local.end();) {
        if (tombstones.count(it->id) != 0) {
            removed_ids.push_back(it->id);
            by_id.erase(it->id);
            ++stats.removed;
            it = local.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& record : incoming) {
        const auto tomb = tombstones.find(record.id);
        if (tomb != tombstones.end()) {
            const auto record_stamp = stamp(record);
            if (!record_stamp.has_value() || *record_stamp <= tomb->second) {
                ++stats.skipped;
                continue;
            }
            tombstones.erase(tomb);
        }
        const auto found = by_id.find(record.id);
        if (found == by_id.end()) {
            local.push_back(record);
            by_id[record.id] = record;
            ++stats.added;
            continue;
        }
        const auto local_stamp = stamp(found->second);
        const auto incoming_stamp = stamp(record);
        if (!local_stamp.has_value() || !incoming_stamp.has_value()) {
            blocked = true;
            return;
        }
        if (*incoming_stamp > *local_stamp) {
            found->second = record;
            ++stats.updated;
        } else if (*incoming_stamp < *local_stamp ||
                   fingerprint(found->second) == fingerprint(record)) {
            ++stats.skipped;
        } else {
            blocked = true;
            return;
        }
    }
    local.clear();
    local.reserve(by_id.size());
    for (const auto& [id, record] : by_id) {
        (void)id;
        local.push_back(record);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void merge_events(ParsedMode& incoming, const std::string& key,
                  MergedState& state, std::map<std::string, std::int64_t>& tomb,
                  BackupStats& stats, std::vector<std::string>& removed_ids,
                  bool& blocked) {
    if (incoming.blocked.count(key + ".events") != 0) {
        state.preview.blocked.push_back(key + ".events");
        return;
    }
    std::map<std::string, DoseEvent> by_id;
    for (const auto& record : state.events[key]) {
        by_id[record.id] = record;
    }
    merge_stamped(state.events[key], incoming.events, tomb, by_id,
                  [](const DoseEvent& value) { return value.updated_at; },
                  [](const DoseEvent& value) {
                      return content_fingerprint(value);
                  },
                  stats, removed_ids, blocked);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void merge_labs(ParsedMode& incoming, const std::string& key,
                MergedState& state, std::map<std::string, std::int64_t>& tomb,
                BackupStats& stats, std::vector<std::string>& removed_ids,
                bool& blocked) {
    if (incoming.blocked.count(key + ".labResults") != 0) {
        state.preview.blocked.push_back(key + ".labResults");
        return;
    }
    std::map<std::string, LabResult> by_id;
    for (const auto& record : state.labs[key]) {
        by_id[record.id] = record;
    }
    merge_stamped(state.labs[key], incoming.labs, tomb, by_id,
                  [](const LabResult& value) { return value.updated_at; },
                  [](const LabResult& value) {
                      return content_fingerprint(value);
                  },
                  stats, removed_ids, blocked);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void merge_templates(ParsedMode& incoming, const std::string& key,
                     MergedState& state,
                     std::map<std::string, std::int64_t>& tomb,
                     BackupStats& stats, std::vector<std::string>& removed_ids,
                     bool& blocked) {
    if (incoming.blocked.count(key + ".doseTemplates") != 0) {
        state.preview.blocked.push_back(key + ".doseTemplates");
        return;
    }
    std::map<std::string, JsonRecord> by_id;
    for (const auto& record : state.templates[key]) {
        by_id[record.id] = record;
    }
    merge_stamped(state.templates[key], incoming.templates, tomb, by_id,
                  [](const JsonRecord& value) { return value.updated_at; },
                  [](const JsonRecord& value) {
                      return content_fingerprint(value);
                  },
                  stats, removed_ids, blocked);
}

void merge_unstamped(std::vector<JsonRecord>& local,
                     const std::vector<JsonRecord>& incoming,
                     std::size_t& added, std::vector<std::string>& blocked,
                     const std::string& category) {
    std::set<std::string> ids;
    for (const auto& record : local) {
        ids.insert(record.id);
    }
    for (const auto& record : incoming) {
        if (ids.count(record.id) != 0) {
            blocked.push_back(category);
            return;
        }
    }
    for (const auto& record : incoming) {
        local.push_back(record);
        ids.insert(record.id);
        ++added;
    }
}

template <typename Repositories>
MergedState merge_bundle(typename Repositories::Store& store,
                         const ParsedBundle& parsed, std::int64_t now_ms) {
    using SettingsRepo = typename Repositories::Settings;
    using PreferencesRepo = typename Repositories::Preferences;
    using TombstoneRepo = typename Repositories::Tombstones;
    using EventRepo = typename Repositories::Events;
    using LabRepo = typename Repositories::Labs;
    using JsonRepo = typename Repositories::JsonRecords;

    MergedState state;
    state.settings = SettingsRepo(store).get();
    for (const auto& preference : PreferencesRepo(store).list()) {
        state.preferences[preference.key] = preference.value_json;
    }
    for (const auto mode : {Mode::Transfem, Mode::Transmasc}) {
        const auto key = mode_key(mode);
        state.events[key] = EventRepo(store).list(mode);
        state.labs[key] = LabRepo(store).list(mode);
        state.templates[key] =
            JsonRepo(store, JsonRecordKind::DoseTemplate).list(mode);
        state.quick[key] =
            JsonRepo(store, JsonRecordKind::QuickDose).list(mode);
        state.supplies[key] =
            JsonRepo(store, JsonRecordKind::Supply).list(mode);
        state.plan[key] =
            JsonRepo(store, JsonRecordKind::PlanItem).list(mode);
    }
    state.tombstones.clear();
    for (const auto& tombstone : TombstoneRepo(store).list()) {
        if (now_ms - tombstone.deleted_at < kTombstoneTtlMs) {
            state.tombstones[tombstone.kind][tombstone.record_id] =
                std::max(state.tombstones[tombstone.kind][tombstone.record_id],
                         tombstone.deleted_at);
        }
    }

    state.preview.ok = parsed.valid;
    state.preview.errors = parsed.errors;
    state.preview.issues = parsed.issues;
    state.preview.missing = parsed.missing;

    for (const auto mode : {Mode::Transfem, Mode::Transmasc}) {
        const auto key = mode_key(mode);
        auto incoming = parsed.modes.at(key);
        const auto& original_tomb = state.tombstones;
        bool blocked = false;
        std::size_t added = 0;
        std::size_t updated = 0;
        std::size_t skipped = 0;
        std::size_t removed = 0;
        std::vector<std::string> removed_ids;

        auto& event_tomb = state.tombstones["events"];
        for (const auto& [id, stamp] : incoming.deletion_events) {
            bool protect = false;
            for (const auto& record : state.events[key]) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            for (const auto& record : incoming.events) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            if (!protect) {
                event_tomb[id] = std::max(event_tomb[id], stamp);
            }
        }
        std::map<std::string, DoseEvent> event_by_id;
        for (const auto& record : state.events[key]) {
            event_by_id[record.id] = record;
        }
        merge_events(incoming, key, state, event_tomb, state.preview.stats,
                     removed_ids, blocked);
        if (blocked) {
            state.events[key] = std::vector<DoseEvent>{};
            for (const auto& [id, record] : event_by_id) {
                (void)id;
                state.events[key].push_back(record);
            }
            state.tombstones = original_tomb;
        }

        auto& lab_tomb = state.tombstones["labResults"];
        for (const auto& [id, stamp] : incoming.deletion_labs) {
            bool protect = false;
            for (const auto& record : state.labs[key]) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            for (const auto& record : incoming.labs) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            if (!protect) {
                lab_tomb[id] = std::max(lab_tomb[id], stamp);
            }
        }
        std::map<std::string, LabResult> lab_by_id;
        for (const auto& record : state.labs[key]) {
            lab_by_id[record.id] = record;
        }
        merge_labs(incoming, key, state, lab_tomb, state.preview.stats,
                   removed_ids, blocked);
        if (blocked) {
            state.labs[key] = std::vector<LabResult>{};
            for (const auto& [id, record] : lab_by_id) {
                (void)id;
                state.labs[key].push_back(record);
            }
            state.tombstones = original_tomb;
        }

        auto& template_tomb = state.tombstones["doseTemplates"];
        for (const auto& [id, stamp] : incoming.deletion_templates) {
            bool protect = false;
            for (const auto& record : state.templates[key]) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            for (const auto& record : incoming.templates) {
                if (record.id == id && record.updated_at.has_value() &&
                    *record.updated_at > stamp) {
                    protect = true;
                }
            }
            if (!protect) {
                template_tomb[id] = std::max(template_tomb[id], stamp);
            }
        }
        std::map<std::string, JsonRecord> template_by_id;
        for (const auto& record : state.templates[key]) {
            template_by_id[record.id] = record;
        }
        merge_templates(incoming, key, state, template_tomb,
                        state.preview.stats, removed_ids, blocked);
        if (blocked) {
            state.templates[key] = std::vector<JsonRecord>{};
            for (const auto& [id, record] : template_by_id) {
                (void)id;
                state.templates[key].push_back(record);
            }
            state.tombstones = original_tomb;
        }

        if (incoming.blocked.count(key + ".quickDoses") == 0) {
            merge_unstamped(state.quick[key], incoming.quick, added,
                            state.preview.blocked, key + ".quickDoses");
        } else {
            state.preview.blocked.push_back(key + ".quickDoses");
        }
        if (incoming.blocked.count(key + ".supplies") == 0) {
            merge_unstamped(state.supplies[key], incoming.supplies, added,
                            state.preview.blocked, key + ".supplies");
        } else {
            state.preview.blocked.push_back(key + ".supplies");
        }
        if (incoming.blocked.count(key + ".plan") == 0) {
            merge_unstamped(state.plan[key], incoming.plan, added,
                            state.preview.blocked, key + ".plan");
        } else {
            state.preview.blocked.push_back(key + ".plan");
        }

        state.preview.stats.added += added;
        state.preview.stats.updated += updated;
        state.preview.stats.skipped += skipped;
        state.preview.stats.removed += removed;
        state.preview.removed_ids.insert(state.preview.removed_ids.end(),
                                         removed_ids.begin(), removed_ids.end());
    }

    // Scalars.
    if (parsed.weight_present) {
        if (!state.settings.body_weight_kg.has_value()) {
            state.settings.body_weight_kg = parsed.weight;
            state.settings.weight_updated_at = parsed.weight_stamp;
        } else {
            const auto local_stamp_value =
                state.settings.weight_updated_at.value_or(0);
            const auto incoming_stamp_value = parsed.weight_stamp.value_or(1);
            if (incoming_stamp_value > local_stamp_value) {
                state.settings.body_weight_kg = parsed.weight;
                state.settings.weight_updated_at = parsed.weight_stamp;
            } else if (incoming_stamp_value == local_stamp_value) {
                const auto local_text =
                    std::to_string(*state.settings.body_weight_kg);
                const auto incoming_text = std::to_string(parsed.weight);
                if (incoming_text > local_text) {
                    state.settings.body_weight_kg = parsed.weight;
                    state.settings.weight_updated_at = parsed.weight_stamp;
                }
            }
        }
    }
    if (parsed.birth_present) {
        if (!state.settings.birth_year.has_value()) {
            state.settings.birth_year = parsed.birth_year;
            state.settings.birth_year_updated_at = parsed.birth_stamp;
        } else {
            const auto local_stamp_value =
                state.settings.birth_year_updated_at.value_or(0);
            const auto incoming_stamp_value = parsed.birth_stamp.value_or(1);
            if (incoming_stamp_value > local_stamp_value) {
                state.settings.birth_year = parsed.birth_year;
                state.settings.birth_year_updated_at = parsed.birth_stamp;
            }
        }
    }
    if (parsed.pk_present) {
        const auto local_present = state.settings.pk_params_json.has_value();
        const auto local_stamp_value =
            state.settings.pk_params_updated_at.value_or(0);
        const auto incoming_stamp_value = parsed.pk_stamp.value_or(1);
        if (!local_present || incoming_stamp_value > local_stamp_value) {
            if (parsed.pk_null) {
                state.settings.pk_params_json.reset();
            } else {
                state.settings.pk_params_json = parsed.pk_json;
            }
            state.settings.pk_params_updated_at = parsed.pk_stamp;
        }
    }
    if (parsed.calibration_present) {
        state.settings.calibration_method = parsed.calibration_method;
        state.settings.calibration_history_mode = parsed.calibration_history;
    }
    state.settings.updated_at = now_ms;
    for (const auto& [key, value] : parsed.preferences) {
        state.preferences[key] = value;
    }
    return state;
}

json event_json(const DoseEvent& event) {
    json value{{"id", event.id},
               {"route", event.route},
               {"ester", event.ester},
               {"timeH", event.time_h},
               {"doseMG", event.dose_mg},
               {"extras", json::parse(event.extras_json)}};
    if (event.updated_at.has_value()) {
        value["updatedAt"] = *event.updated_at;
    }
    return value;
}

json lab_json(const LabResult& lab) {
    json value{{"id", lab.id},        {"timeH", lab.time_h},
               {"concValue", lab.value}, {"unit", lab.unit},
               {"assay", lab.assay}};
    if (lab.updated_at.has_value()) {
        value["updatedAt"] = *lab.updated_at;
    }
    return value;
}

json record_json(const JsonRecord& record) {
    json value = json::parse(record.payload_json);
    if (record.updated_at.has_value()) {
        value["updatedAt"] = *record.updated_at;
    }
    return value;
}

template <typename Repositories>
void write_merged_state(typename Repositories::Store& store,
                        const MergedState& merged) {
    using SettingsRepo = typename Repositories::Settings;
    using PreferencesRepo = typename Repositories::Preferences;
    using TombstoneRepo = typename Repositories::Tombstones;
    using EventRepo = typename Repositories::Events;
    using LabRepo = typename Repositories::Labs;
    using JsonRepo = typename Repositories::JsonRecords;

    SettingsRepo(store).save(merged.settings);
    EventRepo events(store);
    for (const auto& [key, records] : merged.events) {
        (void)key;
        for (const auto& record : records) {
            events.upsert(record);
        }
    }
    LabRepo labs(store);
    for (const auto& [key, records] : merged.labs) {
        (void)key;
        for (const auto& record : records) {
            labs.upsert(record);
        }
    }
    const auto apply_json = [&](const std::map<std::string,
                                               std::vector<JsonRecord>>& table,
                                JsonRecordKind kind) {
        JsonRepo repository(store, kind);
        for (const auto& [key, records] : table) {
            (void)key;
            for (const auto& record : records) {
                repository.upsert(record);
            }
        }
    };
    apply_json(merged.templates, JsonRecordKind::DoseTemplate);
    apply_json(merged.quick, JsonRecordKind::QuickDose);
    apply_json(merged.supplies, JsonRecordKind::Supply);
    apply_json(merged.plan, JsonRecordKind::PlanItem);
    TombstoneRepo tombstones(store);
    for (const auto& [kind, records] : merged.tombstones) {
        for (const auto& [id, stamp] : records) {
            tombstones.record(Tombstone{kind, id, stamp});
        }
    }
    PreferencesRepo preferences(store);
    for (const auto& [key, value] : merged.preferences) {
        if (contains(kPreferences, key)) {
            preferences.set(Preference{key, value, 0});
        }
    }
}

}  // namespace

namespace {

json build_export(const LocalState& state, std::int64_t now_ms) {
    json root{{"format", kFormat},
              {"formatVersion", kFormatVersion},
              {"createdAt", iso_utc(now_ms)},
              {"mode", "transfem"},
              {"modes", json::object()}};
    if (state.settings.body_weight_kg.has_value()) {
        root["weight"] = *state.settings.body_weight_kg;
        if (state.settings.weight_updated_at.has_value()) {
            root["weightUpdatedAt"] = *state.settings.weight_updated_at;
        }
    }
    if (state.settings.birth_year.has_value()) {
        root["birthYear"] = *state.settings.birth_year;
        if (state.settings.birth_year_updated_at.has_value()) {
            root["birthYearUpdatedAt"] = *state.settings.birth_year_updated_at;
        }
    }
    if (state.settings.pk_params_json.has_value()) {
        root["pkParams"] = json::parse(*state.settings.pk_params_json);
        if (state.settings.pk_params_updated_at.has_value()) {
            root["pkParamsUpdatedAt"] = *state.settings.pk_params_updated_at;
        }
    }
    root["calibration"] = {{"method", state.settings.calibration_method},
                           {"historyMode",
                            state.settings.calibration_history_mode}};
    json preferences = json::object();
    for (const auto& preference : state.preferences) {
        if (contains(kPreferences, preference.key)) {
            preferences[preference.key] = json::parse(preference.value_json);
        }
    }
    root["preferences"] = std::move(preferences);

    std::map<std::string, std::map<std::string, std::int64_t>> tombstones;
    for (const auto& tombstone : state.tombstones) {
        if (now_ms - tombstone.deleted_at < kTombstoneTtlMs) {
            tombstones[tombstone.kind][tombstone.record_id] =
                std::max(tombstones[tombstone.kind][tombstone.record_id],
                         tombstone.deleted_at);
        }
    }

    for (const auto mode : {Mode::Transfem, Mode::Transmasc}) {
        const auto key = mode_key(mode);
        json block;
        auto& events = block["events"];
        events = json::array();
        for (const auto& record : state.events.at(key)) {
            events.push_back(event_json(record));
        }
        auto& labs = block["labResults"];
        labs = json::array();
        for (const auto& record : state.labs.at(key)) {
            labs.push_back(lab_json(record));
        }
        auto& templates = block["doseTemplates"];
        templates = json::array();
        for (const auto& record : state.templates.at(key)) {
            templates.push_back(record_json(record));
        }
        auto& quick = block["quickDoses"];
        quick = json::array();
        for (const auto& record : state.quick.at(key)) {
            quick.push_back(record_json(record));
        }
        auto& supplies = block["supplies"];
        supplies = json::array();
        for (const auto& record : state.supplies.at(key)) {
            supplies.push_back(record_json(record));
        }
        auto& plan = block["plan"];
        plan = json::array();
        for (const auto& record : state.plan.at(key)) {
            plan.push_back(record_json(record));
        }
        block["deletions"] = json::object();
        for (const auto& [kind, records] : tombstones) {
            json map = json::object();
            for (const auto& [id, stamp] : records) {
                map[id] = stamp;
            }
            block["deletions"][kind] = std::move(map);
        }
        root["modes"][key] = std::move(block);
    }
    return root;
}

std::string read_backup_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw Error("could not open backup: " + path.string());
    }
    std::string bytes((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
    if (input.bad()) {
        throw Error("could not read backup: " + path.string());
    }
    return bytes;
}

json parse_backup_file(const std::filesystem::path& path,
                       std::string_view backup_phrase) {
    const auto bytes = read_backup_file(path);
    std::string plaintext;
    if (is_encrypted_backup(bytes)) {
        if (backup_phrase.empty()) {
            throw Error("encrypted backup requires a backup phrase");
        }
        plaintext = decrypt_backup_json(bytes, backup_phrase);
    } else {
        plaintext = bytes;
    }
    try {
        return json::parse(plaintext);
    } catch (const std::exception& error) {
        throw Error("could not parse backup: " + std::string(error.what()));
    }
}

void fill_counts(BackupPreview& preview, const ParsedBundle& parsed) {
    for (const auto& [key, mode] : parsed.modes) {
        auto& counts = preview.counts[key];
        counts.events = mode.events.size();
        counts.lab_results = mode.labs.size();
        counts.dose_templates = mode.templates.size();
        counts.quick_doses = mode.quick.size();
        counts.supplies = mode.supplies.size();
        counts.plan = mode.plan.size();
        counts.deletions =
            mode.deletion_events.size() + mode.deletion_labs.size() +
            mode.deletion_templates.size();
    }
}

void write_backup_file(const std::filesystem::path& path,
                       std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw Error("could not write backup: " + path.string());
    }
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        throw Error("could not write backup: " + path.string());
    }
    (void)::chmod(path.c_str(), 0600);
}

}  // namespace

BackupPreview preview_backup(Connection& connection,
                             const std::filesystem::path& path,
                             std::int64_t now_ms) {
    const auto root = parse_backup_file(path, {});
    const auto parsed = parse_bundle(root, now_ms);
    if (!parsed.valid) {
        BackupPreview preview;
        preview.ok = false;
        preview.errors = parsed.errors;
        preview.issues = parsed.issues;
        preview.missing = parsed.missing;
        return preview;
    }
    auto merged = merge_bundle<SqlRepositories>(connection, parsed, now_ms);
    fill_counts(merged.preview, parsed);
    return merged.preview;
}

BackupPreview preview_backup(VaultStore& vault,
                             const std::filesystem::path& path,
                             std::int64_t now_ms,
                             std::string_view backup_phrase) {
    const auto root = parse_backup_file(path, backup_phrase);
    const auto parsed = parse_bundle(root, now_ms);
    if (!parsed.valid) {
        BackupPreview preview;
        preview.ok = false;
        preview.errors = parsed.errors;
        preview.issues = parsed.issues;
        preview.missing = parsed.missing;
        return preview;
    }
    auto merged = merge_bundle<VaultRepositories>(vault, parsed, now_ms);
    fill_counts(merged.preview, parsed);
    return merged.preview;
}

BackupImportResult import_backup(Connection& connection,
                                 const std::filesystem::path& path,
                                 std::int64_t now_ms) {
    const auto root = parse_backup_file(path, {});
    const auto parsed = parse_bundle(root, now_ms);
    if (!parsed.valid) {
        throw Error("backup envelope is invalid: " + parsed.errors.front());
    }
    Transaction transaction(connection);
    auto merged = merge_bundle<SqlRepositories>(connection, parsed, now_ms);
    fill_counts(merged.preview, parsed);
    (void)connection.exec(
        "DELETE FROM dose_events; DELETE FROM lab_results; "
        "DELETE FROM dose_templates; DELETE FROM quick_doses; "
        "DELETE FROM supplies; DELETE FROM plan_items; DELETE FROM tombstones;");
    write_merged_state<SqlRepositories>(connection, merged);
    transaction.commit();
    return BackupImportResult{merged.preview, true};
}

BackupImportResult import_backup(VaultStore& vault,
                                 const std::filesystem::path& path,
                                 std::int64_t now_ms,
                                 std::string_view backup_phrase) {
    const auto root = parse_backup_file(path, backup_phrase);
    const auto parsed = parse_bundle(root, now_ms);
    if (!parsed.valid) {
        throw Error("backup envelope is invalid: " + parsed.errors.front());
    }
    vault.begin();
    try {
        auto merged = merge_bundle<VaultRepositories>(vault, parsed, now_ms);
        fill_counts(merged.preview, parsed);
        vault.clear();
        write_merged_state<VaultRepositories>(vault, merged);
        vault.commit();
        return BackupImportResult{merged.preview, true};
    } catch (...) {
        try {
            vault.rollback();
        } catch (...) {
            // Preserve the original failure.
        }
        throw;
    }
}

std::string export_backup_json(VaultStore& vault, std::int64_t now_ms) {
    return build_export(load_local<VaultRepositories>(vault), now_ms).dump();
}

void export_backup(Connection& connection,
                   const std::filesystem::path& path, std::int64_t now_ms) {
    const auto root = build_export(load_local<SqlRepositories>(connection),
                                   now_ms);
    write_backup_file(path, root.dump(2) + "\n");
}

void export_backup(VaultStore& vault, const std::filesystem::path& path,
                   std::int64_t now_ms) {
    write_backup_file(path, export_backup_json(vault, now_ms));
}

void export_encrypted_backup(VaultStore& vault,
                             const std::filesystem::path& path,
                             std::int64_t now_ms,
                             std::string_view backup_phrase) {
    const auto sealed =
        encrypt_backup_json(export_backup_json(vault, now_ms), backup_phrase);
    write_backup_file(path, sealed);
}

}  // namespace oyama::storage
