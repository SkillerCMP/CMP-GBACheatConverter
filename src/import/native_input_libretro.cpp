#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/retroarch_cht.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::native_input::detail {
namespace {

RetroArchCheatMetadata metadata_from_record(
    const retroarch_cht::Record& record) {
    RetroArchCheatMetadata metadata;
    metadata.code = record.code;
    metadata.handler = record.handler;
    metadata.memory_search_size = record.memory_search_size;
    metadata.cheat_type = record.cheat_type;
    metadata.value = record.value;
    metadata.address = record.address;
    metadata.address_mask = record.address_mask;
    metadata.rumble_type = record.rumble_type;
    metadata.rumble_value = record.rumble_value;
    metadata.rumble_port = record.rumble_port;
    metadata.rumble_primary_strength = record.rumble_primary_strength;
    metadata.rumble_primary_duration = record.rumble_primary_duration;
    metadata.rumble_secondary_strength = record.rumble_secondary_strength;
    metadata.rumble_secondary_duration = record.rumble_secondary_duration;
    metadata.repeat_count = record.repeat_count;
    metadata.repeat_add_to_value = record.repeat_add_to_value;
    metadata.repeat_add_to_address = record.repeat_add_to_address;
    metadata.big_endian = record.big_endian;
    return metadata;
}

std::uint32_t byte_swap(std::uint32_t value, std::uint8_t width) {
    if (width == 2U) {
        return ((value & 0x00FFU) << 8U) |
               ((value & 0xFF00U) >> 8U);
    }
    if (width == 4U) {
        return ((value & 0x000000FFU) << 24U) |
               ((value & 0x0000FF00U) << 8U) |
               ((value & 0x00FF0000U) >> 8U) |
               ((value & 0xFF000000U) >> 24U);
    }
    return value;
}

std::vector<std::string> split_core_code(std::string_view code) {
    std::vector<std::string> rows;
    const std::string normalized = text::normalize_newlines_lf(code);
    for (const std::string& raw_line : text::split_lines(normalized)) {
        std::size_t start = 0U;
        while (start <= raw_line.size()) {
            const std::size_t plus = raw_line.find('+', start);
            const std::size_t end = plus == std::string::npos
                ? raw_line.size() : plus;
            std::string row = text::trim(
                std::string_view(raw_line).substr(start, end - start));
            if (!row.empty()) {
                row = text::trim(text::format_compact_code_lines(row));
                rows.push_back(std::move(row));
            }
            if (plus == std::string::npos) break;
            start = plus + 1U;
        }
    }
    return rows;
}

CheatEntry import_core_record(const retroarch_cht::Record& record,
                              std::size_t index,
                              std::vector<std::string>& warnings) {
    CheatEntry entry;
    entry.name = record.desc.empty()
        ? "Cheat " + std::to_string(index + 1U) : record.desc;
    entry.enabled = record.enabled;
    entry.retroarch = metadata_from_record(record);

    const std::vector<std::string> rows = split_core_code(record.code);
    if (rows.empty()) {
        warnings.push_back(
            "RetroArch cheat '" + entry.name +
            "' has no core code; its native fields were preserved only.");
        return entry;
    }

    bool all8x4 = true;
    bool all8x8 = true;
    for (const std::string& row : rows) {
        all8x4 = all8x4 && text::is_code_line_8x4(row);
        all8x8 = all8x8 && text::is_code_line_8x8(row);
    }
    if (all8x4 == all8x8) {
        warnings.push_back(
            "RetroArch core cheat '" + entry.name +
            "' uses a core-specific code string that cannot be converted "
            "semantically; it remains available for exact RetroArch re-export.");
        return entry;
    }

    NativeEntry native;
    native.name = entry.name;
    native.format = all8x4
        ? InputFormat::FcdRaw : InputFormat::ActionReplayMaxRaw;
    native.code_lines = rows;
    CheatDocument parsed = parse_entry(native);
    if (!parsed.warnings.empty() || parsed.entries.size() != 1U) {
        warnings.push_back(
            "RetroArch core cheat '" + entry.name +
            "' could not be decoded safely; its original code was preserved.");
        return entry;
    }
    entry.operations = std::move(parsed.entries.front().operations);
    return entry;
}

std::optional<std::uint8_t> native_width(std::uint32_t search_size) {
    if (search_size == 3U) return std::uint8_t{1U};
    if (search_size == 4U) return std::uint8_t{2U};
    if (search_size == 5U) return std::uint8_t{4U};
    return std::nullopt;
}

CheatEntry import_memory_record(const retroarch_cht::Record& record,
                                std::size_t index,
                                std::vector<std::string>& warnings) {
    CheatEntry entry;
    entry.name = record.desc.empty()
        ? "Cheat " + std::to_string(index + 1U) : record.desc;
    entry.enabled = record.enabled;
    entry.retroarch = metadata_from_record(record);

    const auto width = native_width(record.memory_search_size);
    if (!width) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' uses a 1-, 2-, or 4-bit memory operation. The exact bit mask "
            "was preserved for RetroArch re-export but is not converted to a "
            "GBA device-code operation.");
        return entry;
    }

    if (record.cheat_type < 1U || record.cheat_type > 7U) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' uses an unsupported native cheat_type; its fields were preserved.");
        return entry;
    }
    if (record.cheat_type >= 4U) {
        warnings.push_back(
            "RetroArch condition '" + entry.name +
            "' controls the next enabled RetroArch native record rather than "
            "an operation inside the same cheat. It was preserved exactly but "
            "not flattened into a device-code condition.");
        return entry;
    }
    if (record.repeat_count == 0U) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' has repeat_count zero and performs no write; it was preserved only.");
        return entry;
    }
    const std::uint64_t address_step =
        static_cast<std::uint64_t>(record.repeat_add_to_address) * *width;
    if (address_step > static_cast<std::uint64_t>(
            std::numeric_limits<std::int32_t>::max())) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' has an address repeat step too large for semantic conversion; "
            "its native fields were preserved.");
        return entry;
    }
    if (record.big_endian && *width > 1U && record.cheat_type != 1U) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' performs big-endian arithmetic. It was preserved exactly but "
            "not converted to a little-endian GBA device operation.");
        return entry;
    }
    if (record.big_endian && *width > 1U &&
        record.repeat_count > 1U && record.repeat_add_to_value != 0U) {
        warnings.push_back(
            "RetroArch native cheat '" + entry.name +
            "' combines big-endian writes with a changing repeated value. It "
            "was preserved exactly but not converted semantically.");
        return entry;
    }

    Operation operation;
    operation.kind = record.cheat_type == 1U
        ? OperationKind::Write
        : (record.cheat_type == 2U
            ? OperationKind::Add : OperationKind::Subtract);
    operation.address = record.address;
    operation.value = record.big_endian
        ? byte_swap(record.value, *width) : record.value;
    operation.width = *width;
    operation.repeat = record.repeat_count;
    operation.address_step = static_cast<std::int32_t>(address_step);
    operation.value_step = static_cast<std::int32_t>(
        record.big_endian ? 0U : record.repeat_add_to_value);
    entry.operations.push_back(operation);

    warnings.push_back(
        "RetroArch native cheat '" + entry.name +
        "' uses an address in RetroArch's flattened core-memory space. The "
        "numeric offset was retained, but conversion to a hardware GBA address "
        "must be verified for the selected Libretro core.");
    if (record.rumble_type != 0U) {
        warnings.push_back(
            "RetroArch rumble settings for '" + entry.name +
            "' were preserved for RetroArch re-export; other formats ignore them.");
    }
    return entry;
}

} // namespace

