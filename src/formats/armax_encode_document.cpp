#include "formats/armax_encode_internal.hpp"

#include "core/text.hpp"
#include "crypto/tea.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace gba::armax::detail {

Result export_encoded_document(const CheatDocument& document,
                               const ExportOptions& options) {
    Result result;
    result.warnings = document.warnings;
    std::ostringstream output;
    crypto::TeaKey stream_output_key = crypto::ProActionReplayV3Key;

    for (const CheatEntry& entry : document.entries) {
        const Operation* invalid_operation = nullptr;
        if (entry_has_invalid_hook(entry, invalid_operation)) {
            result.warnings.push_back(
                entry.name +
                ": hook/master code cannot be represented exactly by "
                "Action Replay MAX; the entire dependent entry was skipped at "
                "source line " +
                std::to_string(invalid_operation->source_line));
            result.success = false;
            continue;
        }

        if (entry_has_unrepresentable_rom_patch(
                entry, invalid_operation)) {
            result.warnings.push_back(
                entry.name +
                ": ROM patch cannot be represented exactly by Action "
                "Replay MAX; the entire dependent entry was skipped at "
                "source line " +
                std::to_string(invalid_operation->source_line));
            result.success = false;
            continue;
        }

        EntryEncoder encoder(entry, result);
        EncodedLines encoded = encoder.encode_entry();

        if (encoded.empty()) {
            if (!entry.operations.empty()) {
                result.warnings.push_back(
                    entry.name +
                    ": no Action Replay MAX-compatible operations");
            }
            continue;
        }

        output << text::format_cheat_header(entry.name) << '\n';
        for (const auto& line : encoded) {
            output << format_line(
                line.first, line.second, options.encrypted,
                stream_output_key)
                   << '\n';
            if (line.first == 0xDEADFACEU) {
                stream_output_key =
                    crypto::pro_action_replay_v3_key_from_deadface(
                        static_cast<std::uint16_t>(line.second));
            }
        }
        output << '\n';
    }

    result.text = output.str();
    return result;
}

} // namespace gba::armax::detail
