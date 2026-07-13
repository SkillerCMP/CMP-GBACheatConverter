#pragma once

#include "core/inline_notes.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gba::inline_notes::detail {

struct Note {
    std::string warning;
    std::string message;
    std::string entry_name;
    std::optional<std::size_t> source_line;
    std::string source_text;
};

std::vector<Note> build_notes(
    const CheatDocument& document,
    const std::vector<std::string>& warnings);

std::string render_notes(
    std::string_view converted_text,
    const std::vector<Note>& notes,
    const Options& options);

} // namespace gba::inline_notes::detail
