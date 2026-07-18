#include "cli/cli_internal.hpp"

#include "core/cmp.hpp"
#include "core/detect.hpp"
#include "core/inline_notes.hpp"
#include "formats/armax.hpp"
#include "formats/gameshark.hpp"
#include "formats/xploder.hpp"
#include "export/output_modes.hpp"

#include <optional>
#include <ostream>
#include <stdexcept>

namespace gba::cli::detail {
namespace {

std::optional<std::string> cli_format(gba::detect::Format format) {
    switch (format) {
    case gba::detect::Format::FcdRaw:
        return "cb-raw";
    case gba::detect::Format::FcdEncrypted:
        return "cb-encrypted";
    case gba::detect::Format::GameSharkRaw:
        return "gsa-raw";
    case gba::detect::Format::GameSharkEncrypted:
        return "gsa-encrypted";
    case gba::detect::Format::ActionReplayMaxRaw:
        return "armax-raw";
    case gba::detect::Format::ActionReplayMaxEncrypted:
        return "armax-encrypted";
    case gba::detect::Format::EzFlash:
        return "ez";
    case gba::detect::Format::Unknown:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

void resolve_auto_format(Options& options,
                         std::string_view input,
                         std::ostream& error_stream) {
    if (options.from != "auto") {
        return;
    }

    const gba::cmp::NormalizedInput cmp_input =
        gba::cmp::normalize_input(input);
    const gba::detect::Result detected = gba::detect::format(
        cmp_input.recognized ? cmp_input.text : input);
    const auto mapped = cli_format(detected.format);
    if (!mapped) {
        std::string message =
            "Auto Detect could not identify a supported format";
        if (!detected.reasons.empty()) {
            message += ": " + detected.reasons.front();
        }
        throw std::runtime_error(message);
    }
    options.from = *mapped;
    error_stream << "detected: " << gba::detect::name(detected.format)
                 << " ("
                 << gba::detect::confidence_name(detected.confidence)
                 << " confidence)\n";
}

bool is_direct_armax_transform(const Options& options) {
    return (options.from == "armax-raw" ||
            options.from == "armax-encrypted") &&
           (options.to == "armax-raw" ||
            options.to == "armax-encrypted");
}

int run_direct_armax_transform(const Options& options,
                               std::string_view input,
                               std::ostream& output_stream,
                               std::ostream& error_stream) {
    const auto result = gba::armax::transform_text(
        input,
        options.from == "armax-encrypted",
        options.to == "armax-encrypted");
    output_stream << gba::inline_notes::apply(
        result.text,
        gba::CheatDocument{},
        result.warnings,
        {gba::inline_notes::Style::Slash, true});
    print_warnings(result.warnings, error_stream);
    return result.success ? 0 : 1;
}

CheatDocument parse_document(const Options& options, std::string_view input) {
    const gba::cmp::NormalizedInput cmp_input =
        gba::cmp::normalize_input(input);
    const std::string_view semantic_input =
        cmp_input.recognized ? std::string_view(cmp_input.text) : input;

    CheatDocument document;
    if (options.from == "cb-raw") {
        document = gba::codebreaker::parse(semantic_input, {false});
    } else if (options.from == "cb-encrypted") {
        if (!gba::codebreaker::find_embedded_seed(semantic_input) &&
            !options.cb_input_seed) {
            throw std::runtime_error(
                "Encrypted CodeBreaker input is missing its embedded key; "
                "use --cb-input-key 9XXXXXXX:YYYY");
        }
        document = gba::codebreaker::parse(
            semantic_input,
            {true, options.cb_input_seed,
             options.cb_input_seed.has_value()});
    } else if (options.from == "gsa-raw") {
        document = gba::gameshark::parse(semantic_input, {false});
    } else if (options.from == "gsa-encrypted") {
        document = gba::gameshark::parse(semantic_input, {true});
    } else if (options.from == "armax-raw") {
        document = gba::armax::parse(semantic_input, {false});
    } else if (options.from == "armax-encrypted") {
        document = gba::armax::parse(semantic_input, {true});
    } else if (options.from == "xploder-raw" || options.from == "xp-raw") {
        document = gba::xploder::parse(semantic_input, {false});
    } else if (options.from == "xploder-encrypted" ||
               options.from == "xp-encrypted") {
        if (!gba::codebreaker::find_embedded_seed(semantic_input) &&
            !options.cb_input_seed) {
            throw std::runtime_error(
                "Encrypted Xploder input is missing its embedded key; "
                "use --cb-input-key 9XXXXXXX:YYYY");
        }
        document = gba::xploder::parse(
            semantic_input,
            {true, options.cb_input_seed,
             options.cb_input_seed.has_value()});
    } else if (options.from == "ez") {
        document = gba::ezflash::parse(semantic_input);
    } else {
        throw std::runtime_error(
            "Unknown semantic input format: " + options.from);
    }

    if (cmp_input.recognized) {
        document = gba::cmp::attach_layout(cmp_input, std::move(document));
    }
    return document;
}

int export_document(const Options& options,
                    const CheatDocument& document,
                    std::ostream& output_stream,
                    std::ostream& error_stream) {
    if (options.to == "armax-dsc") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        gba::output_modes::Options native_options;
        native_options.game_name = options.game_name;
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::ArmaxDsc,
            native_options);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "myboy-cht" || options.to == "myboy") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::MyBoyCht);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "ezflash-cht") {
        const CheatDocument native_document =
            gba::cmp::prepare_for_ezflash(document);
        gba::output_modes::Options native_options;
        native_options.ezflash_mode = options.ez_mode == gba::ezflash::Mode::Original
            ? gba::output_modes::EzFlashMode::Original
            : gba::output_modes::EzFlashMode::Enhanced;
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::EzFlashCht,
            native_options);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "retroarch-cht" ||
        options.to == "libretro-cht" ||
        options.to == "retroarch") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::LibretroCht);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "mgba-cheats" || options.to == "mgba") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::MgbaCheats);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "mednafen-cht" || options.to == "mednafen") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        gba::output_modes::Options native_options;
        native_options.game_name = options.game_name;
        native_options.rom_md5 = options.rom_md5;
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::MednafenCht,
            native_options);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "mister-gg") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::MisterGg);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "mister-zip" || options.to == "mister" ||
        options.to == "mister-gba") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document, gba::output_modes::Format::MisterZip);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    if (options.to == "vba-clt" || options.to == "clt") {
        const CheatDocument native_document =
            gba::cmp::flatten_for_device_output(document);
        const auto result = gba::output_modes::export_document(
            native_document,
            gba::output_modes::Format::VisualBoyAdvanceClt);
        if (!result.data.empty()) {
            output_stream.write(
                reinterpret_cast<const char*>(result.data.data()),
                static_cast<std::streamsize>(result.data.size()));
        }
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }
    const CheatDocument device_document = options.to == "ez"
        ? gba::cmp::prepare_for_ezflash(document)
        : gba::cmp::flatten_for_device_output(document);
    if (options.to == "cb-raw" || options.to == "cb-encrypted") {
        gba::codebreaker::ExportOptions export_options;
        export_options.encrypted = options.to == "cb-encrypted";
        export_options.seed = options.cb_output_seed;
        if (export_options.encrypted && !export_options.seed) {
            throw std::runtime_error(
                "--cb-key is required for encrypted CodeBreaker output");
        }

        const auto result =
            gba::codebreaker::export_document(device_document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            device_document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "gsa-raw" || options.to == "gsa-encrypted") {
        gba::gameshark::ExportOptions export_options;
        export_options.encrypted = options.to == "gsa-encrypted";
        const auto result =
            gba::gameshark::export_document(device_document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            device_document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "armax-raw" || options.to == "armax-encrypted") {
        gba::armax::ExportOptions export_options;
        export_options.encrypted = options.to == "armax-encrypted";
        const auto result =
            gba::armax::export_document(device_document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            device_document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "xploder-raw" ||
        options.to == "xploder-encrypted" ||
        options.to == "xp-raw" ||
        options.to == "xp-encrypted") {
        gba::xploder::ExportOptions export_options;
        export_options.encrypted =
            options.to == "xploder-encrypted" ||
            options.to == "xp-encrypted";
        export_options.seed = options.cb_output_seed;
        if (export_options.encrypted && !export_options.seed) {
            throw std::runtime_error(
                "--cb-key is required for encrypted Xploder output");
        }
        const auto result =
            gba::xploder::export_document(device_document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            device_document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "ez") {
        gba::ezflash::Options export_options;
        export_options.maximum_runtime_records = 128;
        export_options.mode = options.ez_mode;
        export_options.combine_multiple_if_groups = true;
        const auto result =
            gba::ezflash::export_document(device_document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            device_document,
            result.warnings,
            {gba::inline_notes::Style::Hash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    throw std::runtime_error(
        "Unknown semantic output format: " + options.to);
}

} // namespace gba::cli::detail
