#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace gba::cli {

// Returns true when the arguments request a binary format on stdout without
// an explicit --output path. Entry points use this to prevent binary data
// from being printed directly into an interactive console.
bool would_write_binary_to_stdout(const std::vector<std::string>& arguments);

int run(const std::vector<std::string>& arguments,
        std::istream& input_stream,
        std::ostream& output_stream,
        std::ostream& error_stream,
        std::string_view program_name = "GbaCheatConverter");

} // namespace gba::cli
