#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace gba::text {

namespace {

std::optional<std::string> canonical_code_row(std::string_view raw) {
    const std::string stripped = trim(raw);
    std::string compact;
    compact.reserve(stripped.size());
    bool code_only = !stripped.empty();
    bool colon_seen = false;

    for (const char ch : stripped) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            continue;
        }
        if (ch == ':' && !colon_seen && compact.size() == 8U) {
            colon_seen = true;
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            code_only = false;
            break;
        }
        compact.push_back(ch);
    }

    if (!code_only ||
        (compact.size() != 12U && compact.size() != 16U)) {
        return std::nullopt;
    }

    return compact.substr(0U, 8U) + " " + compact.substr(8U);
}


std::optional<std::vector<std::string>> split_flat_code_stream(
    std::string_view raw) {
    const std::string line = trim(raw);
    if (line.empty()) {
        return std::nullopt;
    }

    std::istringstream input(line);
    std::vector<std::string> tokens;
    for (std::string token; input >> token;) {
        tokens.push_back(std::move(token));
    }

    std::vector<std::string> rows;
    for (std::size_t index = 0U; index < tokens.size();) {
        if (const auto complete = canonical_code_row(tokens[index])) {
            rows.push_back(*complete);
            ++index;
            continue;
        }

        if (index + 1U >= tokens.size()) {
            return std::nullopt;
        }

        const std::string paired = tokens[index] + " " + tokens[index + 1U];
        const auto row = canonical_code_row(paired);
        if (!row) {
            return std::nullopt;
        }
        rows.push_back(*row);
        index += 2U;
    }

    // A single row is handled by canonical_code_row(). Only reconstruct
    // line breaks when the clipboard/source flattened two or more rows.
    if (rows.size() < 2U) {
        return std::nullopt;
    }
    return rows;
}

bool has_non_hex_name_letter(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](const char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) != 0 &&
               std::isxdigit(static_cast<unsigned char>(ch)) == 0;
    });
}

std::optional<std::pair<std::string, std::string>>
split_attached_code_suffix(std::string_view raw) {
    const std::string line = trim(raw);
    if (line.empty()) {
        return std::nullopt;
    }

    // Comments, directives, and already-structured headings are not plain
    // CodeTwink cheat-name rows and must remain byte-for-byte intact.
    if (line.front() == '#' || line.front() == ';' || line.front() == '%' ||
        line.front() == '^' || line.front() == '[' ||
        line.rfind("//", 0U) == 0U || line.rfind("/*", 0U) == 0U ||
        line.rfind("--", 0U) == 0U) {
        return std::nullopt;
    }

    for (std::size_t position = 1U; position < line.size(); ++position) {
        // This cleanup is intentionally limited to a code physically glued
        // to the name. A normally spaced code-looking value in a title is not
        // split.
        if (std::isspace(static_cast<unsigned char>(line[position])) != 0 ||
            std::isspace(
                static_cast<unsigned char>(line[position - 1U])) != 0) {
            continue;
        }

        const std::string name = trim(
            std::string_view(line).substr(0U, position));
        if (name.empty() || !has_non_hex_name_letter(name)) {
            continue;
        }

        const auto code = canonical_code_row(
            std::string_view(line).substr(position));
        if (code) {
            return std::make_pair(name, *code);
        }
    }

    return std::nullopt;
}

} // namespace

std::string format_compact_code_lines(std::string_view value) {
    const std::string normalized = normalize_newlines_lf(value);
    const std::vector<std::string> lines = split_lines(normalized);

    std::ostringstream output;
    for (std::size_t line_index = 0;
         line_index < lines.size();
         ++line_index) {
        const std::string& original = lines[line_index];
        if (const auto stream = split_flat_code_stream(original)) {
            for (std::size_t row_index = 0U;
                 row_index < stream->size();
                 ++row_index) {
                output << (*stream)[row_index];
                if (row_index + 1U < stream->size()) {
                    output << '\n';
                }
            }
        } else if (const auto code = canonical_code_row(original)) {
            output << *code;
        } else if (const auto attached =
                       split_attached_code_suffix(original)) {
            output << attached->first << '\n' << attached->second;
        } else {
            output << original;
        }

        if (line_index + 1U < lines.size()) {
            output << '\n';
        }
    }

    return output.str();
}


} // namespace gba::text
