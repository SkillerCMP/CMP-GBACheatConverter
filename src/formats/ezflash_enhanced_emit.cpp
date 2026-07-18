#include "formats/ezflash_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gba::ezflash::detail {
namespace {

std::string safe_utf8_prefix(std::string_view value,
                             std::size_t maximum_bytes) {
    std::size_t index = 0U;
    std::size_t accepted = 0U;
    while (index < value.size() && index < maximum_bytes) {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        std::size_t sequence = 1U;
        if ((lead & 0xE0U) == 0xC0U) sequence = 2U;
        else if ((lead & 0xF0U) == 0xE0U) sequence = 3U;
        else if ((lead & 0xF8U) == 0xF0U) sequence = 4U;
        if (index + sequence > value.size() ||
            index + sequence > maximum_bytes) break;
        bool valid = true;
        for (std::size_t offset = 1U; offset < sequence; ++offset) {
            if ((static_cast<unsigned char>(value[index + offset]) & 0xC0U) !=
                0x80U) {
                valid = false;
                break;
            }
        }
        if (!valid) sequence = 1U;
        index += sequence;
        accepted = index;
    }
    return std::string(value.substr(0U, accepted));
}

std::string sanitize_option_name(std::string_view raw,
                                 std::size_t maximum_bytes) {
    std::string name = text::trim(raw);
    if (name.empty()) name = "ON";
    for (char& character : name) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || character == '=' || character == '[' ||
            character == ']' || character == '#' || character == '\r' ||
            character == '\n') {
            character = '_';
        }
    }
    return safe_utf8_prefix(name, maximum_bytes);
}

std::string unique_option_name(std::string_view preferred,
                               std::size_t maximum_bytes,
                               std::unordered_set<std::string>& used) {
    const std::string base = sanitize_option_name(preferred, maximum_bytes);
    for (std::size_t index = 1U;; ++index) {
        const std::string suffix = index == 1U
            ? std::string{} : "~" + std::to_string(index);
        const std::size_t prefix_limit = suffix.size() >= maximum_bytes
            ? 0U : maximum_bytes - suffix.size();
        std::string candidate = safe_utf8_prefix(base, prefix_limit) + suffix;
        if (candidate.empty()) candidate = "ON";
        if (used.insert(candidate).second) return candidate;
    }
}


struct EntryDisplayMetadata {
    std::string name;
    std::string credits;
};

EntryDisplayMetadata display_metadata(const CheatEntry& entry,
                                      std::string_view preferred_name) {
    EntryDisplayMetadata metadata;
    metadata.name = text::trim(preferred_name);
    metadata.credits = text::trim(entry.credits);

    const std::string by_marker = " , by ";
    const std::string crypt_marker = " , Crypt_";
    const std::size_t by = metadata.name.find(by_marker);
    const std::size_t crypt = metadata.name.find(crypt_marker);

    if (by != std::string::npos) {
        if (metadata.credits.empty()) {
            const std::size_t credit_start = by + by_marker.size();
            const std::size_t credit_end =
                crypt != std::string::npos && crypt > credit_start
                    ? crypt : metadata.name.size();
            metadata.credits = text::trim(std::string_view(metadata.name).substr(
                credit_start, credit_end - credit_start));
        }
        metadata.name = text::trim(
            std::string_view(metadata.name).substr(0U, by));
    } else if (crypt != std::string::npos) {
        metadata.name = text::trim(
            std::string_view(metadata.name).substr(0U, crypt));
    }

    if (metadata.name.empty()) metadata.name = "ON";
    return metadata;
}

std::string sanitize_credit(std::string_view raw,
                            std::size_t maximum_line_length,
                            bool& truncated) {
    std::string credit = text::trim(raw);
    for (char& character : credit) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || character == '\r' || character == '\n') {
            character = ' ';
        }
    }
    constexpr std::string_view prefix = "// Credits: ";
    const std::size_t limit = maximum_line_length > prefix.size()
        ? maximum_line_length - prefix.size() : 0U;
    const std::string clipped = safe_utf8_prefix(credit, limit);
    truncated = clipped.size() != credit.size();
    return clipped;
}

