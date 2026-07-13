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
namespace {

std::string safe_utf8_prefix(std::string_view value,
                             std::size_t maximum_bytes) {
    std::size_t index = 0U;
    std::size_t accepted = 0U;

    while (index < value.size() && index < maximum_bytes) {
        const unsigned char lead =
            static_cast<unsigned char>(value[index]);
        std::size_t sequence = 1U;
        if ((lead & 0x80U) == 0U) {
            sequence = 1U;
        } else if ((lead & 0xE0U) == 0xC0U) {
            sequence = 2U;
        } else if ((lead & 0xF0U) == 0xE0U) {
            sequence = 3U;
        } else if ((lead & 0xF8U) == 0xF0U) {
            sequence = 4U;
        }

        if (index + sequence > value.size() ||
            index + sequence > maximum_bytes) {
            break;
        }

        bool valid = true;
        for (std::size_t offset = 1U; offset < sequence; ++offset) {
            const unsigned char continuation =
                static_cast<unsigned char>(value[index + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            sequence = 1U;
        }

        index += sequence;
        accepted = index;
    }

    return std::string(value.substr(0U, accepted));
}

std::string sanitize_section_name(std::string_view raw) {
    std::string name = text::trim(raw);
    if (name.empty()) {
        name = "Converted EZ-Flash Code";
    }

    for (char& ch : name) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (byte < 0x20U || ch == '[' || ch == ']' ||
            ch == '\r' || ch == '\n') {
            ch = '_';
        }
    }

    return name;
}

std::string kernel_section_key(std::string_view name) {
    std::string key;
    key.reserve(name.size());
    for (const char ch : name) {
        if (ch != ' ' && ch != '\t') {
            key.push_back(ch);
        }
    }
    return key;
}

} // namespace

SectionNameAllocator::SectionNameAllocator(std::size_t maximum_length)
    : maximum_length_(std::max<std::size_t>(1U, maximum_length)) {}

std::string SectionNameAllocator::make(std::string_view preferred) {
    const std::string base = sanitize_section_name(preferred);

    for (std::size_t index = 1U;; ++index) {
        const std::string suffix =
            index == 1U ? std::string{} : "~" + std::to_string(index);
        const std::size_t prefix_limit =
            suffix.size() >= maximum_length_
                ? 0U
                : maximum_length_ - suffix.size();
        std::string candidate =
            safe_utf8_prefix(base, prefix_limit) + suffix;
        if (candidate.empty()) {
            candidate = safe_utf8_prefix("EZ", maximum_length_);
        }

        const std::string key = kernel_section_key(candidate);
        if (used_.insert(key).second) {
            return candidate;
        }
    }
}

std::vector<std::string> emit_byte_run_tokens(
    const std::vector<ByteWrite>& input,
    bool condition,
    std::vector<std::string>& warnings) {
    constexpr std::size_t kMaximumBytesPerRun = 94U;
    std::vector<std::string> tokens;

    std::size_t index = 0U;
    while (index < input.size()) {
        const auto compact = condition
            ? compact_condition_address(input[index].address)
            : compact_write_address(input[index].address);
        if (!compact) {
            warnings.push_back(
                "EZ cannot map address " + text::hex(input[index].address, 8));
            return {};
        }

        std::ostringstream token;
        token << text::hex(*compact, 1);

        std::uint32_t expected = input[index].address;
        std::size_t count = 0U;
        while (index < input.size() &&
               input[index].address == expected &&
               count < kMaximumBytesPerRun) {
            token << ',' << text::hex(input[index].value, 2);
            ++expected;
            ++index;
            ++count;
        }
        token << ';';
        tokens.push_back(token.str());
    }

    return tokens;
}

std::vector<std::string> emit_rom_byte_run_tokens(
    const std::vector<ByteWrite>& input,
    std::vector<std::string>& warnings) {
    constexpr std::size_t kMaximumBytesPerRun = 94U;
    std::vector<std::string> tokens;

    std::size_t index = 0U;
    while (index < input.size()) {
        const auto canonical = canonical_rom_address(input[index].address);
        if (!canonical) {
            warnings.push_back(
                "EZ-Flash Enhanced cannot map ROM address " +
                text::hex(input[index].address, 8));
            return {};
        }

        std::ostringstream token;
        token << text::hex(*canonical, 8);

        std::uint32_t expected = *canonical;
        std::size_t count = 0U;
        while (index < input.size() && count < kMaximumBytesPerRun) {
            const auto current = canonical_rom_address(input[index].address);
            if (!current || *current != expected) {
                break;
            }
            token << ',' << text::hex(input[index].value, 2);
            ++expected;
            ++index;
            ++count;
        }
        token << ';';
        tokens.push_back(token.str());
    }

    return tokens;
}

std::string join_tokens(const std::vector<std::string>& tokens) {
    std::ostringstream output;
    for (const std::string& token : tokens) {
        output << token;
    }
    return output.str();
}

bool emit_wrapped_key(std::ostringstream& output,
                      std::string_view prefix,
                      const std::vector<std::string>& tokens,
                      std::size_t maximum_line_length) {
    if (tokens.empty() || prefix.size() > maximum_line_length) {
        return false;
    }

    std::string line(prefix);
    for (const std::string& token : tokens) {
        if (token.size() > maximum_line_length) {
            return false;
        }
        if (line.size() + token.size() > maximum_line_length) {
            if (line.size() == prefix.size()) {
                return false;
            }
            output << line << '\n';
            line.clear();
        }
        line += token;
    }

    output << line << '\n';
    return true;
}

std::optional<EncodedGroup> encode_group(
    const Group& group,
    std::vector<std::string>& warnings,
    std::size_t maximum_line_length) {
    const auto condition_tokens =
        emit_byte_run_tokens(group.conditions, true, warnings);
    const auto write_tokens =
        emit_byte_run_tokens(group.writes, false, warnings);
    if (condition_tokens.empty() || write_tokens.empty()) {
        return std::nullopt;
    }

    EncodedGroup encoded;
    encoded.prefix = "IF=" + join_tokens(condition_tokens) + "ON=";
    encoded.write_tokens = write_tokens;
    encoded.full = encoded.prefix + join_tokens(write_tokens);

    // Enhanced v3's menu scanner reads at most 298 visible characters from the
    // first physical line. The complete condition and ON= token must be on
    // that first line; only the final write list may continue below it.
    if (encoded.prefix.size() > maximum_line_length) {
        warnings.push_back(
            "EZ-Flash IF condition is too long for the Enhanced v3 menu scanner");
        return std::nullopt;
    }

    return encoded;
}

void emit_section_header(std::ostringstream& output,
                         SectionNameAllocator& names,
                         std::string_view preferred) {
    output << '[' << names.make(preferred) << "]\n";
}

bool emit_conditional_chunk(
    std::ostringstream& output,
    const std::vector<EncodedGroup>& groups,
    std::size_t maximum_line_length) {
    if (groups.empty()) {
        return false;
    }

    std::string line;
    for (std::size_t index = 0U; index + 1U < groups.size(); ++index) {
        if (line.size() + groups[index].full.size() > maximum_line_length) {
            return false;
        }
        line += groups[index].full;
    }

    const EncodedGroup& last = groups.back();
    if (line.size() + last.prefix.size() > maximum_line_length) {
        return false;
    }
    line += last.prefix;

    for (const std::string& token : last.write_tokens) {
        if (token.size() > maximum_line_length) {
            return false;
        }
        if (line.size() + token.size() > maximum_line_length) {
            output << line << '\n';
            line.clear();
        }
        line += token;
    }

    output << line << '\n';
    return true;
}

void emit_direct_section(std::ostringstream& output,
                         const CheatEntry& entry,
                         const std::vector<ByteWrite>& direct_writes,
                         bool also_has_conditions,
                         std::size_t maximum_line_length,
                         SectionNameAllocator& names,
                         Result& result) {
    if (direct_writes.empty()) {
        return;
    }

    const auto tokens =
        emit_byte_run_tokens(direct_writes, false, result.warnings);
    if (tokens.empty()) {
        result.success = false;
        return;
    }

    std::string name = entry.name;
    if (also_has_conditions) {
        name += " - Direct Writes";
    }

    std::ostringstream section;
    emit_section_header(section, names, name);
    if (!emit_wrapped_key(section, "ON=", tokens, maximum_line_length)) {
        result.warnings.push_back(
            entry.name +
            ": direct write line cannot fit the Enhanced v3 physical-line limit");
        result.success = false;
        return;
    }
    section << '\n';
    output << section.str();
}


bool append_tokens_with_wrapping(std::ostringstream& output,
                                 std::string line,
                                 const std::vector<std::string>& tokens,
                                 std::size_t maximum_line_length) {
    if (line.size() > maximum_line_length) {
        return false;
    }
    for (const std::string& token : tokens) {
        if (token.size() > maximum_line_length) {
            return false;
        }
        if (line.size() + token.size() > maximum_line_length) {
            if (line.empty()) {
                return false;
            }
            output << line << '\n';
            line.clear();
        }
        line += token;
    }
    output << line << '\n';
    return true;
}

void emit_rom_section(std::ostringstream& output,
                      const CheatEntry& entry,
                      const std::vector<ByteWrite>& rom_patches,
                      std::string_view suffix,
                      std::size_t maximum_line_length,
                      SectionNameAllocator& names,
                      Result& result) {
    if (rom_patches.empty()) {
        return;
    }
    const auto tokens = emit_rom_byte_run_tokens(rom_patches, result.warnings);
    if (tokens.empty()) {
        result.success = false;
        return;
    }

    std::ostringstream section;
    emit_section_header(section, names, entry.name + std::string(suffix));
    if (!emit_wrapped_key(section, "ROM=", tokens, maximum_line_length)) {
        result.warnings.push_back(
            entry.name +
            ": ROM patch line cannot fit the Enhanced physical-line limit");
        result.success = false;
        return;
    }
    section << '\n';
    output << section.str();
}

void emit_direct_and_rom_section(std::ostringstream& output,
                                 const CheatEntry& entry,
                                 const std::vector<ByteWrite>& direct_writes,
                                 const std::vector<ByteWrite>& rom_patches,
                                 bool also_has_other_groups,
                                 std::size_t maximum_line_length,
                                 SectionNameAllocator& names,
                                 Result& result) {
    if (direct_writes.empty() || rom_patches.empty()) {
        return;
    }
    const auto write_tokens =
        emit_byte_run_tokens(direct_writes, false, result.warnings);
    const auto rom_tokens =
        emit_rom_byte_run_tokens(rom_patches, result.warnings);
    if (write_tokens.empty() || rom_tokens.empty()) {
        result.success = false;
        return;
    }

    std::string name = entry.name;
    if (also_has_other_groups) {
        name += " - Direct Writes + ROM";
    }
    std::ostringstream section;
    emit_section_header(section, names, name);

    std::string line = "ON=";
    for (const std::string& token : write_tokens) {
        if (line.size() + token.size() > maximum_line_length) {
            section << line << '\n';
            line.clear();
        }
        line += token;
    }

    // ROM: is the Enhanced inline action spelling. It may safely begin a
    // continuation row because it contains no '=' and remains part of ON=.
    const std::string marker = "ROM:";
    if (line.size() + marker.size() > maximum_line_length) {
        section << line << '\n';
        line.clear();
    }
    line += marker;
    if (!append_tokens_with_wrapping(
            section, line, rom_tokens, maximum_line_length)) {
        result.warnings.push_back(
            entry.name +
            ": mixed runtime/ROM entry cannot fit the Enhanced line layout");
        result.success = false;
        return;
    }
    section << '\n';
    output << section.str();
}

bool emit_conditional_groups_with_rom_tail(
    std::ostringstream& output,
    const std::vector<EncodedGroup>& groups,
    const std::vector<std::string>& rom_tokens,
    std::size_t maximum_line_length) {
    if (groups.empty() || rom_tokens.empty()) {
        return false;
    }

    std::string line;
    for (const EncodedGroup& group : groups) {
        line += group.full;
    }
    line += "ROM=";

    // Enhanced v3's IF parser requires the ;ROM= marker to be part of the
    // first physical key row. The ROM byte list itself may continue below it.
    if (line.size() > maximum_line_length) {
        return false;
    }
    return append_tokens_with_wrapping(
        output, std::move(line), rom_tokens, maximum_line_length);
}

void emit_rom_guard_sections(std::ostringstream& output,
                             const CheatEntry& entry,
                             const std::vector<RomGuardGroup>& groups,
                             bool also_has_other_groups,
                             std::size_t maximum_line_length,
                             SectionNameAllocator& names,
                             Result& result) {
    for (std::size_t index = 0U; index < groups.size(); ++index) {
        const RomGuardGroup& group = groups[index];
        const auto condition_tokens =
            emit_rom_byte_run_tokens(group.conditions, result.warnings);
        const auto write_tokens =
            emit_byte_run_tokens(group.writes, false, result.warnings);
        const auto rom_tokens =
            emit_rom_byte_run_tokens(group.rom_patches, result.warnings);
        if (condition_tokens.empty() ||
            (write_tokens.empty() && rom_tokens.empty())) {
            result.success = false;
            continue;
        }

        std::string name = entry.name;
        if (also_has_other_groups) {
            name += " - ROM Guard";
        }
        if (groups.size() > 1U) {
            name += " - Part " + std::to_string(index + 1U);
        }

        std::ostringstream section;
        emit_section_header(section, names, name);
        std::string line = "ROMIF=" + join_tokens(condition_tokens);
        if (!write_tokens.empty()) {
            line += "ON=";
            for (const std::string& token : write_tokens) {
                if (line.size() + token.size() > maximum_line_length) {
                    section << line << '\n';
                    line.clear();
                }
                line += token;
            }
            if (!rom_tokens.empty()) {
                if (line.size() + 4U <= maximum_line_length) {
                    line += "ROM=";
                } else {
                    section << line << '\n';
                    line.clear();
                    // ROM: is the continuation-safe compatibility alias.
                    line += "ROM:";
                }
            }
        } else {
            line += "ROM=";
        }

        if (!append_tokens_with_wrapping(
                section, line, rom_tokens, maximum_line_length)) {
            result.warnings.push_back(
                entry.name +
                ": ROMIF entry cannot fit the Enhanced physical-line layout");
            result.success = false;
            continue;
        }
        section << '\n';
        output << section.str();
    }
}

void emit_conditional_sections(std::ostringstream& output,
                               const CheatEntry& entry,
                               const std::vector<Group>& source_groups,
                               bool also_has_direct_writes,
                               const Options& options,
                               std::size_t maximum_line_length,
                               SectionNameAllocator& names,
                               Result& result) {
    if (source_groups.empty()) {
        return;
    }

    std::vector<EncodedGroup> encoded_groups;
    encoded_groups.reserve(source_groups.size());
    for (const Group& group : source_groups) {
        const auto encoded =
            encode_group(group, result.warnings, maximum_line_length);
        if (!encoded) {
            result.success = false;
            continue;
        }
        encoded_groups.push_back(*encoded);
    }

    std::vector<std::vector<EncodedGroup>> chunks;
    std::vector<EncodedGroup> current;
    std::size_t current_full_length = 0U;

    for (const EncodedGroup& group : encoded_groups) {
        const bool combine = options.combine_multiple_if_groups;
        if (!current.empty() &&
            (!combine ||
             current_full_length > maximum_line_length ||
             current_full_length + group.full.size() >
                 maximum_line_length)) {
            chunks.push_back(std::move(current));
            current.clear();
            current_full_length = 0U;
        }

        current.push_back(group);
        current_full_length += group.full.size();

        // A group whose writes need continuation must remain the final and
        // only continuation-bearing group in its section.
        if (current_full_length > maximum_line_length) {
            chunks.push_back(std::move(current));
            current.clear();
            current_full_length = 0U;
        } else if (!combine) {
            chunks.push_back(std::move(current));
            current.clear();
            current_full_length = 0U;
        }
    }

    if (!current.empty()) {
        chunks.push_back(std::move(current));
    }

    for (std::size_t index = 0U; index < chunks.size(); ++index) {
        std::string name = entry.name;
        if (also_has_direct_writes) {
            name += " - Conditions";
        }
        if (chunks.size() > 1U) {
            name += " - Part " + std::to_string(index + 1U);
        }

        std::ostringstream section;
        emit_section_header(section, names, name);
        if (!emit_conditional_chunk(
                section, chunks[index], maximum_line_length)) {
            result.warnings.push_back(
                entry.name +
                ": conditional group cannot fit the Enhanced v3 physical-line "
                "layout");
            result.success = false;
            continue;
        }
        section << '\n';
        output << section.str();
    }
}

} // namespace gba::ezflash::detail
