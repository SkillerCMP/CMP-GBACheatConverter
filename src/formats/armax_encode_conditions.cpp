#include "formats/armax_encode_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gba::armax::detail {

bool EntryEncoder::encode_range(
    std::size_t first,
    std::size_t count,
    EncodedLines& destination) {
    if (first > entry_.operations.size() ||
        count > entry_.operations.size() - first) {
        result_.warnings.push_back(
            entry_.name +
            ": condition range extends past the end of the cheat entry");
        result_.success = false;
        return false;
    }

    const std::size_t original_size = destination.size();
    const std::size_t end = first + count;
    std::size_t index = first;

    while (index < end) {
        const Operation& operation = entry_.operations[index];
        if (!is_condition_kind(operation.kind)) {
            if (!encode_non_condition(operation, destination)) {
                destination.resize(original_size);
                return false;
            }
            ++index;
            continue;
        }

        const std::size_t then_span = operation.condition_span;
        const std::size_t else_span = operation.condition_else_span;
        const bool has_else =
            operation.condition_has_else || else_span != 0U;
        const std::size_t controlled_span = then_span + else_span;
        if (controlled_span == 0U ||
            controlled_span > end - index - 1U) {
            destination.resize(original_size);
            unsupported(
                operation,
                "condition does not have all controlled operations");
            return false;
        }

        if (!operation.condition_terms.empty()) {
            destination.resize(original_size);
            unsupported(
                operation,
                "compound EZ-Flash equality condition has no exact "
                "Action Replay MAX mapping");
            return false;
        }
        if (operation.condition_has_mask) {
            destination.resize(original_size);
            unsupported(
                operation,
                "masked condition has no exact Action Replay MAX mapping");
            return false;
        }

        std::vector<EncodedLine> group;
        if (operation.kind == OperationKind::IfDeviceButton) {
            if (then_span != 1U || else_span != 0U) {
                destination.resize(original_size);
                unsupported(
                    operation,
                    "physical device-button condition must control exactly one write");
                return false;
            }

            const Operation& write = entry_.operations[index + 1U];
            if (!button_write_fits(write)) {
                destination.resize(original_size);
                unsupported(
                    operation,
                    "physical device-button condition cannot represent its controlled write");
                return false;
            }

            const auto button_address = encode_address(write.address);
            const std::uint32_t special =
                write.width == 1U
                    ? kSpecialButton8
                    : (write.width == 2U
                           ? kSpecialButton16
                           : kSpecialButton32);
            group.emplace_back(
                0x00000000U, special | *button_address);
            group.emplace_back(
                write.value & width_mask(write.width),
                write.encoding_hint ==
                        EncodingHint::ActionReplayMaxButtonWrite
                    ? write.encoding_parameter
                    : 0U);
        } else {
            const auto address = encode_address(operation.address);
            const std::uint32_t wb = width_bits(operation.width);
            const std::uint32_t cb =
                condition_code_bits(operation);
            if (!address || wb == 0xFFFFFFFFU) {
                destination.resize(original_size);
                unsupported(
                    operation,
                    "condition address/width cannot be represented");
                return false;
            }
            if (cb == 0U ||
                operation.kind ==
                    OperationKind::IfGreaterOrEqual ||
                operation.kind ==
                    OperationKind::IfLessOrEqual ||
                operation.kind == OperationKind::IfNand) {
                destination.resize(original_size);
                unsupported(
                    operation,
                    "condition has no exact Action Replay MAX mapping");
                return false;
            }

            const bool block =
                has_else ||
                operation.encoding_hint ==
                    EncodingHint::ActionReplayMaxBlock ||
                then_span > 2U;
            if (!block && then_span == 0U) {
                destination.resize(original_size);
                unsupported(
                    operation,
                    "non-block condition controls zero operations");
                return false;
            }

            const std::uint32_t action =
                block
                    ? kActionBlock
                    : (then_span == 2U
                           ? kActionNextTwo
                           : kActionNext);
            group.emplace_back(
                action | cb | wb | *address,
                operation.value & width_mask(operation.width));

            if (!encode_range(index + 1U, then_span, group)) {
                destination.resize(original_size);
                return false;
            }
            if (block) {
                if (has_else) {
                    group.emplace_back(
                        0x00000000U, kSpecialElse);
                    if (!encode_range(
                            index + 1U + then_span,
                            else_span,
                            group)) {
                        destination.resize(original_size);
                        return false;
                    }
                }
                group.emplace_back(
                    0x00000000U, kSpecialEndIf);
            }
        }

        destination.insert(
            destination.end(), group.begin(), group.end());
        index += 1U + controlled_span;
    }

    return true;
}

EncodedLines EntryEncoder::encode_entry() {
    EncodedLines encoded;
    std::size_t operation_index = 0;
    while (operation_index < entry_.operations.size()) {
        const Operation& operation =
            entry_.operations[operation_index];
        if (is_condition_kind(operation.kind)) {
            const std::size_t controlled_span =
                static_cast<std::size_t>(operation.condition_span) +
                operation.condition_else_span;
            const std::size_t available =
                entry_.operations.size() - operation_index - 1U;
            const std::size_t actual =
                std::min(controlled_span, available);
            std::vector<EncodedLine> group;
            if (controlled_span == 0U ||
                !encode_range(
                    operation_index,
                    1U + controlled_span,
                    group)) {
                operation_index += 1U + actual;
                continue;
            }
            encoded.insert(
                encoded.end(), group.begin(), group.end());
            operation_index += 1U + controlled_span;
            continue;
        }

        encode_non_condition(operation, encoded);
        ++operation_index;
    }
    return encoded;
}

} // namespace gba::armax::detail
