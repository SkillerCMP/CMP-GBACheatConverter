#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {

std::vector<ConditionTerm> condition_terms(const Operation& operation) {
    if (!operation.condition_terms.empty()) {
        return operation.condition_terms;
    }
    return {ConditionTerm{
        operation.address,
        operation.value,
        operation.width
    }};
}

std::optional<std::uint32_t> compact_write_address(std::uint32_t address) {
    if (address >= 0x02000000U && address <= 0x0203FFFFU) {
        return address - 0x02000000U;
    }
    if (address >= 0x03000000U && address <= 0x03007FFFU) {
        return 0x40000U + (address - 0x03000000U);
    }
    return std::nullopt;
}

std::optional<std::uint32_t> compact_condition_address(std::uint32_t address) {
    if (const auto normal = compact_write_address(address)) {
        return normal;
    }
    if (address >= 0x04000000U && address <= 0x040003FFU) {
        return 0x80000U + (address - 0x04000000U);
    }
    return std::nullopt;
}

using detail::canonical_rom_address;

bool is_rom_address(std::uint32_t address) {
    // Semantic operations always store full GBA addresses. Relative ROM
    // offsets are accepted only by the textual ROM=/ROMIF= parser.
    return address >= 0x08000000U && address <= 0x0DFFFFFFU;
}

bool rom_patch_is_direct_image_write(const Operation& operation) {
    if (operation.kind != OperationKind::RomPatch ||
        operation.width == 0U || operation.width > 4U) {
        return false;
    }

    // GameShark mode 0 and every audited AR MAX patch are direct 16-bit
    // image replacements. GameShark mode 1/2 interception flags do not have
    // an exact boot-time image-patch equivalent in Enhanced v3.
    if (operation.encoding_hint != EncodingHint::ActionReplayMaxRomPatch &&
        operation.encoding_parameter != 0U) {
        return false;
    }
    return canonical_rom_address(operation.address).has_value();
}

std::vector<ByteWrite> flatten(std::uint32_t address,
                               std::uint32_t value,
                               std::uint8_t width) {
    std::vector<ByteWrite> bytes;
    bytes.reserve(width);
    for (std::uint8_t index = 0; index < width; ++index) {
        bytes.push_back(ByteWrite{
            address + index,
            static_cast<std::uint8_t>(value >> (index * 8U))
        });
    }
    return bytes;
}

void append_expanded_write(std::vector<ByteWrite>& destination,
                           const Operation& operation) {
    const std::uint32_t repeat_count = operation.repeat == 0 ? 1 : operation.repeat;
    for (std::uint32_t repeat = 0; repeat < repeat_count; ++repeat) {
        const std::uint32_t address_step = operation.address_step == 0
            ? operation.width
            : static_cast<std::uint32_t>(operation.address_step);
        const std::uint32_t address = operation.address + repeat * address_step;
        const std::uint32_t value = operation.value +
            repeat * static_cast<std::uint32_t>(operation.value_step);
        const auto bytes = flatten(address, value, operation.width);
        destination.insert(destination.end(), bytes.begin(), bytes.end());
    }
}

bool equal_condition(const std::vector<ByteWrite>& left,
                     const std::vector<ByteWrite>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].address != right[index].address ||
            left[index].value != right[index].value) {
            return false;
        }
    }
    return true;
}

bool is_condition(OperationKind kind) {
    switch (kind) {
    case OperationKind::IfEqual:
    case OperationKind::IfNotEqual:
    case OperationKind::IfGreater:
    case OperationKind::IfLess:
    case OperationKind::IfGreaterOrEqual:
    case OperationKind::IfLessOrEqual:
    case OperationKind::IfAnd:
    case OperationKind::IfNand:
    case OperationKind::IfDeviceButton:
        return true;
    default:
        return false;
    }
}

