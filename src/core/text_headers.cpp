#include "core/text.hpp"

#include <sstream>

namespace gba::text {

std::string normalize_plain_cheat_headers(std::string_view value) {
    const std::string normalized = normalize_newlines_lf(value);
    const std::vector<std::string> lines = split_lines(normalized);

    std::ostringstream output;
    for (std::size_t index = 0U; index < lines.size(); ++index) {
        const std::string line = trim(lines[index]);
        bool promote = false;

        if (!line.empty() &&
            !is_code_line_8x4(line) &&
            !is_code_line_8x8(line) &&
            !(line.size() >= 2U && line.front() == '[' &&
              line.back() == ']') &&
            line.back() != ':' &&
            !is_inline_metadata_name_line(line) &&
            line.find('=') == std::string::npos &&
            line.front() != '#' && line.front() != ';' &&
            line.front() != '%' && line.front() != '^' &&
            line.rfind("//", 0U) != 0U &&
            line.rfind("/*", 0U) != 0U &&
            line.rfind("--", 0U) != 0U) {
            std::size_t next = index + 1U;
            while (next < lines.size() && trim(lines[next]).empty()) {
                ++next;
            }
            if (next < lines.size()) {
                const std::string next_line = trim(lines[next]);
                promote = is_code_line_8x4(next_line) ||
                          is_code_line_8x8(next_line);
            }
        }

        if (promote) {
            output << '[' << line << ']';
        } else {
            output << lines[index];
        }
        if (index + 1U < lines.size()) {
            output << '\n';
        }
    }
    return output.str();
}

bool is_inline_metadata_name_line(std::string_view raw) {
    const std::string line = trim(raw);
    return line.find(" , Crypt_") != std::string::npos;
}

std::string format_cheat_header(std::string_view raw_name) {
    const std::string name = trim(raw_name);
    if (is_inline_metadata_name_line(name)) {
        return name;
    }
    return "[" + name + "]";
}


} // namespace gba::text
