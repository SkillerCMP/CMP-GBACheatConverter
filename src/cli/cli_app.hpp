#pragma once

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace gba::cli {

int run(const std::vector<std::string>& arguments,
        std::istream& input_stream,
        std::ostream& output_stream,
        std::ostream& error_stream,
        std::string_view program_name = "GbaCheatConverter");

} // namespace gba::cli
