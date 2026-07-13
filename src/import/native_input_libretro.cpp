#include "import/native_input_internal.hpp"

#include "core/text.hpp"

#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {
namespace {

struct Item {
    std::string name;
    std::string code;
};

std::optional<std::string> assignment_string(std::string_view raw) {
    const std::string value = text::trim(raw);
    if (value.size() < 2U || value.front() != '"') return std::nullopt;
    bool escaped = false;
    for (std::size_t index = 1U; index < value.size(); ++index) {
        const char ch = value[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            if (!text::trim(std::string_view(value).substr(index + 1U)).empty()) {
                return std::nullopt;
            }
            return quoted_unescape(
                std::string_view(value).substr(1U, index - 1U));
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::string>> indexed_key(
    std::string_view raw) {
    const std::string key = text::trim(raw);
    if (key.rfind("cheat", 0U) != 0U) return std::nullopt;
    std::size_t cursor = 5U;
    if (cursor >= key.size() ||
        std::isdigit(static_cast<unsigned char>(key[cursor])) == 0) {
        return std::nullopt;
    }
    std::size_t index = 0U;
    while (cursor < key.size() &&
           std::isdigit(static_cast<unsigned char>(key[cursor])) != 0) {
        const unsigned digit = static_cast<unsigned>(key[cursor] - '0');
        if (index > (1000000U - digit) / 10U) return std::nullopt;
        index = index * 10U + digit;
        ++cursor;
    }
    if (cursor >= key.size() || key[cursor] != '_') return std::nullopt;
    return std::make_pair(index, key.substr(cursor + 1U));
}

} // namespace

Result import_libretro(std::string_view, std::string_view text_value) {
    const std::string source_name = "Libretro / RetroArch .cht";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_libretro(normalized)) return {};

    std::map<std::size_t, Item> items;
    for (const std::string& raw : text::split_lines(normalized)) {
        const std::string line = text::trim(raw);
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const auto key = indexed_key(
            std::string_view(line).substr(0U, equals));
        if (!key) continue;
        if (key->second != "desc" && key->second != "code") continue;
        const auto value = assignment_string(
            std::string_view(line).substr(equals + 1U));
        if (!value) {
            return recognized_error(
                SourceFormat::LibretroCht, source_name,
                "A Libretro cheat has an invalid quoted value.");
        }
        if (key->second == "desc") items[key->first].name = *value;
        else items[key->first].code = *value;
    }

    std::vector<NativeEntry> entries;
    for (const auto& pair : items) {
        if (pair.second.code.empty()) continue;
        NativeEntry entry;
        entry.name = pair.second.name.empty()
            ? "Cheat " + std::to_string(pair.first + 1U)
            : pair.second.name;

        bool all8x4 = true;
        bool all8x8 = true;
        std::size_t start = 0U;
        while (start <= pair.second.code.size()) {
            const std::size_t plus = pair.second.code.find('+', start);
            const std::size_t end = plus == std::string::npos
                ? pair.second.code.size() : plus;
            std::string line = text::trim(
                std::string_view(pair.second.code).substr(start, end - start));
            line = text::trim(text::format_compact_code_lines(line));
            if (line.empty()) {
                return recognized_error(
                    SourceFormat::LibretroCht, source_name,
                    "A Libretro cheat contains an empty code row.");
            }
            all8x4 = all8x4 && text::is_code_line_8x4(line);
            all8x8 = all8x8 && text::is_code_line_8x8(line);
            entry.code_lines.push_back(std::move(line));
            if (plus == std::string::npos) break;
            start = plus + 1U;
        }
        if (all8x4 == all8x8) {
            return recognized_error(
                SourceFormat::LibretroCht, source_name,
                "A Libretro cheat mixes or contains invalid code widths.");
        }
        entry.format = all8x4
            ? InputFormat::FcdRaw
            : InputFormat::ActionReplayMaxRaw;
        entries.push_back(std::move(entry));
    }

    return finish_entries(
        SourceFormat::LibretroCht, source_name, entries,
        {InputFormat::ActionReplayMaxRaw, InputFormat::FcdRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
