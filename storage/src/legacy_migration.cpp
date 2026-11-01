#include "oyama/storage/legacy_migration.h"

#include "oyama/storage/uuid.h"
#include "oyama/storage/vault_codec.h"
#include "repository_util.h"

#include <nlohmann/json.hpp>
#include <sodium.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oyama { namespace storage {
namespace {

using json = nlohmann::json;

constexpr const char* kSettingsId =
    "00000000-0000-0000-0000-000000000001";
constexpr const char* kPreferencesId =
    "00000000-0000-0000-0000-000000000002";

struct SourceSnapshot {
    oyama::optional<Settings> settings;
    std::vector<Preference> preferences;
    std::vector<DoseEvent> dose_events;
    std::vector<LabResult> lab_results;
    std::vector<JsonRecord> dose_templates;
    std::vector<JsonRecord> quick_doses;
    std::vector<JsonRecord> supplies;
    std::vector<JsonRecord> plan_items;
    std::vector<Tombstone> tombstones;
};

struct Projection {
    std::string kind;
    std::string logical_id;
    std::uint64_t updated_at{0};
    std::string payload;
    Projection() = default;
    Projection(std::string k, std::string l, std::uint64_t t, std::string p)
        : kind(std::move(k)), logical_id(std::move(l)), updated_at(t),
          payload(std::move(p)) {}
};

[[noreturn]] void fail(const std::string& kind, const std::string& id,
                       const std::string& field, const std::string& message) {
    throw LegacyMigrationError(kind, id, field, message);
}

template <typename Function>
auto with_codec_context(const std::string& kind, const std::string& id,
                         Function&& function)
    -> decltype(std::forward<Function>(function)()) {
    try {
        return std::forward<Function>(function)();
    } catch (const VaultCodecError& error) {
        throw LegacyMigrationError(
            error.kind().empty() ? kind : error.kind(),
            error.id().empty() ? id : error.id(), error.field(), error.what());
    }
}

void require_non_negative(const std::string& kind, const std::string& id,
                          std::int64_t value, const char* field) {
    if (value < 0) {
        fail(kind, id, field, "must not be negative");
    }
}

std::uint64_t required_stamp(const std::string& kind, const std::string& id,
                             std::int64_t value, const char* field) {
    require_non_negative(kind, id, value, field);
    return static_cast<std::uint64_t>(value);
}

std::uint64_t optional_stamp(const std::string& kind, const std::string& id,
                             const oyama::optional<std::int64_t>& value) {
    if (!value.has_value()) {
        return 0;
    }
    return required_stamp(kind, id, *value, "updatedAt");
}

std::string canonical_value(const std::string& kind, const std::string& id,
                            const std::string& text) {
    try {
        return json::parse(text).dump();
    } catch (const json::exception& error) {
        fail(kind, id, "value", error.what());
    }
}

bool relation_exists(Connection& source, const std::string& relation) {
    const auto result = source.exec("SELECT to_regclass('" + relation +
                                    "') IS NOT NULL");
    return result.boolean(0, 0);
}

bool column_exists(Connection& source, const std::string& table,
                   const std::string& column) {
    const auto result = source.exec(
        "SELECT EXISTS (SELECT 1 FROM information_schema.columns "
        "WHERE table_schema = current_schema() AND table_name = '" +
        table + "' AND column_name = '" + column + "')");
    return result.boolean(0, 0);
}

bool table_has_rows(Connection& source, const std::string& table) {
    if (!relation_exists(source, table)) {
        return false;
    }
    const auto result =
        source.exec("SELECT EXISTS (SELECT 1 FROM " + table + ")");
    return result.boolean(0, 0);
}

Mode read_mode(const Result& result, int row, const std::string& kind,
               const std::string& id) {
    const auto mode = parse_mode(result.text(row, 1));
    if (!mode.has_value()) {
        fail(kind, id, "mode", "must be transfem or transmasc");
    }
    return *mode;
}

oyama::optional<Settings> read_settings(Connection& source) {
    const bool has_weight_stamp =
        column_exists(source, "settings", "weight_updated_at");
    const bool has_birth_stamp =
        column_exists(source, "settings", "birth_year_updated_at");
    const auto result = source.exec(
        "SELECT body_weight_kg, " +
        std::string(has_weight_stamp ? "weight_updated_at"
                                     : "NULL::bigint") +
        ", birth_year, " +
        std::string(has_birth_stamp ? "birth_year_updated_at"
                                    : "NULL::bigint") +
        ", pk_params::text, pk_params_updated_at, calibration_method, "
        "calibration_history_mode, updated_at FROM settings WHERE id = true");
    if (result.rows() == 0) {
        return oyama::nullopt;
    }
    Settings settings;
    settings.body_weight_kg = detail::nullable_number(result, 0, 0);
    settings.weight_updated_at = detail::nullable_integer(result, 0, 1);
    if (!result.is_null(0, 2)) {
        settings.birth_year = static_cast<int>(result.integer(0, 2));
    }
    settings.birth_year_updated_at = detail::nullable_integer(result, 0, 3);
    settings.pk_params_json = detail::nullable_text(result, 0, 4);
    settings.pk_params_updated_at = detail::nullable_integer(result, 0, 5);
    settings.calibration_method = result.text(0, 6);
    settings.calibration_history_mode = result.text(0, 7);
    settings.updated_at = result.integer(0, 8);
    return settings;
}

std::vector<Preference> read_preferences(Connection& source) {
    const auto result = source.exec(
        "SELECT key, value::text, updated_at FROM preferences ORDER BY key");
    std::vector<Preference> preferences;
    preferences.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        preferences.push_back(Preference{result.text(row, 0),
                                         result.text(row, 1),
                                         result.integer(row, 2)});
    }
    return preferences;
}

