#include "formats/codebreaker_internal.hpp"

#include "core/text.hpp"

#include <cctype>
#include <string>

namespace gba::codebreaker {

using detail::parse_line;

std::optional<Seed> parse_seed_text(std::string_view value) {
    const std::string cleaned = text::trim(value);
    if (cleaned.empty()) {
        return std::nullopt;
    }

    std::size_t separator = cleaned.find(':');
    if (separator == std::string::npos) {
        separator = cleaned.find_first_of(" \t");
    }
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    std::size_t right_begin = separator + 1U;
    while (right_begin < cleaned.size() &&
           std::isspace(static_cast<unsigned char>(cleaned[right_begin])) != 0) {
        ++right_begin;
    }

    const auto op1 = text::parse_hex_u32(
        std::string_view(cleaned).substr(0, separator));
    const auto op2 = text::parse_hex_u16(
        std::string_view(cleaned).substr(right_begin));
    if (!op1 || !op2 || ((*op1 >> 28U) != 0x9U)) {
        return std::nullopt;
    }

    return Seed{*op1, *op2};
}

std::optional<Seed> find_embedded_seed(std::string_view input) {
    const std::string named_input =
        text::normalize_plain_cheat_headers(input);
    const auto lines = text::split_lines(named_input);
    std::optional<RawLine> first_code;
    bool has_payload_row = false;

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto parsed = parse_line(lines[index], index + 1U);
        if (!parsed) {
            continue;
        }

        if (!first_code) {
            first_code = *parsed;
            continue;
        }

        has_payload_row = true;
        break;
    }

    // A lone encrypted payload row can also begin with 9, so it is never safe
    // to advertise that row as an automatically detected key. Require the
    // normal full-list shape: a first plaintext 9 row plus payload after it.
    if (first_code && has_payload_row &&
        (first_code->op1 >> 28U) == 0x9U) {
        return Seed{first_code->op1, first_code->op2};
    }
    return std::nullopt;
}

std::string format_seed(Seed seed, char separator) {
    return text::hex(seed.op1, 8) + separator + text::hex(seed.op2, 4);
}

} // namespace gba::codebreaker