void emit_credit_comment(std::ostringstream& output,
                         std::string_view credits,
                         std::size_t maximum_line_length,
                         std::string_view entry_name,
                         std::vector<std::string>& warnings) {
    if (text::trim(credits).empty()) return;
    bool truncated = false;
    const std::string clean = sanitize_credit(
        credits, maximum_line_length, truncated);
    if (clean.empty()) return;
    output << "// Credits: " << clean << '\n';
    if (truncated) {
        warnings.push_back(std::string(entry_name) +
            ": EZ-Flash E7 credit comment was truncated to the physical-line limit");
    }
}

bool emit_option(std::ostringstream& output,
                 std::string_view option_name,
                 const std::vector<std::string>& commands,
                 std::size_t maximum_line_length) {
    std::string line = std::string(option_name) + '=';
    if (line.size() > maximum_line_length || commands.empty()) return false;

    for (const std::string& command : commands) {
        if (command.empty() || command.size() > maximum_line_length ||
            command.find('=') != std::string::npos) {
            return false;
        }
        if (line.size() + command.size() > maximum_line_length) {
            output << line << '\n';
            line.clear();
        }
        line += command;
    }
    if (line.empty()) return false;
    output << line << '\n';
    return true;
}

struct PendingOption {
    const CheatEntry* entry = nullptr;
    EnhancedEncodedOption encoded;
    std::string display_name;
    std::string credits;
};

struct PendingGroup {
    std::string preferred_name;
    EzFlashGroupMode mode = EzFlashGroupMode::MultiSelect;
    std::vector<PendingOption> options;
};

} // namespace

