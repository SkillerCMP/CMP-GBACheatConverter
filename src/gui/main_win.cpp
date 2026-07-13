#include "gui/gui_state.hpp"

namespace {

std::vector<std::string> command_line_arguments() {
    int argument_count = 0;
    LPWSTR* wide_arguments = CommandLineToArgvW(
        GetCommandLineW(), &argument_count);
    if (!wide_arguments) {
        return {};
    }

    std::vector<std::string> arguments;
    if (argument_count > 1) {
        arguments.reserve(static_cast<std::size_t>(argument_count - 1));
    }
    for (int index = 1; index < argument_count; ++index) {
        arguments.push_back(gba::gui::wide_to_utf8(wide_arguments[index]));
    }
    LocalFree(wide_arguments);
    return arguments;
}

bool valid_standard_handle(DWORD standard_handle) {
    const HANDLE handle = GetStdHandle(standard_handle);
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

bool bind_crt_stream(FILE* stream, DWORD standard_handle, int open_flags) {
    const HANDLE source = GetStdHandle(standard_handle);
    if (source == nullptr || source == INVALID_HANDLE_VALUE) {
        return false;
    }

    HANDLE duplicate = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), source,
                         GetCurrentProcess(), &duplicate,
                         0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return false;
    }

    const int descriptor = _open_osfhandle(
        reinterpret_cast<std::intptr_t>(duplicate), open_flags);
    if (descriptor == -1) {
        CloseHandle(duplicate);
        return false;
    }

    const bool success = _dup2(descriptor, _fileno(stream)) == 0;
    _close(descriptor);
    return success;
}

void prepare_cli_standard_streams() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        const DWORD error = GetLastError();
        const bool has_existing_stream =
            valid_standard_handle(STD_INPUT_HANDLE) ||
            valid_standard_handle(STD_OUTPUT_HANDLE) ||
            valid_standard_handle(STD_ERROR_HANDLE);
        if (error != ERROR_ACCESS_DENIED && !has_existing_stream) {
            AllocConsole();
        }
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    bind_crt_stream(stdin, STD_INPUT_HANDLE, _O_RDONLY | _O_TEXT);
    bind_crt_stream(stdout, STD_OUTPUT_HANDLE, _O_WRONLY | _O_TEXT);
    bind_crt_stream(stderr, STD_ERROR_HANDLE, _O_WRONLY | _O_TEXT);
    std::ios::sync_with_stdio(true);
    std::cin.clear();
    std::cout.clear();
    std::cerr.clear();
}

} // namespace

namespace gba::gui {

LRESULT CALLBACK window_proc(HWND window, UINT message,
                             WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        g_main = window;
        g_main_menu = create_main_menu();
        SetMenu(window, g_main_menu);
        create_controls();
        g_ini_path = executable_directory() + L"\\GbaCheatConverter.ini";
        load_settings();
        DragAcceptFiles(window, TRUE);
        return 0;

    case WM_SIZE:
        layout_controls(LOWORD(lparam), HIWORD(lparam));
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = 960;
        info->ptMinTrackSize.y = 520;
        return 0;
    }

    case WM_COMMAND:
        return handle_command(wparam, lparam);

    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wparam);
        const UINT path_length = DragQueryFileW(drop, 0, nullptr, 0);
        std::vector<wchar_t> path(static_cast<std::size_t>(path_length) + 1U,
                                  L'\0');
        if (path_length > 0U &&
            DragQueryFileW(drop, 0, path.data(), path_length + 1U) > 0U) {
            load_input_path(path.data(), true);
        }
        DragFinish(drop);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lparam);
        if (std::abs(x - splitter_center(window)) <= 6) {
            g_dragging_splitter = true;
            SetCapture(window);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE:
        if (g_dragging_splitter) {
            update_splitter_from_x(GET_X_LPARAM(lparam));
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (g_dragging_splitter) {
            g_dragging_splitter = false;
            ReleaseCapture();
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        g_dragging_splitter = false;
        return 0;

    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(window, &point);
        if (std::abs(point.x - splitter_center(window)) <= 6) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));

        const int center = splitter_center(window);
        RECT splitter{center - 2, 43, center + 2,
                      std::max(43, static_cast<int>(client.bottom) - 60)};
        FillRect(dc, &splitter, GetSysColorBrush(COLOR_3DSHADOW));
        EndPaint(window, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SETFOCUS:
        SetFocus(g_input_edit);
        return 0;

    case WM_DESTROY:
        save_settings();
        DragAcceptFiles(window, FALSE);
        if (g_ui_font) {
            DeleteObject(g_ui_font);
            g_ui_font = nullptr;
        }
        if (g_heading_font) {
            DeleteObject(g_heading_font);
            g_heading_font = nullptr;
        }
        if (g_code_font) {
            DeleteObject(g_code_font);
            g_code_font = nullptr;
        }
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace gba::gui

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    using namespace gba::gui;

    std::vector<std::string> arguments = command_line_arguments();
    if (arguments.size() == 1U && arguments.front() == "--gui") {
        arguments.clear();
    }
    if (!arguments.empty()) {
        prepare_cli_standard_streams();
        const int result = gba::cli::run(
            arguments, std::cin, std::cout, std::cerr,
            "GbaCheatConverter.exe");
        std::cout.flush();
        std::cerr.flush();
        return result;
    }

    SetProcessDPIAware();

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    HICON app_icon = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    HICON app_icon_small = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (!app_icon) {
        app_icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    if (!app_icon_small) {
        app_icon_small = app_icon;
    }
    window_class.hIcon = app_icon;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    window_class.lpszClassName = kWindowClass;
    window_class.hIconSm = app_icon_small;

    if (!RegisterClassExW(&window_class)) {
        return 1;
    }

    HWND window = CreateWindowExW(
        0, kWindowClass, kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1120, 720,
        nullptr, nullptr, instance, nullptr);
    if (!window) {
        return 1;
    }

    SendMessageW(window, WM_SETICON, ICON_BIG,
                 reinterpret_cast<LPARAM>(app_icon));
    SendMessageW(window, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(app_icon_small));
    ShowWindow(window, show_command);
    UpdateWindow(window);

    ACCEL accelerators[] = {
        {FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN},
        {FVIRTKEY | FCONTROL, 'S', ID_FILE_SAVE},
        {FVIRTKEY | FCONTROL, 'A', ID_EDIT_SELECT_ALL},
        {FVIRTKEY | FCONTROL, 'C', ID_EDIT_COPY},
        {FVIRTKEY | FCONTROL, 'X', ID_EDIT_CUT},
        {FVIRTKEY | FCONTROL, 'V', ID_EDIT_PASTE},
        {FVIRTKEY | FCONTROL, 'P', ID_EDIT_PASTE},
        {FVIRTKEY | FCONTROL, 'D', ID_EDIT_CLEAR},
        {FVIRTKEY, VK_F5, ID_CONVERT}
    };
    HACCEL accelerator_table = CreateAcceleratorTableW(
        accelerators, static_cast<int>(std::size(accelerators)));

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(window, accelerator_table, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (accelerator_table) {
        DestroyAcceleratorTable(accelerator_table);
    }
    return static_cast<int>(message.wParam);
}
