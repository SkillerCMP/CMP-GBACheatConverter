#include "core/inline_notes.hpp"

#include "core/inline_notes_internal.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gba::inline_notes {

std::string apply(std::string_view converted_text,
                  const CheatDocument& document,
                  const std::vector<std::string>& warnings,
                  const Options& options) {
    if (warnings.empty()) {
        return std::string(converted_text);
    }

    const std::vector<detail::Note> notes =
        detail::build_notes(document, warnings);
    if (notes.empty()) {
        return std::string(converted_text);
    }

    return detail::render_notes(converted_text, notes, options);
}

} // namespace gba::inline_notes
