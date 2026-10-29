#include <oyama/storage/dose_event_repository.h>

#include "repository_util.h"

#include <oyama/storage/uuid.h>

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

void validate(const DoseEvent& event) {
    if (!is_uuid(event.id)) {
        throw Error("dose id is not a valid UUID");
    }
    detail::require_text(event.route, "dose route");
    detail::require_text(event.ester, "dose ester");
    detail::require_finite(event.time_h, "dose time");
    detail::require_positive(event.dose_mg, "dose amount");
}

DoseEvent read_dose(const Result& result, int row) {
    DoseEvent event;
    event.id = result.text(row, 0);
    const auto mode = parse_mode(result.text(row, 1));
    if (!mode.has_value()) {
        throw Error("invalid mode in dose_events");
    }
    event.mode = *mode;
    event.route = result.text(row, 2);
    event.ester = result.text(row, 3);
    event.time_h = result.number(row, 4);
    event.dose_mg = result.number(row, 5);
    event.extras_json = result.text(row, 6);
    event.updated_at = detail::nullable_integer(result, row, 7);
    event.created_at = result.integer(row, 8);
    return event;
}

constexpr const char* kSelectColumns =
    "id::text, mode, route, ester, time_h, dose_mg, extras::text, updated_at, "
    "created_at";

}  // namespace

DoseEventRepository::DoseEventRepository(Connection& connection)
    : connection_(connection) {}

std::vector<DoseEvent> DoseEventRepository::list(Mode mode) {
    const auto result = connection_.exec_params(
        std::string("SELECT ") + kSelectColumns +
            " FROM dose_events WHERE mode = $1 ORDER BY time_h, id",
        {to_string(mode)});
    std::vector<DoseEvent> events;
    events.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        events.push_back(read_dose(result, row));
    }
    return events;
}

std::optional<DoseEvent> DoseEventRepository::get(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("dose id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        std::string("SELECT ") + kSelectColumns +
            " FROM dose_events WHERE id = $1::uuid",
        {std::string(id)});
    if (result.rows() == 0) {
        return std::nullopt;
    }
    return read_dose(result, 0);
}

void DoseEventRepository::upsert(const DoseEvent& event) {
    validate(event);

    const std::string extras =
        event.extras_json.empty() ? std::string("{}") : event.extras_json;
    std::vector<std::optional<std::string>> params;
    params.reserve(9);
    params.push_back(event.id);
    params.push_back(to_string(event.mode));
    params.push_back(event.route);
    params.push_back(event.ester);
    params.push_back(detail::number_to_string(event.time_h));
    params.push_back(detail::number_to_string(event.dose_mg));
    params.push_back(extras);
    params.push_back(event.updated_at.has_value()
                         ? std::optional<std::string>(
                               std::to_string(*event.updated_at))
                         : std::nullopt);
    params.push_back(std::to_string(event.created_at));

    (void)connection_.exec_params(
        "INSERT INTO dose_events (id, mode, route, ester, time_h, dose_mg, "
        "extras, updated_at, created_at) "
        "VALUES ($1::uuid, $2, $3, $4, $5::double precision, "
        "$6::double precision, $7::jsonb, $8, $9) "
        "ON CONFLICT (id) DO UPDATE SET "
        "mode = EXCLUDED.mode, route = EXCLUDED.route, "
        "ester = EXCLUDED.ester, time_h = EXCLUDED.time_h, "
        "dose_mg = EXCLUDED.dose_mg, extras = EXCLUDED.extras, "
        "updated_at = EXCLUDED.updated_at",
        params);
}

bool DoseEventRepository::remove(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("dose id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        "DELETE FROM dose_events WHERE id = $1::uuid", {std::string(id)});
    return result.affected_rows() > 0;
}

std::size_t DoseEventRepository::count(Mode mode) {
    const auto result = connection_.exec_params(
        "SELECT count(*) FROM dose_events WHERE mode = $1", {to_string(mode)});
    return static_cast<std::size_t>(result.integer(0, 0));
}

}  // namespace oyama::storage
