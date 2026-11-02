#include "oyama/storage/passphrase.h"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace oyama { namespace storage { namespace detail {
extern const std::array<oyama::string_view, 7776> kWordlist;
} } }

namespace oyama { namespace storage {
namespace {

std::vector<std::string> split_words(oyama::string_view phrase) {
    std::vector<std::string> words;
    std::string current;
    for (char ch : phrase) {
        unsigned char byte = static_cast<unsigned char>(ch);
        if (std::isspace(byte) != 0) {
            if (!current.empty()) {
                words.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(static_cast<char>(std::tolower(byte)));
    }
    if (!current.empty()) {
        words.push_back(std::move(current));
    }
    return words;
}

}  // namespace

std::vector<std::string> Passphrase::generate(std::size_t words) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
    std::vector<std::string> result;
    result.reserve(words);
    for (std::size_t i = 0; i < words; ++i) {
        const std::uint32_t index = randombytes_uniform(
            static_cast<std::uint32_t>(detail::kWordlist.size()));
        result.emplace_back(detail::kWordlist[index]);
    }
    return result;
}

std::string Passphrase::join(const std::vector<std::string>& words) {
    std::string joined;
    for (const auto& word : words) {
        if (!joined.empty()) {
            joined.push_back(' ');
        }
        joined += word;
    }
    return normalize(joined);
}

std::string Passphrase::normalize(oyama::string_view phrase) {
    const auto words = split_words(phrase);
    std::string normalized;
    for (const auto& word : words) {
        if (!normalized.empty()) {
            normalized.push_back(' ');
        }
        normalized += word;
    }
    return normalized;
}

std::vector<std::size_t> Passphrase::challenge(std::size_t word_count,
                                               std::size_t count) {
    if (word_count == 0 || count == 0) {
        return {};
    }
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
    count = std::min(count, word_count);
    std::vector<std::size_t> positions;
    positions.reserve(count);
    while (positions.size() < count) {
        const std::uint32_t index = randombytes_uniform(
            static_cast<std::uint32_t>(word_count));
        const auto position = static_cast<std::size_t>(index);
        if (std::find(positions.begin(), positions.end(), position) ==
            positions.end()) {
            positions.push_back(position);
        }
    }
    return positions;
}

bool Passphrase::verify_challenge(
    oyama::string_view phrase, const std::vector<std::size_t>& positions,
    const std::vector<std::string>& answers) {
    if (positions.empty() || positions.size() != answers.size()) {
        return false;
    }
    const auto words = split_words(phrase);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (positions[i] >= words.size()) {
            return false;
        }
        if (normalize(answers[i]) != words[positions[i]]) {
            return false;
        }
    }
    return true;
}

} }  // namespace oyama::storage
