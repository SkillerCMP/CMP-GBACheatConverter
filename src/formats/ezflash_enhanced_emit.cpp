#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {

bool emit_enhanced_segments(std::ostringstream& output,
                            std::string first,
                            const std::vector<std::string>& continuation,
                            std::size_t maximum_line_length) {
    if (first.empty() || first.size() > maximum_line_length) return false;
    std::string line = std::move(first);
    for (const std::string& segment : continuation) {
        if (segment.empty() || segment.size() > maximum_line_length ||
            segment.find('=') != std::string::npos) {
            return false;
        }
        if (line.size() + segment.size() > maximum_line_length) {
            output << line << '\n';
            line = segment;
        } else {
            line += segment;
        }
    }
    output << line << '\n';
    return true;
}

std::optional<std::pair<std::string, std::vector<std::string>>>
encode_action_sequence(const std::vector<Operation>& actions,
                       std::vector<std::string>& warnings,
                       std::string_view entry_name,
                       bool use_equals_for_first) {
    if (actions.empty()) return std::nullopt;
    std::vector<EnhancedActionSegment> encoded_actions;

    for (std::size_t index = 0U; index < actions.size();) {
        const Operation& operation = actions[index];
        const std::vector<std::uint8_t> first_bytes =
            operation_payload(operation);
        if (operation.kind == OperationKind::Write &&
            (operation.repeat == 0U || operation.repeat == 1U) &&
            !first_bytes.empty()) {
            const auto compact = compact_write_address(operation.address);
            if (!compact) return std::nullopt;
            std::vector<std::uint8_t> merged = first_bytes;
            std::uint32_t expected = operation.address +
                static_cast<std::uint32_t>(first_bytes.size());
            std::size_t next = index + 1U;
            while (next < actions.size()) {
                const Operation& candidate = actions[next];
                const std::vector<std::uint8_t> candidate_bytes =
                    operation_payload(candidate);
                if (candidate.kind != OperationKind::Write ||
                    (candidate.repeat != 0U && candidate.repeat != 1U) ||
                    candidate.address != expected || candidate_bytes.empty()) {
                    break;
                }
                merged.insert(merged.end(), candidate_bytes.begin(),
                              candidate_bytes.end());
                expected += static_cast<std::uint32_t>(candidate_bytes.size());
                ++next;
            }
            constexpr std::size_t kMaximumBytesPerRun = 94U;
            std::size_t byte_offset = 0U;
            while (byte_offset < merged.size()) {
                const std::size_t count = std::min(
                    kMaximumBytesPerRun, merged.size() - byte_offset);
                std::vector<std::uint8_t> chunk(
                    merged.begin() + static_cast<std::ptrdiff_t>(byte_offset),
                    merged.begin() + static_cast<std::ptrdiff_t>(byte_offset + count));
                const std::string payload = text::hex(
                    *compact + static_cast<std::uint32_t>(byte_offset), 1) +
                    byte_list_suffix(chunk);
                encoded_actions.push_back(EnhancedActionSegment{
                    "ON=" + payload, "ON:" + payload, count, true});
                byte_offset += count;
            }
            index = next;
            continue;
        }

        const auto encoded = encode_enhanced_action(
            operation, warnings, entry_name);
        if (!encoded) return std::nullopt;
        encoded_actions.push_back(*encoded);
        ++index;
    }

    std::string first = use_equals_for_first
        ? encoded_actions.front().equals_form
        : encoded_actions.front().colon_form;
    std::vector<std::string> rest;
    bool write_context = encoded_actions.front().raw_write;
    for (std::size_t index = 1U; index < encoded_actions.size(); ++index) {
        const EnhancedActionSegment& encoded = encoded_actions[index];
        if (encoded.raw_write && write_context &&
            encoded.colon_form.rfind("ON:", 0U) == 0U) {
            rest.push_back(encoded.colon_form.substr(3U));
        } else {
            rest.push_back(encoded.colon_form);
        }
        write_context = encoded.raw_write;
    }
    return std::make_pair(std::move(first), std::move(rest));
}