std::vector<DoseEvent> read_dose_events(Connection& source) {
    const auto result = source.exec(
        "SELECT id::text, mode, route, ester, time_h, dose_mg, extras::text, "
        "updated_at, created_at FROM dose_events ORDER BY id");
    std::vector<DoseEvent> events;
    events.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        DoseEvent event;
        event.id = result.text(row, 0);
        event.mode = read_mode(result, row, "dose_event", event.id);
        event.route = result.text(row, 2);
        event.ester = result.text(row, 3);
        event.time_h = result.number(row, 4);
        event.dose_mg = result.number(row, 5);
        event.extras_json = result.text(row, 6);
        event.updated_at = detail::nullable_integer(result, row, 7);
        event.created_at = result.integer(row, 8);
        events.push_back(std::move(event));
    }
    return events;
}

std::vector<LabResult> read_lab_results(Connection& source) {
    const auto result = source.exec(
        "SELECT id::text, mode, time_h, value, unit, assay, updated_at, "
        "created_at FROM lab_results ORDER BY id");
    std::vector<LabResult> results;
    results.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        LabResult lab;
        lab.id = result.text(row, 0);
        lab.mode = read_mode(result, row, "lab_result", lab.id);
        lab.time_h = result.number(row, 2);
        lab.value = result.number(row, 3);
        lab.unit = result.text(row, 4);
        lab.assay = result.text(row, 5);
        lab.updated_at = detail::nullable_integer(result, row, 6);
        lab.created_at = result.integer(row, 7);
        results.push_back(std::move(lab));
    }
    return results;
}

std::vector<JsonRecord> read_json_records(Connection& source,
                                          JsonRecordKind kind) {
    const std::string table = table_name(kind);
    const auto result = source.exec("SELECT id::text, mode, payload::text, "
                                    "updated_at FROM " +
                                    table + " ORDER BY id");
    std::vector<JsonRecord> records;
    records.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        JsonRecord record;
        record.id = result.text(row, 0);
        record.mode = read_mode(result, row, table, record.id);
        record.payload_json = result.text(row, 2);
        record.updated_at = detail::nullable_integer(result, row, 3);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<Tombstone> read_tombstones(Connection& source) {
    const auto result = source.exec(
        "SELECT kind, record_id::text, deleted_at FROM tombstones "
        "ORDER BY kind, record_id");
    std::vector<Tombstone> tombstones;
    tombstones.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        tombstones.push_back(Tombstone{result.text(row, 0),
                                       result.text(row, 1),
                                       result.integer(row, 2)});
    }
    return tombstones;
}

