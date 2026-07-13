#include "formats/codebreaker_encode_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <stdexcept>

namespace gba::codebreaker::detail {

Result encode_document(const CheatDocument& document,
                       const ExportOptions& options) {
    EncodeState state(options);
    state.result.warnings = document.warnings;

    if (state.options.encrypted) {
        if (!state.options.seed) {
            throw std::runtime_error(
                "Encrypted CodeBreaker output requires a 9-type key");
        }
        state.cipher.reseed(*state.options.seed);
        state.output << text::hex(state.options.seed->op1, 8) << ' '
                     << text::hex(state.options.seed->op2, 4) << '\n';
    }

    for (const CheatEntry& entry : document.entries) {
        const auto invalid_hook = std::find_if(
            entry.operations.begin(), entry.operations.end(),
            [](const Operation& operation) {
                return operation.kind == OperationKind::Hook &&
                       !hook_fits_fcd(operation);
            });
        if (invalid_hook != entry.operations.end()) {
            state.result.warnings.push_back(
                entry.name +
                ": hook/master code cannot be represented exactly by "
                "CodeBreaker/FCD; the entire dependent entry was skipped at "
                "source line " +
                std::to_string(invalid_hook->source_line));
            state.result.success = false;
            continue;
        }

        const auto rom_patch = std::find_if(
            entry.operations.begin(), entry.operations.end(),
            [](const Operation& operation) {
                return operation.kind == OperationKind::RomPatch;
            });
        if (rom_patch != entry.operations.end()) {
            state.result.warnings.push_back(
                entry.name +
                ": GameShark ROM patch cannot be represented by "
                "CodeBreaker/FCD; the entire dependent entry was skipped at "
                "source line " +
                std::to_string(rom_patch->source_line));
            state.result.success = false;
            continue;
        }

        std::ostringstream entry_output;
        bool has_output = false;

        for (std::size_t operation_index = 0;
             operation_index < entry.operations.size();
             ++operation_index) {
            const Operation& operation = entry.operations[operation_index];
            if (is_condition_kind(operation.kind)) {
                encode_condition_operation(
                    state,
                    entry,
                    operation_index,
                    entry_output,
                    has_output);
            } else {
                encode_regular_operation(
                    state,
                    entry,
                    operation_index,
                    entry_output,
                    has_output);
            }
        }

        if (has_output) {
            state.output << text::format_cheat_header(entry.name) << '\n'
                         << entry_output.str() << '\n';
        } else if (!entry.operations.empty()) {
            state.result.warnings.push_back(
                entry.name + ": no CodeBreaker-compatible operations");
        }
    }

    state.result.text = state.output.str();
    return state.result;
}

} // namespace gba::codebreaker::detail
