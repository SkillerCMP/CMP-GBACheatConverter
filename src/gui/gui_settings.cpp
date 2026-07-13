#include "gui/gui_state.hpp"

namespace gba::gui {

void load_settings() {
    g_auto_convert = GetPrivateProfileIntW(
        L"GUI", L"AutoConvert", 0, g_ini_path.c_str()) != 0;
    g_ezflash_mode = GetPrivateProfileIntW(
        L"EZ-Flash", L"Mode", 1, g_ini_path.c_str()) == 0
        ? gba::ezflash::Mode::Original
        : gba::ezflash::Mode::Enhanced;
    const int ratio = static_cast<int>(GetPrivateProfileIntW(
        L"GUI", L"SplitterPercent", 50, g_ini_path.c_str()));
    g_split_ratio = std::clamp(ratio / 100.0, 0.25, 0.75);

    const auto map_legacy_format = [](int legacy) {
        switch (legacy) {
        case 0:
        case 6:
            return GuiFormat::FcdRaw;
        case 1:
        case 7:
            return GuiFormat::FcdEncrypted;
        case 2:
            return GuiFormat::GameSharkRaw;
        case 3:
            return GuiFormat::GameSharkEncrypted;
        case 4:
            return GuiFormat::ActionReplayMaxRaw;
        case 5:
            return GuiFormat::ActionReplayMaxEncrypted;
        case 8:
            return GuiFormat::EzFlash;
        case -1:
        default:
            return GuiFormat::AutoDetect;
        }
    };

    const int saved_input = static_cast<int>(GetPrivateProfileIntW(
        L"Formats", L"InputMenu", static_cast<UINT>(-1),
        g_ini_path.c_str()));
    const int saved_output = static_cast<int>(GetPrivateProfileIntW(
        L"Formats", L"OutputMenu", static_cast<UINT>(-1),
        g_ini_path.c_str()));

    if (saved_input >= 0 && saved_input <= 7) {
        g_input_format = static_cast<GuiFormat>(saved_input);
    } else {
        g_input_format = map_legacy_format(static_cast<int>(
            GetPrivateProfileIntW(
                L"Formats", L"Input", static_cast<UINT>(-1),
                g_ini_path.c_str())));
    }

    if (saved_output >= 0 && saved_output <= 6) {
        g_output_format = static_cast<GuiFormat>(saved_output);
    } else {
        g_output_format = map_legacy_format(static_cast<int>(
            GetPrivateProfileIntW(
                L"Formats", L"Output", 8, g_ini_path.c_str())));
    }

    wchar_t seed[64] = L"9ABCDEF0:1234";
    GetPrivateProfileStringW(L"CodeBreaker", L"Seed",
                             L"9ABCDEF0:1234", seed, 64,
                             g_ini_path.c_str());
    SetWindowTextW(g_output_seed_edit, seed);

    update_auto_convert_menu();
    update_ezflash_mode_menu();
    update_format_menus();
    update_format_labels();
    update_seed_controls();
}

void save_settings() {
    WritePrivateProfileStringW(
        L"GUI", L"AutoConvert", g_auto_convert ? L"1" : L"0",
        g_ini_path.c_str());
    WritePrivateProfileStringW(
        L"GUI", L"SplitterPercent",
        std::to_wstring(static_cast<int>(g_split_ratio * 100.0)).c_str(),
        g_ini_path.c_str());
    WritePrivateProfileStringW(
        L"Formats", L"InputMenu",
        std::to_wstring(input_format_index()).c_str(), g_ini_path.c_str());
    WritePrivateProfileStringW(
        L"Formats", L"OutputMenu",
        std::to_wstring(output_format_index()).c_str(), g_ini_path.c_str());
    WritePrivateProfileStringW(
        L"CodeBreaker", L"Seed", get_window_text(g_output_seed_edit).c_str(),
        g_ini_path.c_str());
    WritePrivateProfileStringW(
        L"EZ-Flash", L"Mode",
        g_ezflash_mode == gba::ezflash::Mode::Original ? L"0" : L"1",
        g_ini_path.c_str());
}

} // namespace gba::gui
