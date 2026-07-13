#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {

std::string_view condition_key_for_kind(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual: return "IF";
    case OperationKind::IfNotEqual: return "IFNE";
    case OperationKind::IfLess: return "IFLT";
    case OperationKind::IfGreater: return "IFGT";
    case OperationKind::IfLessOrEqual: return "IFLE";
    case OperationKind::IfGreaterOrEqual: return "IFGE";
    default: return {};
    }
}

std::vector<std::uint8_t> operation_payload(const Operation& operation) {
    if (!operation.byte_payload.empty()) {
        return operation.byte_payload;
    }
    if (operation.width == 0U || operation.width > 4U) {
        return {};
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(operation.width);
    for (std::uint8_t index = 0U; index < operation.width; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(
            operation.value >> (index * 8U)));
    }
    return bytes;
}
std::string byte_list_suffix(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream output;
    for (const std::uint8_t byte : bytes) {
        output << ',' << text::hex(byte, 2);
    }
    output << ';';
    return output.str();
}

std::optional<EnhancedActionSegment> encode_enhanced_action(
    const Operation& operation,
    std::vector<std::string>& warnings,
    std::string_view entry_name) {
    const std::vector<std::uint8_t> bytes = operation_payload(operation);
    const auto compact = compact_write_address(operation.address);

    if (operation.kind == OperationKind::Write) {
        if (!compact || bytes.empty()) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced v3 cannot encode runtime write at " +
                text::hex(operation.address, 8));
            return std::nullopt;
        }
        const std::uint32_t count = operation.repeat == 0U
            ? 1U : operation.repeat;
        if (count == 1U) {
            const std::string payload =
                text::hex(*compact, 1) + byte_list_suffix(bytes);
            return EnhancedActionSegment{
                "ON=" + payload, "ON:" + payload, bytes.size(), true};
        }

        const std::int32_t address_step = operation.address_step == 0
            ? static_cast<std::int32_t>(bytes.size())
            : operation.address_step;
        const bool fill = operation.value_step == 0 &&
            address_step == static_cast<std::int32_t>(bytes.size());
        std::ostringstream payload;
        payload << text::hex(*compact, 1) << ','
                << text::hex(count, 8);
        if (!fill) {
            payload << ',' << text::hex(
                static_cast<std::uint32_t>(address_step), 8)
                    << ',' << text::hex(
                static_cast<std::uint32_t>(operation.value_step), 8);
        }
        payload << byte_list_suffix(bytes);
        const std::string name = fill ? "FILL" : "SLIDE";
        return EnhancedActionSegment{
            name + "=" + payload.str(),
            name + ":" + payload.str(),
            static_cast<std::size_t>(count) * bytes.size(), false};
    }

    if (operation.kind == OperationKind::Add ||
        operation.kind == OperationKind::Subtract) {
        if (!compact || bytes.empty()) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced v3 cannot encode arithmetic operation");
            return std::nullopt;
        }
        const std::string name = operation.kind == OperationKind::Add
            ? "ADD" : "SUB";
        const std::string payload =
            text::hex(*compact, 1) + byte_list_suffix(bytes);
        return EnhancedActionSegment{
            name + "=" + payload,
            name + ":" + payload,
            1U + bytes.size(), false};
    }

    if (operation.kind == OperationKind::PointerWrite) {
        if (!compact || bytes.empty() ||
            (operation.address & 3U) != 0U) {
            warnings.push_back(std::string(entry_name) +
                ": Enhanced v3 pointer base/payload is not representable");
            return std::nullopt;
        }
        const std::string payload =
            text::hex(*compact, 1) + ',' +
            text::hex(operation.pointer_offset, 8) +
            byte_list_suffix(bytes);
        return EnhancedActionSegment{
            "PTR=" + payload, "PTR:" + payload,
            3U + bytes.size(), false};
    }

    return std::nullopt;
}
std::optional<std::vector<ByteWrite>> flatten_condition_operation(
    const Operation& operation) {
    std::vector<ByteWrite> bytes;
    for (const ConditionTerm& term : condition_terms(operation)) {
        if (term.width == 0U || term.width > 4U) return std::nullopt;
        const auto part = flatten(term.address, term.value, term.width);
        bytes.insert(bytes.end(), part.begin(), part.end());
    }
    return bytes.empty() ? std::nullopt
                         : std::optional<std::vector<ByteWrite>>(bytes);
}

