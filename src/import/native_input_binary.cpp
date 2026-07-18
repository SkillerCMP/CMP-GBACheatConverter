#include "import/native_input_internal.hpp"

#include "core/text.hpp"
#include "formats/vba_clt_codec.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace gba::native_input::detail {

Result import_armax_dsc(std::string_view,
                        const std::vector<std::uint8_t>& data) {
    constexpr std::size_t kEntryCountOffset = 0x174U;
    constexpr std::size_t kEntriesOffset = 0x178U;
    const std::string source_name = "Action Replay MAX .dsc";

    if (!equals_ascii(data, 0U, "ARDS000000000001")) {
        return {};
    }
    if (data.size() < kEntriesOffset) {
        return recognized_error(
            SourceFormat::ArmaxDsc, source_name,
            "The Action Replay MAX .dsc header is truncated.");
    }
    const auto entry_count = read_u32(data, kEntryCountOffset);
    if (!entry_count) {
        return recognized_error(
            SourceFormat::ArmaxDsc, source_name,
            "The Action Replay MAX .dsc entry count is missing.");
    }
    if (*entry_count > (data.size() - kEntriesOffset) / 24U) {
        return recognized_error(
            SourceFormat::ArmaxDsc, source_name,
            "The Action Replay MAX .dsc entry count exceeds the file size.");
    }

    std::vector<NativeEntry> entries;
    entries.reserve(*entry_count);
    std::size_t offset = kEntriesOffset;
    for (std::uint32_t entry_index = 0U;
         entry_index < *entry_count; ++entry_index) {
        if (offset > data.size() || data.size() - offset < 24U) {
            return recognized_error(
                SourceFormat::ArmaxDsc, source_name,
                "An Action Replay MAX .dsc cheat record is truncated.");
        }
        const auto dword_count = read_u32(data, offset);
        if (!dword_count || *dword_count == 0U ||
            (*dword_count & 1U) != 0U) {
            return recognized_error(
                SourceFormat::ArmaxDsc, source_name,
                "An Action Replay MAX .dsc cheat has an invalid code count.");
        }
        const std::size_t payload_dwords = *dword_count;
        if (payload_dwords > (data.size() - offset - 24U) / 4U) {
            return recognized_error(
                SourceFormat::ArmaxDsc, source_name,
                "An Action Replay MAX .dsc code payload is truncated.");
        }

        NativeEntry entry;
        entry.name = fixed_text(data, offset + 4U, 20U);
        if (entry.name.empty()) {
            entry.name = "Cheat " + std::to_string(entry_index + 1U);
        }
        entry.format = InputFormat::ActionReplayMaxEncrypted;
        const std::size_t pair_count = payload_dwords / 2U;
        entry.code_lines.reserve(pair_count);
        std::size_t code_offset = offset + 24U;
        for (std::size_t pair = 0U; pair < pair_count; ++pair) {
            const auto first = read_u32(data, code_offset);
            const auto second = read_u32(data, code_offset + 4U);
            if (!first || !second) {
                return recognized_error(
                    SourceFormat::ArmaxDsc, source_name,
                    "An Action Replay MAX .dsc code row is truncated.");
            }
            entry.code_lines.push_back(
                text::hex(*first, 8U) + " " + text::hex(*second, 8U));
            code_offset += 8U;
        }
        entries.push_back(std::move(entry));
        offset += 24U + payload_dwords * 4U;
    }

    return finish_entries(
        SourceFormat::ArmaxDsc, source_name, entries,
        {InputFormat::ActionReplayMaxEncrypted});
}

namespace {

using VbaRecord = vba_clt::Record;

std::optional<std::string> normalize_8x4(std::string_view raw) {
    std::string compact;
    for (char ch : raw) {
        if (std::isxdigit(static_cast<unsigned char>(ch)) != 0) {
            compact.push_back(static_cast<char>(std::toupper(
                static_cast<unsigned char>(ch))));
        }
    }
    if (compact.size() != 12U) return std::nullopt;
    return compact.substr(0U, 8U) + " " + compact.substr(8U, 4U);
}

std::optional<std::string> normalize_8x8(std::string_view raw) {
    std::string compact;
    for (char ch : raw) {
        if (std::isxdigit(static_cast<unsigned char>(ch)) != 0) {
            compact.push_back(static_cast<char>(std::toupper(
                static_cast<unsigned char>(ch))));
        }
    }
    if (compact.size() != 16U) return std::nullopt;
    return compact.substr(0U, 8U) + " " + compact.substr(8U, 8U);
}

std::optional<std::uint8_t> vba_direct_width(std::int32_t size) {
    switch (size) {
    case 0: return std::uint8_t{1};
    case 1:
    case 114: return std::uint8_t{2};
    case 2:
    case 115: return std::uint8_t{4};
    default: return std::nullopt;
    }
}

std::string vba_group_name(const VbaRecord& record, std::size_t index) {
    std::string name = text::trim(record.description);
    if (name.empty()) name = "Cheat " + std::to_string(index + 1U);
    return name;
}

} // namespace

