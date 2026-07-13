#include "gui/gui_state.hpp"

namespace gba::gui {

HMENU create_main_menu() {
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU edit = CreatePopupMenu();
    HMENU input = CreatePopupMenu();
    HMENU output = CreatePopupMenu();
    g_input_menu = input;
    HMENU options = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    g_input_raw_menu = CreatePopupMenu();
    g_input_encrypted_menu = CreatePopupMenu();
    g_output_raw_menu = CreatePopupMenu();
    g_output_encrypted_menu = CreatePopupMenu();
    g_ezflash_menu = CreatePopupMenu();
    g_save_output_as_menu = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, ID_FILE_OPEN, L"&Open Input...\tCtrl+O");
    AppendMenuW(g_save_output_as_menu, MF_STRING, ID_FILE_SAVE,
                L"Current Output Text...\tCtrl+S");
    AppendMenuW(g_save_output_as_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_EZFLASH_CHT, L"EZ-Flash (.cht)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_ARMAX_DSC, L"Action Replay MAX (.dsc)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_VBA_CLT, L"VisualBoy Advance (.clt)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_MYBOY_CHT, L"My Boy! (.cht)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_MISTER_ZIP, L"MiSTer (.zip)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_MEDNAFEN_CHT, L"Mednafen (.cht)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_MGBA_CHEATS, L"mGBA (.cheats)...");
    AppendMenuW(g_save_output_as_menu, MF_STRING,
                ID_FILE_SAVE_LIBRETRO_CHT,
                L"Libretro / RetroArch (.cht)...");
    AppendMenuW(file, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_save_output_as_menu),
                L"&Save Output As");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_EXIT, L"E&xit");

    AppendMenuW(edit, MF_STRING, ID_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(edit, MF_STRING, ID_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(edit, MF_STRING, ID_EDIT_PASTE,
                L"&Paste\tCtrl+V / Ctrl+P");
    AppendMenuW(edit, MF_STRING, ID_EDIT_CLEAR, L"C&lear\tCtrl+D");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, ID_EDIT_SELECT_ALL, L"Select &All\tCtrl+A");

    const wchar_t* fcd = L"CodeBreaker / GameShark SP / Xploder Advance";
    const wchar_t* gsa = L"GameShark Advance / Action Replay GBX";
    const wchar_t* armax = L"Action Replay MAX";

    AppendMenuW(input, MF_STRING, ID_INPUT_AUTO_DETECT, L"&Auto Detect");
    AppendMenuW(input, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_input_raw_menu, MF_STRING, ID_INPUT_RAW_FCD, fcd);
    AppendMenuW(g_input_raw_menu, MF_STRING, ID_INPUT_RAW_GSA, gsa);
    AppendMenuW(g_input_raw_menu, MF_STRING, ID_INPUT_RAW_ARMAX, armax);
    AppendMenuW(g_input_raw_menu, MF_STRING, ID_INPUT_RAW_EZ, L"EZ-Flash");
    AppendMenuW(g_input_encrypted_menu, MF_STRING,
                ID_INPUT_ENCRYPTED_FCD, fcd);
    AppendMenuW(g_input_encrypted_menu, MF_STRING,
                ID_INPUT_ENCRYPTED_GSA, gsa);
    AppendMenuW(g_input_encrypted_menu, MF_STRING,
                ID_INPUT_ENCRYPTED_ARMAX, armax);
    AppendMenuW(input, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_input_raw_menu), L"&RAW");
    AppendMenuW(input, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_input_encrypted_menu),
                L"&Encrypted");

    AppendMenuW(g_output_raw_menu, MF_STRING, ID_OUTPUT_RAW_FCD, fcd);
    AppendMenuW(g_output_raw_menu, MF_STRING, ID_OUTPUT_RAW_GSA, gsa);
    AppendMenuW(g_output_raw_menu, MF_STRING, ID_OUTPUT_RAW_ARMAX, armax);
    AppendMenuW(g_output_raw_menu, MF_STRING, ID_OUTPUT_RAW_EZ, L"EZ-Flash");
    AppendMenuW(g_output_encrypted_menu, MF_STRING,
                ID_OUTPUT_ENCRYPTED_FCD, fcd);
    AppendMenuW(g_output_encrypted_menu, MF_STRING,
                ID_OUTPUT_ENCRYPTED_GSA, gsa);
    AppendMenuW(g_output_encrypted_menu, MF_STRING,
                ID_OUTPUT_ENCRYPTED_ARMAX, armax);
    AppendMenuW(output, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_output_raw_menu), L"&RAW");
    AppendMenuW(output, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_output_encrypted_menu),
                L"&Encrypted");

    AppendMenuW(options, MF_STRING, ID_OPTIONS_AUTO_CONVERT,
                L"&Auto Convert");
    AppendMenuW(options, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g_ezflash_menu, MF_STRING,
                ID_OPTIONS_EZ_ORIGINAL, L"&Original");
    AppendMenuW(g_ezflash_menu, MF_STRING,
                ID_OPTIONS_EZ_ENHANCED, L"&Enhanced");
    AppendMenuW(options, MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_ezflash_menu),
                L"&EZ-Flash");

    AppendMenuW(help, MF_STRING, ID_HELP_ABOUT, L"&About");

    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"&Edit");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(input), L"&Input");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(output), L"&Output");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(options), L"&Options");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"&Help");
    return bar;
}

