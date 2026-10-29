#pragma once

#include <stdexcept>
#include <string>

namespace oyama::storage {

/// Base class for every storage-layer failure.  sqlstate() is the five-character
/// PostgreSQL SQLSTATE when the error came from the server, and empty otherwise.
class Error : public std::runtime_error {
public:
    explicit Error(std::string message, std::string sqlstate = {});

    [[nodiscard]] const std::string& sqlstate() const noexcept;

private:
    std::string sqlstate_;
};

}  // namespace oyama::storage
