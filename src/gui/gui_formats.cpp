#include "gui/gui_state.hpp"

namespace gba::gui {

int input_format_index() {
    return static_cast<int>(g_input_format);
}

int output_format_index() {
    return static_cast<int>(g_output_format);
}

bool is_fcd_encrypted(GuiFormat format) {
    return format == GuiFormat::FcdEncrypted;
}

bool is_armax_family(GuiFormat format) {
    return format == GuiFormat::ActionReplayMaxRaw ||
           format == GuiFormat::ActionReplayMaxEncrypted;
}

bool is_armax_encrypted(GuiFormat format) {
    return format == GuiFormat::ActionReplayMaxEncrypted;
}

std::optional<GuiFormat> gui_format_from_detection(gba::detect::Format format) {
    switch (format) {
    case gba::detect::Format::FcdRaw:
        return GuiFormat::FcdRaw;
    case gba::detect::Format::FcdEncrypted:
        return GuiFormat::FcdEncrypted;
    case gba::detect::Format::GameSharkRaw:
        return GuiFormat::GameSharkRaw;
    case gba::detect::Format::GameSharkEncrypted:
        return GuiFormat::GameSharkEncrypted;
    case gba::detect::Format::ActionReplayMaxRaw:
        return GuiFormat::ActionReplayMaxRaw;
    case gba::detect::Format::ActionReplayMaxEncrypted:
        return GuiFormat::ActionReplayMaxEncrypted;
    case gba::detect::Format::EzFlash:
        return GuiFormat::EzFlash;
    case gba::detect::Format::Unknown:
        return std::nullopt;
    }
    return std::nullopt;
}

std::wstring format_name(GuiFormat format, bool output) {
    switch (format) {
    case GuiFormat::AutoDetect:
        return L"Auto Detect";
    case GuiFormat::FcdRaw:
        return L"RAW - CodeBreaker / GameShark SP / Xploder Advance";
    case GuiFormat::GameSharkRaw:
        return L"RAW - GameShark Advance / Action Replay GBX";
    case GuiFormat::ActionReplayMaxRaw:
        return L"RAW - Action Replay MAX";
    case GuiFormat::EzFlash:
        if (output) {
            return g_ezflash_mode == gba::ezflash::Mode::Original
                ? L"RAW - EZ-Flash (Original)"
                : L"RAW - EZ-Flash (Enhanced)";
        }
        return L"RAW - EZ-Flash";
    case GuiFormat::FcdEncrypted:
        return L"Encrypted - CodeBreaker / GameShark SP / Xploder Advance";
    case GuiFormat::GameSharkEncrypted:
        return L"Encrypted - GameShark Advance / Action Replay GBX";
    case GuiFormat::ActionReplayMaxEncrypted:
        return L"Encrypted - Action Replay MAX";
    }
    return L"Unknown";
}

std::wstring format_mode_name(GuiFormat format) {
    switch (format) {
    case GuiFormat::AutoDetect:
        return L"Auto Detect";
    case GuiFormat::FcdRaw:
    case GuiFormat::GameSharkRaw:
    case GuiFormat::ActionReplayMaxRaw:
    case GuiFormat::EzFlash:
        return L"RAW";
    case GuiFormat::FcdEncrypted:
    case GuiFormat::GameSharkEncrypted:
    case GuiFormat::ActionReplayMaxEncrypted:
        return L"Encrypted";
    }
    return L"Unknown";
}

std::wstring format_family_name(GuiFormat format, bool output) {
    switch (format) {
    case GuiFormat::AutoDetect:
        return L"Paste or open code";
    case GuiFormat::FcdRaw:
    case GuiFormat::FcdEncrypted:
        return L"CodeBreaker / GameShark SP / Xploder Advance";
    case GuiFormat::GameSharkRaw:
    case GuiFormat::GameSharkEncrypted:
        return L"GameShark Advance / Action Replay GBX";
    case GuiFormat::ActionReplayMaxRaw:
    case GuiFormat::ActionReplayMaxEncrypted:
        return L"Action Replay MAX";
    case GuiFormat::EzFlash:
        if (output) {
            return g_ezflash_mode == gba::ezflash::Mode::Original
                ? L"EZ-Flash (Original)"
                : L"EZ-Flash (Enhanced)";
        }
        return L"EZ-Flash";
    }
    return L"Unknown";
}

void update_format_labels() {
    GuiFormat displayed_input = g_input_format;
    if (g_input_format == GuiFormat::AutoDetect && g_last_detected_input) {
        displayed_input = *g_last_detected_input;
    }

    const std::wstring input_heading =
        L"Input: " + format_mode_name(displayed_input) + L"\r\n" +
        format_family_name(displayed_input, false);
    const std::wstring output_heading =
        L"Output: " + format_mode_name(g_output_format) + L"\r\n" +
        format_family_name(g_output_format, true);

    SetWindowTextW(g_input_label, input_heading.c_str());
    SetWindowTextW(g_output_label, output_heading.c_str());
}

void set_menu_check(HMENU menu, int id, bool checked) {
    if (!menu) {
        return;
    }
    CheckMenuItem(menu, id,
                  MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
}

void update_format_menus() {
    set_menu_check(g_input_menu, ID_INPUT_AUTO_DETECT,
                   g_input_format == GuiFormat::AutoDetect);
    set_menu_check(g_input_raw_menu, ID_INPUT_RAW_FCD,
                   g_input_format == GuiFormat::FcdRaw);
    set_menu_check(g_input_raw_menu, ID_INPUT_RAW_GSA,
                   g_input_format == GuiFormat::GameSharkRaw);
    set_menu_check(g_input_raw_menu, ID_INPUT_RAW_ARMAX,
                   g_input_format == GuiFormat::ActionReplayMaxRaw);
    set_menu_check(g_input_raw_menu, ID_INPUT_RAW_EZ,
                   g_input_format == GuiFormat::EzFlash);
    set_menu_check(g_input_encrypted_menu, ID_INPUT_ENCRYPTED_FCD,
                   g_input_format == GuiFormat::FcdEncrypted);
    set_menu_check(g_input_encrypted_menu, ID_INPUT_ENCRYPTED_GSA,
                   g_input_format == GuiFormat::GameSharkEncrypted);
    set_menu_check(g_input_encrypted_menu, ID_INPUT_ENCRYPTED_ARMAX,
                   g_input_format == GuiFormat::ActionReplayMaxEncrypted);

    set_menu_check(g_output_raw_menu, ID_OUTPUT_RAW_FCD,
                   g_output_format == GuiFormat::FcdRaw);
    set_menu_check(g_output_raw_menu, ID_OUTPUT_RAW_GSA,
                   g_output_format == GuiFormat::GameSharkRaw);
    set_menu_check(g_output_raw_menu, ID_OUTPUT_RAW_ARMAX,
                   g_output_format == GuiFormat::ActionReplayMaxRaw);
    set_menu_check(g_output_raw_menu, ID_OUTPUT_RAW_EZ,
                   g_output_format == GuiFormat::EzFlash);
    set_menu_check(g_output_encrypted_menu, ID_OUTPUT_ENCRYPTED_FCD,
                   g_output_format == GuiFormat::FcdEncrypted);
    set_menu_check(g_output_encrypted_menu, ID_OUTPUT_ENCRYPTED_GSA,
                   g_output_format == GuiFormat::GameSharkEncrypted);
    set_menu_check(g_output_encrypted_menu, ID_OUTPUT_ENCRYPTED_ARMAX,
                   g_output_format == GuiFormat::ActionReplayMaxEncrypted);
}

bool input_seed_controls_relevant() {
    return is_fcd_encrypted(g_input_format) ||
           (g_input_format == GuiFormat::AutoDetect &&
            g_last_detected_input &&
            is_fcd_encrypted(*g_last_detected_input));
}

bool manual_input_key_enabled() {
    return SendMessageW(g_input_manual_key, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void set_manual_input_key_enabled(bool enabled) {
    SendMessageW(g_input_manual_key, BM_SETCHECK,
                 enabled ? BST_CHECKED : BST_UNCHECKED, 0);
}

void update_input_seed_label() {
    SetWindowTextW(g_input_seed_label, L"In Key:");
}

void set_input_seed_text(std::wstring_view value) {
    g_updating_input_seed = true;
    SetWindowTextW(g_input_seed_edit, std::wstring(value).c_str());
    g_updating_input_seed = false;
}

std::string current_cleaned_input() {
    return gba::text::cleanup_gamehacking_org_blocks(
        normalize_from_edit(get_window_text(g_input_edit)));
}

void sync_input_seed_from_text(bool announce_detection) {
    if (!input_seed_controls_relevant()) {
        return;
    }

    if (manual_input_key_enabled()) {
        SendMessageW(g_input_seed_edit, EM_SETREADONLY, FALSE, 0);
        g_input_seed_source = parse_seed_edit(g_input_seed_edit)
            ? InputSeedSource::Manual
            : InputSeedSource::None;
        update_input_seed_label();
        return;
    }

    SendMessageW(g_input_seed_edit, EM_SETREADONLY, TRUE, 0);
    const std::string cleaned = current_cleaned_input();
    const gba::cmp::NormalizedInput cmp_input =
        gba::cmp::normalize_input(cleaned);
    const auto embedded = gba::codebreaker::find_embedded_seed(
        cmp_input.recognized ? cmp_input.text : cleaned);
    if (embedded) {
        const std::wstring formatted = format_seed_wide(*embedded);
        const bool changed =
            g_input_seed_source != InputSeedSource::Detected ||
            get_window_text(g_input_seed_edit) != formatted;
        set_input_seed_text(formatted);
        g_input_seed_source = InputSeedSource::Detected;
        update_input_seed_label();
        if (announce_detection && changed) {
            set_status(L"Input key detected: " + formatted);
        }
        return;
    }

    if (g_input_seed_source == InputSeedSource::Detected ||
        !get_window_text(g_input_seed_edit).empty()) {
        set_input_seed_text(L"");
    }
    g_input_seed_source = InputSeedSource::None;
    update_input_seed_label();
}

void update_seed_controls() {
    const bool input_visible = input_seed_controls_relevant();
    ShowWindow(g_input_seed_label, input_visible ? SW_SHOW : SW_HIDE);
    ShowWindow(g_input_seed_edit, input_visible ? SW_SHOW : SW_HIDE);
    ShowWindow(g_input_manual_key, input_visible ? SW_SHOW : SW_HIDE);

    EnableWindow(g_input_seed_edit, input_visible ? TRUE : FALSE);
    EnableWindow(g_input_manual_key, input_visible ? TRUE : FALSE);
    if (input_visible) {
        SendMessageW(g_input_seed_edit, EM_SETREADONLY,
                     manual_input_key_enabled() ? FALSE : TRUE, 0);
    }

    const bool output_enabled = is_fcd_encrypted(g_output_format);
    ShowWindow(g_output_seed_label, output_enabled ? SW_SHOW : SW_HIDE);
    ShowWindow(g_output_seed_edit, output_enabled ? SW_SHOW : SW_HIDE);
    EnableWindow(g_output_seed_label, output_enabled ? TRUE : FALSE);
    EnableWindow(g_output_seed_edit, output_enabled ? TRUE : FALSE);
}

void select_input_format(GuiFormat format) {
    g_input_format = format;
    g_last_detected_input.reset();
    update_format_menus();
    update_format_labels();
    update_seed_controls();
    sync_input_seed_from_text(false);
    set_status(L"Input format: " + format_name(format, false));
    maybe_auto_convert();
}

void select_output_format(GuiFormat format) {
    g_output_format = format;
    update_format_menus();
    update_format_labels();
    update_seed_controls();
    set_status(L"Output format: " + format_name(format, true));
    maybe_auto_convert();
}

void update_auto_convert_menu() {
    CheckMenuItem(g_main_menu, ID_OPTIONS_AUTO_CONVERT,
                  MF_BYCOMMAND | (g_auto_convert ? MF_CHECKED : MF_UNCHECKED));
}

void update_cmp_output_menu() {
    CheckMenuItem(g_main_menu, ID_OPTIONS_CMP_OUTPUT,
                  MF_BYCOMMAND | (g_cmp_output ? MF_CHECKED : MF_UNCHECKED));
}

void update_word_wrap_menus() {
    CheckMenuItem(
        g_main_menu, ID_EDIT_INPUT_WORD_WRAP,
        MF_BYCOMMAND |
            (g_input_word_wrap ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(
        g_main_menu, ID_EDIT_OUTPUT_WORD_WRAP,
        MF_BYCOMMAND |
            (g_output_word_wrap ? MF_CHECKED : MF_UNCHECKED));
}

void update_ezflash_mode_menu() {
    if (!g_ezflash_menu) {
        return;
    }

    const UINT selected =
        g_ezflash_mode == gba::ezflash::Mode::Original
            ? ID_OPTIONS_EZ_ORIGINAL
            : ID_OPTIONS_EZ_ENHANCED;

    CheckMenuRadioItem(
        g_ezflash_menu,
        ID_OPTIONS_EZ_ORIGINAL,
        ID_OPTIONS_EZ_ENHANCED,
        selected,
        MF_BYCOMMAND);
    update_format_labels();
}


} // namespace gba::gui
