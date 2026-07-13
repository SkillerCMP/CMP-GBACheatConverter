#include "core/inline_notes_internal.hpp"

#include "core/text.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace gba::inline_notes::detail {
namespace {

std::string comment_prefix(Style style) {
    return style == Style::Hash ? "#" : "//";
}

std::string header_name(std::string_view line) {
    const std::string trimmed = text::trim(line);
    if (trimmed.size() >= 2U &&
        trimmed.front() == '[' &&
        trimmed.back() == ']') {
        return trimmed.substr(1U, trimmed.size() - 2U);
    }

    if (text::is_inline_metadata_name_line(trimmed)) {
        return trimmed;
    }

    if (!trimmed.empty() && trimmed.back() == ':' &&
        trimmed.rfind("//", 0) != 0 &&
        trimmed.rfind("#", 0) != 0 &&
        trimmed.rfind(";", 0) != 0) {
        return text::trim(
            std::string_view(trimmed).substr(0, trimmed.size() - 1U));
    }

    return {};
}

bool header_matches_entry(const std::string& header,
                          const std::string& entry_name) {
    if (header == entry_name) {
        return true;
    }
    const std::string split_prefix = entry_name + " - ";
    return header.rfind(split_prefix, 0) == 0;
}

void emit_note(std::ostringstream& output,
               const Note& note,
               const std::string& prefix) {
    output << prefix << " Conversion Note: ";
    if (note.source_line) {
        output << "Line " << *note.source_line << ": ";
    }
    output << note.message << '\n';

    if (!note.source_text.empty()) {
        output << prefix << " Source: " << note.source_text << '\n';
    }
}

} // namespace

std::string render_notes(
    std::string_view converted_text,
    const std::vector<Note>& notes,
    const Options& options) {
    if (notes.empty()) {
        return std::string(converted_text);
    }

    std::unordered_map<std::string, std::vector<std::size_t>> by_entry;
    std::vector<std::size_t> global_notes;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        if (notes[index].entry_name.empty()) {
            global_notes.push_back(index);
        } else {
            by_entry[notes[index].entry_name].push_back(index);
        }
    }

    std::vector<bool> emitted(notes.size(), false);
    const std::string prefix = comment_prefix(options.style);
    std::ostringstream output;

    const std::vector<std::string> lines =
        text::split_lines(converted_text);
    for (const std::string& line : lines) {
        output << line << '\n';

        const std::string header = header_name(line);
        if (header.empty()) {
            continue;
        }

        for (const auto& [entry_name, indices] : by_entry) {
            if (!header_matches_entry(header, entry_name)) {
                continue;
            }

            bool wrote_any = false;
            for (const std::size_t index : indices) {
                if (emitted[index]) {
                    continue;
                }
                emit_note(output, notes[index], prefix);
                emitted[index] = true;
                wrote_any = true;
            }
            if (wrote_any) {
                output << '\n';
            }
            break;
        }
    }

    // Entries with no compatible output still receive an inline block.
    for (const auto& [entry_name, indices] : by_entry) {
        bool has_remaining = false;
        for (const std::size_t index : indices) {
            has_remaining = has_remaining || !emitted[index];
        }
        if (!has_remaining) {
            continue;
        }

        if (!output.str().empty() && output.str().back() != '\n') {
            output << '\n';
        }
        output << text::format_cheat_header(entry_name) << '\n';
        for (const std::size_t index : indices) {
            if (emitted[index]) {
                continue;
            }
            emit_note(output, notes[index], prefix);
            emitted[index] = true;
        }
        output << '\n';
    }

    if (!global_notes.empty()) {
        for (const std::size_t index : global_notes) {
            if (!emitted[index]) {
                emit_note(output, notes[index], prefix);
                emitted[index] = true;
            }
        }
        output << '\n';
    }

    if (options.include_summary) {
        output << prefix << " Conversion Summary: "
               << notes.size()
               << " inline note(s); output may be partial.\n";
    }

    return output.str();
}

} // namespace gba::inline_notes::detail
