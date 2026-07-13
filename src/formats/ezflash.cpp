#include "formats/ezflash.hpp"

#include "formats/ezflash_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::ezflash {

using namespace detail;

Result export_document(const CheatDocument& document, const Options& options) {
    if (options.mode == Mode::Enhanced) {
        return export_enhanced_v3(document, options);
    }

    Result result;
    result.warnings = document.warnings;
    std::ostringstream output;

    const std::size_t runtime_limit =
        std::max<std::size_t>(
            1U,
            std::min(options.maximum_runtime_records,
                     kEnhancedRuntimeRecordLimit));
    const std::size_t section_name_limit =
        std::max<std::size_t>(
            8U,
            std::min(options.maximum_section_name_length,
                     kEnhancedSectionNameLimit));
    const std::size_t physical_line_limit =
        std::max<std::size_t>(
            16U,
            std::min(options.maximum_physical_line_length,
                     kEnhancedPhysicalLineLimit));

    if (options.maximum_runtime_records > kEnhancedRuntimeRecordLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced v3 retains the hard 128-record runtime "
            "table; the requested larger limit was clamped to 128");
    }
    if (options.maximum_section_name_length > kEnhancedSectionNameLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced v3 section names are limited to 49 bytes; "
            "the requested larger limit was clamped");
    }
    if (options.maximum_physical_line_length > kEnhancedPhysicalLineLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced v3 menu lines are limited to 298 visible "
            "characters; the requested larger limit was clamped");
    }

    SectionNameAllocator names(section_name_limit);
    std::size_t total_exported_records = 0U;

    for (const CheatEntry& entry : document.entries) {
        const EntryGroups groups =
            build_groups(entry, options, result.warnings);
        if (groups.compatibility_error) {
            result.success = false;
        }
        if (groups.direct_writes.empty() &&
            groups.direct_rom_patches.empty() &&
            groups.conditional_groups.empty() &&
            groups.rom_guard_groups.empty()) {
            result.warnings.push_back(
                entry.name + ": no EZ-compatible operations");
            continue;
        }

        const std::size_t required_records = runtime_records(groups);
        if (required_records > runtime_limit) {
            result.warnings.push_back(
                entry.name + ": requires " +
                std::to_string(required_records) +
                " EZ runtime records, but Enhanced v3 can activate at most " +
                std::to_string(runtime_limit) +
                "; the complete entry was not exported");
            result.success = false;
            continue;
        }
        total_exported_records += required_records;

        const bool has_conditions = !groups.conditional_groups.empty();
        const bool has_rom_guards = !groups.rom_guard_groups.empty();
        bool direct_rom_handled = false;

        if (options.mode == Mode::Enhanced &&
            !groups.direct_writes.empty() &&
            !groups.direct_rom_patches.empty()) {
            emit_direct_and_rom_section(
                output, entry, groups.direct_writes,
                groups.direct_rom_patches,
                has_conditions || has_rom_guards,
                physical_line_limit, names, result);
            direct_rom_handled = true;
        } else {
            emit_direct_section(
                output, entry, groups.direct_writes,
                has_conditions || has_rom_guards ||
                    !groups.direct_rom_patches.empty(),
                physical_line_limit, names, result);
        }

        bool condition_rom_combined = false;
        if (options.mode == Mode::Enhanced &&
            !groups.conditional_groups.empty() &&
            !groups.direct_rom_patches.empty() &&
            groups.direct_writes.empty() &&
            groups.rom_guard_groups.empty()) {
            std::vector<EncodedGroup> encoded;
            for (const Group& group : groups.conditional_groups) {
                const auto value = encode_group(
                    group, result.warnings, physical_line_limit);
                if (!value) {
                    encoded.clear();
                    break;
                }
                encoded.push_back(*value);
            }
            const auto rom_tokens = emit_rom_byte_run_tokens(
                groups.direct_rom_patches, result.warnings);
            if (!encoded.empty() && !rom_tokens.empty()) {
                std::ostringstream section;
                emit_section_header(section, names, entry.name);
                if (emit_conditional_groups_with_rom_tail(
                        section, encoded, rom_tokens,
                        physical_line_limit)) {
                    section << '\n';
                    output << section.str();
                    condition_rom_combined = true;
                    direct_rom_handled = true;
                } else {
                    result.warnings.push_back(
                        entry.name +
                        ": IF/ON/ROM could not fit one Enhanced option; "
                        "conditions and ROM patch were emitted as separate "
                        "menu sections");
                }
            }
        }

        if (options.mode == Mode::Enhanced && !condition_rom_combined) {
            emit_conditional_sections(
                output, entry, groups.conditional_groups,
                !groups.direct_writes.empty() ||
                    !groups.direct_rom_patches.empty() || has_rom_guards,
                options, physical_line_limit, names, result);
        }

        if (options.mode == Mode::Enhanced) {
            emit_rom_guard_sections(
                output, entry, groups.rom_guard_groups,
                !groups.direct_writes.empty() || has_conditions ||
                    !groups.direct_rom_patches.empty(),
                physical_line_limit, names, result);

            if (!direct_rom_handled &&
                !groups.direct_rom_patches.empty()) {
                emit_rom_section(
                    output, entry, groups.direct_rom_patches,
                    (groups.direct_writes.empty() && !has_conditions &&
                     !has_rom_guards)
                        ? std::string_view{}
                        : std::string_view{" - ROM Patch"},
                    physical_line_limit, names, result);
            }
        }
    }

    if (total_exported_records > kEnhancedRuntimeRecordLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced v3 uses one shared 128-record table for runtime operations "
            "for all enabled menu entries. Enabling every exported entry "
            "together would require " +
            std::to_string(total_exported_records) +
            " records; keep the active combination at 128 or fewer");
    }

    result.text = output.str();
    return result;
}


} // namespace gba::ezflash
