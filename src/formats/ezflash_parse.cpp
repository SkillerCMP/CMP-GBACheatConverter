#include "formats/ezflash.hpp"

#include "core/text.hpp"
#include "formats/ezflash_parse_internal.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gba::ezflash {
namespace {

void classify_value(std::string_view value,
                    bool& saw_original,
                    bool& saw_enhanced) {
    const std::string clean = text::trim(value);
    if (clean.empty()) return;
    if (clean.find(':') != std::string::npos) {
        saw_enhanced = true;
    } else if (clean.find(',') != std::string::npos) {
        saw_original = true;
    }
}

} // namespace

Syntax detect_syntax(std::string_view input) {
    bool saw_original = false;
    bool saw_enhanced = false;
    std::string pending_value;

    const auto flush = [&]() {
        classify_value(pending_value, saw_original, saw_enhanced);
        pending_value.clear();
    };

    for (std::string line : text::split_lines(input)) {
        line = text::trim(line);
        if (line.empty() || line.front() == '#' || line.front() == '/' ||
            line.front() == ';' || line.front() == '[') {
            continue;
        }
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) line.resize(comment);
        line = text::trim(line);
        if (line.empty()) continue;

        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            flush();
            pending_value = line.substr(equals + 1U);
        } else if (!pending_value.empty()) {
            pending_value += line;
        }
    }
    flush();

    if (saw_original && saw_enhanced) return Syntax::Mixed;
    if (saw_enhanced) return Syntax::Enhanced;
    if (saw_original) return Syntax::Original;
    return Syntax::Unknown;
}

CheatDocument parse(std::string_view input) {
    return parse_detail::parse_document(input);
}

} // namespace gba::ezflash
