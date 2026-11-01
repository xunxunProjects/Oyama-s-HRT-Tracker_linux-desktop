#pragma once
#include <oyama/compat/all.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

/// Diceware-style paper recovery phrases built from the EFF long wordlist.
class Passphrase {
public:
    OYAMA_NODISCARD static std::vector<std::string> generate(
        std::size_t words = 7);
    OYAMA_NODISCARD static std::string join(
        const std::vector<std::string>& words);
    OYAMA_NODISCARD static std::string normalize(oyama::string_view phrase);
    OYAMA_NODISCARD static std::vector<std::size_t> challenge(
        std::size_t word_count, std::size_t count = 3);
    OYAMA_NODISCARD static bool verify_challenge(
        oyama::string_view phrase, const std::vector<std::size_t>& positions,
        const std::vector<std::string>& answers);
};

} }  // namespace oyama::storage
