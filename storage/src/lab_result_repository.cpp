#include <oyama/storage/lab_result_repository.h>

#include "repository_util.h"

#include <oyama/storage/uuid.h>

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

void validate(const LabResult& result) {
    if (!is_uuid(result.id)) {
        throw Error("lab id is not a valid UUID");
    }
    detail::require_finite(result.time_h, "lab time");
    detail::require_finite(result.value, "lab value");
    detail::require_text(result.unit, "lab unit");
    if (result.assay != "immuno" && result.assay != "lcms") {
        throw Error("invalid lab assay: " + result.assay);
    }
}

LabResult read_lab(const Result& result, int row) {
    LabResult lab;
    lab.id = result.text(row, 0);
    const auto mode = parse_mode(result.text(row, 1));
    if (!mode.has_value()) {
        throw Error("invalid mode in lab_results");
    }
    lab.mode = *mode;
    lab.time_h = result.number(row, 2);
    lab.value = result.number(row, 3);
    lab.unit = result.text(row, 4);
    lab.assay = result.text(row, 5);
    lab.updated_at = detail::nullable_integer(result, row, 6);
    lab.created_at = result.integer(row, 7);
    return lab;
}

constexpr const char* kSelectColumns =
    "id::text, mode, time_h, value, unit, assay, updated_at, created_at";

}  // namespace

LabResultRepository::LabResultRepository(Connection& connection)
    : connection_(connection) {}

std::vector<LabResult> LabResultRepository::list(Mode mode) {
    const auto result = connection_.exec_params(
        std::string("SELECT ") + kSelectColumns +
            " FROM lab_results WHERE mode = $1 ORDER BY time_h, id",
        {to_string(mode)});
    std::vector<LabResult> labs;
    labs.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        labs.push_back(read_lab(result, row));
    }
    return labs;
}

std::optional<LabResult> LabResultRepository::get(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("lab id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        std::string("SELECT ") + kSelectColumns +
            " FROM lab_results WHERE id = $1::uuid",
        {std::string(id)});
    if (result.rows() == 0) {
        return std::nullopt;
    }
    return read_lab(result, 0);
}

void LabResultRepository::upsert(const LabResult& lab) {
    validate(lab);

    std::vector<std::optional<std::string>> params;
    params.reserve(8);
    params.push_back(lab.id);
    params.push_back(to_string(lab.mode));
    params.push_back(detail::number_to_string(lab.time_h));
    params.push_back(detail::number_to_string(lab.value));
    params.push_back(lab.unit);
    params.push_back(lab.assay);
    params.push_back(lab.updated_at.has_value()
                         ? std::optional<std::string>(
                               std::to_string(*lab.updated_at))
                         : std::nullopt);
    params.push_back(std::to_string(lab.created_at));

    (void)connection_.exec_params(
        "INSERT INTO lab_results (id, mode, time_h, value, unit, assay, "
        "updated_at, created_at) "
        "VALUES ($1::uuid, $2, $3::double precision, $4::double precision, "
        "$5, $6, $7, $8) "
        "ON CONFLICT (id) DO UPDATE SET "
        "mode = EXCLUDED.mode, time_h = EXCLUDED.time_h, "
        "value = EXCLUDED.value, unit = EXCLUDED.unit, "
        "assay = EXCLUDED.assay, updated_at = EXCLUDED.updated_at",
        params);
}

bool LabResultRepository::remove(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("lab id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        "DELETE FROM lab_results WHERE id = $1::uuid", {std::string(id)});
    return result.affected_rows() > 0;
}

}  // namespace oyama::storage
