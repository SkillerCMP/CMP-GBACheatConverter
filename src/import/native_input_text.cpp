#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/codebreaker.hpp"
#include "formats/ezflash.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
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

bool is_hex_string(std::string_view value, std::size_t expected) {
    if (value.size() != expected) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

bool is_ez_key(std::string_view raw) {
    const std::string line = lower_copy(text::trim(raw));
    static constexpr std::string_view keys[] = {
        "on=", "on:", "if=", "if:", "ifne=", "ifne:",
        "iflt=", "iflt:", "ifgt=", "ifgt:", "ifle=", "ifle:",
        "ifge=", "ifge:", "else", "endif", "rom=", "rom:",
        "romif=", "romif:", "add=", "add:", "sub=", "sub:",
        "ptr=", "ptr:", "fill=", "fill:", "slide=", "slide:"
    };
    return std::any_of(std::begin(keys), std::end(keys),
        [&line](std::string_view key) {
            return line.rfind(key, 0U) == 0U;
        });
}

} // namespace

bool looks_like_myboy(std::string_view text_value) {
    const std::string lower = lower_copy(text_value);
    return lower.find("<cheats") != std::string::npos &&
           lower.find("<cheat") != std::string::npos &&
           lower.find("<code>") != std::string::npos;
}

bool looks_like_mgba(std::string_view text_value) {
    const std::string lower = lower_copy(text_value);
    const std::vector<std::string> lines = text::split_lines(text_value);
    const bool has_fcd = std::any_of(
        lines.begin(), lines.end(), [](const std::string& line) {
            return text::is_code_line_8x4(line);
        });
    return lower.find("!disabled") != std::string::npos &&
           (lower.find("!parv3") != std::string::npos || has_fcd);
}

bool looks_like_libretro(std::string_view text_value) {
    const std::string lower = lower_copy(text_value);
    return lower.find("cheats =") != std::string::npos &&
           lower.find("_desc =") != std::string::npos &&
           lower.find("_code =") != std::string::npos;
}

bool looks_like_mednafen(std::string_view text_value) {
    bool header = false;
    bool record = false;
    for (const std::string& raw : text::split_lines(text_value)) {
        const std::string line = text::trim(raw);
        if (line.size() >= 35U && line.front() == '[') {
            const std::size_t close = line.find(']');
            if (close == 33U && is_hex_string(
                    std::string_view(line).substr(1U, 32U), 32U)) {
                header = true;
            }
        }
        if (line.rfind("R I ", 0U) == 0U) record = true;
    }
    return header && record;
}

bool looks_like_ezflash(std::string_view text_value) {
    bool section = false;
    bool key = false;
    for (const std::string& raw : text::split_lines(text_value)) {
        const std::string line = text::trim(raw);
        if (line.size() >= 2U && line.front() == '[' && line.back() == ']') {
            section = true;
        }
        if (is_ez_key(line)) key = true;
    }
    return section && key;
}

Result import_ezflash(std::string_view, std::string_view text_value) {
    const std::string source_name = "EZ-Flash .cht";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_ezflash(normalized)) return {};
    const CheatDocument document = ezflash::parse(normalized);
    if (document.entries.empty()) {
        return recognized_error(
            SourceFormat::EzFlashCht, source_name,
            document.warnings.empty()
                ? "The EZ-Flash .cht file contains no cheat entries."
                : document.warnings.front());
    }
    Result result;
    result.recognized = true;
    result.success = true;
    result.source_format = SourceFormat::EzFlashCht;
    result.input_format = InputFormat::EzFlash;
    result.source_name = source_name;
    result.text = normalized;
    if (!result.text.empty() && result.text.back() != '\n') {
        result.text.push_back('\n');
    }
    result.warnings = document.warnings;
    return result;
}

Result import_mednafen(std::string_view, std::string_view text_value) {
    const std::string source_name = "Mednafen .cht";
    const std::string normalized = text::normalize_newlines_lf(
        text::strip_utf8_bom(text_value));
    if (!looks_like_mednafen(normalized)) return {};

    CheatDocument document;
    for (const std::string& raw : text::split_lines(normalized)) {
        const std::string line = text::trim(raw);
        if (line.rfind("R I ", 0U) != 0U) continue;

        std::istringstream input(line);
        char r = 0;
        char i = 0;
        char l = 0;
        unsigned width = 0U;
        unsigned repeat = 0U;
        std::string address_text;
        std::string value_text;
        if (!(input >> r >> i >> width >> l >> repeat >>
              address_text >> value_text) ||
            r != 'R' || i != 'I' || l != 'L' || repeat != 0U ||
            (width != 1U && width != 2U && width != 4U)) {
            return recognized_error(
                SourceFormat::MednafenCht, source_name,
                "A Mednafen cheat record has an unsupported layout.");
        }
        const auto address = text::parse_hex_u32(address_text);
        const auto value = text::parse_hex_u32(value_text);
        if (!address || !value) {
            return recognized_error(
                SourceFormat::MednafenCht, source_name,
                "A Mednafen cheat record has an invalid address or value.");
        }
        std::string name;
        std::getline(input, name);
        name = text::trim(name);
        if (name.empty()) name = "Imported Write";

        if (document.entries.empty() ||
            document.entries.back().name != name) {
            CheatEntry entry;
            entry.name = name;
            document.entries.push_back(std::move(entry));
        }
        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = *address;
        operation.value = *value;
        operation.width = static_cast<std::uint8_t>(width);
        document.entries.back().operations.push_back(std::move(operation));
    }

    return render_document(
        SourceFormat::MednafenCht, source_name, document,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
