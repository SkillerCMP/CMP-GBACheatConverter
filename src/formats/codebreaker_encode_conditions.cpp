#include "formats/codebreaker_encode_internal.hpp"

#include <algorithm>

namespace gba::codebreaker::detail {

void encode_condition_operation(EncodeState& state,
                                const CheatEntry& entry,
                                std::size_t& operation_index,
                                std::ostringstream& entry_output,
                                bool& has_output) {
    const Operation& operation = entry.operations[operation_index];
    const std::size_t span = operation.condition_span == 0U
        ? 1U
        : operation.condition_span;
    const std::size_t else_span = operation.condition_else_span;
    const std::size_t total_span = span + else_span;
    const std::size_t available =
        entry.operations.size() - operation_index - 1U;
    const std::size_t skip = std::min(total_span, available);

    if (operation.condition_has_else || else_span != 0U) {
        warn_unsupported(
            state,
            entry,
            operation,
            "CodeBreaker/FCD has no exact ELSE-branch encoding; "
            "the condition and both branches were skipped");
        operation_index += skip;
        return;
    }

    if (!operation.condition_terms.empty()) {
        warn_unsupported(
            state,
            entry,
            operation,
            "compound EZ-Flash equality condition has no exact "
            "CodeBreaker/FCD encoding; the condition and controlled "
            "operations were skipped");
        operation_index += skip;
        return;
    }

    const auto condition_words = encode_condition_words(operation);

    if (!condition_words) {
        warn_unsupported(
            state,
            entry,
            operation,
            operation.kind == OperationKind::IfDeviceButton
                ? "physical GameShark device button has no CodeBreaker/FCD equivalent"
                : "condition has no exact CodeBreaker/FCD encoding");
        operation_index += skip;
        return;
    }

    if (span > available) {
        warn_unsupported(
            state,
            entry,
            operation,
            "condition does not have all controlled operations");
        operation_index += skip;
        return;
    }

    if (span > 1U) {
        const std::size_t packed_count =
            packed_write_run_length(
                entry,
                operation_index + 1U,
                span,
                1U,
                true);
        if (packed_count == span) {
            emit_line(state,
                      condition_words->first,
                      condition_words->second,
                      entry_output);
            emit_packed_list(
                state, entry, operation_index + 1U, span, entry_output);
            operation_index += span;
            has_output = true;
            return;
        }

        // EZ-Flash Enhanced can place several writes under one IF group. FCD
        // conditions control only the next physical operation, so expand a
        // non-packable group by repeating the condition before every emitted
        // write row. A 32-bit semantic write becomes two conditioned rows.
        bool compatible = true;
        for (std::size_t offset = 1U; offset <= span; ++offset) {
            const Operation& action =
                entry.operations[operation_index + offset];
            if (action.kind != OperationKind::Write ||
                !can_encode_fcd_action(action)) {
                compatible = false;
                break;
            }
        }
        if (!compatible) {
            warn_unsupported(
                state,
                entry,
                operation,
                "condition controls multiple operations that "
                "cannot be represented safely by CodeBreaker/FCD");
            operation_index += span;
            return;
        }

        for (std::size_t offset = 1U; offset <= span; ++offset) {
            const Operation& action =
                entry.operations[operation_index + offset];
            const std::uint32_t repeat =
                action.repeat == 0U ? 1U : action.repeat;
            const std::int64_t step =
                action.address_step == 0
                    ? action.width
                    : action.address_step;

            for (std::uint32_t repeat_index = 0U;
                 repeat_index < repeat;
                 ++repeat_index) {
                const std::uint32_t current_address =
                    static_cast<std::uint32_t>(
                        static_cast<std::int64_t>(action.address) +
                        static_cast<std::int64_t>(repeat_index) * step);
                const std::uint32_t current_value =
                    static_cast<std::uint32_t>(
                        static_cast<std::int64_t>(action.value) +
                        static_cast<std::int64_t>(repeat_index) *
                            action.value_step);

                const auto emit_condition = [&]() {
                    emit_line(state,
                              condition_words->first,
                              condition_words->second,
                              entry_output);
                };

                if (action.width == 1U) {
                    emit_condition();
                    emit_line(state,
                              0x30000000U | current_address,
                              static_cast<std::uint16_t>(
                                  current_value & 0xFFU),
                              entry_output);
                } else if (action.width == 2U) {
                    emit_condition();
                    emit_line(state,
                              0x80000000U | current_address,
                              static_cast<std::uint16_t>(
                                  current_value & 0xFFFFU),
                              entry_output);
                } else {
                    emit_condition();
                    emit_line(state,
                              0x80000000U | current_address,
                              static_cast<std::uint16_t>(
                                  current_value & 0xFFFFU),
                              entry_output);
                    emit_condition();
                    emit_line(
                        state,
                        0x80000000U |
                            ((current_address + 2U) & 0x0FFFFFFFU),
                        static_cast<std::uint16_t>(current_value >> 16U),
                        entry_output);
                }
            }
        }

        operation_index += span;
        has_output = true;
        return;
    }

    const Operation& controlled = entry.operations[operation_index + 1U];
    if (!can_encode_fcd_action(controlled)) {
        warn_unsupported(
            state,
            entry,
            operation,
            "conditioned operation cannot be represented safely "
            "by CodeBreaker/FCD");
        ++operation_index;
        return;
    }

    // A direct 32-bit FCD write is physically two 16-bit rows. Repeat the
    // condition before both rows so the upper half can never execute
    // unconditionally. The same rule applies when a non-slide repeated write
    // expands to several physical rows.
    const bool compact_slide =
        controlled.kind == OperationKind::Write &&
        controlled.width == 2U &&
        controlled.repeat > 1U &&
        controlled.repeat <= 0xFFFFU &&
        controlled.address < 0x10000000U &&
        controlled.value <= 0xFFFFU &&
        controlled.address_step >= 0 &&
        controlled.address_step <= 0xFFFF &&
        controlled.value_step >= 0 &&
        controlled.value_step <= 0xFFFF;

    if (controlled.kind == OperationKind::Write &&
        !compact_slide &&
        (controlled.width == 4U || controlled.repeat > 1U)) {
        const std::uint32_t repeat =
            controlled.repeat == 0U ? 1U : controlled.repeat;
        const std::int64_t step =
            controlled.address_step == 0
                ? controlled.width
                : controlled.address_step;
        for (std::uint32_t repeat_index = 0U;
             repeat_index < repeat;
             ++repeat_index) {
            const std::uint32_t current_address =
                static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(controlled.address) +
                    static_cast<std::int64_t>(repeat_index) * step);
            const std::uint32_t current_value =
                static_cast<std::uint32_t>(
                    static_cast<std::int64_t>(controlled.value) +
                    static_cast<std::int64_t>(repeat_index) *
                        controlled.value_step);

            emit_line(state,
                      condition_words->first,
                      condition_words->second,
                      entry_output);
            if (controlled.width == 1U) {
                emit_line(state,
                          0x30000000U | current_address,
                          static_cast<std::uint16_t>(current_value & 0xFFU),
                          entry_output);
            } else {
                emit_line(state,
                          0x80000000U | current_address,
                          static_cast<std::uint16_t>(current_value & 0xFFFFU),
                          entry_output);
                if (controlled.width == 4U) {
                    emit_line(state,
                              condition_words->first,
                              condition_words->second,
                              entry_output);
                    emit_line(
                        state,
                        0x80000000U |
                            ((current_address + 2U) & 0x0FFFFFFFU),
                        static_cast<std::uint16_t>(current_value >> 16U),
                        entry_output);
                }
            }
        }
        ++operation_index;
        has_output = true;
        return;
    }

    emit_line(state,
              condition_words->first,
              condition_words->second,
              entry_output);
    has_output = true;
}

} // namespace gba::codebreaker::detail
