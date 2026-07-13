#include "core/inline_notes_internal.hpp"

#include "core/text.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gba::inline_notes::detail {
namespace {

std::optional<std::size_t> source_line_from_warning(
    std::string_view warning) {
    constexpr std::string_view marker = "source line ";
    const std::size_t position = warning.rfind(marker);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t cursor = position + marker.size();
    if (cursor >= warning.size() ||
        !std::isdigit(static_cast<unsigned char>(warning[cursor]))) {
        return std::nullopt;
    }

    std::size_t value = 0;
    while (cursor < warning.size() &&
           std::isdigit(static_cast<unsigned char>(warning[cursor]))) {
        value = value * 10U +
            static_cast<std::size_t>(warning[cursor] - '0');
        ++cursor;
    }
    return value;
}

std::string remove_source_line_suffix(std::string message) {
    constexpr std::string_view marker = " at source line ";
    const std::size_t position = message.rfind(marker);
    if (position == std::string::npos) {
        return message;
    }

    std::size_t cursor = position + marker.size();
    if (cursor >= message.size() ||
        !std::isdigit(static_cast<unsigned char>(message[cursor]))) {
        return message;
    }

    while (cursor < message.size() &&
           std::isdigit(static_cast<unsigned char>(message[cursor]))) {
        ++cursor;
    }

    if (cursor == message.size()) {
        message.erase(position);
    }
    return message;
}

std::string trim_entry_prefix(std::string warning,
                              const std::string& entry_name) {
    const std::string prefix = entry_name + ":";
    if (warning.rfind(prefix, 0) != 0) {
        return warning;
    }

    warning.erase(0, prefix.size());
    return text::trim(warning);
}

const Operation* find_operation(const CheatDocument& document,
                                const std::string& entry_name,
                                std::size_t source_line) {
    for (const CheatEntry& entry : document.entries) {
        if (!entry_name.empty() && entry.name != entry_name) {
            continue;
        }
        for (const Operation& operation : entry.operations) {
            if (operation.source_line == source_line) {
                return &operation;
            }
        }
    }
    return nullptr;
}

std::string infer_entry_from_line(const CheatDocument& document,
                                  std::size_t source_line) {
    std::string match;
    for (const CheatEntry& entry : document.entries) {
        for (const Operation& operation : entry.operations) {
            if (operation.source_line != source_line) {
                continue;
            }
            if (!match.empty() && match != entry.name) {
                return {};
            }
            match = entry.name;
        }
    }
    return match;
}

bool is_generic_export_note(const std::string& message) {
    return message == "unsupported source operation" ||
           message.rfind("no CodeBreaker-compatible operations", 0) == 0 ||
           message.rfind("no GameShark/AR GBX-compatible operations", 0) == 0 ||
           message.rfind("no Action Replay MAX-compatible operations", 0) == 0 ||
           message.rfind("no EZ-compatible operations", 0) == 0;
}

std::string source_identity(const Note& note) {
    if (note.entry_name.empty() || !note.source_line) {
        return {};
    }
    return note.entry_name + "#" + std::to_string(*note.source_line);
}

Note make_note(const CheatDocument& document,
               const std::string& warning) {
    Note note;
    note.warning = warning;
    note.source_line = source_line_from_warning(warning);

    for (const CheatEntry& entry : document.entries) {
        const std::string prefix = entry.name + ":";
        if (warning.rfind(prefix, 0) == 0) {
            note.entry_name = entry.name;
            break;
        }
    }

    if (note.entry_name.empty() && note.source_line) {
        note.entry_name =
            infer_entry_from_line(document, *note.source_line);
    }

    note.message = trim_entry_prefix(warning, note.entry_name);
    note.message = remove_source_line_suffix(note.message);
    note.message = text::trim(note.message);

    if (note.source_line) {
        const Operation* operation =
            find_operation(document, note.entry_name, *note.source_line);
        if (operation && !operation->source_text.empty()) {
            note.source_text = operation->source_text;
        }
    }

    return note;
}

} // namespace

std::vector<Note> build_notes(
    const CheatDocument& document,
    const std::vector<std::string>& warnings) {
    std::vector<Note> notes;
    std::unordered_set<std::string> seen;
    std::unordered_map<std::string, std::size_t> source_notes;

    for (const std::string& warning : warnings) {
        if (warning.empty() || !seen.insert(warning).second) {
            continue;
        }

        Note candidate = make_note(document, warning);
        const std::string identity = source_identity(candidate);
        if (identity.empty()) {
            notes.push_back(std::move(candidate));
            continue;
        }

        const auto existing = source_notes.find(identity);
        if (existing == source_notes.end()) {
            source_notes.emplace(identity, notes.size());
            notes.push_back(std::move(candidate));
            continue;
        }

        Note& previous = notes[existing->second];
        const bool previous_generic =
            is_generic_export_note(previous.message);
        const bool candidate_generic =
            is_generic_export_note(candidate.message);

        // Prefer a source-format explanation over a generic destination
        // rejection for the same original row.
        if (previous_generic && !candidate_generic) {
            previous = std::move(candidate);
        }
    }

    if (notes.empty()) {
        return notes;
    }

    // A destination exporter often adds a final "no compatible operations"
    // warning after already reporting the exact failed source row. Keep the
    // specific explanation and suppress the redundant entry-level summary.
    std::unordered_set<std::string> entries_with_specific_notes;
    for (const Note& note : notes) {
        if (!note.entry_name.empty() &&
            !is_generic_export_note(note.message)) {
            entries_with_specific_notes.insert(note.entry_name);
        }
    }
    notes.erase(
        std::remove_if(
            notes.begin(),
            notes.end(),
            [&](const Note& note) {
                return !note.entry_name.empty() &&
                       is_generic_export_note(note.message) &&
                       entries_with_specific_notes.find(note.entry_name) !=
                           entries_with_specific_notes.end();
            }),
        notes.end());

    return notes;
}

} // namespace gba::inline_notes::detail
