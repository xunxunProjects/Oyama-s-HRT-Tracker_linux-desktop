#include <oyama/storage/settings_repository.h>

#include "repository_util.h"

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

bool valid_calibration_method(const std::string& method) {
    return method == "off" || method == "ekf" || method == "ou_kalman" ||
           method == "mipd";
}

bool valid_history_mode(const std::string& mode) {
    return mode == "forward" || mode == "retrospective";
}

void validate(const Settings& settings) {
    if (settings.body_weight_kg.has_value()) {
        detail::require_positive(*settings.body_weight_kg, "body weight");
    }
    if (settings.birth_year.has_value() &&
        (*settings.birth_year < 1900 || *settings.birth_year > 2200)) {
        throw Error("birth year is out of range");
    }
    if (!valid_calibration_method(settings.calibration_method)) {
        throw Error("invalid calibration method: " +
                    settings.calibration_method);
    }
    if (!valid_history_mode(settings.calibration_history_mode)) {
        throw Error("invalid calibration history mode: " +
                    settings.calibration_history_mode);
    }
    if (settings.pk_params_json.has_value() &&
        settings.pk_params_json->empty()) {
        throw Error("PK params JSON must not be empty");
    }
}

}  // namespace

SettingsRepository::SettingsRepository(Connection& connection)
    : connection_(connection) {}

Settings SettingsRepository::get() {
    (void)connection_.exec(
        "INSERT INTO settings (id, updated_at) VALUES (true, 0) "
        "ON CONFLICT (id) DO NOTHING");

    const auto result = connection_.exec(
        "SELECT body_weight_kg, birth_year, pk_params::text, "
        "pk_params_updated_at, calibration_method, calibration_history_mode, "
        "updated_at FROM settings WHERE id = true");
    if (result.rows() != 1) {
        throw Error("settings row is missing");
    }

    Settings settings;
    settings.body_weight_kg = detail::nullable_number(result, 0, 0);
    if (!result.is_null(0, 1)) {
        settings.birth_year = static_cast<int>(result.integer(0, 1));
    }
    settings.pk_params_json = detail::nullable_text(result, 0, 2);
    settings.pk_params_updated_at = detail::nullable_integer(result, 0, 3);
    settings.calibration_method = result.text(0, 4);
    settings.calibration_history_mode = result.text(0, 5);
    settings.updated_at = result.integer(0, 6);
    return settings;
}

void SettingsRepository::save(const Settings& settings) {
    validate(settings);

    std::vector<std::optional<std::string>> params;
    params.reserve(7);
    params.push_back(settings.body_weight_kg.has_value()
                         ? std::optional<std::string>(
                               detail::number_to_string(*settings.body_weight_kg))
                         : std::nullopt);
    params.push_back(settings.birth_year.has_value()
                         ? std::optional<std::string>(
                               std::to_string(*settings.birth_year))
                         : std::nullopt);
    params.push_back(settings.pk_params_json);
    params.push_back(settings.pk_params_updated_at.has_value()
                         ? std::optional<std::string>(
                               std::to_string(*settings.pk_params_updated_at))
                         : std::nullopt);
    params.push_back(settings.calibration_method);
    params.push_back(settings.calibration_history_mode);
    params.push_back(std::to_string(settings.updated_at));

    (void)connection_.exec_params(
        "INSERT INTO settings (id, body_weight_kg, birth_year, pk_params, "
        "pk_params_updated_at, calibration_method, calibration_history_mode, "
        "updated_at) VALUES (true, $1, $2, $3::jsonb, $4, $5, $6, $7) "
        "ON CONFLICT (id) DO UPDATE SET "
        "body_weight_kg = EXCLUDED.body_weight_kg, "
        "birth_year = EXCLUDED.birth_year, "
        "pk_params = EXCLUDED.pk_params, "
        "pk_params_updated_at = EXCLUDED.pk_params_updated_at, "
        "calibration_method = EXCLUDED.calibration_method, "
        "calibration_history_mode = EXCLUDED.calibration_history_mode, "
        "updated_at = EXCLUDED.updated_at",
        params);
}

}  // namespace oyama::storage
