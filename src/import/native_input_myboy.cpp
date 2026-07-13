#include "import/native_input_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {
namespace {

std::string lower_copy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

std::optional<std::string> attribute_value(
    std::string_view opening_tag, std::string_view name) {
    const std::string lower = lower_copy(opening_tag);
    const std::string key = lower_copy(name);
    std::size_t position = 0U;
    while ((position = lower.find(key, position)) != std::string::npos) {
        const bool left_ok = position == 0U ||
            std::isspace(static_cast<unsigned char>(lower[position - 1U])) != 0;
        std::size_t cursor = position + key.size();
        while (cursor < lower.size() &&
               std::isspace(static_cast<unsigned char>(lower[cursor])) != 0) {
            ++cursor;
        }
        if (!left_ok || cursor >= lower.size() || lower[cursor] != '=') {
            position += key.size();
            continue;
        }
        ++cursor;
        while (cursor < lower.size() &&
               std::isspace(static_cast<unsigned char>(lower[cursor])) != 0) {
            ++cursor;
        }
        if (cursor >= opening_tag.size() ||
            (opening_tag[cursor] != '"' && opening_tag[cursor] != '\'')) {
            return std::nullopt;
        }
        const char quote = opening_tag[cursor++];
        const std::size_t end = opening_tag.find(quote, cursor);
        if (end == std::string_view::npos) return std::nullopt;
        return xml_unescape(opening_tag.substr(cursor, end - cursor));
    }
    return std::nullopt;
}

bool valid_cheat_tag(std::string_view lower, std::size_t position) {
    constexpr std::string_view prefix = "<cheat";
    if (position + prefix.size() >= lower.size()) return false;
    const char next = lower[position + prefix.size()];
    return next == '>' ||
           std::isspace(static_cast<unsigned char>(next)) != 0;
}

} // namespace

Result import_myboy(std::string_view, std::string_view text_value) {
    const std::string source_name = "My Boy! .cht";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_myboy(normalized)) return {};

    const std::string lower = lower_copy(normalized);
    std::vector<NativeEntry> entries;
    std::size_t search = 0U;
    while (true) {
        std::size_t start = lower.find("<cheat", search);
        while (start != std::string::npos && !valid_cheat_tag(lower, start)) {
            start = lower.find("<cheat", start + 6U);
        }
        if (start == std::string::npos) break;
        const std::size_t open_end = lower.find('>', start + 6U);
        if (open_end == std::string::npos) {
            return recognized_error(
                SourceFormat::MyBoyCht, source_name,
                "A My Boy! cheat opening tag is truncated.");
        }
        const std::size_t close = lower.find("</cheat>", open_end + 1U);
        if (close == std::string::npos) {
            return recognized_error(
                SourceFormat::MyBoyCht, source_name,
                "A My Boy! cheat block is missing </cheat>.");
        }

        const std::string_view opening = std::string_view(normalized).substr(
            start, open_end - start + 1U);
        const auto type_value = attribute_value(opening, "type");
        const auto name_value = attribute_value(opening, "name");
        if (!type_value) {
            return recognized_error(
                SourceFormat::MyBoyCht, source_name,
                "A My Boy! cheat is missing its type attribute.");
        }
        const std::string type = lower_copy(*type_value);
        InputFormat format = InputFormat::FcdRaw;
        if (type == "cb") format = InputFormat::FcdRaw;
        else if (type == "gs3") format = InputFormat::ActionReplayMaxRaw;
        else {
            return recognized_error(
                SourceFormat::MyBoyCht, source_name,
                "A My Boy! cheat uses an unsupported type: " + *type_value);
        }

        NativeEntry entry;
        entry.name = name_value && !text::trim(*name_value).empty()
            ? *name_value
            : "Cheat " + std::to_string(entries.size() + 1U);
        entry.format = format;

        std::size_t code_search = open_end + 1U;
        while (true) {
            const std::size_t code_start = lower.find("<code>", code_search);
            if (code_start == std::string::npos || code_start >= close) break;
            const std::size_t code_end = lower.find("</code>", code_start + 6U);
            if (code_end == std::string::npos || code_end > close) {
                return recognized_error(
                    SourceFormat::MyBoyCht, source_name,
                    "A My Boy! code element is missing </code>.");
            }
            const std::string decoded = xml_unescape(
                std::string_view(normalized).substr(
                    code_start + 6U, code_end - code_start - 6U));
            const std::string formatted = text::format_compact_code_lines(decoded);
            for (const std::string& raw_line : text::split_lines(formatted)) {
                const std::string line = text::trim(raw_line);
                if (line.empty()) continue;
                const bool valid = format == InputFormat::FcdRaw
                    ? text::is_code_line_8x4(line)
                    : text::is_code_line_8x8(line);
                if (!valid) {
                    return recognized_error(
                        SourceFormat::MyBoyCht, source_name,
                        "A My Boy! code row does not match its cheat type.");
                }
                entry.code_lines.push_back(line);
            }
            code_search = code_end + 7U;
        }
        if (entry.code_lines.empty()) {
            return recognized_error(
                SourceFormat::MyBoyCht, source_name,
                "A My Boy! cheat contains no code rows.");
        }
        entries.push_back(std::move(entry));
        search = close + 8U;
    }

    return finish_entries(
        SourceFormat::MyBoyCht, source_name, entries,
        {InputFormat::ActionReplayMaxRaw, InputFormat::FcdRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
