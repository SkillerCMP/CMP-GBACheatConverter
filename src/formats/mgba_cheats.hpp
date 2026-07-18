#pragma once

#include "core/types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gba::mgba_cheats {

struct Record {
    std::string name;
    bool enabled = true;
    MgbaCodeFamily family = MgbaCodeFamily::AutoDetect;
    std::vector<std::string> code_lines;
};

struct ParseResult {
    bool recognized = false;
    bool success = false;
    std::vector<Record> records;
    std::vector<std::string> warnings;
};

// Content-only signature used before extension fallback. A normal CodeBreaker
// file can look identical to an enabled mGBA set, so directive-free files are
// recognized authoritatively by their .cheats extension instead.
bool looks_like(std::string_view text);

// force_extension_hint accepts directive-free canonical .cheats files.
ParseResult parse(std::string_view text, bool force_extension_hint = false);
std::string serialize(const std::vector<Record>& records);
std::string_view directive_name(MgbaCodeFamily family);

} // namespace gba::mgba_cheats