EntryGroups build_groups(const CheatEntry& entry,
                         const Options& options,
                         std::vector<std::string>& warnings) {
    EntryGroups result;

    const auto hook = std::find_if(
        entry.operations.begin(), entry.operations.end(),
        [](const Operation& operation) {
            return operation.kind == OperationKind::Hook;
        });
    if (hook != entry.operations.end()) {
        warnings.push_back(
            entry.name +
            ": hook/master code cannot be represented by EZ-Flash; the "
            "entire dependent entry was skipped at source line " +
            std::to_string(hook->source_line));
        result.compatibility_error = true;
        return result;
    }

    if (options.mode == Mode::Original) {
        const auto rom_patch = std::find_if(
            entry.operations.begin(), entry.operations.end(),
            [](const Operation& operation) {
                return operation.kind == OperationKind::RomPatch;
            });
        if (rom_patch != entry.operations.end()) {
            warnings.push_back(
                entry.name +
                ": EZ-Flash Original cannot represent ROM patches; the "
                "complete entry was skipped at source line " +
                std::to_string(rom_patch->source_line));
            result.compatibility_error = true;
            return result;
        }
    }

    for (std::size_t index = 0; index < entry.operations.size(); ++index) {
        const Operation& operation = entry.operations[index];

        if (operation.kind == OperationKind::EncryptionSeed ||
            operation.kind == OperationKind::GameId) {
            continue;
        }

        if (is_condition(operation.kind)) {
            const std::uint32_t span =
                operation.condition_span == 0 ? 1U : operation.condition_span;
            const std::uint32_t else_span = operation.condition_else_span;
            const std::uint32_t total_span = span + else_span;

            if (operation.condition_has_else || else_span != 0U) {
                warnings.push_back(
                    entry.name +
                    ": EZ-Flash Enhanced v3 does not support ELSE branches; "
                    "the condition and both branches were not exported at "
                    "source line " + std::to_string(operation.source_line));
                result.compatibility_error = true;
                index += std::min<std::size_t>(
                    total_span, entry.operations.size() - index - 1U);
                continue;
            }

            if (options.mode == Mode::Original) {
                warnings.push_back(
                    entry.name +
                    ": EZ-Flash Original supports ON= only; the condition at "
                    "source line " + std::to_string(operation.source_line) +
                    " and its controlled operation(s) were not exported");
                result.compatibility_error = true;
                index += std::min<std::size_t>(
                    span, entry.operations.size() - index - 1U);
                continue;
            }

            if (operation.kind != OperationKind::IfEqual) {
                warnings.push_back(
                    entry.name +
                    ": EZ-Flash Enhanced v3 supports exact equality "
                    "conditions only; unsupported condition at source line " +
                    std::to_string(operation.source_line));
                result.compatibility_error = true;
                index += std::min<std::size_t>(
                    span, entry.operations.size() - index - 1U);
                continue;
            }

            if (index + span >= entry.operations.size()) {
                warnings.push_back(
                    entry.name + ": condition does not have all " +
                    std::to_string(span) + " following operation(s)");
                result.compatibility_error = true;
                continue;
            }

            const std::vector<ConditionTerm> terms = condition_terms(operation);
            const bool rom_condition = !terms.empty() &&
                std::all_of(terms.begin(), terms.end(),
                    [](const ConditionTerm& term) {
                        return is_rom_address(term.address);
                    });
            const bool runtime_condition = !terms.empty() &&
                std::all_of(terms.begin(), terms.end(),
                    [](const ConditionTerm& term) {
                        return compact_condition_address(term.address).has_value();
                    });

            if (!rom_condition && !runtime_condition) {
                warnings.push_back(
                    entry.name +
                    ": equality condition mixes or uses unsupported address "
                    "ranges at source line " +
                    std::to_string(operation.source_line));
                result.compatibility_error = true;
                index += span;
                continue;
            }

            if (rom_condition) {
                RomGuardGroup group;
                bool compatible = true;
                for (const ConditionTerm& term : terms) {
                    if (term.width == 0U || term.width > 4U) {
                        compatible = false;
                        break;
                    }
                    const auto canonical = canonical_rom_address(term.address);
                    if (!canonical) {
                        compatible = false;
                        break;
                    }
                    const auto bytes = flatten(*canonical, term.value, term.width);
                    group.conditions.insert(
                        group.conditions.end(), bytes.begin(), bytes.end());
                }

                for (std::uint32_t offset = 1U;
                     compatible && offset <= span; ++offset) {
                    const Operation& action = entry.operations[index + offset];
                    if (action.kind == OperationKind::Write) {
                        append_expanded_write(group.writes, action);
                    } else if (action.kind == OperationKind::RomPatch &&
                               rom_patch_is_direct_image_write(action)) {
                        const auto canonical = canonical_rom_address(action.address);
                        Operation normalized = action;
                        normalized.address = *canonical;
                        append_expanded_write(group.rom_patches, normalized);
                    } else {
                        warnings.push_back(
                            entry.name +
                            ": ROMIF cannot represent a controlled operation "
                            "at source line " +
                            std::to_string(action.source_line));
                        result.compatibility_error = true;
                        compatible = false;
                    }
                }

                if (!compatible ||
                    (group.writes.empty() && group.rom_patches.empty())) {
                    result.compatibility_error = true;
                    index += span;
                    continue;
                }

                if (!result.rom_guard_groups.empty() &&
                    equal_condition(result.rom_guard_groups.back().conditions,
                                    group.conditions)) {
                    auto& previous = result.rom_guard_groups.back();
                    previous.writes.insert(previous.writes.end(),
                                           group.writes.begin(),
                                           group.writes.end());
                    previous.rom_patches.insert(previous.rom_patches.end(),
                                                group.rom_patches.begin(),
                                                group.rom_patches.end());
                } else {
                    result.rom_guard_groups.push_back(std::move(group));
                }

                index += span;
                continue;
            }

            Group group;
            bool compatible = true;
            for (const ConditionTerm& term : terms) {
                if (term.width == 0U || term.width > 4U) {
                    warnings.push_back(
                        entry.name +
                        ": EZ-Flash equality term has an unsupported width at "
                        "source line " +
                        std::to_string(operation.source_line));
                    result.compatibility_error = true;
                    compatible = false;
                    break;
                }
                const auto bytes = flatten(term.address, term.value, term.width);
                group.conditions.insert(
                    group.conditions.end(), bytes.begin(), bytes.end());
            }

            for (std::uint32_t offset = 1U;
                 compatible && offset <= span; ++offset) {
                const Operation& action = entry.operations[index + offset];
                if (action.kind != OperationKind::Write) {
                    warnings.push_back(
                        entry.name +
                        ": a runtime IF cannot control ROM or unsupported "
                        "operations at source line " +
                        std::to_string(action.source_line));
                    result.compatibility_error = true;
                    compatible = false;
                    break;
                }
                append_expanded_write(group.writes, action);
            }

            if (!compatible) {
                index += span;
                continue;
            }

            if (!result.conditional_groups.empty() &&
                equal_condition(result.conditional_groups.back().conditions,
                                group.conditions)) {
                auto& previous = result.conditional_groups.back().writes;
                previous.insert(previous.end(),
                                group.writes.begin(), group.writes.end());
            } else {
                result.conditional_groups.push_back(std::move(group));
            }

            index += span;
            continue;
        }

        if (operation.kind == OperationKind::Write) {
            append_expanded_write(result.direct_writes, operation);
            continue;
        }

        if (operation.kind == OperationKind::RomPatch) {
            if (options.mode != Mode::Enhanced ||
                !rom_patch_is_direct_image_write(operation)) {
                warnings.push_back(
                    entry.name +
                    ": ROM patch mode/metadata has no exact EZ-Flash "
                    "Enhanced v3 image-patch mapping at source line " +
                    std::to_string(operation.source_line));
                result.compatibility_error = true;
                return EntryGroups{{}, {}, {}, {}, true};
            }
            Operation normalized = operation;
            normalized.address = *canonical_rom_address(operation.address);
            append_expanded_write(result.direct_rom_patches, normalized);
            continue;
        }

        if (operation.kind == OperationKind::PointerWrite) {
            warnings.push_back(
                entry.name +
                ": Action Replay MAX pointer write has no EZ-Flash Enhanced "
                "v2.1 equivalent at source line " +
                std::to_string(operation.source_line));
            result.compatibility_error = true;
            continue;
        }

        if (operation.kind == OperationKind::DeviceSlowdown) {
            warnings.push_back(
                entry.name +
                ": physical GameShark slowdown control has no EZ-Flash "
                "Enhanced v3 equivalent at source line " +
                std::to_string(operation.source_line));
            result.compatibility_error = true;
            continue;
        }

        warnings.push_back(
            entry.name +
            ": unsupported EZ-Flash Enhanced operation from source line " +
            std::to_string(operation.source_line));
        result.compatibility_error = true;
    }

    return result;
}

std::size_t runtime_records(const Group& group) {
    return group.conditions.size() + group.writes.size() + 1U;
}

std::size_t runtime_records(const RomGuardGroup& group) {
    // ROMIF signature bytes and ROM patch bytes use separate pre-launch
    // tables. Only its runtime ON= bytes consume the 128-record IRQ table.
    return group.writes.size();
}

std::size_t runtime_records(const EntryGroups& groups) {
    std::size_t total = groups.direct_writes.size();
    for (const Group& group : groups.conditional_groups) {
        total += runtime_records(group);
    }
    for (const RomGuardGroup& group : groups.rom_guard_groups) {
        total += runtime_records(group);
    }
    return total;
}

} // namespace gba::ezflash::detail
