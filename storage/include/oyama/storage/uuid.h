#pragma once
#include <oyama/compat/all.h>

#include <string>
#include <string_view>

namespace oyama { namespace storage {

/// Generates a random RFC 4122 version-4 UUID.  This is an identifier helper,
/// not a cryptographic primitive; it deliberately uses only the C++ standard
/// library so the storage layer does not need OpenSSL.
OYAMA_NODISCARD std::string generate_uuid_v4();

OYAMA_NODISCARD bool is_uuid(oyama::string_view text);

} }  // namespace oyama::storage
