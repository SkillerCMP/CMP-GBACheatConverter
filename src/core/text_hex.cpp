#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace gba::text {

static std::optional<std::uint32_t> parse_hex(std::string_view input,
                                               unsigned maximum_digits) {
    const std::string value = trim(input);
    if (value.empty() || value.size() > maximum_digits) {
        return std::nullopt;
    }

    std::uint32_t result = 0;
    for (const char ch : value) {
        result <<= 4;
        if (ch >= '0' && ch <= '9') {
            result |= static_cast<std::uint32_t>(ch - '0');
        } else if (ch >= 'A' && ch <= 'F') {
            result |= static_cast<std::uint32_t>(ch - 'A' + 10);
        } else if (ch >= 'a' && ch <= 'f') {
            result |= static_cast<std::uint32_t>(ch - 'a' + 10);
        } else {
            return std::nullopt;
        }
    }

    return result;
}

std::optional<std::uint32_t> parse_hex_u32(std::string_view value) {
    return parse_hex(value, 8);
}

std::optional<std::uint16_t> parse_hex_u16(std::string_view value) {
    const auto parsed = parse_hex(value, 4);
    if (!parsed) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*parsed);
}

std::string hex(std::uint32_t value, unsigned width) {
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0')
           << std::setw(static_cast<int>(width)) << value;
    return output.str();
}

static bool code_shape(std::string_view raw,
                       std::size_t left_width,
                       std::size_t right_width) {
    const std::string line = trim(raw);
    if (line.size() != left_width + 1 + right_width ||
        !std::isspace(static_cast<unsigned char>(line[left_width]))) {
        return false;
    }

    const auto all_hex = [](std::string_view value) {
        return std::all_of(value.begin(), value.end(), [](char ch) {
            return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
        });
    };

    return all_hex(std::string_view(line).substr(0, left_width)) &&
           all_hex(std::string_view(line).substr(left_width + 1, right_width));
}

bool is_code_line_8x4(std::string_view line) {
    return code_shape(line, 8, 4);
}

bool is_code_line_8x8(std::string_view line) {
    return code_shape(line, 8, 8);
}


} // namespace gba::text