Result export_enhanced_v4(const CheatDocument& document,
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

    if (options.maximum_runtime_records > kEnhancedRuntimeRecordLimit) {
        result.warnings.push_back(
            "EZ-Flash Enhanced E7 uses a fixed 128-record runtime table; "
            "the requested larger limit was clamped");
    }
    if (options.maximum_section_name_length > kEnhancedSectionNameLimit) {
        result.warnings.push_back(
            "EZ-Flash E7 group and code names are limited to 49 bytes; "
            "the requested larger limit was clamped");
    }
    if (options.maximum_physical_line_length > kEnhancedPhysicalLineLimit) {
        result.warnings.push_back(
            "EZ-Flash E7 database rows are limited to 298 visible characters; "
            "the requested larger limit was clamped");
    }

    std::vector<PendingOption> standalone;
    std::vector<PendingGroup> groups;
    std::unordered_map<std::string, std::size_t> grouped_indices;
    bool saw_grouped_entry = false;
    bool reordered_standalone = false;

    for (const CheatEntry& entry : document.entries) {
        const auto encoded = encode_enhanced_v4_option(entry, result.warnings);
        if (!encoded) {
            result.warnings.push_back(
                entry.name + ": complete EZ-Flash E7 code was skipped");
            result.success = false;
            continue;
        }
        if (encoded->runtime_records > runtime_limit) {
            result.warnings.push_back(
                entry.name + ": requires " +
                std::to_string(encoded->runtime_records) +
                " EZ-Flash E7 runtime records; the complete code was skipped");
            result.success = false;
            continue;
        }
        if (encoded->runtime_write_work > kEnhancedRuntimeWriteWorkLimit) {
            result.warnings.push_back(
                entry.name + ": performs " +
                std::to_string(encoded->runtime_write_work) +
                " runtime writes per pass, exceeding the safe E7 budget of " +
                std::to_string(kEnhancedRuntimeWriteWorkLimit) +
                "; the complete code was skipped");
            result.success = false;
            continue;
        }

        const bool explicitly_grouped = !entry.ezflash_group_name.empty();
        const std::string_view preferred_name = explicitly_grouped &&
                !entry.ezflash_option_name.empty()
            ? std::string_view(entry.ezflash_option_name)
            : std::string_view(entry.name);
        EntryDisplayMetadata metadata = display_metadata(entry, preferred_name);
        if (!explicitly_grouped) {
            if (saw_grouped_entry) reordered_standalone = true;
            standalone.push_back(PendingOption{
                &entry, *encoded, std::move(metadata.name),
                std::move(metadata.credits)});
            continue;
        }

        saw_grouped_entry = true;
        const EzFlashGroupMode mode =
            entry.ezflash_group_mode == EzFlashGroupMode::ZeroOrOne
                ? EzFlashGroupMode::ZeroOrOne
                : EzFlashGroupMode::MultiSelect;
        const std::string key = entry.ezflash_group_name +
            (mode == EzFlashGroupMode::ZeroOrOne ? "\x1fONE" : "\x1fMULTI");
        const auto found = grouped_indices.find(key);
        std::size_t group_index = 0U;
        if (found == grouped_indices.end()) {
            group_index = groups.size();
            grouped_indices.emplace(key, group_index);
            groups.push_back(PendingGroup{
                entry.ezflash_group_name, mode, {}});
        } else {
            group_index = found->second;
        }
        groups[group_index].options.push_back(PendingOption{
            &entry, *encoded, std::move(metadata.name),
            std::move(metadata.credits)});
    }

    if (reordered_standalone) {
        result.warnings.push_back(
            "EZ-Flash E7 permits standalone rows only before the first group; "
            "standalone codes were moved ahead of grouped codes");
    }

    std::unordered_set<std::string> used_standalone_names;
    for (const PendingOption& pending : standalone) {
        const CheatEntry& entry = *pending.entry;
        emit_credit_comment(output, pending.credits, line_limit,
                            entry.name, result.warnings);
        const std::string code_name = unique_option_name(
            pending.display_name, section_limit, used_standalone_names);
        if (!emit_option(output, code_name, pending.encoded.commands,
                         line_limit)) {
            result.warnings.push_back(
                entry.name +
                ": standalone code cannot fit the EZ-Flash E7 physical-line layout");
            result.success = false;
        }
    }
    if (!standalone.empty() && !groups.empty()) output << '\n';

    SectionNameAllocator section_names(section_limit);
    for (PendingGroup& group : groups) {
        const bool one = group.mode == EzFlashGroupMode::ZeroOrOne;
        const std::string visible_name = section_names.make(
            group.preferred_name, one ? 4U : 0U);
        output << '[' << visible_name;
        if (one) output << "|ONE";
        output << "]\n";

        std::unordered_set<std::string> used_options;
        for (const PendingOption& pending : group.options) {
            const CheatEntry& entry = *pending.entry;
            emit_credit_comment(output, pending.credits, line_limit,
                                entry.name, result.warnings);
            const std::string option_name = unique_option_name(
                pending.display_name, section_limit, used_options);
            if (!emit_option(output, option_name, pending.encoded.commands,
                             line_limit)) {
                result.warnings.push_back(
                    entry.name +
                    ": grouped code cannot fit the EZ-Flash E7 physical-line layout");
                result.success = false;
            }
        }
        output << '\n';
    }

    // Every standalone row and every sibling in a plain [Group] can be active
    // together. Only [Group|ONE] contributes its largest single sibling.
    std::size_t selectable_records = 0U;
    std::size_t selectable_write_work = 0U;
    for (const PendingOption& pending : standalone) {
        selectable_records += pending.encoded.runtime_records;
        selectable_write_work += pending.encoded.runtime_write_work;
    }
    for (const PendingGroup& group : groups) {
        if (group.mode == EzFlashGroupMode::ZeroOrOne) {
            std::size_t group_records = 0U;
            std::size_t group_write_work = 0U;
            for (const PendingOption& pending : group.options) {
                group_records = std::max(
                    group_records, pending.encoded.runtime_records);
                group_write_work = std::max(
                    group_write_work, pending.encoded.runtime_write_work);
            }
            selectable_records += group_records;
            selectable_write_work += group_write_work;
        } else {
            for (const PendingOption& pending : group.options) {
                selectable_records += pending.encoded.runtime_records;
                selectable_write_work += pending.encoded.runtime_write_work;
            }
        }
    }
    if (selectable_records > runtime_limit) {
        result.warnings.push_back(
            "EZ-Flash E7 shares one " + std::to_string(runtime_limit) +
            "-record runtime table across all selected standalone and grouped "
            "codes. The largest selectable combination requires " +
            std::to_string(selectable_records) + " records");
    }
    if (selectable_write_work > kEnhancedRuntimeWriteWorkLimit) {
        result.warnings.push_back(
            "The largest selectable EZ-Flash E7 combination performs " +
            std::to_string(selectable_write_work) +
            " runtime writes per pass; keep the active combination at " +
            std::to_string(kEnhancedRuntimeWriteWorkLimit) + " or fewer");
    }

    result.text = output.str();
    return result;
}

} // namespace gba::ezflash::detail