Result import_libretro(std::string_view, std::string_view text_value) {
    const std::string source_name = "Libretro / RetroArch .cht";
    const retroarch_cht::ParseResult parsed = retroarch_cht::parse(text_value);
    if (!parsed.recognized) return {};
    if (!parsed.success) {
        return recognized_error(
            SourceFormat::LibretroCht, source_name,
            parsed.warnings.empty()
                ? "The RetroArch .cht file is invalid."
                : parsed.warnings.front());
    }

    Result result;
    result.recognized = true;
    result.success = true;
    result.source_format = SourceFormat::LibretroCht;
    result.source_name = source_name;
    result.warnings = parsed.warnings;

    CheatDocument semantic;
    for (std::size_t index = 0U; index < parsed.records.size(); ++index) {
        const retroarch_cht::Record& record = parsed.records[index];
        CheatEntry entry;
        if (record.handler == 0U) {
            entry = import_core_record(record, index, result.warnings);
        } else if (record.handler == 1U) {
            entry = import_memory_record(record, index, result.warnings);
        } else {
            entry.name = record.desc.empty()
                ? "Cheat " + std::to_string(index + 1U) : record.desc;
            entry.enabled = record.enabled;
            entry.retroarch = metadata_from_record(record);
            result.warnings.push_back(
                "RetroArch cheat '" + entry.name +
                "' uses an unknown handler value; its native fields were preserved.");
        }
        if (!entry.operations.empty()) semantic.entries.push_back(entry);
        result.document.entries.push_back(std::move(entry));
    }
    result.has_document = true;

    if (!semantic.entries.empty()) {
        Result rendered = render_document(
            SourceFormat::LibretroCht, source_name, semantic,
            {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
             InputFormat::EzFlash});
        if (rendered.success) {
            result.input_format = rendered.input_format;
            result.text = std::move(rendered.text);
        } else {
            result.input_format = InputFormat::FcdRaw;
            result.warnings.push_back(
                "The semantically convertible RetroArch records require mixed "
                "device formats, so the GUI text view was not generated.");
        }
    } else {
        result.input_format = InputFormat::FcdRaw;
        result.warnings.push_back(
            "This RetroArch file contains only native/core-specific records. "
            "They are preserved for native-to-native CLI conversion, but no "
            "editable device-code text could be generated.");
    }
    return result;
}

} // namespace gba::native_input::detail
