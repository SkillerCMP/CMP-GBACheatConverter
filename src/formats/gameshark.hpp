#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::gameshark {

struct RawLine {
    std::uint32_t op1 = 0;
    std::uint32_t op2 = 0;
    std::size_t source_line = 0;
    std::string source_text;
};

struct ParseOptions {
    bool encrypted = false;
};

struct ExportOptions {
    bool encrypted = false;
};

struct Result {
    std::string text;
    std::vector<std::string> warnings;
    bool success = true;
};

CheatDocument parse(std::string_view input, const ParseOptions& options = {});
Result export_document(const CheatDocument& document,
                       const ExportOptions& options = {});

} // namespace gba::gameshark
