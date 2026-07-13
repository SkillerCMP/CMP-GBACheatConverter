#include "export/output_modes_internal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

std::optional<std::array<std::uint32_t, 4>> mister_record(
    const Operation& operation, std::uint32_t depth, bool condition) {
    if (operation.width != 1U && operation.width != 2U &&
        operation.width != 4U) return std::nullopt;
    const unsigned lane = operation.address & 3U;
    if ((operation.width == 2U && (lane != 0U && lane != 2U)) ||
        (operation.width == 4U && lane != 0U)) return std::nullopt;
    std::uint32_t mask = 0U;
    if (operation.width == 1U) mask = 0x10U << lane;
    if (operation.width == 2U) mask = 0x30U << lane;
    if (operation.width == 4U) mask = 0xF0U;
    const unsigned shift = lane * 8U;
    return std::array<std::uint32_t, 4>{
        mask | (condition ? 0x01U : 0U),
        operation.address & ~3U,
        depth,
        operation.value << shift
    };
}

std::optional<std::vector<std::array<std::uint32_t, 4>>>
encode_mister_entry(const CheatEntry& entry) {
    std::vector<std::array<std::uint32_t, 4>> records;
    for (std::size_t index = 0; index < entry.operations.size();) {
        const Operation& operation = entry.operations[index];
        if (operation.kind == OperationKind::Write) {
            const auto record = mister_record(operation, 0U, false);
            if (!record) return std::nullopt;
            records.push_back(*record);
            ++index;
            continue;
        }
        if (operation.kind != OperationKind::IfEqual ||
            operation.condition_has_else ||
            !operation.condition_terms.empty() ||
            operation.condition_span == 0U ||
            index + operation.condition_span >= entry.operations.size()) {
            return std::nullopt;
        }
        const auto condition = mister_record(operation, 0U, true);
        if (!condition) return std::nullopt;
        records.push_back(*condition);
        for (std::uint32_t offset = 1U;
             offset <= operation.condition_span; ++offset) {
            const Operation& controlled = entry.operations[index + offset];
            if (controlled.kind != OperationKind::Write) return std::nullopt;
            const auto record = mister_record(controlled, 1U, false);
            if (!record) return std::nullopt;
            records.push_back(*record);
        }
        index += static_cast<std::size_t>(operation.condition_span) + 1U;
    }
    if (records.empty() || records.size() > 128U) return std::nullopt;
    return records;
}

std::string safe_filename(std::string_view raw) {
    std::string name = single_line_name(raw);
    for (char& ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (uch < 0x20U || ch == '/' || ch == '\\' || ch == ':' ||
            ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
            ch == '>' || ch == '|') ch = '_';
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '.'))
        name.pop_back();
    if (name.empty()) name = "Unnamed Cheat";
    if (name.size() > 180U) name.resize(180U);
    return name;
}

} // namespace

Result export_mister(const CheatDocument& document) {
    Result result;
    std::vector<ZipItem> items;
    std::vector<std::string> used_names;
    for (const CheatEntry& entry : document.entries) {
        const auto records = encode_mister_entry(entry);
        if (!records) {
            warn_omitted(result, entry, "MiSTer");
            continue;
        }
        std::string base = safe_filename(entry.name);
        std::string filename = base + ".gg";
        for (unsigned duplicate = 2U;
             std::find(used_names.begin(), used_names.end(), filename) !=
                 used_names.end(); ++duplicate) {
            filename = base + " (" + std::to_string(duplicate) + ").gg";
        }
        used_names.push_back(filename);
        std::string payload;
        payload.reserve(records->size() * 16U);
        for (const auto& record : *records) {
            for (std::uint32_t value : record) {
                for (unsigned byte = 0U; byte < 4U; ++byte) {
                    payload.push_back(static_cast<char>(
                        (value >> (byte * 8U)) & 0xFFU));
                }
            }
        }
        items.push_back({filename, std::move(payload)});
        result.exported_records += records->size();
        ++result.exported_entries;
    }
    result.data = store_zip(items);
    result.success = !items.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