SourceSnapshot read_source(Connection& source) {
    SourceSnapshot snapshot;
    snapshot.settings = read_settings(source);
    snapshot.preferences = read_preferences(source);
    snapshot.dose_events = read_dose_events(source);
    snapshot.lab_results = read_lab_results(source);
    snapshot.dose_templates =
        read_json_records(source, JsonRecordKind::DoseTemplate);
    snapshot.quick_doses =
        read_json_records(source, JsonRecordKind::QuickDose);
    snapshot.supplies = read_json_records(source, JsonRecordKind::Supply);
    snapshot.plan_items = read_json_records(source, JsonRecordKind::PlanItem);
    snapshot.tombstones = read_tombstones(source);
    return snapshot;
}

LegacyCounts counts_of(const SourceSnapshot& snapshot) {
    LegacyCounts counts;
    counts.settings = snapshot.settings.has_value() ? 1 : 0;
    counts.preferences = snapshot.preferences.size();
    counts.dose_events = snapshot.dose_events.size();
    counts.lab_results = snapshot.lab_results.size();
    counts.dose_templates = snapshot.dose_templates.size();
    counts.quick_doses = snapshot.quick_doses.size();
    counts.supplies = snapshot.supplies.size();
    counts.plan_items = snapshot.plan_items.size();
    counts.tombstones = snapshot.tombstones.size();
    return counts;
}

void report(const LegacyMigrator::Progress& progress, oyama::string_view kind,
            std::size_t done, std::size_t total) {
    if (progress) {
        progress(kind, done, total);
    }
}

