#include "formats/codebreaker_encode_internal.hpp"

#include "core/text.hpp"

#include <array>

namespace gba::codebreaker::detail {

std::size_t packed_write_run_length(const CheatEntry& entry,
                                    std::size_t first_index,
                                    std::size_t maximum_count,
                                    std::size_t minimum_count,
                                    bool allow_preceding_condition) {
    if (first_index >= entry.operations.size()) {
        return 0;
    }

    if (!allow_preceding_condition && first_index > 0 &&
        is_condition_kind(entry.operations[first_index - 1U].kind)) {
        return 0;
    }

    const Operation& first = entry.operations[first_index];
    if (first.kind != OperationKind::Write ||
        first.width != 2 ||
        first.repeat != 1 ||
        first.address_step != 0 ||
        first.value_step != 0 ||
        first.address >= 0x10000000U ||
        first.value > 0xFFFFU) {
        return 0;
    }

    std::size_t count = 1;
    while (first_index + count < entry.operations.size() &&
           count < maximum_count &&
           count < 0xFFFFU) {
        const Operation& candidate = entry.operations[first_index + count];
        if (candidate.kind != OperationKind::Write ||
            candidate.width != 2 ||
            candidate.repeat != 1 ||
            candidate.address_step != 0 ||
            candidate.value_step != 0 ||
            candidate.address >= 0x10000000U ||
            candidate.value > 0xFFFFU ||
            candidate.address != first.address +
                static_cast<std::uint32_t>(count * 2U)) {
            break;
        }
        ++count;
    }

    // Unconditioned export normally uses three as the compression threshold.
    // A conditioned list can use a smaller threshold because preserving one
    // physical controlled row matters more than reducing line count.
    return count >= minimum_count ? count : 0U;
}

std::optional<std::pair<std::uint32_t, std::uint16_t>>
encode_condition_words(const Operation& operation) {
    if (operation.width != 2U || operation.value > 0xFFFFU) {
        return std::nullopt;
    }

    if (operation.condition_has_mask) {
        if (operation.value != 0U || operation.condition_mask > 0xFFFFU) {
            return std::nullopt;
        }
        if (operation.kind == OperationKind::IfEqual &&
            operation.address == 0x04000130U) {
            return std::pair{
                0xD0000020U,
                static_cast<std::uint16_t>(operation.condition_mask)};
        }
        if (operation.kind == OperationKind::IfNotEqual &&
            operation.address < 0x10000000U) {
            return std::pair{
                0xF0000000U | (operation.address & 0x0FFFFFFFU),
                static_cast<std::uint16_t>(operation.condition_mask)};
        }
        return std::nullopt;
    }

    // PAR v3 distinguishes signed and unsigned strict comparisons. FCD B/C
    // are unsigned, so a preserved signed PAR v3 condition must not be
    // silently changed while crossing formats.
    const std::uint32_t preserved_condition =
        operation.encoding_parameter & 0x38000000U;
    if (preserved_condition == 0x18000000U ||
        preserved_condition == 0x20000000U) {
        return std::nullopt;
    }

    const std::uint32_t address = operation.address & 0x0FFFFFFFU;
    if (operation.address >= 0x10000000U &&
        operation.kind != OperationKind::IfNand) {
        return std::nullopt;
    }

    switch (operation.kind) {
    case OperationKind::IfEqual:
        return std::pair{0x70000000U | address,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfNotEqual:
        return std::pair{0xA0000000U | address,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfGreater:
        return std::pair{0xB0000000U | address,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfLess:
        return std::pair{0xC0000000U | address,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfAnd:
        return std::pair{0xF0000000U | address,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfNand:
        if (operation.address != 0x04000130U) {
            return std::nullopt;
        }
        return std::pair{0xD0000020U,
                         static_cast<std::uint16_t>(operation.value)};
    case OperationKind::IfDeviceButton:
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

bool hook_fits_fcd(const Operation& operation) {
    return operation.kind == OperationKind::Hook &&
           operation.address >= 0x08000000U &&
           operation.address <= 0x09FFFFFFU &&
           operation.value <= 0xFFFFU;
}

bool can_encode_fcd_action(const Operation& operation) {
    if (is_condition_kind(operation.kind)) {
        return false;
    }

    switch (operation.kind) {
    case OperationKind::Write: {
        if (operation.width != 1U &&
            operation.width != 2U &&
            operation.width != 4U) {
            return false;
        }
        const std::uint32_t repeat = operation.repeat == 0U
            ? 1U
            : operation.repeat;
        const std::int64_t step = operation.address_step == 0
            ? operation.width
            : operation.address_step;
        for (std::uint32_t index = 0; index < repeat; ++index) {
            const std::int64_t address =
                static_cast<std::int64_t>(operation.address) +
                static_cast<std::int64_t>(index) * step;
            if (address < 0 || address >= 0x10000000LL) {
                return false;
            }
            if (operation.width == 4U && address + 2 >= 0x10000000LL) {
                return false;
            }
        }
        return true;
    }
    case OperationKind::Or:
    case OperationKind::And:
    case OperationKind::Add:
    case OperationKind::Subtract:
        return operation.width == 2U &&
               operation.address < 0x10000000U &&
               operation.value <= 0xFFFFU;
    case OperationKind::Hook:
        return hook_fits_fcd(operation);
    case OperationKind::PointerWrite:
    case OperationKind::RomPatch:
    case OperationKind::DeviceSlowdown:
        return false;
    case OperationKind::GameId:
    case OperationKind::EncryptionSeed:
        // Metadata/control records are not safe as the executable target of
        // a runtime condition.
        return false;
    case OperationKind::IfEqual:
    case OperationKind::IfNotEqual:
    case OperationKind::IfGreater:
    case OperationKind::IfLess:
    case OperationKind::IfGreaterOrEqual:
    case OperationKind::IfLessOrEqual:
    case OperationKind::IfAnd:
    case OperationKind::IfNand:
    case OperationKind::IfXor:
    case OperationKind::IfNotXor:
    case OperationKind::IfOr:
    case OperationKind::IfNotOr:
    case OperationKind::IfDeviceButton:
    case OperationKind::Transfer:
    case OperationKind::ReadSubstitute:
    case OperationKind::CompareReadSubstitute:
    case OperationKind::Unsupported:
        return false;
    }
    return false;
}

void emit_line(EncodeState& state,
               std::uint32_t op1,
               std::uint16_t op2,
               std::ostringstream& destination) {
    RawLine line{op1, op2, 0, {}};
    if (state.options.encrypted) {
        line = state.cipher.encrypt(line);
    }
    destination << text::hex(line.op1, 8) << ' '
                << text::hex(line.op2, 4) << '\n';
}

void warn_unsupported(EncodeState& state,
                      const CheatEntry& entry,
                      const Operation& operation,
                      const std::string& reason) {
    state.result.warnings.push_back(
        entry.name + ": " + reason + " at source line " +
        std::to_string(operation.source_line));
    state.result.success = false;
}

void emit_packed_list(EncodeState& state,
                      const CheatEntry& entry,
                      std::size_t first_index,
                      std::size_t packed_count,
                      std::ostringstream& destination) {
    const Operation& first = entry.operations[first_index];
    emit_line(
        state,
        0x50000000U | (first.address & 0x0FFFFFFFU),
        static_cast<std::uint16_t>(packed_count),
        destination);

    for (std::size_t packed_index = 0;
         packed_index < packed_count;
         packed_index += 3U) {
        std::array<std::uint16_t, 3> values{};
        for (std::size_t slot = 0; slot < 3U; ++slot) {
            if (packed_index + slot >= packed_count) {
                break;
            }
            values[slot] = byte_swap_16(
                static_cast<std::uint16_t>(
                    entry.operations[first_index + packed_index + slot].value));
        }

        emit_line(
            state,
            (static_cast<std::uint32_t>(values[0]) << 16U) | values[1],
            values[2],
            destination);
    }
}

} // namespace gba::codebreaker::detail