EnhancedEntryPlan build_enhanced_v3_plan(
    const CheatEntry& entry,
    std::vector<std::string>& warnings) {
    EnhancedEntryPlan plan;

    for (std::size_t index = 0U; index < entry.operations.size(); ++index) {
        const Operation& operation = entry.operations[index];
        if (operation.kind == OperationKind::EncryptionSeed ||
            operation.kind == OperationKind::GameId) {
            continue;
        }
        if (operation.kind == OperationKind::Hook) {
            warnings.push_back(entry.name +
                ": hook/master code cannot be represented by EZ-Flash; "
                "the dependent entry was skipped");
            plan.compatibility_error = true;
            return plan;
        }

        if (is_condition(operation.kind)) {
            const std::uint32_t true_span = operation.condition_span == 0U
                ? 1U : operation.condition_span;
            const std::uint32_t false_span = operation.condition_else_span;
            const std::uint64_t total_span =
                static_cast<std::uint64_t>(true_span) + false_span;
            if (index + total_span >= entry.operations.size()) {
                warnings.push_back(entry.name +
                    ": condition does not contain all controlled operations");
                plan.compatibility_error = true;
                break;
            }

            const auto condition_bytes =
                flatten_condition_operation(operation);
            if (!condition_bytes) {
                warnings.push_back(entry.name +
                    ": condition byte array is not representable by Enhanced v3");
                plan.compatibility_error = true;
                index += static_cast<std::size_t>(total_span);
                continue;
            }

            const bool rom_condition = std::all_of(
                condition_bytes->begin(), condition_bytes->end(),
                [](const ByteWrite& byte) { return is_rom_address(byte.address); });
            if (rom_condition) {
                if (operation.kind != OperationKind::IfEqual ||
                    operation.condition_has_else || false_span != 0U) {
                    warnings.push_back(entry.name +
                        ": Enhanced v3 ROMIF supports equality without ELSE only");
                    plan.compatibility_error = true;
                    index += static_cast<std::size_t>(total_span);
                    continue;
                }
                EnhancedRomGuardBlock block;
                for (const ByteWrite& byte : *condition_bytes) {
                    const auto canonical = canonical_rom_address(byte.address);
                    if (!canonical) {
                        block.conditions.clear();
                        break;
                    }
                    block.conditions.push_back(ByteWrite{*canonical, byte.value});
                }
                bool valid = !block.conditions.empty();
                for (std::uint32_t offset = 1U;
                     valid && offset <= true_span; ++offset) {
                    const Operation& action = entry.operations[index + offset];
                    if (action.kind == OperationKind::RomPatch &&
                        rom_patch_is_direct_image_write(action)) {
                        Operation normalized = action;
                        normalized.address = *canonical_rom_address(action.address);
                        append_expanded_write(block.rom_patches, normalized);
                    } else if (encode_enhanced_action(
                                   action, warnings, entry.name)) {
                        block.runtime_actions.push_back(action);
                    } else {
                        valid = false;
                    }
                }
                if (valid && (!block.runtime_actions.empty() ||
                              !block.rom_patches.empty())) {
                    plan.rom_guards.push_back(std::move(block));
                } else {
                    plan.compatibility_error = true;
                }
                index += static_cast<std::size_t>(total_span);
                continue;
            }

            if (condition_key_for_kind(operation.kind).empty()) {
                warnings.push_back(entry.name +
                    ": condition type has no Enhanced v3 equivalent");
                plan.compatibility_error = true;
                index += static_cast<std::size_t>(total_span);
                continue;
            }
            const bool runtime_addresses = std::all_of(
                condition_bytes->begin(), condition_bytes->end(),
                [](const ByteWrite& byte) {
                    return compact_condition_address(byte.address).has_value();
                });
            if (!runtime_addresses) {
                warnings.push_back(entry.name +
                    ": condition address is outside Enhanced runtime ranges");
                plan.compatibility_error = true;
                index += static_cast<std::size_t>(total_span);
                continue;
            }

            EnhancedConditionBlock block;
            block.condition = operation;
            bool valid = true;
            for (std::uint32_t offset = 1U; offset <= true_span; ++offset) {
                const Operation& action = entry.operations[index + offset];
                if (!encode_enhanced_action(action, warnings, entry.name)) {
                    valid = false;
                    break;
                }
                block.true_actions.push_back(action);
            }
            for (std::uint32_t offset = 0U;
                 valid && offset < false_span; ++offset) {
                const Operation& action =
                    entry.operations[index + 1U + true_span + offset];
                if (!encode_enhanced_action(action, warnings, entry.name)) {
                    valid = false;
                    break;
                }
                block.false_actions.push_back(action);
            }
            if (valid) {
                bool merged = false;
                if (!block.condition.condition_has_else &&
                    block.false_actions.empty() &&
                    !plan.conditions.empty()) {
                    EnhancedConditionBlock& previous =
                        plan.conditions.back();
                    const auto previous_bytes =
                        flatten_condition_operation(previous.condition);
                    const auto current_bytes =
                        flatten_condition_operation(block.condition);
                    if (!previous.condition.condition_has_else &&
                        previous.false_actions.empty() &&
                        previous.condition.kind == block.condition.kind &&
                        previous_bytes && current_bytes &&
                        equal_condition(*previous_bytes, *current_bytes)) {
                        previous.true_actions.insert(
                            previous.true_actions.end(),
                            block.true_actions.begin(),
                            block.true_actions.end());
                        previous.condition.condition_span =
                            static_cast<std::uint32_t>(
                                previous.true_actions.size());
                        merged = true;
                    }
                }
                if (!merged) {
                    plan.conditions.push_back(std::move(block));
                }
            } else {
                plan.compatibility_error = true;
            }
            index += static_cast<std::size_t>(total_span);
            continue;
        }

        if (operation.kind == OperationKind::RomPatch) {
            if (!rom_patch_is_direct_image_write(operation)) {
                warnings.push_back(entry.name +
                    ": ROM patch mode is not a direct Enhanced image patch");
                plan.compatibility_error = true;
                continue;
            }
            Operation normalized = operation;
            normalized.address = *canonical_rom_address(operation.address);
            append_expanded_write(plan.direct_rom_patches, normalized);
            continue;
        }

        if (encode_enhanced_action(operation, warnings, entry.name)) {
            plan.direct_actions.push_back(operation);
            continue;
        }

        if (operation.kind == OperationKind::DeviceSlowdown) {
            warnings.push_back(entry.name +
                ": physical GameShark slowdown has no Enhanced v3 equivalent");
        } else {
            warnings.push_back(entry.name +
                ": unsupported Enhanced v3 operation at source line " +
                std::to_string(operation.source_line));
        }
        // Do not emit a partially functional selectable entry when one of its
        // independent direct operations cannot be represented.
        plan = EnhancedEntryPlan{};
        plan.compatibility_error = true;
        return plan;
    }
    return plan;
}