std::string json_kind_name(JsonRecordKind kind) {
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

void import_json_records(VaultStore& target,
                         const std::vector<JsonRecord>& records,
                         JsonRecordKind record_kind,
                         const LegacyMigrator::Progress& progress) {
    const std::string kind = json_kind_name(record_kind);
    const std::size_t total = records.size();
    report(progress, kind, 0, total);
    std::size_t done = 0;
    for (const auto& record : records) {
        const std::uint64_t stamp = optional_stamp(kind, record.id,
                                                   record.updated_at);
        const std::string payload = with_codec_context(
            kind, record.id,
            [&record, record_kind] {
                return encode_json_record(record, record_kind);
            });
        target.put(VaultRecord{record.id, kind, stamp, payload});
        ++done;
        report(progress, kind, done, total);
    }
}

void import_dose_events(VaultStore& target,
                        const std::vector<DoseEvent>& events,
                        const LegacyMigrator::Progress& progress) {
    const std::string kind = "dose_event";
    const std::size_t total = events.size();
    report(progress, kind, 0, total);
    std::size_t done = 0;
    for (const auto& event : events) {
        require_non_negative(kind, event.id, event.created_at, "createdAt");
        const std::uint64_t stamp =
            optional_stamp(kind, event.id, event.updated_at);
        const std::string payload = with_codec_context(
            kind, event.id, [&event] { return encode_dose_event(event); });
        target.put(VaultRecord{event.id, kind, stamp, payload});
        ++done;
        report(progress, kind, done, total);
    }
}

void import_lab_results(VaultStore& target,
                        const std::vector<LabResult>& results,
                        const LegacyMigrator::Progress& progress) {
    const std::string kind = "lab_result";
    const std::size_t total = results.size();
    report(progress, kind, 0, total);
    std::size_t done = 0;
    for (const auto& result : results) {
        require_non_negative(kind, result.id, result.created_at, "createdAt");
        const std::uint64_t stamp =
            optional_stamp(kind, result.id, result.updated_at);
        const std::string payload = with_codec_context(
            kind, result.id, [&result] { return encode_lab_result(result); });
        target.put(VaultRecord{result.id, kind, stamp, payload});
        ++done;
        report(progress, kind, done, total);
    }
}

void import_tombstones(VaultStore& target,
                       const std::vector<Tombstone>& tombstones,
                       const LegacyMigrator::Progress& progress) {
    const std::string kind = "tombstone";
    const std::size_t total = tombstones.size();
    report(progress, kind, 0, total);
    std::size_t done = 0;
    for (const auto& tombstone : tombstones) {
        require_non_negative(kind, tombstone.record_id, tombstone.deleted_at,
                             "deletedAt");
        const std::uint64_t stamp =
            static_cast<std::uint64_t>(tombstone.deleted_at);
        const std::string payload = with_codec_context(
            kind, tombstone.record_id,
            [&tombstone] { return encode_tombstone(tombstone); });
        target.put(VaultRecord{generate_uuid_v4(), kind, stamp, payload});
        ++done;
        report(progress, kind, done, total);
    }
}

void add_json_record_projections(std::vector<Projection>& projections,
                                 const std::vector<JsonRecord>& records,
                                 JsonRecordKind record_kind) {
    const std::string kind = json_kind_name(record_kind);
    for (const auto& record : records) {
        const std::uint64_t stamp = optional_stamp(kind, record.id,
                                                   record.updated_at);
        const std::string payload = with_codec_context(
            kind, record.id,
            [&record, record_kind] {
                return encode_json_record(record, record_kind);
            });
        projections.push_back(Projection{kind, record.id, stamp, payload});
    }
}

std::vector<Projection> source_projections(const SourceSnapshot& snapshot) {
    std::vector<Projection> projections;
    if (snapshot.settings.has_value()) {
        const Settings& settings = *snapshot.settings;
        const std::uint64_t stamp =
            required_stamp("settings", kSettingsId, settings.updated_at,
                           "updatedAt");
        const std::string payload = with_codec_context(
            "settings", kSettingsId,
            [&settings] { return encode_settings(settings); });
        projections.push_back(
            Projection{"settings", kSettingsId, stamp, payload});
    }
    for (const auto& preference : snapshot.preferences) {
        const std::uint64_t stamp =
            required_stamp("preferences", preference.key,
                           preference.updated_at, "updatedAt");
        projections.push_back(
            Projection{"preferences", preference.key, stamp,
                       canonical_value("preferences", preference.key,
                                       preference.value_json)});
    }
    for (const auto& event : snapshot.dose_events) {
        const std::uint64_t stamp =
            optional_stamp("dose_event", event.id, event.updated_at);
        projections.push_back(Projection{
            "dose_event", event.id, stamp,
            with_codec_context("dose_event", event.id, [&event] {
                return encode_dose_event(event);
            })});
    }
    for (const auto& result : snapshot.lab_results) {
        const std::uint64_t stamp =
            optional_stamp("lab_result", result.id, result.updated_at);
        projections.push_back(Projection{
            "lab_result", result.id, stamp,
            with_codec_context("lab_result", result.id, [&result] {
                return encode_lab_result(result);
            })});
    }
    add_json_record_projections(projections, snapshot.dose_templates,
                                JsonRecordKind::DoseTemplate);
    add_json_record_projections(projections, snapshot.quick_doses,
                                JsonRecordKind::QuickDose);
    add_json_record_projections(projections, snapshot.supplies,
                                JsonRecordKind::Supply);
    add_json_record_projections(projections, snapshot.plan_items,
                                JsonRecordKind::PlanItem);
    for (const auto& tombstone : snapshot.tombstones) {
        const std::uint64_t stamp =
            required_stamp("tombstone", tombstone.record_id,
                           tombstone.deleted_at, "deletedAt");
        projections.push_back(Projection{
            "tombstone", tombstone.kind + ":" + tombstone.record_id, stamp,
            with_codec_context("tombstone", tombstone.record_id,
                                [&tombstone] {
                                    return encode_tombstone(tombstone);
                                })});
    }
    return projections;
}

std::vector<Projection> target_projections(VaultStore& target) {
    std::vector<Projection> projections;
    for (const auto& record : target.scan()) {
        if (record.kind == "settings") {
            const Settings settings = with_codec_context(
                "settings", record.id,
                [&record] { return decode_settings(record.payload); });
            const std::uint64_t stamp = required_stamp(
                "settings", kSettingsId, settings.updated_at, "updatedAt");
            projections.push_back(Projection{
                "settings", kSettingsId, stamp,
                with_codec_context("settings", kSettingsId, [&settings] {
                    return encode_settings(settings);
                })});
            continue;
        }
        if (record.kind == "preferences") {
            const auto preferences = with_codec_context(
                "preferences", record.id,
                [&record] { return decode_preferences(record.payload); });
            for (const auto& preference : preferences) {
                const std::uint64_t stamp = required_stamp(
                    "preferences", preference.key, preference.updated_at,
                    "updatedAt");
                projections.push_back(Projection{
                    "preferences", preference.key, stamp,
                    canonical_value("preferences", preference.key,
                                    preference.value_json)});
            }
            continue;
        }
        if (record.kind == "dose_event") {
            const DoseEvent event = with_codec_context(
                "dose_event", record.id,
                [&record] { return decode_dose_event(record.payload); });
            projections.push_back(Projection{
                "dose_event", event.id, record.updated_at_ms,
                with_codec_context("dose_event", event.id, [&event] {
                    return encode_dose_event(event);
                })});
            continue;
        }
        if (record.kind == "lab_result") {
            const LabResult result = with_codec_context(
                "lab_result", record.id,
                [&record] { return decode_lab_result(record.payload); });
            projections.push_back(Projection{
                "lab_result", result.id, record.updated_at_ms,
                with_codec_context("lab_result", result.id, [&result] {
                    return encode_lab_result(result);
                })});
            continue;
        }
        if (record.kind == "dose_template" ||
            record.kind == "quick_dose" || record.kind == "supply" ||
            record.kind == "plan_item") {
            const JsonRecordKind record_kind =
                record.kind == "dose_template" ? JsonRecordKind::DoseTemplate
                : record.kind == "quick_dose" ? JsonRecordKind::QuickDose
                : record.kind == "supply"     ? JsonRecordKind::Supply
                                              : JsonRecordKind::PlanItem;
            const JsonRecord decoded = with_codec_context(
                record.kind, record.id,
                [&record, record_kind] {
                    return decode_json_record(record.payload, record_kind);
                });
            projections.push_back(Projection{
                record.kind, decoded.id, record.updated_at_ms,
                with_codec_context(record.kind, decoded.id,
                                    [&decoded, record_kind] {
                                        return encode_json_record(
                                            decoded, record_kind);
                                    })});
            continue;
        }
        if (record.kind == "tombstone") {
            const Tombstone tombstone = with_codec_context(
                "tombstone", record.id,
                [&record] { return decode_tombstone(record.payload); });
            const std::uint64_t stamp = required_stamp(
                "tombstone", tombstone.record_id, tombstone.deleted_at,
                "deletedAt");
            projections.push_back(Projection{
                "tombstone", tombstone.kind + ":" + tombstone.record_id, stamp,
                with_codec_context("tombstone", tombstone.record_id,
                                    [&tombstone] {
                                        return encode_tombstone(tombstone);
                                    })});
            continue;
        }
        fail("vault", record.id, "kind",
             "unknown vault record kind: " + record.kind);
    }
    return projections;
}

std::string hash_projections(std::vector<Projection> projections) {
    std::sort(projections.begin(), projections.end(),
              [](const Projection& left, const Projection& right) {
                  if (left.kind != right.kind) {
                      return left.kind < right.kind;
                  }
                  if (left.logical_id != right.logical_id) {
                      return left.logical_id < right.logical_id;
                  }
                  return left.payload < right.payload;
              });
    json records = json::array();
    for (const auto& projection : projections) {
        json payload =
            json::parse(projection.payload, nullptr, false);
        if (payload.is_discarded()) {
            fail("canonical", projection.logical_id, "payload",
                 "canonical payload is not valid JSON");
        }
        records.push_back({{"kind", projection.kind},
                           {"id", projection.logical_id},
                           {"updatedAt", projection.updated_at},
                           {"payload", payload}});
    }
    const std::string canonical =
        records.dump(2, ' ', false, json::error_handler_t::strict);

    unsigned char digest[crypto_hash_sha256_BYTES];
    if (crypto_hash_sha256(
            digest,
            reinterpret_cast<const unsigned char*>(canonical.data()),
            canonical.size()) != 0) {
        fail("canonical", "", "hash", "SHA-256 calculation failed");
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(crypto_hash_sha256_BYTES * 2);
    for (unsigned char byte : digest) {
        result.push_back(kHex[byte >> 4]);
        result.push_back(kHex[byte & 0x0f]);
    }
    return result;
}

}  // namespace

LegacyMigrationError::LegacyMigrationError(std::string kind, std::string id,
                                           std::string field,
                                           std::string message)
    : std::runtime_error(
          (kind.empty() ? std::string("legacy record") : kind) +
          (id.empty() ? std::string() : " " + id) + " field " + field + ": " +
          message),
      kind_(std::move(kind)),
      id_(std::move(id)),
      field_(std::move(field)) {}

LegacyDetection detect_legacy(Connection& source) {
    LegacyDetection detection;
    if (relation_exists(source, "vault_meta")) {
        const auto meta = source.exec("SELECT EXISTS (SELECT 1 FROM vault_meta)");
        detection.has_meta = meta.boolean(0, 0);
    }

    if (relation_exists(source, "settings")) {
        const auto settings = source.exec(
            "SELECT EXISTS (SELECT 1 FROM settings WHERE "
            "body_weight_kg IS NOT NULL OR birth_year IS NOT NULL OR "
            "pk_params IS NOT NULL OR pk_params_updated_at IS NOT NULL OR "
            "calibration_method <> 'mipd' OR "
            "calibration_history_mode <> 'retrospective' OR "
            "updated_at <> 0)");
        detection.has_legacy = settings.boolean(0, 0);
    }

    for (const char* table :
         {"dose_events", "lab_results", "dose_templates", "quick_doses",
          "supplies", "plan_items", "preferences", "tombstones"}) {
        detection.has_legacy =
            detection.has_legacy || table_has_rows(source, table);
    }
    return detection;
}

LegacyMigrator::LegacyMigrator(Connection& source, VaultStore& target,
                               Progress progress)
    : source_(source), target_(target), progress_(std::move(progress)) {}

LegacyImportResult LegacyMigrator::import_all() {
    const SourceSnapshot snapshot = read_source(source_);
    const LegacyCounts counts = counts_of(snapshot);

    target_.begin();
    try {
        target_.clear();

        if (snapshot.settings.has_value()) {
            report(progress_, "settings", 0, 1);
            const Settings& settings = *snapshot.settings;
            const std::uint64_t stamp = required_stamp(
                "settings", kSettingsId, settings.updated_at, "updatedAt");
            const std::string payload = with_codec_context(
                "settings", kSettingsId,
                [&settings] { return encode_settings(settings); });
            target_.put(
                VaultRecord{kSettingsId, "settings", stamp, payload});
            report(progress_, "settings", 1, 1);
        } else {
            report(progress_, "settings", 0, 0);
        }

        const std::size_t preference_count = snapshot.preferences.size();
        report(progress_, "preferences", 0, preference_count);
        if (preference_count != 0) {
            std::int64_t max_stamp = 0;
            for (const auto& preference : snapshot.preferences) {
                require_non_negative("preferences", preference.key,
                                     preference.updated_at, "updatedAt");
                max_stamp = std::max(max_stamp, preference.updated_at);
            }
            const std::string payload = with_codec_context(
                "preferences", kPreferencesId,
                [&snapshot] {
                    return encode_preferences(snapshot.preferences);
                });
            target_.put(VaultRecord{kPreferencesId, "preferences",
                                    static_cast<std::uint64_t>(max_stamp),
                                    payload});
        }
        for (std::size_t index = 0; index < preference_count; ++index) {
            report(progress_, "preferences", index + 1, preference_count);
        }

        import_dose_events(target_, snapshot.dose_events, progress_);
        import_lab_results(target_, snapshot.lab_results, progress_);
        import_json_records(target_, snapshot.dose_templates,
                            JsonRecordKind::DoseTemplate, progress_);
        import_json_records(target_, snapshot.quick_doses,
                            JsonRecordKind::QuickDose, progress_);
        import_json_records(target_, snapshot.supplies,
                            JsonRecordKind::Supply, progress_);
        import_json_records(target_, snapshot.plan_items,
                            JsonRecordKind::PlanItem, progress_);
        import_tombstones(target_, snapshot.tombstones, progress_);

        target_.commit();
    } catch (...) {
        try {
            target_.rollback();
        } catch (...) {
        }
        throw;
    }

    LegacyImportResult result;
    result.counts = counts;
    result.source_sha256 = hash_projections(source_projections(snapshot));
    result.target_sha256 = hash_projections(target_projections(target_));
    return result;
}

} }  // namespace oyama::storage
