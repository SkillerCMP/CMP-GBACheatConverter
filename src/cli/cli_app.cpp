#include "cli/cli_app.hpp"

#include "cli/cli_internal.hpp"
#include "core/text.hpp"
#include "core/version.hpp"

#include <exception>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace gba::cli {

int run(const std::vector<std::string>& arguments,
        std::istream& input_stream,
        std::ostream& output_stream,
        std::ostream& error_stream,
        std::string_view program_name) {
    try {
        if (arguments.empty()) {
            detail::usage(error_stream, program_name);
            return 2;
        }

        detail::Options options = detail::parse_arguments(arguments);
        if (options.action == detail::Action::Version) {
            output_stream << "GBA Cheat Converter v"
                          << GBA_CHEAT_VERSION << '\n';
            return 0;
        }
        if (options.action == detail::Action::Help) {
            detail::usage(error_stream, program_name);
            return 0;
        }

        const std::string input =
            detail::read_input(options.input_path, input_stream);
        if (gba::text::trim(input).empty()) {
            throw std::runtime_error("Input is empty");
        }

        if (!options.crypt_format.empty()) {
            return detail::run_crypto(options, input, output_stream);
        }

        if (options.from.empty() || options.to.empty()) {
            throw std::runtime_error("--from and --to are required");
        }

        detail::resolve_auto_format(options, input, error_stream);
        if (detail::is_direct_armax_transform(options)) {
            return detail::run_direct_armax_transform(
                options, input, output_stream, error_stream);
        }

        const CheatDocument document =
            detail::parse_document(options, input);
        return detail::export_document(
            options, document, output_stream, error_stream);
    } catch (const std::exception& error) {
        error_stream << "error: " << error.what() << '\n';
        return 1;
    }
}

} // namespace gba::cli
