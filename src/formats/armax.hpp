#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gba::armax {

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

// Semantic conversion. Block IF/ELSE and physical device-button writes are
// represented atomically. Unsupported dependent operations are retained as
// warnings rather than silently becoming unconditional writes.
CheatDocument parse(std::string_view input, const ParseOptions& options = {});
Result export_document(const CheatDocument& document,
                       const ExportOptions& options = {});

// Lossless static-key raw/encrypted conversion for the same AR MAX family.
// This path preserves every 8+8 row, including master/ROM-patch rows that are
// intentionally not representable in the common semantic model.
Result transform_text(std::string_view input,
                      bool input_encrypted,
                      bool output_encrypted);

} // namespace gba::armax
