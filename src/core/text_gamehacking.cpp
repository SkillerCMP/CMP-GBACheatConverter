#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace gba::text {

namespace {

std::string lower_ascii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

std::string compact_label_key(std::string_view value) {
    std::string key;
    for (const char ch : lower_ascii(trim(value))) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            key.push_back(ch);
        }
    }
    return key;
}

std::optional<std::string> canonical_crypt_label(std::string_view raw) {
    const std::string key = compact_label_key(raw);

    if (key == "codebreaker/gamesharksp/xploder" ||
        key == "codebreaker/gamesharksp/xploderadvance" ||
        key == "codebreaker-gamesharksp-xploder") {
        return std::string("Codebreaker/GameShark SP/Xploder");
    }

    if (key == "gamesharkadvance/actionreplaygbx" ||
        key == "gamesharkadvance-actionreplaygbx") {
        return std::string("GameShark Advance/Action Replay GBX");
    }

    // GameHacking.org uses the shorter device label without the GBX suffix.
    // Keep that public label intact while mapping it to the same Datel family.
    if (key == "gamesharkadvance/actionreplay" ||
        key == "gamesharkadvance-actionreplay" ||
        key == "actionreplay/gamesharkadvance" ||
        key == "actionreplay-gamesharkadvance") {
        return std::string("GameShark Advance/Action Replay");
    }

    if (key == "actionreplaymax" ||
        key == "proactionreplayv3" ||
        key == "parv3") {
        return std::string("Action Replay MAX");
    }

    if (key == "ez-flash" ||
        key == "ezflash") {
        return std::string("EZ-Flash");
    }

    return std::nullopt;
}

bool is_code_row(std::string_view line) {
    return is_code_line_8x4(line) || is_code_line_8x8(line);
}

bool is_legacy_gamehacking_separator(std::string_view raw) {
    const std::string line = trim(raw);
    if (line.size() < 5U) {
        return false;
    }

    return std::all_of(
        line.begin(), line.end(),
        [](const char ch) { return ch == '_'; });
}

std::string normalize_credit_suffix(std::string heading) {
    heading = trim(heading);
    if (heading.find(" , by ") != std::string::npos ||
        heading.find(" , Crypt_") != std::string::npos) {
        return heading;
    }

    const std::string lower = lower_ascii(heading);
    const std::size_t position = lower.rfind(" by ");
    if (position == std::string::npos) {
        return heading;
    }

    const std::string name = trim(
        std::string_view(heading).substr(0U, position));
    const std::string author = trim(
        std::string_view(heading).substr(position + 4U));

    if (name.empty() || author.empty()) {
        return heading;
    }

    return name + " , by " + author;
}

} // namespace

std::string cleanup_gamehacking_org_blocks(std::string_view value) {
    const std::string compacted =
        format_compact_code_lines(strip_utf8_bom(value));
    const std::vector<std::string> lines = split_lines(compacted);

    std::vector<std::string> output;
    output.reserve(lines.size());

    std::size_t index = 0;
    while (index < lines.size()) {
        const std::string heading = trim(lines[index]);

        // Older versions appended underscore-only separator rows. Remove
        // those rows whether the surrounding block is newly converted or
        // already in the inline metadata format.
        if (is_legacy_gamehacking_separator(heading)) {
            ++index;
            continue;
        }

        // Already-clean metadata headings remain untouched.
        if (is_inline_metadata_name_line(heading)) {
            output.push_back(lines[index]);
            ++index;
            continue;
        }

        if (!heading.empty() &&
            index + 2U < lines.size()) {
            const auto crypt = canonical_crypt_label(lines[index + 1U]);
            const std::string first_code = trim(lines[index + 2U]);

            if (crypt && is_code_row(first_code)) {
                std::string cleaned_heading =
                    normalize_credit_suffix(heading);
                cleaned_heading += " , Crypt_" + *crypt;
                output.push_back(std::move(cleaned_heading));

                index += 2U;
                while (index < lines.size() &&
                       is_code_row(trim(lines[index]))) {
                    output.push_back(trim(lines[index]));
                    ++index;
                }

                if (index < lines.size() &&
                    is_legacy_gamehacking_separator(lines[index])) {
                    ++index;
                }
                continue;
            }
        }

        output.push_back(lines[index]);
        ++index;
    }

    std::ostringstream joined;
    for (std::size_t line_index = 0;
         line_index < output.size();
         ++line_index) {
        joined << output[line_index];
        if (line_index + 1U < output.size()) {
            joined << '\n';
        }
    }
    return joined.str();
}


} // namespace gba::text
