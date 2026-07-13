#include "cli/cli_internal.hpp"

#include "core/detect.hpp"
#include "core/inline_notes.hpp"
#include "formats/armax.hpp"
#include "formats/gameshark.hpp"
#include "formats/xploder.hpp"

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

    const gba::detect::Result detected = gba::detect::format(input);
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
    if (options.from == "cb-raw") {
        return gba::codebreaker::parse(input, {false});
    }
    if (options.from == "cb-encrypted") {
        if (!gba::codebreaker::find_embedded_seed(input) &&
            !options.cb_input_seed) {
            throw std::runtime_error(
                "Encrypted CodeBreaker input is missing its embedded key; "
                "use --cb-input-key 9XXXXXXX:YYYY");
        }
        return gba::codebreaker::parse(
            input,
            {true,
             options.cb_input_seed,
             options.cb_input_seed.has_value()});
    }
    if (options.from == "gsa-raw") {
        return gba::gameshark::parse(input, {false});
    }
    if (options.from == "gsa-encrypted") {
        return gba::gameshark::parse(input, {true});
    }
    if (options.from == "armax-raw") {
        return gba::armax::parse(input, {false});
    }
    if (options.from == "armax-encrypted") {
        return gba::armax::parse(input, {true});
    }
    if (options.from == "xploder-raw" || options.from == "xp-raw") {
        return gba::xploder::parse(input, {false});
    }
    if (options.from == "xploder-encrypted" ||
        options.from == "xp-encrypted") {
        if (!gba::codebreaker::find_embedded_seed(input) &&
            !options.cb_input_seed) {
            throw std::runtime_error(
                "Encrypted Xploder input is missing its embedded key; "
                "use --cb-input-key 9XXXXXXX:YYYY");
        }
        return gba::xploder::parse(
            input,
            {true,
             options.cb_input_seed,
             options.cb_input_seed.has_value()});
    }
    if (options.from == "ez") {
        return gba::ezflash::parse(input);
    }
    throw std::runtime_error(
        "Unknown semantic input format: " + options.from);
}

int export_document(const Options& options,
                    const CheatDocument& document,
                    std::ostream& output_stream,
                    std::ostream& error_stream) {
    if (options.to == "cb-raw" || options.to == "cb-encrypted") {
        gba::codebreaker::ExportOptions export_options;
        export_options.encrypted = options.to == "cb-encrypted";
        export_options.seed = options.cb_output_seed;
        if (export_options.encrypted && !export_options.seed) {
            throw std::runtime_error(
                "--cb-key is required for encrypted CodeBreaker output");
        }

        const auto result =
            gba::codebreaker::export_document(document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "gsa-raw" || options.to == "gsa-encrypted") {
        gba::gameshark::ExportOptions export_options;
        export_options.encrypted = options.to == "gsa-encrypted";
        const auto result =
            gba::gameshark::export_document(document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            document,
            result.warnings,
            {gba::inline_notes::Style::Slash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    if (options.to == "armax-raw" || options.to == "armax-encrypted") {
        gba::armax::ExportOptions export_options;
        export_options.encrypted = options.to == "armax-encrypted";
        const auto result =
            gba::armax::export_document(document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            document,
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
            gba::xploder::export_document(document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            document,
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
            gba::ezflash::export_document(document, export_options);
        output_stream << gba::inline_notes::apply(
            result.text,
            document,
            result.warnings,
            {gba::inline_notes::Style::Hash, true});
        print_warnings(result.warnings, error_stream);
        return result.success ? 0 : 1;
    }

    throw std::runtime_error(
        "Unknown semantic output format: " + options.to);
}

} // namespace gba::cli::detail
