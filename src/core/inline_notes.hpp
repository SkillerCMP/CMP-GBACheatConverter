#pragma once

#include "core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gba::inline_notes {

enum class Style {
    Slash,
    Hash
};

struct Options {
    Style style = Style::Slash;
    bool include_summary = true;
};

// Inserts conversion warnings directly into the generated text. Notes are
// placed immediately below the matching cheat header when possible. Warnings
// for entries that produced no output receive a small header-only block so the
// skipped code does not disappear silently.
std::string apply(std::string_view converted_text,
                  const CheatDocument& document,
                  const std::vector<std::string>& warnings,
                  const Options& options = {});

} // namespace gba::inline_notes
