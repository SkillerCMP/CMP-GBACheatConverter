#include "import/native_input_internal.hpp"

#include "core/detect.hpp"
#include "core/text.hpp"
#include "formats/mgba_cheats.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input::detail {
namespace {

bool all_8x4(const std::vector<std::string>& lines) {
    return !lines.empty() && std::all_of(
        lines.begin(), lines.end(), [](const std::string& line) {
            return text::is_code_line_8x4(line);
        });
}

bool all_8x8(const std::vector<std::string>& lines) {
    return !lines.empty() && std::all_of(
        lines.begin(), lines.end(), [](const std::string& line) {
            return text::is_code_line_8x8(line);
        });
}

bool is_vba_line(std::string_view raw) {
    const std::string line = text::trim(raw);
    if (line.size() != 11U && line.size() != 13U && line.size() != 17U) {
        return false;
    }
    if (line[8] != ':') return false;
    return text::parse_hex_u32(std::string_view(line).substr(0U, 8U))
               .has_value() &&
           text::parse_hex_u32(std::string_view(line).substr(9U))
               .has_value();
}

bool all_vba(const std::vector<std::string>& lines) {
    return !lines.empty() && std::all_of(
        lines.begin(), lines.end(), [](const std::string& line) {
            return is_vba_line(line);
        });
}

std::string joined_lines(const std::vector<std::string>& lines) {
    std::ostringstream out;
    for (const std::string& line : lines) out << text::trim(line) << '\n';
    return out.str();
}

std::optional<InputFormat> family_format(MgbaCodeFamily family) {
    switch (family) {
    case MgbaCodeFamily::AutoDetect: return std::nullopt;
    case MgbaCodeFamily::GameSharkV1Encrypted:
        return InputFormat::GameSharkEncrypted;
    case MgbaCodeFamily::GameSharkV1Raw:
        return InputFormat::GameSharkRaw;
    case MgbaCodeFamily::ProActionReplayV3Encrypted:
        return InputFormat::ActionReplayMaxEncrypted;
    case MgbaCodeFamily::ProActionReplayV3Raw:
        return InputFormat::ActionReplayMaxRaw;
    }
    return std::nullopt;
}

std::optional<InputFormat> auto_8x8_format(
    const std::vector<std::string>& lines) {
    const detect::Result detected = detect::format(joined_lines(lines));
    switch (detected.format) {
    case detect::Format::GameSharkRaw:
        return InputFormat::GameSharkRaw;
    case detect::Format::GameSharkEncrypted:
        return InputFormat::GameSharkEncrypted;
    case detect::Format::ActionReplayMaxRaw:
        return InputFormat::ActionReplayMaxRaw;
    case detect::Format::ActionReplayMaxEncrypted:
        return InputFormat::ActionReplayMaxEncrypted;
    default:
        return std::nullopt;
    }
}

MgbaCodeFamily resolved_family(InputFormat format) {
    switch (format) {
    case InputFormat::GameSharkEncrypted:
        return MgbaCodeFamily::GameSharkV1Encrypted;
    case InputFormat::GameSharkRaw:
        return MgbaCodeFamily::GameSharkV1Raw;
    case InputFormat::ActionReplayMaxEncrypted:
        return MgbaCodeFamily::ProActionReplayV3Encrypted;
    case InputFormat::ActionReplayMaxRaw:
        return MgbaCodeFamily::ProActionReplayV3Raw;
    default:
        return MgbaCodeFamily::AutoDetect;
    }
}

CheatDocument parse_vba_lines(const mgba_cheats::Record& record) {
    CheatDocument document;
    CheatEntry entry;
    entry.name = record.name;
    entry.enabled = record.enabled;
    for (const std::string& raw : record.code_lines) {
        const std::string line = text::trim(raw);
        const auto address = text::parse_hex_u32(
            std::string_view(line).substr(0U, 8U));
        const std::string_view value_text =
            std::string_view(line).substr(9U);
        const auto value = text::parse_hex_u32(value_text);
        if (!address || !value) {
            document.warnings.push_back(
                "An mGBA VBA row contains invalid hexadecimal data.");
            return document;
        }
        Operation operation;
        operation.address = *address;
        operation.value = *value;
        operation.width = static_cast<std::uint8_t>(value_text.size() / 2U);
        operation.kind = (*address >= 0x08000000U &&
                          *address < 0x0E000000U)
            ? OperationKind::RomPatch
            : OperationKind::Write;
        operation.source_text = line;
        entry.operations.push_back(std::move(operation));
    }
    document.entries.push_back(std::move(entry));
    return document;
}

struct ParsedRecord {
    CheatEntry entry;
    std::optional<NativeEntry> semantic;
};

ParsedRecord parse_record(const mgba_cheats::Record& source,
                          std::vector<std::string>& warnings) {
    ParsedRecord result;
    result.entry.name = source.name;
    result.entry.enabled = source.enabled;
    result.entry.mgba = MgbaCheatMetadata{source.family, source.code_lines};

    if (source.code_lines.empty()) {
        warnings.push_back(
            "mGBA set '" + source.name +
            "' is empty and is available only for lossless native export.");
        return result;
    }

    NativeEntry semantic;
    semantic.name = source.name;
    semantic.code_lines = source.code_lines;

    if (all_8x4(source.code_lines)) {
        semantic.format = InputFormat::FcdRaw;
    } else if (all_8x8(source.code_lines)) {
        std::optional<InputFormat> format = family_format(source.family);
        if (!format) format = auto_8x8_format(source.code_lines);
        if (!format) {
            warnings.push_back(
                "mGBA set '" + source.name +
                "' uses auto-detected 8+8 rows whose family could not be "
                "resolved safely; the original rows are preserved.");
            return result;
        }
        semantic.format = *format;
        result.entry.mgba->family = resolved_family(*format);
    } else if (all_vba(source.code_lines)) {
        CheatDocument parsed = parse_vba_lines(source);
        if (!parsed.warnings.empty() || parsed.entries.size() != 1U) {
            warnings.insert(warnings.end(), parsed.warnings.begin(),
                            parsed.warnings.end());
            return result;
        }
        result.entry.operations = std::move(parsed.entries.front().operations);
        // Render VBA rows through raw FCD for the GUI where exact.
        const auto rendered = render_document(
            SourceFormat::MgbaCheats, "mGBA .cheats", parsed,
            {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
             InputFormat::EzFlash});
        if (rendered.success && !rendered.text.empty()) {
            semantic.name = source.name;
            semantic.format = rendered.input_format;
            semantic.code_lines.clear();
            for (const std::string& raw : text::split_lines(rendered.text)) {
                const std::string line = text::trim(raw);
                if (text::is_code_line_8x4(line) ||
                    text::is_code_line_8x8(line)) {
                    semantic.code_lines.push_back(line);
                }
            }
            if (!semantic.code_lines.empty()) result.semantic = semantic;
        }
        return result;
    } else {
        warnings.push_back(
            "mGBA set '" + source.name +
            "' mixes code-row families and is available only for lossless "
            "native export.");
        return result;
    }

    CheatDocument parsed = parse_entry(semantic);
    if (!parsed.warnings.empty() || parsed.entries.size() != 1U ||
        parsed.entries.front().operations.empty()) {
        warnings.push_back(
            "mGBA set '" + source.name +
            "' could not be normalized safely; the original rows are "
            "preserved for native export.");
        return result;
    }
    result.entry.operations = std::move(parsed.entries.front().operations);
    result.semantic = std::move(semantic);
    return result;
}

} // namespace

