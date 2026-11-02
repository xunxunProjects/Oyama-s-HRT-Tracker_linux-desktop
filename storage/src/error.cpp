#include <oyama/storage/error.h>

#include <utility>

namespace oyama { namespace storage {

Error::Error(std::string message, std::string sqlstate)
    : std::runtime_error(std::move(message)), sqlstate_(std::move(sqlstate)) {}

const std::string& Error::sqlstate() const noexcept {
    return sqlstate_;
}

} }  // namespace oyama::storage
