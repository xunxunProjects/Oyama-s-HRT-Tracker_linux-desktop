#include "oyama/storage/vault_codec.h"

#include "oyama/storage/types.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

template <typename Fn>
bool throws_field(Fn&& fn, oyama::string_view field) {
    try {
        fn();
    } catch (const oyama::storage::VaultCodecError& error) {
        return std::string(error.what()).find(field) != std::string::npos;
    } catch (...) {
        return false;
    }
    return false;
}

nlohmann::json dose_json() {
    return nlohmann::json{
        {"id", "00000000-0000-0000-0000-000000000010"},
        {"mode", "transmasc"},
        {"route", "injection"},
        {"ester", "EN"},
        {"timeH", 12.5},
        {"doseMG", 4.0},
        {"extras", {{"site", "thigh"}}},
        {"updatedAt", 555},
        {"createdAt", 1},
    };
}

void test_settings() {
    using oyama::storage::Settings;
    Settings settings;
    settings.body_weight_kg = 61.5;
    settings.weight_updated_at = 111;
    settings.birth_year = 1990;
    settings.birth_year_updated_at = 222;
    settings.pk_params_json = R"({"e2_kClear":0.4})";
    settings.pk_params_updated_at = 333;
    settings.calibration_method = "mipd";
    settings.calibration_history_mode = "retrospective";
    settings.updated_at = 444;

    const auto decoded =
        oyama::storage::decode_settings(oyama::storage::encode_settings(settings));
    check(decoded.body_weight_kg.has_value() &&
              *decoded.body_weight_kg == 61.5,
          "settings weight");
    check(decoded.weight_updated_at == 111, "settings weight stamp");
    check(decoded.birth_year == 1990, "settings birth year");
    check(decoded.birth_year_updated_at == 222, "settings birth stamp");
    check(decoded.pk_params_updated_at == 333, "settings pk stamp");
    check(decoded.updated_at == 444, "settings stamp");
    check(decoded.calibration_method == "mipd", "settings method");
    check(decoded.calibration_history_mode == "retrospective",
          "settings history");

    Settings empty;
    const auto empty_decoded =
        oyama::storage::decode_settings(oyama::storage::encode_settings(empty));
    check(!empty_decoded.body_weight_kg.has_value(), "empty weight");
    check(!empty_decoded.birth_year.has_value(), "empty birth year");
    check(!empty_decoded.pk_params_json.has_value(), "empty pk params");
}

void test_dose() {
    using oyama::storage::DoseEvent;
    using oyama::storage::Mode;
    DoseEvent dose;
    dose.id = "00000000-0000-0000-0000-000000000010";
    dose.mode = Mode::Transmasc;
    dose.route = "injection";
    dose.ester = "EN";
    dose.time_h = 12.5;
    dose.dose_mg = 4.0;
    dose.extras_json = R"({"site":"thigh"})";
    dose.updated_at = 555;
    dose.created_at = 1;

    const auto decoded =
        oyama::storage::decode_dose_event(oyama::storage::encode_dose_event(dose));
    check(decoded.id == dose.id, "dose id");
    check(decoded.mode == Mode::Transmasc, "dose mode");
    check(decoded.route == "injection", "dose route");
    check(decoded.ester == "EN", "dose ester");
    check(decoded.time_h == 12.5, "dose time");
    check(decoded.dose_mg == 4.0, "dose amount");
    check(decoded.extras_json == dose.extras_json, "dose extras");
    check(decoded.updated_at == 555, "dose stamp");

    check(!oyama::storage::decode_dose_event(
               oyama::storage::encode_dose_event(
                   DoseEvent{dose.id, dose.mode, dose.route, dose.ester,
                             dose.time_h, dose.dose_mg, dose.extras_json,
                             oyama::nullopt, dose.created_at}))
               .updated_at.has_value(),
          "missing dose stamp stays missing");

    check(throws_field(
              [] {
                  (void)oyama::storage::decode_dose_event(
                      R"({"id":"bad","mode":"transfem"})");
              },
              "id"),
          "invalid id names the field");

    auto bad_route = dose_json();
    bad_route["route"] = "teleport";
    check(throws_field(
              [&] { (void)oyama::storage::decode_dose_event(bad_route.dump()); },
              "route"),
          "invalid route rejected");

    auto bad_ester = dose_json();
    bad_ester["ester"] = "nope";
    check(throws_field(
              [&] { (void)oyama::storage::decode_dose_event(bad_ester.dump()); },
              "ester"),
          "invalid ester rejected");

    auto bad_time = dose_json();
    bad_time["timeH"] = -1;
    check(throws_field(
              [&] { (void)oyama::storage::decode_dose_event(bad_time.dump()); },
              "timeH"),
          "negative time rejected");

    auto unknown = dose_json();
    unknown["bogus"] = true;
    check(throws_field(
              [&] { (void)oyama::storage::decode_dose_event(unknown.dump()); },
              "bogus"),
          "unknown field rejected");

    DoseEvent infinite = dose;
    infinite.dose_mg = std::numeric_limits<double>::infinity();
    check(throws_field(
              [&] { (void)oyama::storage::encode_dose_event(infinite); },
              "doseMG"),
          "non-finite dose rejected");
}

