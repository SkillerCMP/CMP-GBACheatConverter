#include "formats/codebreaker_internal.hpp"

#include <array>
#include <string>
#include <utility>

namespace gba::codebreaker::detail {
namespace {

Operation make_operation(OperationKind kind,
                         const RawLine& line,
                         std::uint32_t address,
                         std::uint32_t value,
                         std::uint8_t width,
                         std::string note = {}) {
    Operation operation;
    operation.kind = kind;
    operation.address = address;
    operation.value = value;
    operation.width = width;
    operation.source_line = line.source_line;
    operation.source_text = line.source_text;
    operation.note = std::move(note);
    return operation;
}

} // namespace

std::uint16_t byte_swap_16(std::uint16_t value) {
    return static_cast<std::uint16_t>((value >> 8U) | (value << 8U));
}

bool is_condition_kind(OperationKind kind) {
    switch (kind) {
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
        return true;
    default:
        return false;
    }
}

void warn_incomplete_pending(CheatDocument& document,
                             const PendingOperation& pending) {
    if (pending.kind == PendingKind::None) {
        return;
    }

    const std::string kind = pending.kind == PendingKind::Slide
        ? "slide/fill"
        : "packed write list";
    document.warnings.push_back(
        "Incomplete CodeBreaker " + kind + " beginning at source line " +
        std::to_string(pending.header_line));
}

void decode_line(CheatEntry& entry,
                 CheatDocument& document,
                 const RawLine& line,
                 PendingOperation& pending) {
    if (pending.kind == PendingKind::PackedList) {
        const std::array<std::uint16_t, 3> packed{
            static_cast<std::uint16_t>(line.op1 >> 16U),
            static_cast<std::uint16_t>(line.op1),
            line.op2
        };

        for (const std::uint16_t raw_value : packed) {
            if (pending.remaining_values == 0) {
                break;
            }

            Operation operation = make_operation(
                OperationKind::Write,
                line,
                pending.next_address,
                byte_swap_16(raw_value),
                2,
                "CodeBreaker packed write list; header line " +
                    std::to_string(pending.header_line));
            entry.operations.push_back(std::move(operation));
            pending.next_address += 2U;
            --pending.remaining_values;
        }

        if (pending.remaining_values == 0) {
            if (pending.controlling_condition_index &&
                *pending.controlling_condition_index < entry.operations.size()) {
                Operation& condition =
                    entry.operations[*pending.controlling_condition_index];
                const std::size_t controlled_count =
                    entry.operations.size() - pending.first_operation_index;
                condition.condition_span = static_cast<std::uint32_t>(
                    controlled_count);
                condition.note +=
                    "; controls CodeBreaker packed write list of " +
                    std::to_string(controlled_count) + " writes";
            }
            pending = {};
        }
        return;
    }

    if (pending.kind == PendingKind::Slide) {
        if (pending.operation_index >= entry.operations.size()) {
            document.warnings.push_back(
                "Internal CodeBreaker slide state was invalid at source line " +
                std::to_string(line.source_line));
            pending = {};
            return;
        }

        Operation& operation = entry.operations[pending.operation_index];
        operation.repeat = line.op1 & 0xFFFFU;
        operation.value_step = static_cast<std::int32_t>(line.op1 >> 16U);
        operation.address_step = static_cast<std::int32_t>(line.op2);
        operation.note =
            "CodeBreaker slide/fill; count=" +
            std::to_string(operation.repeat) +
            ", value step=" + std::to_string(operation.value_step) +
            ", address step=" + std::to_string(operation.address_step);
        pending = {};
        return;
    }

    const std::uint8_t type = static_cast<std::uint8_t>(line.op1 >> 28U);
    const std::uint32_t address = line.op1 & 0x0FFFFFFFU;

    switch (type) {
    case 0x0:
        entry.operations.push_back(
            make_operation(OperationKind::GameId, line, address, line.op2, 0,
                           "FCD Enable Code 1 - Game ID"));
        break;

    case 0x1:
        entry.operations.push_back(
            make_operation(OperationKind::Hook, line,
                           0x08000000U | address, line.op2, 0,
                           "FCD Enable Code 2 - Hook Address"));
        break;

    case 0x2:
        entry.operations.push_back(
            make_operation(OperationKind::Or, line, address, line.op2, 2,
                           "FCD 16-bit OR"));
        break;

    case 0x3:
        entry.operations.push_back(
            make_operation(OperationKind::Write, line, address,
                           line.op2 & 0xFFU, 1));
        break;

    case 0x4:
        entry.operations.push_back(
            make_operation(OperationKind::Write, line, address, line.op2, 2,
                           "CodeBreaker slide/fill; awaiting continuation"));
        pending.kind = PendingKind::Slide;
        pending.operation_index = entry.operations.size() - 1U;
        pending.header_line = line.source_line;
        break;

    case 0x5:
        if (line.op2 == 0) {
            entry.operations.push_back(
                make_operation(OperationKind::Unsupported, line,
                               address, 0, 2,
                               "CodeBreaker packed write list has zero values"));
            document.warnings.push_back(
                "CodeBreaker packed write list has zero values at source line " +
                std::to_string(line.source_line));
            break;
        }
        pending.kind = PendingKind::PackedList;
        pending.remaining_values = line.op2;
        pending.next_address = address;
        pending.header_line = line.source_line;
        pending.first_operation_index = entry.operations.size();
        if (!entry.operations.empty() &&
            is_condition_kind(entry.operations.back().kind)) {
            pending.controlling_condition_index =
                entry.operations.size() - 1U;
        }
        break;

    case 0x6:
        entry.operations.push_back(
            make_operation(OperationKind::And, line, address, line.op2, 2,
                           "FCD 16-bit AND"));
        break;

    case 0x7:
        entry.operations.push_back(
            make_operation(OperationKind::IfEqual, line, address, line.op2, 2));
        break;

    case 0x8:
        entry.operations.push_back(
            make_operation(OperationKind::Write, line, address, line.op2, 2));
        break;

    case 0x9:
        entry.operations.push_back(
            make_operation(OperationKind::EncryptionSeed, line,
                           line.op1, line.op2, 0));
        break;

    case 0xA:
        entry.operations.push_back(
            make_operation(OperationKind::IfNotEqual, line,
                           address, line.op2, 2));
        break;

    case 0xB: {
        Operation condition =
            make_operation(OperationKind::IfGreater, line,
                           address, line.op2, 2,
                           "FCD unsigned greater-than condition");
        condition.encoding_hint = EncodingHint::FcdUnsignedComparison;
        entry.operations.push_back(std::move(condition));
        break;
    }

    case 0xC: {
        Operation condition =
            make_operation(OperationKind::IfLess, line,
                           address, line.op2, 2,
                           "FCD unsigned less-than condition");
        condition.encoding_hint = EncodingHint::FcdUnsignedComparison;
        entry.operations.push_back(std::move(condition));
        break;
    }

    case 0xD:
        if (address == 0x20U) {
            Operation condition =
                make_operation(OperationKind::IfEqual, line,
                               0x04000130U, 0U, 2,
                               "CodeBreaker special masked button condition");
            condition.condition_has_mask = true;
            condition.condition_mask = line.op2;
            entry.operations.push_back(std::move(condition));
        } else {
            entry.operations.push_back(
                make_operation(OperationKind::Unsupported, line,
                               address, line.op2, 2,
                               "Unknown CodeBreaker D-type"));
            document.warnings.push_back(
                "Unsupported CodeBreaker D-type at source line " +
                std::to_string(line.source_line));
        }
        break;

    case 0xE:
        entry.operations.push_back(
            make_operation(OperationKind::Add, line, address, line.op2, 2,
                           "FCD 16-bit ADD"));
        break;

    case 0xF:
        entry.operations.push_back(
            make_operation(OperationKind::IfAnd, line,
                           address, line.op2, 2));
        break;
    }
}

} // namespace gba::codebreaker::detail
