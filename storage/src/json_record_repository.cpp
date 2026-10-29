#include <oyama/storage/json_record_repository.h>

#include "repository_util.h"

#include <oyama/storage/uuid.h>

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

void validate(const JsonRecord& record) {
    if (!is_uuid(record.id)) {
        throw Error("record id is not a valid UUID");
    }
    if (record.payload_json.empty()) {
        throw Error("record payload JSON must not be empty");
    }
}

JsonRecord read_record(const Result& result, int row) {
    JsonRecord record;
    record.id = result.text(row, 0);
    const auto mode = parse_mode(result.text(row, 1));
    if (!mode.has_value()) {
        throw Error("invalid mode in JSON record table");
    }
    record.mode = *mode;
    record.payload_json = result.text(row, 2);
    record.updated_at = detail::nullable_integer(result, row, 3);
    return record;
}

}  // namespace

JsonRecordRepository::JsonRecordRepository(Connection& connection,
                                           JsonRecordKind kind)
    : connection_(connection), kind_(kind), table_(table_name(kind)) {}

JsonRecordKind JsonRecordRepository::kind() const noexcept {
    return kind_;
}

std::vector<JsonRecord> JsonRecordRepository::list(Mode mode) {
    const auto result = connection_.exec_params(
        "SELECT id::text, mode, payload::text, updated_at FROM " + table_ +
            " WHERE mode = $1 ORDER BY id",
        {to_string(mode)});
    std::vector<JsonRecord> records;
    records.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        records.push_back(read_record(result, row));
    }
    return records;
}

std::optional<JsonRecord> JsonRecordRepository::get(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("record id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        "SELECT id::text, mode, payload::text, updated_at FROM " + table_ +
            " WHERE id = $1::uuid",
        {std::string(id)});
    if (result.rows() == 0) {
        return std::nullopt;
    }
    return read_record(result, 0);
}

void JsonRecordRepository::upsert(const JsonRecord& record) {
    validate(record);

    const std::string payload =
        record.payload_json.empty() ? std::string("{}") : record.payload_json;
    std::vector<std::optional<std::string>> params;
    params.reserve(4);
    params.push_back(record.id);
    params.push_back(to_string(record.mode));
    params.push_back(payload);
    params.push_back(record.updated_at.has_value()
                         ? std::optional<std::string>(
                               std::to_string(*record.updated_at))
                         : std::nullopt);

    (void)connection_.exec_params(
        "INSERT INTO " + table_ +
            " (id, mode, payload, updated_at) "
            "VALUES ($1::uuid, $2, $3::jsonb, $4) "
            "ON CONFLICT (id) DO UPDATE SET mode = EXCLUDED.mode, "
            "payload = EXCLUDED.payload, updated_at = EXCLUDED.updated_at",
        params);
}

bool JsonRecordRepository::remove(std::string_view id) {
    if (!is_uuid(id)) {
        throw Error("record id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        "DELETE FROM " + table_ + " WHERE id = $1::uuid", {std::string(id)});
    return result.affected_rows() > 0;
}

}  // namespace oyama::storage
