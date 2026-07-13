#include "core/text.hpp"

#include <cctype>

namespace gba::text {

std::string strip_utf8_bom(std::string_view value) {
    if (value.size() >= 3U &&
        static_cast<unsigned char>(value[0]) == 0xEFU &&
        static_cast<unsigned char>(value[1]) == 0xBBU &&
        static_cast<unsigned char>(value[2]) == 0xBFU) {
        value.remove_prefix(3U);
    }
    return std::string(value);
}

std::string trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    return std::string(value.substr(first, last - first));
}

std::string normalize_newlines_lf(std::string_view value) {
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == '\r') {
            output.push_back('\n');
            if (index + 1U < value.size() && value[index + 1U] == '\n') {
                ++index;
            }
        } else {
            output.push_back(ch);
        }
    }

    return output;
}

std::string normalize_newlines_crlf(std::string_view value) {
    std::string output;
    output.reserve(value.size() + value.size() / 8U);

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == '\r') {
            output.push_back('\r');
            output.push_back('\n');
            if (index + 1U < value.size() && value[index + 1U] == '\n') {
                ++index;
            }
        } else if (ch == '\n') {
            output.push_back('\r');
            output.push_back('\n');
        } else {
            output.push_back(ch);
        }
    }

    return output;
}

} // namespace gba::text
