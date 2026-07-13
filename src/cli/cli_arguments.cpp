#include "cli/cli_internal.hpp"

#include "core/version.hpp"

#include <ostream>
#include <stdexcept>

namespace gba::cli::detail {

void usage(std::ostream& error_stream, std::string_view program_name) {
    error_stream
        << "GBA Cheat Converter v" GBA_CHEAT_VERSION "\n\n"
        << "Semantic conversion:\n"
        << "  " << program_name << " --from FORMAT --to FORMAT "
           "[--cb-input-key 9XXXXXXX:YYYY] [--cb-key 9XXXXXXX:YYYY] "
           "[--ez-mode original|enhanced] file\n\n"
        << "Formats:\n"
        << "  cb-raw, cb-encrypted, gsa-raw, gsa-encrypted,\n"
        << "  armax-raw, armax-encrypted, xploder-raw,\n"
        << "  xploder-encrypted, ez\n"
        << "  EZ Enhanced follows Omega DE Kernel 1.06 Enhanced v3: IF-family comparisons, ELSE, ADD/SUB, PTR, FILL/SLIDE, ROM/ROMIF, and the shared 128-record runtime limit.\n\n"
        << "Crypto-only 8+8 conversion:\n"
        << "  " << program_name << " --crypt gsa-v1|par-v3 "
           "--encrypt|--decrypt file\n";
}

Options parse_arguments(const std::vector<std::string>& arguments) {
    Options options;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--from" && index + 1U < arguments.size()) {
            options.from = arguments[++index];
        } else if (argument == "--to" && index + 1U < arguments.size()) {
            options.to = arguments[++index];
        } else if (argument == "--crypt" && index + 1U < arguments.size()) {
            options.crypt_format = arguments[++index];
        } else if ((argument == "--cb-input-key" ||
                    argument == "--cb-input-seed") &&
                   index + 1U < arguments.size()) {
            options.cb_input_seed =
                gba::codebreaker::parse_seed_text(arguments[++index]);
            if (!options.cb_input_seed) {
                throw std::runtime_error(
                    "Invalid CodeBreaker input key; expected 9XXXXXXX:YYYY");
            }
        } else if ((argument == "--cb-key" || argument == "--cb-seed") &&
                   index + 1U < arguments.size()) {
            options.cb_output_seed =
                gba::codebreaker::parse_seed_text(arguments[++index]);
            if (!options.cb_output_seed) {
                throw std::runtime_error(
                    "Invalid CodeBreaker output key; expected 9XXXXXXX:YYYY");
            }
        } else if (argument == "--ez-mode" &&
                   index + 1U < arguments.size()) {
            const std::string mode = arguments[++index];
            if (mode == "original") {
                options.ez_mode = gba::ezflash::Mode::Original;
            } else if (mode == "enhanced" || mode == "cheat-mod" ||
                       mode == "mod") {
                options.ez_mode = gba::ezflash::Mode::Enhanced;
            } else {
                throw std::runtime_error(
                    "Invalid EZ-Flash mode; expected original or enhanced");
            }
        } else if (argument == "--encrypt") {
            options.encrypt = true;
        } else if (argument == "--decrypt") {
            options.decrypt = true;
        } else if (argument == "--version" || argument == "-V") {
            options.action = Action::Version;
            return options;
        } else if (argument == "--help" || argument == "-h") {
            options.action = Action::Help;
            return options;
        } else if (argument == "-" ||
                   (!argument.empty() && argument[0] != '-')) {
            options.input_path = argument;
        } else {
            throw std::runtime_error("Unknown argument: " + argument);
        }
    }

    if (options.input_path.empty()) {
        options.input_path = "-";
    }
    return options;
}

} // namespace gba::cli::detail
