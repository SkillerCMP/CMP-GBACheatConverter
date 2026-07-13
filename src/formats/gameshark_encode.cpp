#include "formats/gameshark_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

namespace gba::gameshark::detail {

bool address_fits(std::uint32_t address) {
    return (address & 0xF0000000U) == 0;
}

bool hook_fits_gameshark(const Operation& operation) {
    return operation.kind == OperationKind::Hook &&
           operation.address >= 0x08000000U &&
           operation.address <= 0x09FFFFFFU;
}

bool rom_patch_fits_gameshark(const Operation& operation) {
    if (operation.kind != OperationKind::RomPatch ||
        operation.encoding_hint == EncodingHint::ActionReplayMaxRomPatch ||
        operation.width != 2U ||
        operation.address < 0x08000000U ||
        operation.address > 0x09FFFFFEU ||
        (operation.address & 1U) != 0U ||
        operation.value > 0xFFFFU) {
        return false;
    }

    // Canonical rows use only the mode nibble in bits 28-31 and keep
    // bits 16-27 clear. Preserve modes 0-3 exactly.
    return (operation.encoding_parameter & 0x0FFF0000U) == 0U;
}

bool slowdown_fits_gameshark(const Operation& operation) {
    return operation.kind == OperationKind::DeviceSlowdown &&
           operation.encoding_hint != EncodingHint::ActionReplayMaxSlowdown &&
           operation.value <= 0xFFFFU &&
           (operation.encoding_parameter & 0xFFFF0000U) == 0U;
}

bool button_write_fits_gameshark(const Operation& operation) {
    return operation.kind == OperationKind::Write &&
           (operation.width == 1U || operation.width == 2U) &&
           operation.repeat == 1U &&
           operation.address_step == 0 &&
           operation.value_step == 0 &&
           operation.address < 0x10000000U &&
           (operation.address & 0x00F00000U) == 0U;
}

std::optional<RawLine> encode_button_write(
    const Operation& condition,
    const Operation& write) {
    if (condition.kind != OperationKind::IfDeviceButton ||
        condition.condition_span != 1U ||
        !button_write_fits_gameshark(write)) {
        return std::nullopt;
    }

    const std::uint32_t width_marker =
        write.width == 1U ? 0x00100000U : 0x00200000U;
    return RawLine{
        0x80000000U |
            (write.address & 0x0F0FFFFFU) |
            width_marker,
        write.value & (write.width == 1U ? 0xFFU : 0xFFFFU),
        condition.source_line,
        {}};
}

std::vector<RawLine> encode_operation(const Operation& operation,
                                      std::vector<std::string>& warnings,
                                      bool& success,
                                      const std::string& entry_name) {
    std::vector<RawLine> lines;

    auto unsupported = [&](std::string reason) {
        warnings.push_back(entry_name + ": " + std::move(reason) +
                           " at source line " +
                           std::to_string(operation.source_line));
        success = false;
    };

    const auto append = [&](std::uint32_t op1, std::uint32_t op2) {
        lines.push_back(RawLine{op1, op2, operation.source_line, {}});
    };

    if (!address_fits(operation.address) &&
        operation.kind != OperationKind::Hook &&
        operation.kind != OperationKind::GameId &&
        operation.kind != OperationKind::EncryptionSeed &&
        operation.kind != OperationKind::RomPatch &&
        operation.kind != OperationKind::DeviceSlowdown &&
        operation.kind != OperationKind::Add &&
        operation.kind != OperationKind::Subtract) {
        unsupported("address cannot be represented by GameShark/AR GBX");
        return lines;
    }

    switch (operation.kind) {
    case OperationKind::Write: {
        const std::uint32_t repeat =
            operation.repeat == 0 ? 1U : operation.repeat;
        for (std::uint32_t index = 0; index < repeat; ++index) {
            const std::uint32_t step =
                operation.address_step == 0
                    ? operation.width
                    : static_cast<std::uint32_t>(operation.address_step);
            const std::uint32_t address =
                operation.address + index * step;
            const std::uint32_t value =
                operation.value +
                index * static_cast<std::uint32_t>(operation.value_step);

            if (!address_fits(address)) {
                unsupported("expanded write address cannot be represented");
                break;
            }

            if (operation.width == 1) {
                append(address & 0x0FFFFFFFU, value & 0xFFU);
            } else if (operation.width == 2) {
                append(0x10000000U | (address & 0x0FFFFFFFU),
                       value & 0xFFFFU);
            } else if (operation.width == 4) {
                append(0x20000000U | (address & 0x0FFFFFFFU), value);
            } else {
                unsupported("unsupported write width");
                break;
            }
        }
        break;
    }


    case OperationKind::GameId:
        if (operation.value != 0x001DC0DEU) {
            unsupported(
                "FCD game-ID metadata has no exact GameShark/AR GBX mapping");
            break;
        }
        append(operation.address, operation.value);
        break;

    case OperationKind::EncryptionSeed:
        if (operation.encoding_hint == EncodingHint::GameSharkDeadface) {
            append(0xDEADFACEU, operation.value);
        }
        break;

    case OperationKind::IfEqual:
    case OperationKind::IfNotEqual:
    case OperationKind::IfLessOrEqual:
    case OperationKind::IfGreaterOrEqual: {
        if (operation.width != 2U) {
            unsupported("GameShark/AR GBX conditions require 16-bit values");
            break;
        }
        if (operation.condition_span == 0U ||
            operation.condition_span > 0xFFU) {
            unsupported("condition scope must contain 1-255 operations");
            break;
        }

        std::uint32_t subtype = 0U;
        if (operation.kind == OperationKind::IfNotEqual) {
            subtype = 1U;
        } else if (operation.kind == OperationKind::IfLessOrEqual) {
            subtype = 2U;
        } else if (operation.kind == OperationKind::IfGreaterOrEqual) {
            subtype = 3U;
        }

        const bool preserve_multiline =
            operation.encoding_hint ==
            EncodingHint::GameSharkMultilineCondition;
        if (operation.condition_span > 1U || preserve_multiline) {
            append(0xE0000000U |
                       (operation.condition_span << 16U) |
                       (operation.value & 0xFFFFU),
                   (subtype << 28U) |
                       (operation.address & 0x0FFFFFFFU));
        } else {
            append(0xD0000000U |
                       (operation.address & 0x0FFFFFFFU),
                   (subtype << 20U) |
                       (operation.value & 0xFFFFU));
        }
        break;
    }

    case OperationKind::IfGreater:
    case OperationKind::IfLess:
        unsupported(
            "strict greater/less condition has no exact GameShark/AR GBX v1 equivalent");
        break;

    case OperationKind::IfAnd:
    case OperationKind::IfNand:
        unsupported("bit-mask condition is not encoded by GameShark/AR GBX v1/v2");
        break;

    case OperationKind::IfDeviceButton:
        unsupported(
            "physical GameShark button condition must control one compatible "
            "8-bit or 16-bit write");
        break;

    case OperationKind::PointerWrite:
        unsupported(
            "Action Replay MAX pointer write has no GameShark/AR GBX v1/v2 equivalent");
        break;

    case OperationKind::Add:
    case OperationKind::Subtract: {
        if (operation.width != 4U || operation.repeat != 1U ||
            operation.address_step != 0 || operation.value_step != 0) {
            unsupported("GameShark/AR GBX arithmetic requires one 32-bit operation");
            break;
        }

        const bool subtract = operation.kind == OperationKind::Subtract;
        if (operation.encoding_hint == EncodingHint::GameSharkArithmetic) {
            const std::uint32_t parameter =
                operation.encoding_parameter & 0x00FFFFFFU;
            const std::uint32_t subtype = (parameter >> 16U) & 0xFFU;
            const bool subtype_add =
                subtype == 0x10U || subtype == 0x30U || subtype == 0x50U;
            const bool subtype_subtract =
                subtype == 0x20U || subtype == 0x40U || subtype == 0x60U;
            if ((!subtract && !subtype_add) ||
                (subtract && !subtype_subtract)) {
                unsupported("preserved GameShark arithmetic subtype does not match the operation");
                break;
            }

            const std::uint32_t immediate_mask =
                (subtype == 0x10U || subtype == 0x20U)
                    ? 0xFFU
                    : (subtype == 0x30U || subtype == 0x40U)
                          ? 0xFFFFU
                          : 0xFFFFFFFFU;
            if ((operation.value & immediate_mask) != operation.value) {
                unsupported("preserved GameShark arithmetic immediate no longer fits its source subtype");
                break;
            }

            append(0x30000000U | parameter, operation.address);
            if (subtype == 0x50U || subtype == 0x60U) {
                append(operation.value, operation.encoding_auxiliary);
            }
            break;
        }

        if (operation.value <= 0xFFFFU) {
            append(0x30000000U |
                       ((subtract ? 0x40U : 0x30U) << 16U) |
                       operation.value,
                   operation.address);
        } else {
            append(0x30000000U |
                       ((subtract ? 0x60U : 0x50U) << 16U),
                   operation.address);
            append(operation.value, 0U);
        }
        break;
    }

    case OperationKind::Or:
    case OperationKind::And:
        unsupported("logical operation is not supported by this format");
        break;

    case OperationKind::RomPatch:
        if (!rom_patch_fits_gameshark(operation)) {
            unsupported("ROM patch address, width, value, or mode flags cannot be represented");
            break;
        }
        append(
            0x60000000U |
                ((operation.address - 0x08000000U) >> 1U),
            (operation.encoding_parameter & 0xFFFF0000U) |
                (operation.value & 0xFFFFU));
        break;

    case OperationKind::DeviceSlowdown:
        if (!slowdown_fits_gameshark(operation)) {
            unsupported("physical device-button slowdown row is noncanonical");
            break;
        }
        append(0x80F00000U, operation.value & 0xFFFFU);
        break;

    case OperationKind::Hook:
        if (!hook_fits_gameshark(operation)) {
            unsupported("hook/master address is outside GBA ROM");
            break;
        }
        append(0xF0000000U |
                   (operation.address & 0x0FFFFFFFU),
               operation.value);
        break;

    case OperationKind::Unsupported:
        unsupported("unsupported source operation");
        break;
    }

    return lines;
}

Result encode_document(const CheatDocument& document,
                       const ExportOptions& options) {
    Result result;
    result.warnings = document.warnings;
    std::ostringstream output;
    crypto::TeaKey stream_output_key = crypto::GameSharkV1Key;

    for (const CheatEntry& entry : document.entries) {
        const auto invalid_hook = std::find_if(
            entry.operations.begin(), entry.operations.end(),
            [](const Operation& operation) {
                return operation.kind == OperationKind::Hook &&
                       !hook_fits_gameshark(operation);
            });
        if (invalid_hook != entry.operations.end()) {
            result.warnings.push_back(
                entry.name +
                ": hook/master code cannot be represented exactly by "
                "GameShark/AR GBX; the entire dependent entry was skipped at "
                "source line " +
                std::to_string(invalid_hook->source_line));
            result.success = false;
            continue;
        }

        const auto encode_range =
            [&](std::size_t first,
                std::size_t count,
                bool& range_success) {
                std::vector<RawLine> range_lines;
                std::size_t cursor = first;
                const std::size_t end = first + count;

                while (cursor < end) {
                    const std::size_t group_count =
                        assignment_group_length(
                            entry, cursor, end - cursor);
                    if (group_count > 0U) {
                        const auto group_lines =
                            encode_assignment_group(
                                entry, cursor, group_count);
                        range_lines.insert(
                            range_lines.end(),
                            group_lines.begin(),
                            group_lines.end());
                        cursor += group_count;
                        continue;
                    }

                    const Operation& action = entry.operations[cursor];
                    if (is_condition_kind(action.kind)) {
                        result.warnings.push_back(
                            entry.name +
                            ": nested conditions are not exported to "
                            "GameShark/AR GBX because their scope cannot be "
                            "preserved safely at source line " +
                            std::to_string(action.source_line));
                        range_success = false;
                        return range_lines;
                    }

                    if (action.kind == OperationKind::GameId ||
                        action.kind == OperationKind::EncryptionSeed ||
                        action.kind == OperationKind::Hook ||
                        action.kind == OperationKind::DeviceSlowdown) {
                        result.warnings.push_back(
                            entry.name +
                            ": condition cannot safely control metadata, "
                            "master-code, or device-control rows at source line " +
                            std::to_string(action.source_line));
                        range_success = false;
                        return range_lines;
                    }

                    auto action_lines = encode_operation(
                        action,
                        result.warnings,
                        range_success,
                        entry.name);
                    if (!range_success || action_lines.empty()) {
                        range_success = false;
                        return range_lines;
                    }

                    range_lines.insert(
                        range_lines.end(),
                        action_lines.begin(),
                        action_lines.end());
                    ++cursor;
                }

                return range_lines;
            };

        std::vector<RawLine> encoded;
        for (std::size_t operation_index = 0;
             operation_index < entry.operations.size();
             ++operation_index) {
            const Operation& operation = entry.operations[operation_index];

            if (is_condition_kind(operation.kind)) {
                const std::size_t span =
                    operation.condition_span == 0U
                        ? 0U
                        : operation.condition_span;
                const std::size_t else_span =
                    operation.condition_else_span;
                const std::size_t total_span = span + else_span;
                const std::size_t available =
                    entry.operations.size() - operation_index - 1U;
                const std::size_t skip =
                    std::min(total_span, available);

                if (operation.condition_has_else || else_span != 0U) {
                    result.warnings.push_back(
                        entry.name +
                        ": GameShark/AR GBX has no exact ELSE-branch "
                        "encoding; the condition and both branches were "
                        "skipped at source line " +
                        std::to_string(operation.source_line));
                    result.success = false;
                    operation_index += skip;
                    continue;
                }

                if (!operation.condition_terms.empty()) {
                    result.warnings.push_back(
                        entry.name +
                        ": compound EZ-Flash equality condition has no exact "
                        "GameShark/AR GBX encoding; the condition and "
                        "controlled operations were skipped at source line " +
                        std::to_string(operation.source_line));
                    result.success = false;
                    operation_index += skip;
                    continue;
                }

                if (span == 0U) {
                    result.warnings.push_back(
                        entry.name +
                        ": condition controls zero operations at source line " +
                        std::to_string(operation.source_line));
                    result.success = false;
                    continue;
                }

                if (available < span) {
                    result.warnings.push_back(
                        entry.name +
                        ": condition controls " +
                        std::to_string(span) +
                        " operation(s), but only " +
                        std::to_string(available) +
                        " remain at source line " +
                        std::to_string(operation.source_line));
                    result.success = false;
                    operation_index += skip;
                    continue;
                }

                if (operation.kind ==
                    OperationKind::IfDeviceButton) {
                    if (span != 1U) {
                        result.warnings.push_back(
                            entry.name +
                            ": physical GameShark button condition must control "
                            "exactly one write at source line " +
                            std::to_string(operation.source_line));
                        result.success = false;
                        operation_index += span;
                        continue;
                    }

                    const Operation& controlled =
                        entry.operations[operation_index + 1U];
                    const auto button_line =
                        encode_button_write(operation, controlled);
                    if (!button_line) {
                        result.warnings.push_back(
                            entry.name +
                            ": physical GameShark button condition cannot "
                            "represent its controlled operation at source line " +
                            std::to_string(operation.source_line));
                        result.success = false;
                        operation_index += 1U;
                        continue;
                    }

                    encoded.push_back(*button_line);
                    operation_index += 1U;
                    continue;
                }

                bool group_success = true;
                auto condition_lines = encode_operation(
                    operation,
                    result.warnings,
                    group_success,
                    entry.name);
                auto action_lines = encode_range(
                    operation_index + 1U,
                    span,
                    group_success);

                if (!group_success ||
                    condition_lines.empty() ||
                    action_lines.empty()) {
                    result.success = false;
                    operation_index += span;
                    continue;
                }

                encoded.insert(
                    encoded.end(),
                    condition_lines.begin(),
                    condition_lines.end());
                encoded.insert(
                    encoded.end(),
                    action_lines.begin(),
                    action_lines.end());
                operation_index += span;
                continue;
            }

            const std::size_t group_count =
                assignment_group_length(
                    entry,
                    operation_index,
                    entry.operations.size() - operation_index);
            if (group_count > 0U) {
                const auto group_lines =
                    encode_assignment_group(
                        entry, operation_index, group_count);
                encoded.insert(
                    encoded.end(),
                    group_lines.begin(),
                    group_lines.end());
                operation_index += group_count - 1U;
                continue;
            }

            auto lines = encode_operation(
                operation,
                result.warnings,
                result.success,
                entry.name);
            encoded.insert(encoded.end(), lines.begin(), lines.end());
        }

        if (encoded.empty()) {
            result.warnings.push_back(
                entry.name + ": no GameShark/AR GBX-compatible operations");
            continue;
        }

        output << text::format_cheat_header(entry.name) << '\n';
        for (const RawLine& line : encoded) {
            output << format_line(
                line, options.encrypted, stream_output_key) << '\n';
            if (line.op1 == 0xDEADFACEU) {
                stream_output_key =
                    crypto::game_shark_v1_key_from_deadface(
                        static_cast<std::uint16_t>(line.op2));
            }
        }
        output << '\n';
    }

    result.text = output.str();
    return result;
}

} // namespace gba::gameshark::detail
