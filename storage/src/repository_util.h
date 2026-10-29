#pragma once

#include <oyama/storage/connection.h>
#include <oyama/storage/error.h>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace oyama::storage::detail {

inline std::optional<std::int64_t> nullable_integer(const Result& result,
                                                    int row, int column) {
    if (result.is_null(row, column)) {
        return std::nullopt;
    }
    return result.integer(row, column);
}

inline std::optional<double> nullable_number(const Result& result, int row,
                                             int column) {
    if (result.is_null(row, column)) {
        return std::nullopt;
    }
    return result.number(row, column);
}

inline std::optional<std::string> nullable_text(const Result& result, int row,
                                                int column) {
    if (result.is_null(row, column)) {
        return std::nullopt;
    }
    return result.text(row, column);
}

inline void require_text(std::string_view value, const char* field) {
    if (value.empty()) {
        throw Error(std::string(field) + " must not be empty");
    }
}

inline void require_finite(double value, const char* field) {
    if (!std::isfinite(value)) {
        throw Error(std::string(field) + " must be finite");
    }
}

inline void require_positive(double value, const char* field) {
    require_finite(value, field);
    if (!(value > 0.0)) {
        throw Error(std::string(field) + " must be positive");
    }
}

inline std::string number_to_string(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << value;
    return output.str();
}

}  // namespace oyama::storage::detail
