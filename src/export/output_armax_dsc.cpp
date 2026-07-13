#include "export/output_modes_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::output_modes::detail {
namespace {

struct EncodedEntry {
    std::string name;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> lines8x8;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> lines8x4;
};

void append_fixed_ascii(std::vector<std::uint8_t>& out,
                        std::string_view value,
                        std::size_t width) {
    const std::size_t count = std::min(width, value.size());
    append_bytes(out, value.substr(0U, count));
    out.insert(out.end(), width - count, 0U);
}

void append_fixed_utf16_ascii(std::vector<std::uint8_t>& out,
                              std::string_view value,
                              std::size_t code_units) {
    std::size_t written = 0U;
    for (char raw_ch : value) {
        if (written + 1U >= code_units) break;
        const auto ch = static_cast<unsigned char>(raw_ch);
        append_u16(out, ch < 0x80U ? ch : static_cast<std::uint16_t>('?'));
        ++written;
    }
    while (written < code_units) {
        append_u16(out, 0U);
        ++written;
    }
}

} // namespace

Result export_armax_dsc(const CheatDocument& document,
                        const Options& options) {
    Result result;
    std::vector<EncodedEntry> entries;
    for (const CheatEntry& entry : document.entries) {
        const bool master_like = std::any_of(
            entry.operations.begin(), entry.operations.end(),
            [](const Operation& operation) {
                return operation.kind == OperationKind::Hook ||
                       operation.kind == OperationKind::GameId ||
                       operation.kind == OperationKind::EncryptionSeed;
            });
        if (master_like) {
            warn_omitted(result, entry, "Action Replay MAX .dsc");
            continue;
        }
        if (const auto ar = exact_armax(entry, true)) {
            entries.push_back({single_line_name(entry.name), *ar, {}});
            result.exported_records += ar->size();
        } else {
            warn_omitted(result, entry, "Action Replay MAX .dsc");
        }
    }
    result.exported_entries = entries.size();
    const std::string game = options.game_name.empty()
        ? "Converted GBA Cheats" : single_line_name(options.game_name);
    std::vector<std::uint8_t> out;
    const std::string signature = "ARDS000000000001";
    append_bytes(out, signature);
    for (int copy = 0; copy < 3; ++copy) {
        append_fixed_utf16_ascii(out, game, 32U);
    }
    if (out.size() < 0x158U) out.resize(0x158U, 0U);
    const std::string short_name = game.substr(0U, std::min<std::size_t>(22U, game.size()));
    append_fixed_ascii(out, short_name, 23U);
    out.push_back(0x1FU);
    append_u32(out, 0U);
    append_u32(out, static_cast<std::uint32_t>(entries.size()));
    for (const EncodedEntry& entry : entries) {
        const std::uint32_t dword_count = static_cast<std::uint32_t>(
            entry.lines8x8.size() * 2U);
        append_u32(out, dword_count);
        const std::size_t name_count =
            std::min<std::size_t>(20U, entry.name.size());
        append_bytes(out, std::string_view(entry.name).substr(0U, name_count));
        out.insert(out.end(), 20U - name_count,
                   static_cast<std::uint8_t>(' '));
        for (const auto& line : entry.lines8x8) {
            append_u32(out, line.first);
            append_u32(out, line.second);
        }
    }
    out.insert(out.end(), 20U, 0U);
    result.data = std::move(out);
    result.success = !entries.empty() || document.entries.empty();
    return result;
}

} // namespace gba::output_modes::detail
