#include "formats/codebreaker_encode_internal.hpp"

namespace gba::codebreaker::detail {

void encode_regular_operation(EncodeState& state,
                              const CheatEntry& entry,
                              std::size_t& operation_index,
                              std::ostringstream& entry_output,
                              bool& has_output) {
    const Operation& operation = entry.operations[operation_index];
    const std::uint32_t address = operation.address & 0x0FFFFFFFU;

    if (operation.kind == OperationKind::Write &&
        operation.width == 2 &&
        operation.repeat > 1U &&
        operation.repeat <= 0xFFFFU &&
        operation.address < 0x10000000U &&
        operation.value <= 0xFFFFU &&
        operation.address_step >= 0 &&
        operation.address_step <= 0xFFFF &&
        operation.value_step >= 0 &&
        operation.value_step <= 0xFFFF) {
        emit_line(
            state,
            0x40000000U | address,
            static_cast<std::uint16_t>(operation.value),
            entry_output);
        emit_line(
            state,
            (static_cast<std::uint32_t>(operation.value_step) << 16U) |
                operation.repeat,
            static_cast<std::uint16_t>(operation.address_step),
            entry_output);
        has_output = true;
        return;
    }

    if (operation.kind == OperationKind::Write) {
        const std::size_t packed_count =
            packed_write_run_length(
                entry,
                operation_index,
                entry.operations.size() - operation_index,
                3U,
                false);
        if (packed_count > 0) {
            emit_packed_list(
                state, entry, operation_index, packed_count, entry_output);
            operation_index += packed_count - 1U;
            has_output = true;
            return;
        }
    }

    switch (operation.kind) {
    case OperationKind::Write: {
        const std::uint32_t repeat =
            operation.repeat == 0 ? 1U : operation.repeat;
        const std::int64_t step =
            operation.address_step == 0
                ? operation.width
                : operation.address_step;
        for (std::uint32_t index = 0; index < repeat; ++index) {
            const std::int64_t current_address_signed =
                static_cast<std::int64_t>(operation.address) +
                static_cast<std::int64_t>(index) * step;
            const std::int64_t current_value_signed =
                static_cast<std::int64_t>(operation.value) +
                static_cast<std::int64_t>(index) * operation.value_step;

            if (current_address_signed < 0 ||
                current_address_signed >= 0x10000000LL) {
                warn_unsupported(
                    state,
                    entry,
                    operation,
                    "address cannot be represented by CodeBreaker");
                break;
            }

            const std::uint32_t current_address =
                static_cast<std::uint32_t>(current_address_signed);
            const std::uint32_t current_value =
                static_cast<std::uint32_t>(current_value_signed);
            const std::uint32_t compact = current_address;

            if ((current_address & 0xF0000000U) != 0) {
                warn_unsupported(
                    state,
                    entry,
                    operation,
                    "address cannot be represented by CodeBreaker");
                break;
            }

            if (operation.width == 1) {
                emit_line(state,
                          0x30000000U | compact,
                          static_cast<std::uint16_t>(current_value & 0xFFU),
                          entry_output);
            } else if (operation.width == 2) {
                emit_line(state,
                          0x80000000U | compact,
                          static_cast<std::uint16_t>(current_value & 0xFFFFU),
                          entry_output);
            } else if (operation.width == 4) {
                emit_line(state,
                          0x80000000U | compact,
                          static_cast<std::uint16_t>(current_value & 0xFFFFU),
                          entry_output);
                emit_line(state,
                          0x80000000U |
                              ((compact + 2U) & 0x0FFFFFFFU),
                          static_cast<std::uint16_t>(current_value >> 16U),
                          entry_output);
            } else {
                warn_unsupported(
                    state, entry, operation, "unsupported write width");
                break;
            }
            has_output = true;
        }
        break;
    }

    case OperationKind::Or:
        if (operation.width != 2) {
            warn_unsupported(
                state,
                entry,
                operation,
                "CodeBreaker OR requires a 16-bit operand");
            break;
        }
        emit_line(state,
                  0x20000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::And:
        if (operation.width != 2) {
            warn_unsupported(
                state,
                entry,
                operation,
                "CodeBreaker AND requires a 16-bit operand");
            break;
        }
        emit_line(state,
                  0x60000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::Add:
    case OperationKind::Subtract:
        if (operation.width != 2) {
            warn_unsupported(
                state,
                entry,
                operation,
                "CodeBreaker arithmetic requires a 16-bit operand");
            break;
        }
        emit_line(
            state,
            0xE0000000U | address,
            static_cast<std::uint16_t>(
                operation.kind == OperationKind::Subtract
                    ? (~operation.value + 1U)
                    : operation.value),
            entry_output);
        has_output = true;
        break;

    case OperationKind::IfEqual:
        emit_line(state,
                  0x70000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::IfNotEqual:
        emit_line(state,
                  0xA0000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::IfGreater:
        emit_line(state,
                  0xB0000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::IfLess:
        emit_line(state,
                  0xC0000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::IfGreaterOrEqual:
    case OperationKind::IfLessOrEqual:
        warn_unsupported(
            state,
            entry,
            operation,
            "inclusive greater/less condition has no exact CodeBreaker equivalent");
        break;

    case OperationKind::IfAnd:
        emit_line(state,
                  0xF0000000U | address,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::IfDeviceButton:
        warn_unsupported(
            state,
            entry,
            operation,
            "physical GameShark device button has no CodeBreaker/FCD equivalent");
        break;

    case OperationKind::IfNand:
        if (operation.address != 0x04000130U) {
            warn_unsupported(
                state,
                entry,
                operation,
                "CodeBreaker special NAND condition is only defined "
                "for GBA KEYINPUT");
            break;
        }
        emit_line(state,
                  0xD0000020U,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::PointerWrite:
        warn_unsupported(
            state,
            entry,
            operation,
            "Action Replay MAX pointer write has no CodeBreaker/FCD equivalent");
        break;

    case OperationKind::RomPatch:
        warn_unsupported(
            state,
            entry,
            operation,
            "GameShark ROM patch has no CodeBreaker/FCD equivalent");
        break;

    case OperationKind::DeviceSlowdown:
        warn_unsupported(
            state,
            entry,
            operation,
            "physical GameShark slowdown control has no CodeBreaker/FCD equivalent");
        break;

    case OperationKind::Hook:
        if (!hook_fits_fcd(operation)) {
            warn_unsupported(
                state,
                entry,
                operation,
                "hook/master code cannot be represented exactly");
            break;
        }
        emit_line(state,
                  0x10000000U |
                      ((operation.address - 0x08000000U) & 0x01FFFFFFU),
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::GameId:
        if (operation.address >= 0x10000000U ||
            operation.value > 0xFFFFU) {
            warn_unsupported(
                state,
                entry,
                operation,
                "foreign game-ID metadata has no exact FCD mapping");
            break;
        }
        emit_line(state,
                  operation.address & 0x0FFFFFFFU,
                  static_cast<std::uint16_t>(operation.value),
                  entry_output);
        has_output = true;
        break;

    case OperationKind::EncryptionSeed:
        // The selected output seed controls encryption and is emitted once at
        // the start of the output stream.
        break;

    case OperationKind::Unsupported:
        warn_unsupported(
            state, entry, operation, "unsupported source operation");
        break;
    }
}

} // namespace gba::codebreaker::detail
