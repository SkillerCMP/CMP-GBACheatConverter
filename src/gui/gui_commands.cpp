#include "gui/gui_state.hpp"

namespace gba::gui {

LRESULT handle_command(WPARAM wparam, LPARAM lparam) {
    const int id = LOWORD(wparam);
    const int notification = HIWORD(wparam);

    if ((id == ID_INPUT_EDIT || id == ID_OUTPUT_EDIT) &&
        notification == EN_SETFOCUS) {
        g_last_code_editor =
            id == ID_OUTPUT_EDIT ? g_output_edit : g_input_edit;
        return 0;
    }
    if (id == ID_INPUT_EDIT && notification == EN_CHANGE) {
        if (!g_in_convert && g_input_format == GuiFormat::AutoDetect) {
            g_last_detected_input.reset();
            update_format_labels();
        }
        if (!g_in_convert) {
            update_seed_controls();
            sync_input_seed_from_text(true);
        }
        maybe_auto_convert();
        return 0;
    }
    if (id == ID_INPUT_SEED_EDIT && notification == EN_CHANGE) {
        if (!g_updating_input_seed && manual_input_key_enabled()) {
            g_input_seed_source = parse_seed_edit(g_input_seed_edit)
                ? InputSeedSource::Manual
                : InputSeedSource::None;
            update_input_seed_label();
            maybe_auto_convert();
        }
        return 0;
    }
    if (id == ID_INPUT_MANUAL_KEY && notification == BN_CLICKED) {
        const bool manual = manual_input_key_enabled();
        if (manual) {
            SendMessageW(g_input_seed_edit, EM_SETREADONLY, FALSE, 0);
            g_input_seed_source = parse_seed_edit(g_input_seed_edit)
                ? InputSeedSource::Manual
                : InputSeedSource::None;
            update_input_seed_label();
            set_status(L"Use enabled. Every input code row will be "
                       L"decrypted with In Key.");
            SetFocus(g_input_seed_edit);
        } else {
            sync_input_seed_from_text(true);
            set_status(g_input_seed_source == InputSeedSource::Detected
                ? L"Use disabled; embedded input key restored."
                : L"Use disabled; no embedded input key detected.");
        }
        update_seed_controls();
        maybe_auto_convert();
        return 0;
    }
    if (id == ID_OUTPUT_SEED_EDIT && notification == EN_CHANGE) {
        maybe_auto_convert();
        return 0;
    }


    switch (id) {
    case ID_INPUT_AUTO_DETECT:
        select_input_format(GuiFormat::AutoDetect);
        return 0;
    case ID_INPUT_RAW_FCD:
        select_input_format(GuiFormat::FcdRaw);
        return 0;
    case ID_INPUT_RAW_GSA:
        select_input_format(GuiFormat::GameSharkRaw);
        return 0;
    case ID_INPUT_RAW_ARMAX:
        select_input_format(GuiFormat::ActionReplayMaxRaw);
        return 0;
    case ID_INPUT_RAW_EZ:
        select_input_format(GuiFormat::EzFlash);
        return 0;
    case ID_INPUT_ENCRYPTED_FCD:
        select_input_format(GuiFormat::FcdEncrypted);
        return 0;
    case ID_INPUT_ENCRYPTED_GSA:
        select_input_format(GuiFormat::GameSharkEncrypted);
        return 0;
    case ID_INPUT_ENCRYPTED_ARMAX:
        select_input_format(GuiFormat::ActionReplayMaxEncrypted);
        return 0;
    case ID_OUTPUT_RAW_FCD:
        select_output_format(GuiFormat::FcdRaw);
        return 0;
    case ID_OUTPUT_RAW_GSA:
        select_output_format(GuiFormat::GameSharkRaw);
        return 0;
    case ID_OUTPUT_RAW_ARMAX:
        select_output_format(GuiFormat::ActionReplayMaxRaw);
        return 0;
    case ID_OUTPUT_RAW_EZ:
        select_output_format(GuiFormat::EzFlash);
        return 0;
    case ID_OUTPUT_ENCRYPTED_FCD:
        select_output_format(GuiFormat::FcdEncrypted);
        return 0;
    case ID_OUTPUT_ENCRYPTED_GSA:
        select_output_format(GuiFormat::GameSharkEncrypted);
        return 0;
    case ID_OUTPUT_ENCRYPTED_ARMAX:
        select_output_format(GuiFormat::ActionReplayMaxEncrypted);
        return 0;
    default:
        break;
    }

    switch (id) {
    case ID_SWAP:
        swap_input_output();
        return 0;
    case ID_CONVERT:
        perform_conversion(true);
        return 0;
    case ID_FILE_OPEN:
        open_input_file();
        return 0;
    case ID_FILE_SAVE:
        save_output_file();
        return 0;
    case ID_FILE_SAVE_EZFLASH_CHT:
    case ID_FILE_SAVE_ARMAX_DSC:
    case ID_FILE_SAVE_VBA_CLT:
    case ID_FILE_SAVE_MYBOY_CHT:
    case ID_FILE_SAVE_MISTER_ZIP:
    case ID_FILE_SAVE_MEDNAFEN_CHT:
    case ID_FILE_SAVE_MGBA_CHEATS:
    case ID_FILE_SAVE_LIBRETRO_CHT:
        save_native_output(id);
        return 0;
    case ID_FILE_EXIT:
        DestroyWindow(g_main);
        return 0;
    case ID_EDIT_CUT:
        SendMessageW(focused_edit_control(), WM_CUT, 0, 0);
        return 0;
    case ID_EDIT_COPY:
        SendMessageW(focused_edit_control(), WM_COPY, 0, 0);
        return 0;
    case ID_EDIT_PASTE:
        SendMessageW(focused_edit_control(), WM_PASTE, 0, 0);
        return 0;
    case ID_EDIT_CLEAR:
        clear_focused_code_editor();
        return 0;
    case ID_EDIT_SELECT_ALL:
        SendMessageW(focused_edit_control(), EM_SETSEL, 0, -1);
        return 0;
    case ID_OPTIONS_AUTO_CONVERT:
        g_auto_convert = !g_auto_convert;
        update_auto_convert_menu();
        set_status(g_auto_convert ? L"Auto Convert enabled."
                                  : L"Auto Convert disabled.");
        if (g_auto_convert) {
            maybe_auto_convert();
        }
        return 0;
    case ID_OPTIONS_EZ_ORIGINAL:
        g_ezflash_mode = gba::ezflash::Mode::Original;
        update_ezflash_mode_menu();
        set_status(L"EZ-Flash mode: Original (ON= only).");
        if (g_auto_convert && g_output_format == GuiFormat::EzFlash) {
            maybe_auto_convert();
        }
        return 0;
    case ID_OPTIONS_EZ_ENHANCED:
        g_ezflash_mode = gba::ezflash::Mode::Enhanced;
        update_ezflash_mode_menu();
        set_status(L"EZ-Flash mode: Enhanced v3.");
        if (g_auto_convert && g_output_format == GuiFormat::EzFlash) {
            maybe_auto_convert();
        }
        return 0;
    case ID_HELP_ABOUT:
        MessageBoxW(
            g_main,
            L"GBA Cheat Converter v" GBA_CHEAT_VERSION_W,
            L"About GBA Cheat Converter", MB_OK | MB_ICONINFORMATION);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(g_main, WM_COMMAND, wparam, lparam);
}

} // namespace gba::gui
