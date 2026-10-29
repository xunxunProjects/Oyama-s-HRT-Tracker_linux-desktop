#include <oyama/storage/preferences_repository.h>

#include "repository_util.h"

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

void validate(const Preference& preference) {
    detail::require_text(preference.key, "preference key");
    if (preference.value_json.empty()) {
        throw Error("preference value JSON must not be empty");
    }
}

Preference read_preference(const Result& result, int row) {
    return Preference{
        result.text(row, 0),
        result.text(row, 1),
        result.integer(row, 2),
    };
}

}  // namespace

PreferencesRepository::PreferencesRepository(Connection& connection)
    : connection_(connection) {}

std::optional<Preference> PreferencesRepository::get(std::string_view key) {
    detail::require_text(key, "preference key");
    const auto result = connection_.exec_params(
        "SELECT key, value::text, updated_at FROM preferences WHERE key = $1",
        {std::string(key)});
    if (result.rows() == 0) {
        return std::nullopt;
    }
    return read_preference(result, 0);
}

std::vector<Preference> PreferencesRepository::list() {
    const auto result = connection_.exec(
        "SELECT key, value::text, updated_at FROM preferences ORDER BY key");
    std::vector<Preference> preferences;
    preferences.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        preferences.push_back(read_preference(result, row));
    }
    return preferences;
}

void PreferencesRepository::set(const Preference& preference) {
    validate(preference);
    (void)connection_.exec_params(
        "INSERT INTO preferences (key, value, updated_at) "
        "VALUES ($1, $2::jsonb, $3) "
        "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value, "
        "updated_at = EXCLUDED.updated_at",
        {preference.key, preference.value_json,
         std::to_string(preference.updated_at)});
}

bool PreferencesRepository::remove(std::string_view key) {
    detail::require_text(key, "preference key");
    const auto result = connection_.exec_params(
        "DELETE FROM preferences WHERE key = $1", {std::string(key)});
    return result.affected_rows() > 0;
}

}  // namespace oyama::storage
