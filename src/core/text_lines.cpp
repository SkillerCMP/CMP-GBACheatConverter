#include "core/text.hpp"

namespace gba::text {

std::vector<std::string> split_lines(std::string_view value) {
    std::vector<std::string> lines;
    std::size_t start = 0;

    while (start <= value.size()) {
        const std::size_t end = value.find_first_of("\r\n", start);
        if (end == std::string_view::npos) {
            lines.emplace_back(value.substr(start));
            break;
        }

        lines.emplace_back(value.substr(start, end - start));
        start = end + 1;
        if (value[end] == '\r' && start < value.size() && value[start] == '\n') {
            ++start;
        }
    }

    return lines;
}


} // namespace gba::text
