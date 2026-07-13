#include "formats/armax_encode_internal.hpp"

#include <algorithm>

namespace gba::armax::detail {

bool hook_fits_armax(const Operation& operation) {
    return operation.kind == OperationKind::Hook &&
           operation.address >= 0x08000000U &&
           operation.address <= 0x09FFFFFFU &&
           (operation.address & 1U) == 0U;
}

bool entry_has_invalid_hook(const CheatEntry& entry,
                            const Operation*& invalid_operation) {
    const auto invalid_hook = std::find_if(
        entry.operations.begin(), entry.operations.end(),
        [](const Operation& operation) {
            return operation.kind == OperationKind::Hook &&
                   !hook_fits_armax(operation);
        });
    if (invalid_hook == entry.operations.end()) {
        invalid_operation = nullptr;
        return false;
    }
    invalid_operation = &*invalid_hook;
    return true;
}

bool entry_has_unrepresentable_rom_patch(
    const CheatEntry& entry,
    const Operation*& invalid_operation) {
    const auto rom_patch = std::find_if(
        entry.operations.begin(), entry.operations.end(),
        [](const Operation& operation) {
            if (operation.kind != OperationKind::RomPatch) {
                return false;
            }
            if (operation.encoding_hint ==
                EncodingHint::ActionReplayMaxRomPatch) {
                return false;
            }
            const bool direct_image_patch =
                operation.encoding_hint ==
                    EncodingHint::EzFlashEnhancedRomPatch &&
                operation.encoding_parameter == 0U &&
                operation.address >= 0x08000000U &&
                operation.address <= 0x09FFFFFEU &&
                (operation.address & 1U) == 0U &&
                (operation.width == 2U ||
                 (operation.width == 4U &&
                  operation.address <= 0x09FFFFFCU));
            return !direct_image_patch;
        });
    if (rom_patch == entry.operations.end()) {
        invalid_operation = nullptr;
        return false;
    }
    invalid_operation = &*rom_patch;
    return true;
}

EntryEncoder::EntryEncoder(const CheatEntry& entry, Result& result)
    : entry_(entry), result_(result) {}

void EntryEncoder::unsupported(const Operation& operation,
                               const std::string& reason) {
    result_.warnings.push_back(
        entry_.name + ": " + reason + " at source line " +
        std::to_string(operation.source_line));
    result_.success = false;
}

std::uint32_t EntryEncoder::condition_code_bits(
    const Operation& operation) {
    const std::uint32_t preserved =
        operation.encoding_parameter & kConditionMask;
    if (preserved != 0U) {
        return preserved;
    }
    if (operation.encoding_hint == EncodingHint::FcdUnsignedComparison) {
        return operation.kind == OperationKind::IfLess
            ? kCondLessUnsigned
            : operation.kind == OperationKind::IfGreater
                  ? kCondGreaterUnsigned
                  : condition_bits(operation.kind);
    }
    return condition_bits(operation.kind);
}

bool EntryEncoder::button_write_fits(const Operation& operation) {
    return operation.kind == OperationKind::Write &&
           (operation.width == 1U ||
            operation.width == 2U ||
            operation.width == 4U) &&
           operation.repeat == 1U &&
           operation.address_step == 0 &&
           operation.value_step == 0 &&
           encode_address(operation.address).has_value();
}

} // namespace gba::armax::detail
