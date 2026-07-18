#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gba::cmp {

enum class SourceKind {
    GroupHeader,
    Entry
};

struct SourceItem {
    SourceKind kind = SourceKind::Entry;
    std::string synthetic_name;
    std::vector<std::string> group_path;
    std::string display_name;
    std::string credits;
    std::size_t order = 0U;
};

struct NormalizedInput {
    bool recognized = false;
    std::string text;
    std::vector<SourceItem> items;
    std::vector<std::string> warnings;
};

bool looks_like(std::string_view input);
NormalizedInput normalize_input(std::string_view input);
CheatDocument attach_layout(const NormalizedInput& normalized,
                            CheatDocument parsed);

// Expands inherited CMP group header operations into each entry for ordinary
// non-CMP device output.
CheatDocument flatten_for_device_output(const CheatDocument& document);

// Converts CMP hierarchy into native EZ-Flash grouped sections. Header codes
// are inherited by each option because EZ-Flash has no group-header code row.
CheatDocument prepare_for_ezflash(const CheatDocument& document);

enum class RenderKind {
    GroupOpen,
    GroupClose,
    GroupHeader,
    Entry
};

struct RenderItem {
    RenderKind kind = RenderKind::Entry;
    std::string group_name;
    std::string synthetic_name;
    std::string display_name;
    std::string credits;
};

struct PreparedOutput {
    CheatDocument document;
    std::vector<RenderItem> layout;
    std::vector<std::pair<std::string, std::string>> name_map;
};

// Builds a synthetic document whose entries can be passed through any normal
// GBA exporter while retaining an independent CMP layout description.
PreparedOutput prepare_output(const CheatDocument& document);

// Converts exporter output for the synthetic document into CMP text.
std::string render_output(std::string_view exported_text,
                          const PreparedOutput& prepared);

void restore_warning_names(std::vector<std::string>& warnings,
                           const PreparedOutput& prepared);

} // namespace gba::cmp
