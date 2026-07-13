#include "import/native_input_internal.hpp"

#include "core/text.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {

Result import_mgba(std::string_view, std::string_view text_value) {
    const std::string source_name = "mGBA .cheats";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_mgba(normalized)) return {};

    std::vector<NativeEntry> entries;
    std::optional<NativeEntry> current;
    const auto finalize = [&]() -> bool {
        if (!current) return true;
        if (current->code_lines.empty()) return false;
        if (text::trim(current->name).empty()) {
            current->name = "Cheat " + std::to_string(entries.size() + 1U);
        }
        entries.push_back(std::move(*current));
        current.reset();
        return true;
    };

    for (const std::string& raw : text::split_lines(normalized)) {
        const std::string line = text::trim(raw);
        if (line.empty()) continue;
        if (line == "!disabled") {
            if (!finalize()) {
                return recognized_error(
                    SourceFormat::MgbaCheats, source_name,
                    "An mGBA cheat contains no code rows.");
            }
            current.emplace();
            current->format = InputFormat::FcdRaw;
            continue;
        }
        if (!current) {
            current.emplace();
            current->format = InputFormat::FcdRaw;
        }
        if (line == "!PARv3") {
            if (!current->code_lines.empty()) {
                return recognized_error(
                    SourceFormat::MgbaCheats, source_name,
                    "An mGBA !PARv3 marker appears after code rows.");
            }
            current->format = InputFormat::ActionReplayMaxEncrypted;
            continue;
        }
        if (line.front() == '#') {
            if (current->name.empty()) {
                current->name = text::trim(
                    std::string_view(line).substr(1U));
            }
            continue;
        }
        if (line.front() == '!') continue;

        const bool valid = current->format == InputFormat::FcdRaw
            ? text::is_code_line_8x4(line)
            : text::is_code_line_8x8(line);
        if (!valid) {
            return recognized_error(
                SourceFormat::MgbaCheats, source_name,
                "An mGBA code row does not match its active code type.");
        }
        current->code_lines.push_back(line);
    }
    if (!finalize()) {
        return recognized_error(
            SourceFormat::MgbaCheats, source_name,
            "An mGBA cheat contains no code rows.");
    }

    return finish_entries(
        SourceFormat::MgbaCheats, source_name, entries,
        {InputFormat::ActionReplayMaxRaw, InputFormat::FcdRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
