#include "cli/cli_internal.hpp"

#include "core/version.hpp"

#include <ostream>
#include <stdexcept>

namespace gba::cli::detail {

void list_formats(std::ostream& output_stream) {
    output_stream
        << "Semantic text formats (input/output):\n"
        << "  cb-raw\n"
        << "  cb-encrypted\n"
        << "  gsa-raw\n"
        << "  gsa-encrypted\n"
        << "  armax-raw\n"
        << "  armax-encrypted\n"
        << "  xploder-raw\n"
        << "  xploder-encrypted\n"
        << "  ez\n"
        << "  ezflash-original  input alias for stock ON= syntax\n"
        << "  ezflash-enhanced  input/output alias for latest E7 syntax\n\n"
        << "Native file formats:\n"
        << "  armax-dsc       input/output\n"
        << "  vba-clt         input/output\n"
        << "  myboy-cht       input/output (provisional schema)\n"
        << "  retroarch-cht   input/output\n"
        << "  mgba-cheats     input/output\n"
        << "  mednafen-cht    input/output\n"
        << "  mister-gg       input/output (one cheat)\n"
        << "  mister-zip      input/output (multiple .gg files)\n"
        << "  ezflash-cht     input/output\n\n"
        << "Automatic input:\n"
        << "  auto, native\n";
}

void usage(std::ostream& error_stream, std::string_view program_name) {
    error_stream
        << "GBA Cheat Converter v" GBA_CHEAT_VERSION "\n\n"
        << "Semantic or native conversion:\n"
        << "  " << program_name << " --from FORMAT --to FORMAT "
           "[--cb-input-key 9XXXXXXX:YYYY] [--cb-key 9XXXXXXX:YYYY] "
           "[--ez-mode original|enhanced] [--rom-md5 MD5] [--game-name NAME] [--output FILE] file\n\n"
        << "Inspection:\n"
        << "  " << program_name << " --detect-only file\n"
        << "  " << program_name << " --list-formats\n\n"
        << "Common native examples:\n"
        << "  " << program_name
        << " --from auto --to vba-clt --output game.clt game.cht\n"
        << "  " << program_name
        << " --from auto --to ezflash-enhanced --output enhanced.cht original.cht\n"
        << "  " << program_name
        << " --from auto --to retroarch-cht --output retroarch.cht game.cht\n"
        << "  " << program_name
        << " --from auto --to mgba-cheats --output game.cheats game.cht\n"
        << "  " << program_name
        << " --from auto --to mister-zip --output game.zip game.cht\n"
        << "  " << program_name
        << " --from auto --to mednafen-cht --rom-md5 "
           "0123456789abcdef0123456789abcdef --game-name Game --output gba.cht game.cht\n\n"
        << "Use --output FILE (or -o FILE) to write text or binary output directly.\n"
        << "Binary stdout still supports shell redirection with > FILE; never use >>.\n"
        << "Use --list-formats for the complete canonical format list.\n"
        << "Warnings are shown by default; --show-warnings is accepted for "
           "explicit scripts.\n\n"
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
            if (options.from == "ezflash-original" ||
                options.from == "ez-original" ||
                options.from == "ezflash-enhanced" ||
                options.from == "ez-enhanced") {
                options.from = "ez";
            }
        } else if (argument == "--to" && index + 1U < arguments.size()) {
            options.to = arguments[++index];
            if (options.to == "ezflash-original" ||
                options.to == "ez-original") {
                options.to = "ez";
                options.ez_mode = gba::ezflash::Mode::Original;
            } else if (options.to == "ezflash-enhanced" ||
                       options.to == "ez-enhanced") {
                options.to = "ez";
                options.ez_mode = gba::ezflash::Mode::Enhanced;
            }
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
        } else if ((argument == "--output" || argument == "-o") &&
                   index + 1U < arguments.size()) {
            options.output_path = arguments[++index];
            if (options.output_path.empty()) {
                throw std::runtime_error("Output path cannot be empty");
            }
        } else if (argument == "--rom-md5" &&
                   index + 1U < arguments.size()) {
            options.rom_md5 = arguments[++index];
        } else if (argument == "--game-name" &&
                   index + 1U < arguments.size()) {
            options.game_name = arguments[++index];
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
        } else if (argument == "--show-warnings") {
            options.show_warnings = true;
        } else if (argument == "--detect-only") {
            options.action = Action::DetectOnly;
        } else if (argument == "--list-formats") {
            options.action = Action::ListFormats;
            return options;
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
