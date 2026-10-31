#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

/// Diceware-style paper recovery phrases built from the EFF long wordlist.
class Passphrase {
public:
    [[nodiscard]] static std::vector<std::string> generate(
        std::size_t words = 7);
    [[nodiscard]] static std::string join(
        const std::vector<std::string>& words);
    [[nodiscard]] static std::string normalize(std::string_view phrase);
    [[nodiscard]] static std::vector<std::size_t> challenge(
        std::size_t word_count, std::size_t count = 3);
    [[nodiscard]] static bool verify_challenge(
        std::string_view phrase, const std::vector<std::size_t>& positions,
        const std::vector<std::string>& answers);
};

}  // namespace oyama::storage
