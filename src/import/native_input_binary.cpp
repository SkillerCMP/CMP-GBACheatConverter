#include "import/native_input_internal.hpp"

#include "core/text.hpp"

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

Result import_vba_clt(std::string_view,
                      const std::vector<std::uint8_t>& data) {
    constexpr std::size_t kHeaderSize = 12U;
    constexpr std::size_t kRecordSize = 80U;
    constexpr std::size_t kMaximumRecords = 100U;
    const std::string source_name = "VisualBoy Advance .clt";

    const auto version = read_u32(data, 0U);
    const auto list_type = read_u32(data, 4U);
    const auto count = read_u32(data, 8U);
    if (!version || !list_type || !count ||
        *version != 1U || *list_type != 1U) {
        return {};
    }
    if (*count > kMaximumRecords) {
        return recognized_error(
            SourceFormat::VisualBoyAdvanceClt, source_name,
            "The VisualBoy Advance .clt record count exceeds 100.");
    }
    const std::size_t required =
        kHeaderSize + static_cast<std::size_t>(*count) * kRecordSize;
    if (data.size() < required) {
        return recognized_error(
            SourceFormat::VisualBoyAdvanceClt, source_name,
            "The VisualBoy Advance .clt record table is truncated.");
    }

    CheatDocument document;
    for (std::uint32_t record_index = 0U;
         record_index < *count; ++record_index) {
        const std::size_t offset = kHeaderSize +
            static_cast<std::size_t>(record_index) * kRecordSize;
        const auto record_type = read_u32(data, offset);
        const auto width = read_u32(data, offset + 4U);
        const auto address = read_u32(data, offset + 16U);
        const auto value = read_u32(data, offset + 20U);
        if (!record_type || !width || !address || !value ||
            *record_type != 2U ||
            (*width != 1U && *width != 2U && *width != 4U)) {
            return recognized_error(
                SourceFormat::VisualBoyAdvanceClt, source_name,
                "A VisualBoy Advance .clt record has an unsupported type or width.");
        }
        std::string name = fixed_text(data, offset + 48U, 32U);
        if (name.empty()) {
            name = "Cheat " + std::to_string(record_index + 1U);
        }
        if (document.entries.empty() ||
            document.entries.back().name != name) {
            CheatEntry entry;
            entry.name = std::move(name);
            document.entries.push_back(std::move(entry));
        }
        Operation operation;
        operation.kind = OperationKind::Write;
        operation.address = *address;
        operation.value = *value;
        operation.width = static_cast<std::uint8_t>(*width);
        document.entries.back().operations.push_back(std::move(operation));
    }

    return render_document(
        SourceFormat::VisualBoyAdvanceClt, source_name, document,
        {InputFormat::FcdRaw, InputFormat::ActionReplayMaxRaw,
         InputFormat::EzFlash});
}

} // namespace gba::native_input::detail
