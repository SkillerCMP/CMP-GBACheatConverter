#include "cli/cli_internal.hpp"

#include "core/text.hpp"

#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace gba::cli::detail {

std::string read_input(const std::string& path, std::istream& input_stream) {
    std::string raw_input;
    if (path == "-") {
        std::ostringstream input;
        input << input_stream.rdbuf();
        raw_input = input.str();
    } else {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Unable to open input file: " + path);
        }

        std::ostringstream data;
        data << input.rdbuf();
        raw_input = data.str();
    }

    return gba::text::cleanup_gamehacking_org_blocks(
        gba::text::strip_utf8_bom(raw_input));
}

void print_warnings(const std::vector<std::string>& warnings,
                    std::ostream& error_stream) {
    for (const std::string& warning : warnings) {
        error_stream << "warning: " << warning << '\n';
    }
}

} // namespace gba::cli::detail