Result import_vba_clt(std::string_view,
                      const std::vector<std::uint8_t>& data) {
    const std::string source_name = "VisualBoy Advance-M .clt";

    std::string decode_error;
    const auto decoded = vba_clt::decode(data, decode_error);
    if (!decoded) {
        if (!vba_clt::has_supported_header(data)) return {};
        return recognized_error(
            SourceFormat::VisualBoyAdvanceClt, source_name, decode_error);
    }
    const std::vector<VbaRecord>& records = decoded->records;

    bool cba_encrypted = false;
    for (const VbaRecord& record : records) {
        if (record.code != 512) continue;
        if (const auto line = normalize_8x4(record.code_string)) {
            cba_encrypted = !line->empty() && (*line)[0] == '9';
        }
        break;
    }

    struct Group {
        std::string name;
        std::vector<VbaRecord> records;
    };
    std::vector<Group> groups;
    for (std::size_t index = 0U; index < records.size(); ++index) {
        const std::string name = vba_group_name(records[index], index);
        if (groups.empty() || groups.back().name != name ||
            groups.back().records.back().code != records[index].code) {
            groups.push_back({name, {}});
        }
        groups.back().records.push_back(records[index]);
    }

    std::vector<NativeEntry> native_entries;
    CheatDocument combined;
    std::vector<std::string> warnings;
    bool all_native = true;
    std::optional<InputFormat> common_format;

    for (const Group& group : groups) {
        NativeEntry native;
        native.name = group.name;
        bool native_group = true;
        const std::int32_t family = group.records.front().code;
        if (family == 512) {
            native.format = cba_encrypted
                ? InputFormat::FcdEncrypted : InputFormat::FcdRaw;
            for (const VbaRecord& record : group.records) {
                const auto line = normalize_8x4(record.code_string);
                if (!line) { native_group = false; break; }
                native.code_lines.push_back(*line);
            }
        } else if (family == 257 || family == 256) {
            native.format = family == 257
                ? InputFormat::ActionReplayMaxEncrypted
                : InputFormat::GameSharkEncrypted;
            for (const VbaRecord& record : group.records) {
                const auto line = normalize_8x8(record.code_string);
                if (!line) { native_group = false; break; }
                native.code_lines.push_back(*line);
            }
        } else {
            native_group = false;
        }

        if (native_group && !native.code_lines.empty()) {
            if (!common_format) common_format = native.format;
            else if (*common_format != native.format) all_native = false;
            native_entries.push_back(native);
            CheatDocument parsed = parse_entry(native);
            if (parsed.warnings.empty() && parsed.entries.size() == 1U &&
                !parsed.entries.front().operations.empty()) {
                combined.entries.push_back(std::move(parsed.entries.front()));
            } else {
                warnings.push_back(
                    "Could not semantically decode VBA-M cheat '" +
                    group.name + "'; its stored code rows were preserved only.");
            }
            continue;
        }

        all_native = false;
        CheatEntry entry;
        entry.name = group.name;
        bool supported = true;
        for (const VbaRecord& record : group.records) {
            const auto width = vba_direct_width(record.size);
            if (!width) { supported = false; break; }
            Operation operation;
            operation.kind = OperationKind::Write;
            operation.address = record.address;
            operation.value = record.value;
            operation.width = *width;
            entry.operations.push_back(operation);
            if (!record.enabled) {
                warnings.push_back(
                    "Imported disabled VBA-M cheat '" + group.name +
                    "' as an ordinary cheat because enable state is not part "
                    "of the converter document model.");
            }
        }
        if (supported && !entry.operations.empty()) {
            combined.entries.push_back(std::move(entry));
        } else {
            warnings.push_back(
                "Omitted unsupported VBA-M internal record group '" +
                group.name + "'.");
        }
    }

    if (all_native && common_format &&
        native_entries.size() == groups.size()) {
        Result result = finish_entries(
            SourceFormat::VisualBoyAdvanceClt, source_name, native_entries,
            {*common_format});
        result.warnings.insert(result.warnings.end(),
                               warnings.begin(), warnings.end());
        return result;
    }

    if (combined.entries.empty()) {
        return recognized_error(
            SourceFormat::VisualBoyAdvanceClt, source_name,
            warnings.empty()
                ? "The VBA-M .clt file contains no supported cheat records."
                : warnings.front());
    }
    Result result = render_document(
        SourceFormat::VisualBoyAdvanceClt, source_name, combined,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
    result.warnings.insert(result.warnings.end(),
                           warnings.begin(), warnings.end());
    return result;
}

} // namespace gba::native_input::detail