Result export_enhanced_v3(const CheatDocument& document,
                          const Options& options) {
    Result result;
    result.warnings = document.warnings;
    std::ostringstream output;
    const std::size_t runtime_limit = std::max<std::size_t>(
        1U, std::min(options.maximum_runtime_records,
                     kEnhancedRuntimeRecordLimit));
    const std::size_t section_limit = std::max<std::size_t>(
        8U, std::min(options.maximum_section_name_length,
                     kEnhancedSectionNameLimit));
    const std::size_t line_limit = std::max<std::size_t>(
        16U, std::min(options.maximum_physical_line_length,
                      kEnhancedPhysicalLineLimit));
    SectionNameAllocator names(section_limit);
    std::size_t shared_records = 0U;

    for (const CheatEntry& entry : document.entries) {
        const EnhancedEntryPlan plan =
            build_enhanced_v3_plan(entry, result.warnings);
        if (plan.compatibility_error) result.success = false;
        const std::size_t records = enhanced_plan_records(plan);
        if (records > runtime_limit) {
            result.warnings.push_back(entry.name + ": requires " +
                std::to_string(records) +
                " Enhanced v3 runtime records; the complete entry was skipped");
            result.success = false;
            continue;
        }
        shared_records += records;

        const bool multiple_parts =
            (!plan.direct_actions.empty() || !plan.direct_rom_patches.empty()) +
            plan.conditions.size() + plan.rom_guards.size() > 1U;
        bool rom_consumed = false;

        if (!plan.direct_actions.empty()) {
            const auto sequence = encode_action_sequence(
                plan.direct_actions, result.warnings, entry.name, true);
            if (sequence) {
                std::string name = entry.name;
                if (multiple_parts) name += " - Direct";
                std::ostringstream section;
                emit_section_header(section, names, name);
                std::string first = sequence->first;
                std::vector<std::string> rest = sequence->second;
                if (!plan.direct_rom_patches.empty()) {
                    const auto rom_tokens = emit_rom_byte_run_tokens(
                        plan.direct_rom_patches, result.warnings);
                    if (!rom_tokens.empty()) {
                        rest.push_back("ROM:" + rom_tokens.front());
                        rest.insert(rest.end(), rom_tokens.begin() + 1U,
                                    rom_tokens.end());
                        rom_consumed = true;
                    }
                }
                if (!emit_enhanced_segments(
                        section, std::move(first), rest, line_limit)) {
                    result.warnings.push_back(entry.name +
                        ": direct Enhanced v3 option exceeds the line layout");
                    result.success = false;
                } else {
                    section << '\n';
                    output << section.str();
                }
            }
        }

        bool conditions_consumed = false;
        if (options.combine_multiple_if_groups &&
            !plan.conditions.empty() && plan.direct_actions.empty() &&
            plan.rom_guards.empty()) {
            bool can_combine = true;
            std::string combined;
            for (const EnhancedConditionBlock& block : plan.conditions) {
                if (block.condition.condition_has_else ||
                    !block.false_actions.empty()) {
                    can_combine = false;
                    break;
                }
                const auto condition_bytes =
                    flatten_condition_operation(block.condition);
                const auto condition_tokens = condition_bytes
                    ? emit_byte_run_tokens(*condition_bytes, true,
                                           result.warnings)
                    : std::vector<std::string>{};
                const auto sequence = encode_action_sequence(
                    block.true_actions, result.warnings, entry.name, true);
                if (condition_tokens.empty() || !sequence) {
                    can_combine = false;
                    break;
                }
                combined += std::string(
                    condition_key_for_kind(block.condition.kind)) + "=" +
                    join_tokens(condition_tokens) + sequence->first;
                for (const std::string& segment : sequence->second) {
                    combined += segment;
                }
            }
            if (can_combine && !plan.direct_rom_patches.empty()) {
                const auto rom_tokens = emit_rom_byte_run_tokens(
                    plan.direct_rom_patches, result.warnings);
                if (rom_tokens.empty()) {
                    can_combine = false;
                } else {
                    combined += "ROM=" + join_tokens(rom_tokens);
                    rom_consumed = true;
                }
            }
            if (can_combine && combined.size() <= line_limit) {
                std::ostringstream section;
                emit_section_header(section, names, entry.name);
                section << combined << '\n' << '\n';
                output << section.str();
                conditions_consumed = true;
            }
        }

        for (std::size_t block_index = 0U;
             !conditions_consumed && block_index < plan.conditions.size();
             ++block_index) {
            const EnhancedConditionBlock& block =
                plan.conditions[block_index];
            const auto condition_bytes =
                flatten_condition_operation(block.condition);
            const auto condition_tokens = condition_bytes
                ? emit_byte_run_tokens(*condition_bytes, true, result.warnings)
                : std::vector<std::string>{};
            const auto true_sequence = encode_action_sequence(
                block.true_actions, result.warnings, entry.name, true);
            if (condition_tokens.empty() || !true_sequence) {
                result.success = false;
                continue;
            }
            std::string name = entry.name;
            if (!options.combine_multiple_if_groups &&
                plan.conditions.size() > 1U) {
                name += " - Part " + std::to_string(block_index + 1U);
            } else {
                if (multiple_parts) name += " - Condition";
                if (plan.conditions.size() > 1U)
                    name += " " + std::to_string(block_index + 1U);
            }
            std::ostringstream section;
            emit_section_header(section, names, name);
            std::string first = std::string(
                condition_key_for_kind(block.condition.kind)) + "=" +
                join_tokens(condition_tokens) + true_sequence->first;
            std::vector<std::string> rest = true_sequence->second;
            if (block.condition.condition_has_else ||
                !block.false_actions.empty()) {
                const auto false_sequence = encode_action_sequence(
                    block.false_actions, result.warnings, entry.name, false);
                if (!false_sequence) {
                    result.success = false;
                    continue;
                }
                rest.push_back("ELSE;" + false_sequence->first);
                rest.insert(rest.end(), false_sequence->second.begin(),
                            false_sequence->second.end());
            }
            rest.push_back("ENDIF;");
            if (plan.conditions.size() == 1U &&
                plan.direct_actions.empty() &&
                plan.rom_guards.empty() &&
                !plan.direct_rom_patches.empty()) {
                const auto rom_tokens = emit_rom_byte_run_tokens(
                    plan.direct_rom_patches, result.warnings);
                if (!rom_tokens.empty()) {
                    rest.push_back("ROM:" + rom_tokens.front());
                    rest.insert(rest.end(), rom_tokens.begin() + 1U,
                                rom_tokens.end());
                    rom_consumed = true;
                }
            }
            if (!emit_enhanced_segments(
                    section, std::move(first), rest, line_limit)) {
                result.warnings.push_back(entry.name +
                    ": condition cannot fit the Enhanced v3 physical-line layout");
                result.success = false;
            } else {
                section << '\n';
                output << section.str();
            }
        }

        for (std::size_t guard_index = 0U;
             guard_index < plan.rom_guards.size(); ++guard_index) {
            const EnhancedRomGuardBlock& guard = plan.rom_guards[guard_index];
            const auto condition_tokens = emit_rom_byte_run_tokens(
                guard.conditions, result.warnings);
            if (condition_tokens.empty()) {
                result.success = false;
                continue;
            }
            std::string name = entry.name;
            if (multiple_parts) name += " - ROM Guard";
            if (plan.rom_guards.size() > 1U)
                name += " " + std::to_string(guard_index + 1U);
            std::ostringstream section;
            emit_section_header(section, names, name);
            std::string first = "ROMIF=" + join_tokens(condition_tokens);
            std::vector<std::string> rest;
            if (!guard.runtime_actions.empty()) {
                const auto sequence = encode_action_sequence(
                    guard.runtime_actions, result.warnings, entry.name, true);
                if (!sequence) {
                    result.success = false;
                    continue;
                }
                first += sequence->first;
                rest = sequence->second;
            }
            if (!guard.rom_patches.empty()) {
                const auto rom_tokens = emit_rom_byte_run_tokens(
                    guard.rom_patches, result.warnings);
                if (rom_tokens.empty()) {
                    result.success = false;
                    continue;
                }
                if (rest.empty() &&
                    first.size() + 4U + rom_tokens.front().size() <= line_limit) {
                    first += "ROM=" + rom_tokens.front();
                } else {
                    rest.push_back("ROM:" + rom_tokens.front());
                }
                rest.insert(rest.end(), rom_tokens.begin() + 1U,
                            rom_tokens.end());
            }
            if (!emit_enhanced_segments(
                    section, std::move(first), rest, line_limit)) {
                result.warnings.push_back(entry.name +
                    ": ROMIF option cannot fit the Enhanced v3 line layout");
                result.success = false;
            } else {
                section << '\n';
                output << section.str();
            }
        }

        if (!rom_consumed && !plan.direct_rom_patches.empty()) {
            const auto rom_tokens = emit_rom_byte_run_tokens(
                plan.direct_rom_patches, result.warnings);
            if (!rom_tokens.empty()) {
                std::string name = entry.name;
                if (multiple_parts) name += " - ROM Patch";
                std::ostringstream section;
                emit_section_header(section, names, name);
                if (!emit_wrapped_key(section, "ROM=", rom_tokens, line_limit)) {
                    result.success = false;
                } else {
                    section << '\n';
                    output << section.str();
                }
            }
        }
    }

    if (shared_records > kEnhancedRuntimeRecordLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced v3 uses one shared 128-record table for runtime operations. "
            "Enabling every exported entry together would require " +
            std::to_string(shared_records) + " records");
    }
    result.text = output.str();
    return result;
}

} // namespace gba::ezflash::detail
