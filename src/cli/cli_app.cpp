#include "cli/cli_app.hpp"

#include "cli/cli_internal.hpp"
#include "core/cmp.hpp"
#include "core/detect.hpp"
#include "core/text.hpp"
#include "core/version.hpp"
#include "import/native_input.hpp"

#include <exception>
#include <istream>
#include <ostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gba::cli {
namespace {

std::string semantic_name(native_input::InputFormat format) {
    switch (format) {
    case native_input::InputFormat::FcdRaw: return "cb-raw";
    case native_input::InputFormat::FcdEncrypted: return "cb-encrypted";
    case native_input::InputFormat::GameSharkRaw: return "gsa-raw";
    case native_input::InputFormat::GameSharkEncrypted:
        return "gsa-encrypted";
    case native_input::InputFormat::ActionReplayMaxRaw: return "armax-raw";
    case native_input::InputFormat::ActionReplayMaxEncrypted:
        return "armax-encrypted";
    case native_input::InputFormat::EzFlash: return "ez";
    }
    return {};
}

std::string semantic_name(detect::Format format) {
    switch (format) {
    case detect::Format::FcdRaw: return "cb-raw";
    case detect::Format::FcdEncrypted: return "cb-encrypted";
    case detect::Format::GameSharkRaw: return "gsa-raw";
    case detect::Format::GameSharkEncrypted: return "gsa-encrypted";
    case detect::Format::ActionReplayMaxRaw: return "armax-raw";
    case detect::Format::ActionReplayMaxEncrypted: return "armax-encrypted";
    case detect::Format::EzFlash: return "ez";
    case detect::Format::Unknown: return "unknown";
    }
    return "unknown";
}

std::string cleaned_semantic_input(std::string_view input) {
    return text::cleanup_gamehacking_org_blocks(text::strip_utf8_bom(input));
}

bool source_is(native_input::SourceFormat actual,
               native_input::SourceFormat expected) {
    return actual == expected;
}

std::string forced_native_filename(const detail::Options& options,
                                   const std::vector<std::uint8_t>& bytes) {
    if (options.input_path != "-") return options.input_path;
    if (options.from == "mgba-cheats" || options.from == "mgba") {
        return "input.cheats";
    }
    if (options.from == "mister" || options.from == "mister-zip" ||
        options.from == "mister-gg") {
        const bool zip_signature = bytes.size() >= 4U &&
            bytes[0] == 0x50U && bytes[1] == 0x4BU &&
            (bytes[2] == 0x03U || bytes[2] == 0x05U ||
             bytes[2] == 0x07U);
        return zip_signature ? "input.zip" : "input.gg";
    }
    if (options.from == "vba-clt" || options.from == "clt") {
        return "input.clt";
    }
    if (options.from == "armax-dsc") return "input.dsc";
    if (options.from == "myboy-cht" || options.from == "myboy" ||
        options.from == "retroarch-cht" ||
        options.from == "libretro-cht" ||
        options.from == "retroarch" ||
        options.from == "mednafen-cht" || options.from == "mednafen" ||
        options.from == "ezflash-cht") {
        return "input.cht";
    }
    return options.input_path;
}

void require_source(const native_input::Result& imported,
                    bool required,
                    native_input::SourceFormat expected,
                    std::string_view message) {
    if (required && !source_is(imported.source_format, expected)) {
        throw std::runtime_error(std::string(message));
    }
}

int run_detect_only(const detail::Options& options,
                    const std::string& raw_input,
                    std::ostream& output_stream,
                    std::ostream& error_stream) {
    const std::vector<std::uint8_t> bytes(raw_input.begin(), raw_input.end());
    const native_input::Result imported = native_input::import_file(
        options.input_path, bytes);
    if (imported.recognized) {
        if (!imported.success) {
            throw std::runtime_error(
                imported.warnings.empty()
                    ? "Native file detection failed"
                    : imported.warnings.front());
        }
        std::string native_name(
            native_input::source_format_cli_name(imported.source_format));
        if (imported.source_format == native_input::SourceFormat::MisterZip) {
            const bool zip_signature = bytes.size() >= 4U &&
                bytes[0] == 0x50U && bytes[1] == 0x4BU &&
                (bytes[2] == 0x03U || bytes[2] == 0x05U ||
                 bytes[2] == 0x07U);
            native_name = zip_signature ? "mister-zip" : "mister-gg";
        }
        output_stream << native_name
                      << "\t" << native_input::source_format_name(
                             imported.source_format)
                      << "\t" << native_input::confidence_name(
                             imported.detection_confidence)
                      << "\n";
        if (options.show_warnings) {
            detail::print_warnings(imported.warnings, error_stream);
        }
        return 0;
    }

    const std::string semantic = cleaned_semantic_input(raw_input);
    const cmp::NormalizedInput cmp_input = cmp::normalize_input(semantic);
    const detect::Result detected = detect::format(
        cmp_input.recognized ? cmp_input.text : semantic);
    if (detected.format == detect::Format::Unknown) {
        throw std::runtime_error(
            detected.reasons.empty()
                ? "No supported native or semantic format was detected"
                : detected.reasons.front());
    }
    output_stream << semantic_name(detected.format) << "\t"
                  << detect::name(detected.format) << "\t"
                  << detect::confidence_name(detected.confidence) << "\n";
    return 0;
}

} // namespace