std::size_t enhanced_action_records(const Operation& operation) {
    const std::vector<std::uint8_t> bytes = operation_payload(operation);
    if (operation.kind == OperationKind::Write) {
        const std::size_t count = operation.repeat == 0U
            ? 1U : operation.repeat;
        return count * bytes.size();
    }
    if (operation.kind == OperationKind::Add ||
        operation.kind == OperationKind::Subtract) {
        return 1U + bytes.size();
    }
    if (operation.kind == OperationKind::PointerWrite) {
        return 3U + bytes.size();
    }
    return 0U;
}

std::size_t enhanced_plan_records(const EnhancedEntryPlan& plan) {
    std::size_t total = 0U;
    for (const Operation& action : plan.direct_actions) {
        total += enhanced_action_records(action);
    }
    for (const EnhancedConditionBlock& block : plan.conditions) {
        const auto condition_bytes =
            flatten_condition_operation(block.condition);
        total += 1U + (condition_bytes ? condition_bytes->size() : 0U) + 1U;
        if (block.condition.condition_has_else ||
            !block.false_actions.empty()) total += 1U;
        for (const Operation& action : block.true_actions)
            total += enhanced_action_records(action);
        for (const Operation& action : block.false_actions)
            total += enhanced_action_records(action);
    }
    for (const EnhancedRomGuardBlock& block : plan.rom_guards) {
        for (const Operation& action : block.runtime_actions)
            total += enhanced_action_records(action);
    }
    return total;
}

} // namespace gba::ezflash::detail