void test_lab_and_tombstone() {
    using oyama::storage::LabResult;
    using oyama::storage::Mode;
    using oyama::storage::Tombstone;

    LabResult lab;
    lab.id = "00000000-0000-0000-0000-000000000020";
    lab.mode = Mode::Transfem;
    lab.time_h = 3.0;
    lab.value = 100.0;
    lab.unit = "pg/ml";
    lab.assay = "immuno";
    lab.updated_at = 12;
    lab.created_at = 1;
    const auto decoded_lab = oyama::storage::decode_lab_result(
        oyama::storage::encode_lab_result(lab));
    check(decoded_lab.value == 100.0 && decoded_lab.unit == "pg/ml",
          "lab round-trip");

    Tombstone tombstone{"dose_event",
                        "00000000-0000-0000-0000-000000000030", 123};
    const auto decoded_tombstone = oyama::storage::decode_tombstone(
        oyama::storage::encode_tombstone(tombstone));
    check(decoded_tombstone.kind == tombstone.kind &&
              decoded_tombstone.record_id == tombstone.record_id &&
              decoded_tombstone.deleted_at == tombstone.deleted_at,
          "tombstone round-trip");
}

void test_preferences() {
    using oyama::storage::Preference;
    const std::vector<Preference> preferences{
        {"theme", R"("dark")", 10},
        {"lang", R"("zh")", 20},
    };
    const auto decoded = oyama::storage::decode_preferences(
        oyama::storage::encode_preferences(preferences));
    check(decoded.size() == 2, "preferences count");
    check(decoded[0].key == "lang" && decoded[0].value_json == R"("zh")",
          "preferences sorted first");
    check(decoded[1].key == "theme" &&
              decoded[1].value_json == R"("dark")",
          "preferences sorted second");
}

void test_json_records() {
    using oyama::storage::JsonRecord;
    using oyama::storage::JsonRecordKind;
    using oyama::storage::Mode;
    const std::string id = "00000000-0000-0000-0000-000000000040";

    struct Case {
        JsonRecordKind kind;
        std::string payload;
    };
    const std::vector<Case> cases{
        {JsonRecordKind::DoseTemplate,
         R"({"name":"weekly","route":"injection","ester":"EV","doseMG":2.0,"extras":{},"createdAt":1})"},
        {JsonRecordKind::QuickDose,
         R"({"route":"oral","ester":"E2","value":1.0,"createdAt":1})"},
        {JsonRecordKind::Supply,
         R"({"route":"injection","ester":"EV","pack":{"kind":"vial","ml":10,"mgPerMl":40},"count":1,"startedAt":1})"},
        {JsonRecordKind::PlanItem,
         R"({"route":"injection","ester":"EV","doseMG":2.0,"extras":{},"everyDays":7,"timesPerDay":1})"},
    };

    for (const auto& item : cases) {
        JsonRecord record{id, Mode::Transfem, item.payload, 42};
        const auto encoded =
            oyama::storage::encode_json_record(record, item.kind);
        const auto decoded =
            oyama::storage::decode_json_record(encoded, item.kind);
        check(decoded.id == id && decoded.mode == Mode::Transfem &&
                  decoded.updated_at == 42,
              "json record identity");
        const auto payload = nlohmann::json::parse(decoded.payload_json);
        check(payload.at("route") == "injection" ||
                  payload.at("route") == "oral",
              "json record payload");
    }

    JsonRecord bad{id, Mode::Transfem,
                   R"({"route":"teleport","ester":"E2","value":1.0,"createdAt":1})",
                   42};
    check(throws_field(
              [&] {
                  (void)oyama::storage::encode_json_record(
                      bad, JsonRecordKind::QuickDose);
              },
              "route"),
          "json record invalid route rejected");
}

}  // namespace

int main() {
    test_settings();
    test_dose();
    test_lab_and_tombstone();
    test_preferences();
    test_json_records();

    if (failures != 0) {
        std::cerr << "vault_codec_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "vault_codec_test: ok\n";
    return 0;
}