bool would_write_binary_to_stdout(
    const std::vector<std::string>& arguments) {
    std::string output_format;
    bool explicit_file = false;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--to" && index + 1U < arguments.size()) {
            output_format = arguments[++index];
        } else if ((argument == "--output" || argument == "-o") &&
                   index + 1U < arguments.size()) {
            explicit_file = arguments[++index] != "-";
        } else if (argument == "--detect-only" ||
                   argument == "--list-formats" ||
                   argument == "--version" || argument == "-V" ||
                   argument == "--help" || argument == "-h") {
            return false;
        }
    }

    if (explicit_file) {
        return false;
    }
    return output_format == "armax-dsc" ||
           output_format == "vba-clt" || output_format == "clt" ||
           output_format == "mister" ||
           output_format == "mister-gba" ||
           output_format == "mister-gg" ||
           output_format == "mister-zip";
}

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
        if (options.action == detail::Action::ListFormats) {
            detail::list_formats(output_stream);
            return 0;
        }

        const std::string raw_input =
            detail::read_input(options.input_path, input_stream);
        if (raw_input.empty()) {
            throw std::runtime_error("Input is empty");
        }

        std::ostringstream buffered_output(
            std::ios::out | std::ios::binary);
        std::ostream* active_output = &output_stream;
        if (!options.output_path.empty() && options.output_path != "-") {
            active_output = &buffered_output;
        }
        const auto finish_output = [&](int result) {
            if (result != 0 || active_output == &output_stream) {
                return result;
            }
            const std::string data = buffered_output.str();
            detail::write_output_file(options.output_path, data);
            error_stream << "wrote output: " << options.output_path
                         << " (" << data.size() << " bytes)\n";
            return 0;
        };

        if (options.action == detail::Action::DetectOnly) {
            return finish_output(run_detect_only(
                options, raw_input, *active_output, error_stream));
        }

        if (!options.crypt_format.empty()) {
            const std::string input = cleaned_semantic_input(raw_input);
            if (text::trim(input).empty()) {
                throw std::runtime_error("Input is empty");
            }
            return finish_output(
                detail::run_crypto(options, input, *active_output));
        }

        if (options.from.empty() || options.to.empty()) {
            throw std::runtime_error("--from and --to are required");
        }

        std::string semantic_input;
        std::optional<CheatDocument> native_document;
        const bool retroarch_required = options.from == "retroarch-cht" ||
            options.from == "libretro-cht" || options.from == "retroarch";
        const bool mgba_required = options.from == "mgba-cheats" ||
            options.from == "mgba";
        const bool mister_required = options.from == "mister" ||
            options.from == "mister-zip" || options.from == "mister-gg";
        const bool mednafen_required = options.from == "mednafen" ||
            options.from == "mednafen-cht";
        const bool myboy_required = options.from == "myboy" ||
            options.from == "myboy-cht";
        const bool ezflash_required = options.from == "ezflash-cht";
        const bool vba_required = options.from == "vba-clt" ||
            options.from == "clt";
        const bool armax_dsc_required = options.from == "armax-dsc";
        const bool native_required = options.from == "native" ||
            options.from == "native-auto" || options.from == "file" ||
            retroarch_required || mgba_required || mister_required ||
            mednafen_required || myboy_required || ezflash_required ||
            vba_required || armax_dsc_required;
        if (native_required || options.from == "auto") {
            const std::vector<std::uint8_t> bytes(raw_input.begin(),
                                                  raw_input.end());
            const std::string native_filename =
                forced_native_filename(options, bytes);
            const native_input::Result imported = native_input::import_file(
                native_filename, bytes);
            if (imported.recognized) {
                if (!imported.success) {
                    throw std::runtime_error(
                        imported.warnings.empty()
                            ? "Native file import failed"
                            : imported.warnings.front());
                }
                require_source(imported, retroarch_required,
                    native_input::SourceFormat::LibretroCht,
                    "The input is not a Libretro / RetroArch .cht file");
                require_source(imported, mgba_required,
                    native_input::SourceFormat::MgbaCheats,
                    "The input is not an mGBA .cheats file");
                require_source(imported, mister_required,
                    native_input::SourceFormat::MisterZip,
                    "The input is not a MiSTer GBA .gg or .zip file");
                require_source(imported, mednafen_required,
                    native_input::SourceFormat::MednafenCht,
                    "The input is not a Mednafen .cht file");
                require_source(imported, myboy_required,
                    native_input::SourceFormat::MyBoyCht,
                    "The input is not a My Boy! .cht file");
                require_source(imported, ezflash_required,
                    native_input::SourceFormat::EzFlashCht,
                    "The input is not an EZ-Flash .cht file");
                require_source(imported, vba_required,
                    native_input::SourceFormat::VisualBoyAdvanceClt,
                    "The input is not a VisualBoy Advance-M .clt file");
                require_source(imported, armax_dsc_required,
                    native_input::SourceFormat::ArmaxDsc,
                    "The input is not an Action Replay MAX .dsc file");
                if (options.show_warnings) {
                    detail::print_warnings(imported.warnings, error_stream);
                }
                if (imported.has_document) {
                    native_document = imported.document;
                }
                options.from = semantic_name(imported.input_format);
                semantic_input = imported.text;
                error_stream << "detected native: "
                    << native_input::source_format_name(imported.source_format)
                    << " (" << native_input::confidence_name(
                        imported.detection_confidence) << " confidence)";
                if (!imported.text.empty()) {
                    error_stream << " -> "
                        << native_input::input_format_name(imported.input_format);
                } else {
                    error_stream << " (native-only document)";
                }
                error_stream << '\n';
            } else if (native_required) {
                throw std::runtime_error(
                    "The input is not a recognized native cheat file");
            }
        }

        if (semantic_input.empty() && !native_document) {
            semantic_input = cleaned_semantic_input(raw_input);
            if (text::trim(semantic_input).empty()) {
                throw std::runtime_error("Input is empty");
            }
            detail::resolve_auto_format(options, semantic_input, error_stream);
        }

        if (!native_document && !cmp::looks_like(semantic_input) &&
            detail::is_direct_armax_transform(options)) {
            return finish_output(detail::run_direct_armax_transform(
                options, semantic_input, *active_output, error_stream));
        }

        const CheatDocument document = native_document
            ? *native_document
            : detail::parse_document(options, semantic_input);
        return finish_output(detail::export_document(
            options, document, *active_output, error_stream));
    } catch (const std::exception& error) {
        error_stream << "error: " << error.what() << '\n';
        return 1;
    }
}

} // namespace gba::cli
