#pragma once

#include <string>
#include <string_view>

namespace oyama::storage {

/// Generates a random RFC 4122 version-4 UUID.  This is an identifier helper,
/// not a cryptographic primitive; it deliberately uses only the C++ standard
/// library so the storage layer does not need OpenSSL.
[[nodiscard]] std::string generate_uuid_v4();

[[nodiscard]] bool is_uuid(std::string_view text);

}  // namespace oyama::storage
