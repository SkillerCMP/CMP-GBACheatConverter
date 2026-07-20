#include "gui/gui_state.hpp"

namespace gba::gui {

void show_warnings(const std::vector<std::string>& warnings,
                   std::wstring_view prefix) {
    if (warnings.empty()) {
        return;
    }

    set_status(
        std::wstring(prefix) +
        L"Converted with " + std::to_wstring(warnings.size()) +
        L" inline conversion note(s). Review the output text.");
}

bool perform_conversion(bool modal_errors) {
    if (g_in_convert) {
        return false;
    }
    g_in_convert = true;

    try {
        const std::string input = current_cleaned_input();
        if (gba::text::trim(input).empty()) {
            SetWindowTextW(g_output_edit, L"");
            set_status(L"Input is empty.");
            g_in_convert = false;
            return false;
        }

        const gba::cmp::NormalizedInput cmp_input =
            gba::cmp::normalize_input(input);
        const std::string& semantic_input =
            cmp_input.recognized ? cmp_input.text : input;

        GuiFormat input_format = g_input_format;
        const GuiFormat output = g_output_format;
        std::wstring detection_status;

        if (input_format == GuiFormat::AutoDetect) {
            const gba::detect::Result detected =
                gba::detect::format(semantic_input);
            const auto mapped = gui_format_from_detection(detected.format);
            if (!mapped) {
                std::string message =
                    "Auto Detect could not identify a supported format.";
                if (!detected.reasons.empty()) {
                    message += " " + detected.reasons.front();
                }
                throw std::runtime_error(message);
            }

            input_format = *mapped;
            g_last_detected_input = input_format;
            update_format_labels();
            update_seed_controls();
            sync_input_seed_from_text(true);
            detection_status =
                L"Detected " + format_name(input_format, false) + L" (" +
                utf8_to_wide(gba::detect::confidence_name(
                    detected.confidence)) +
                L" confidence). ";
        }

        const gba::ezflash::Syntax ezflash_input_syntax =
            input_format == GuiFormat::EzFlash
                ? gba::ezflash::detect_syntax(semantic_input)
                : gba::ezflash::Syntax::Unknown;

        // Original EZ-Flash is a source-preservation mode in the GUI.  The
        // only automatic migration is Original -> current Enhanced E7.
        if (!cmp_input.recognized && !g_cmp_output &&
            input_format == GuiFormat::EzFlash &&
            output == GuiFormat::EzFlash &&
            g_ezflash_mode == gba::ezflash::Mode::Original) {
            set_editor_text(g_output_edit, semantic_input);
            set_status(detection_status +
                L"EZ-Flash Original selected; EZ-Flash input was preserved without conversion.");
            g_in_convert = false;
            return true;
        }

        if (!cmp_input.recognized && !g_cmp_output &&
            is_armax_family(input_format) && is_armax_family(output)) {
            const auto transformed = gba::armax::transform_text(
                input, is_armax_encrypted(input_format),
                is_armax_encrypted(output));
            const std::string annotated = gba::inline_notes::apply(
                transformed.text,
                gba::CheatDocument{},
                transformed.warnings,
                {gba::inline_notes::Style::Slash, true});
            set_editor_text(g_output_edit, annotated);
            if (transformed.warnings.empty()) {
                set_status(detection_status +
                    (transformed.success
                        ? (output == GuiFormat::ActionReplayMaxRaw
                            ? L"Decrypted/converted to Action Replay MAX Raw."
                            : L"Encrypted/converted to Action Replay MAX Encrypted.")
                        : L"Conversion completed with compatibility errors."));
            } else {
                show_warnings(transformed.warnings, detection_status);
            }
            g_in_convert = false;
            return transformed.success;
        }

        gba::CheatDocument document;
        std::wstring input_seed_status;
        switch (input_format) {
        case GuiFormat::FcdRaw:
            document = gba::codebreaker::parse(semantic_input, {false});
            break;
        case GuiFormat::FcdEncrypted: {
            sync_input_seed_from_text(false);
            const bool manual_key = manual_input_key_enabled();
            const auto embedded =
                gba::codebreaker::find_embedded_seed(semantic_input);
            const auto input_seed = parse_seed_edit(g_input_seed_edit);

            if (manual_key && !input_seed) {
                throw std::runtime_error(
                    "Use is checked, but In Key is missing or invalid. "
                    "Enter it as 9XXXXXXX:YYYY.");
            }
            if (!manual_key && !embedded) {
                throw std::runtime_error(
                    "Encrypted FCD input has no safely detectable embedded "
                    "key row. Check Use and enter In Key as "
                    "9XXXXXXX:YYYY for a keyless code.");
            }

            document = gba::codebreaker::parse(
                semantic_input,
                {true, manual_key ? input_seed : std::nullopt, manual_key});
            const auto active_seed = manual_key ? input_seed : embedded;
            if (active_seed) {
                input_seed_status =
                    L"Input key " + format_seed_wide(*active_seed) +
                    (manual_key ? L" forced manually. " : L" detected. ");
            }
            break;
        }
        case GuiFormat::GameSharkRaw:
            document = gba::gameshark::parse(semantic_input, {false});
            break;
        case GuiFormat::GameSharkEncrypted:
            document = gba::gameshark::parse(semantic_input, {true});
            break;
        case GuiFormat::ActionReplayMaxRaw:
            document = gba::armax::parse(semantic_input, {false});
            break;
        case GuiFormat::ActionReplayMaxEncrypted:
            document = gba::armax::parse(semantic_input, {true});
            break;
        case GuiFormat::EzFlash:
            document = gba::ezflash::parse(semantic_input);
            break;
        case GuiFormat::AutoDetect:
            throw std::runtime_error(
                "Auto Detect did not resolve to a concrete input format");
        }
        if (cmp_input.recognized) {
            document = gba::cmp::attach_layout(cmp_input, std::move(document));
        }

        std::optional<gba::cmp::PreparedOutput> cmp_output;
        gba::CheatDocument output_document;
        if (output == GuiFormat::EzFlash) {
            output_document = gba::cmp::prepare_for_ezflash(document);
        } else if (g_cmp_output) {
            cmp_output = gba::cmp::prepare_output(document);
            output_document = cmp_output->document;
        } else {
            output_document = gba::cmp::flatten_for_device_output(document);
        }

        std::vector<std::string> warnings;
        bool success = true;
        std::string converted_text;
        std::wstring completed_status;

        if (output == GuiFormat::FcdRaw ||
            output == GuiFormat::FcdEncrypted) {
            gba::codebreaker::ExportOptions options;
            options.encrypted = output == GuiFormat::FcdEncrypted;
            if (options.encrypted) {
                options.seed = parse_seed_edit(g_output_seed_edit);
                if (!options.seed) {
                    throw std::runtime_error(
                        "Encrypted CodeBreaker output requires a key in "
                        "9XXXXXXX:YYYY form");
                }
            }

            const auto converted =
                gba::codebreaker::export_document(output_document, options);
            converted_text = converted.text;
            warnings = converted.warnings;
            success = converted.success;
            completed_status = output == GuiFormat::FcdRaw
                ? L"Converted to CodeBreaker / GameShark SP / Xploder Advance Raw."
                : L"Converted to CodeBreaker / GameShark SP / Xploder Advance Encrypted.";
        } else if (output == GuiFormat::GameSharkRaw ||
                   output == GuiFormat::GameSharkEncrypted) {
            gba::gameshark::ExportOptions options;
            options.encrypted = output == GuiFormat::GameSharkEncrypted;
            const auto converted =
                gba::gameshark::export_document(output_document, options);
            converted_text = converted.text;
            warnings = converted.warnings;
            success = converted.success;
            completed_status = output == GuiFormat::GameSharkRaw
                ? L"Converted to GameShark Advance / Action Replay GBX Raw."
                : L"Converted to GameShark Advance / Action Replay GBX Encrypted.";
        } else if (output == GuiFormat::ActionReplayMaxRaw ||
                   output == GuiFormat::ActionReplayMaxEncrypted) {
            gba::armax::ExportOptions options;
            options.encrypted = output == GuiFormat::ActionReplayMaxEncrypted;
            const auto converted =
                gba::armax::export_document(output_document, options);
            converted_text = converted.text;
            warnings = converted.warnings;
            success = converted.success;
            completed_status = output == GuiFormat::ActionReplayMaxRaw
                ? L"Converted to Action Replay MAX Raw."
                : L"Converted to Action Replay MAX Encrypted.";
        } else if (output == GuiFormat::EzFlash) {
            gba::ezflash::Options options;
            options.maximum_runtime_records = 128;
            options.mode = g_ezflash_mode;
            options.combine_multiple_if_groups = true;
            const auto converted =
                gba::ezflash::export_document(output_document, options);
            converted_text = converted.text;
            warnings = converted.warnings;
            success = converted.success;
            if (g_ezflash_mode == gba::ezflash::Mode::Original) {
                completed_status =
                    L"Converted to EZ-Flash Original format (ON= only).";
            } else if (input_format == GuiFormat::EzFlash &&
                       ezflash_input_syntax == gba::ezflash::Syntax::Original) {
                completed_status =
                    L"Detected EZ-Flash Original input and migrated it to Enhanced E7 revision-6 format.";
            } else {
                completed_status =
                    L"Converted to EZ-Flash Enhanced E7 revision-6 format.";
            }
        } else {
            throw std::runtime_error("Unknown output format selection");
        }

        const gba::inline_notes::Style note_style =
            output == GuiFormat::EzFlash
                ? gba::inline_notes::Style::Hash
                : gba::inline_notes::Style::Slash;
        converted_text = gba::inline_notes::apply(
            converted_text,
            output_document,
            warnings,
            {note_style, true});

        if (cmp_output) {
            converted_text =
                gba::cmp::render_output(converted_text, *cmp_output);
            gba::cmp::restore_warning_names(warnings, *cmp_output);
            completed_status += L" CMP formatting applied.";
        }

        set_editor_text(g_output_edit, converted_text);
        if (warnings.empty()) {
            set_status(success
                ? detection_status + input_seed_status + completed_status
                : detection_status + input_seed_status +
                    L"Conversion completed with compatibility errors.");
        } else {
            show_warnings(warnings, detection_status + input_seed_status);
        }

        g_in_convert = false;
        return success;
    } catch (const std::exception& error) {
        const std::wstring message = utf8_to_wide(error.what());
        SetWindowTextW(g_output_edit, L"");
        set_status(L"Error: " + message);
        if (modal_errors) {
            MessageBoxW(g_main, message.c_str(), L"Conversion error",
                        MB_OK | MB_ICONERROR);
        }
        g_in_convert = false;
        return false;
    }
}

void maybe_auto_convert() {
    if (g_auto_convert && !g_in_convert) {
        perform_conversion(false);
    }
}

} // namespace gba::gui
