#pragma once

#include "core/types.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::codebreaker {

struct RawLine {
    std::uint32_t op1 = 0;
    std::uint16_t op2 = 0;
    std::size_t source_line = 0;
    std::string source_text;
};

struct Seed {
    std::uint32_t op1 = 0;
    std::uint16_t op2 = 0;
};

class Cipher {
public:
    void reseed(Seed seed);
    bool active() const noexcept;

    RawLine decrypt(const RawLine& encrypted) const;
    RawLine encrypt(const RawLine& raw) const;

private:
    static constexpr std::size_t TableSize = 48;

    mutable std::uint32_t rng_state_ = 0;
    std::uint32_t master_ = 0;
    std::array<std::uint32_t, 4> seeds_{};
    std::array<std::uint8_t, TableSize> table_{};

    std::uint32_t random();
    std::size_t swap_index();
};

struct ParseOptions {
    ParseOptions(bool input_encrypted = false,
                 std::optional<Seed> input_seed = std::nullopt,
                 bool force_input_seed = false)
        : encrypted(input_encrypted), seed(input_seed),
          force_seed(force_input_seed) {}

    bool encrypted = false;
    // Optional initial key for encrypted input whose plaintext 9-type key row
    // is missing. In normal mode an embedded plaintext key takes precedence.
    std::optional<Seed> seed;
    // Manual-key mode: initialize the cipher before the first code row and
    // treat every physical row as encrypted payload. This is required when a
    // keyless stream begins with an encrypted row whose high nibble is 9.
    bool force_seed = false;
};

struct ExportOptions {
    bool encrypted = false;
    std::optional<Seed> seed;
};

struct Result {
    std::string text;
    std::vector<std::string> warnings;
    bool success = true;
};

std::optional<Seed> parse_seed_text(std::string_view value);
std::optional<Seed> find_embedded_seed(std::string_view input);
std::string format_seed(Seed seed, char separator = ':');

CheatDocument parse(std::string_view input, const ParseOptions& options);
Result export_document(const CheatDocument& document,
                       const ExportOptions& options = {});
std::string format_raw(std::string_view input,
                       bool input_encrypted,
                       std::optional<Seed> output_seed);

} // namespace gba::codebreaker
