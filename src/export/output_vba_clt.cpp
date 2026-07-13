#include "export/output_modes_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

std::string compact_fcd_for_operation(const Operation& operation) {
    CheatEntry entry;
    entry.name = "Write";
    entry.operations.push_back(operation);
    const auto exact = exact_fcd(entry);
    if (!exact || exact->empty()) return {};
    return text::hex(exact->front().first, 8U) +
           text::hex(exact->front().second, 4U);
}

} // namespace

Result export_vba_clt(const CheatDocument& document) {
    Result result;
    struct Record { Operation operation; std::string name; std::string code; };
    std::vector<Record> records;
    for (const CheatEntry& entry : document.entries) {
        if (!direct_writes_only(entry)) {
            warn_omitted(result, entry, "VisualBoy Advance .clt");
            continue;
        }
        if (records.size() + entry.operations.size() > 100U) {
            result.warnings.push_back(
                "VisualBoy Advance .clt supports at most 100 records; "
                "remaining writes were omitted.");
            break;
        }
        for (const Operation& operation : entry.operations) {
            const std::string code = compact_fcd_for_operation(operation);
            if (code.empty()) {
                warn_omitted(result, entry, "VisualBoy Advance .clt");
                continue;
            }
            records.push_back({operation, single_line_name(entry.name), code});
            ++result.exported_records;
        }
        ++result.exported_entries;
    }
    std::vector<std::uint8_t> out;
    append_u32(out, 1U);
    append_u32(out, 1U);
    append_u32(out, static_cast<std::uint32_t>(records.size()));
    for (std::size_t slot = 0; slot < 100U; ++slot) {
        const std::size_t start = out.size();
        out.resize(start + 80U, 0U);
        if (slot >= records.size()) continue;
        const Record& record = records[slot];
        patch_u32(out, start + 0U, 2U);
        patch_u32(out, start + 4U, record.operation.width);
        patch_u32(out, start + 8U, 0U);
        patch_u32(out, start + 12U, 0U);
        patch_u32(out, start + 16U, record.operation.address);
        patch_u32(out, start + 20U, record.operation.value);
        patch_u32(out, start + 24U, 0U);
        const std::size_t code_count = std::min<std::size_t>(19U, record.code.size());
        for (std::size_t index = 0U; index < code_count; ++index) {
            out[start + 28U + index] = static_cast<std::uint8_t>(
                static_cast<unsigned char>(record.code[index]));
        }
        const std::size_t name_count = std::min<std::size_t>(31U, record.name.size());
        for (std::size_t index = 0U; index < name_count; ++index) {
            out[start + 48U + index] = static_cast<std::uint8_t>(
                static_cast<unsigned char>(record.name[index]));
        }
    }
    result.data = std::move(out);
    result.success = !records.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