Result import_mgba(std::string_view filename, std::string_view text_value) {
    const std::string source_name = "mGBA .cheats";
    const bool extension_hint = filename_extension(filename) == ".cheats";
    const mgba_cheats::ParseResult parsed =
        mgba_cheats::parse(text_value, extension_hint);
    if (!parsed.recognized) return {};
    if (!parsed.success) {
        return recognized_error(
            SourceFormat::MgbaCheats, source_name,
            parsed.warnings.empty()
                ? "The mGBA .cheats file could not be parsed."
                : parsed.warnings.front());
    }

    CheatDocument document;
    std::vector<NativeEntry> semantic_entries;
    bool all_semantic = true;
    std::vector<std::string> warnings = parsed.warnings;
    for (const mgba_cheats::Record& record : parsed.records) {
        ParsedRecord converted = parse_record(record, warnings);
        if (converted.semantic) {
            semantic_entries.push_back(std::move(*converted.semantic));
        } else {
            all_semantic = false;
        }
        document.entries.push_back(std::move(converted.entry));
    }

    Result result;
    if (all_semantic && !semantic_entries.empty()) {
        result = finish_entries(
            SourceFormat::MgbaCheats, source_name, semantic_entries,
            {InputFormat::ActionReplayMaxRaw, InputFormat::FcdRaw,
             InputFormat::EzFlash});
        if (!result.success) {
            result = {};
            result.recognized = true;
            result.success = true;
            result.source_format = SourceFormat::MgbaCheats;
            result.source_name = source_name;
            warnings.push_back(
                "The mGBA sets use mixed families that cannot be displayed "
                "as one GUI input format; use the CLI for lossless export.");
        }
    } else {
        result.recognized = true;
        result.success = true;
        result.source_format = SourceFormat::MgbaCheats;
        result.source_name = source_name;
    }

    result.document = std::move(document);
    result.has_document = true;
    result.warnings.insert(result.warnings.end(),
                           warnings.begin(), warnings.end());
    return result;
}

} // namespace gba::native_input::detail