HWND create_control(const wchar_t* class_name,
                    const wchar_t* text,
                    DWORD style,
                    DWORD ex_style,
                    int id) {
    return CreateWindowExW(
        ex_style, class_name, text,
        WS_CHILD | WS_VISIBLE | style,
        0, 0, 1, 1,
        g_main, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
}

void create_controls() {
    g_ui_font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_heading_font = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_code_font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    g_input_label = create_control(
        L"STATIC", L"Input", SS_LEFT | SS_NOPREFIX, 0, 0);
    g_output_label = create_control(
        L"STATIC", L"Output", SS_LEFT | SS_NOPREFIX, 0, 0);
    g_input_seed_label = create_control(
        L"STATIC", L"In Key:", SS_LEFT, 0, 0);
    g_input_seed_edit = create_control(
        L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP,
        WS_EX_CLIENTEDGE, ID_INPUT_SEED_EDIT);
    g_input_manual_key = create_control(
        L"BUTTON", L"Use", BS_AUTOCHECKBOX | WS_TABSTOP,
        0, ID_INPUT_MANUAL_KEY);

    g_output_seed_label = create_control(
        L"STATIC", L"Out Key:", SS_LEFT, 0, 0);
    g_output_seed_edit = create_control(
        L"EDIT", L"9ABCDEF0:1234", ES_AUTOHSCROLL | WS_TABSTOP,
        WS_EX_CLIENTEDGE, ID_OUTPUT_SEED_EDIT);

    const DWORD editor_style = ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP;
    g_input_edit = create_control(
        L"EDIT", L"", editor_style, WS_EX_CLIENTEDGE, ID_INPUT_EDIT);
    g_output_edit = create_control(
        L"EDIT", L"", editor_style, WS_EX_CLIENTEDGE, ID_OUTPUT_EDIT);

    g_input_edit_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(
            g_input_edit, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(code_editor_proc)));
    g_output_edit_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(
            g_output_edit, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(code_editor_proc)));

    g_swap = create_control(
        L"BUTTON", L"Swap", BS_PUSHBUTTON | WS_TABSTOP, 0, ID_SWAP);
    g_convert = create_control(
        L"BUTTON", L"Convert", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, ID_CONVERT);
    g_status = create_control(
        L"STATIC", L"Ready.", SS_LEFT | SS_CENTERIMAGE,
        WS_EX_STATICEDGE, 0);

    const HWND ui_controls[] = {
        g_input_seed_label, g_input_seed_edit, g_input_manual_key,
        g_output_seed_label, g_output_seed_edit,
        g_swap, g_convert, g_status
    };
    for (HWND control : ui_controls) {
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(g_ui_font), TRUE);
    }
    SendMessageW(g_input_label, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_heading_font), TRUE);
    SendMessageW(g_output_label, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_heading_font), TRUE);
    SendMessageW(g_input_edit, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_code_font), TRUE);
    SendMessageW(g_output_edit, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_code_font), TRUE);

    g_last_code_editor = g_input_edit;
    update_format_labels();
    SendMessageW(g_input_seed_edit, EM_SETLIMITTEXT, 13, 0);
    SendMessageW(g_output_seed_edit, EM_SETLIMITTEXT, 13, 0);
}

} // namespace gba::gui
