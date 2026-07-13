#include "gui/gui_state.hpp"

namespace gba::gui {

std::wstring utf8_to_wide(std::string_view input) {
    if (input.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
        static_cast<int>(input.size()), nullptr, 0);
    if (required <= 0) {
        const int ansi_required = MultiByteToWideChar(
            CP_ACP, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
        if (ansi_required <= 0) {
            return {};
        }
        std::wstring result(static_cast<std::size_t>(ansi_required), L'\0');
        MultiByteToWideChar(CP_ACP, 0, input.data(),
                            static_cast<int>(input.size()),
                            result.data(), ansi_required);
        return result;
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                        static_cast<int>(input.size()),
                        result.data(), required);
    return result;
}

std::string wide_to_utf8(std::wstring_view input) {
    if (input.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(),
                        static_cast<int>(input.size()),
                        result.data(), required, nullptr, nullptr);
    return result;
}

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
        arguments.push_back(wide_to_utf8(wide_arguments[index]));
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

std::wstring normalize_for_edit(std::string_view input) {
    const std::string formatted =
        gba::text::cleanup_gamehacking_org_blocks(input);
    return utf8_to_wide(
        gba::text::normalize_newlines_crlf(formatted));
}

std::wstring normalize_wide_for_edit(std::wstring_view input) {
    return normalize_for_edit(wide_to_utf8(input));
}

std::string normalize_from_edit(std::wstring_view input) {
    std::wstring normalized;
    normalized.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] == L'\r') {
            normalized.push_back(L'\n');
            if (index + 1U < input.size() && input[index + 1U] == L'\n') {
                ++index;
            }
        } else {
            normalized.push_back(input[index]);
        }
    }
    return wide_to_utf8(normalized);
}

std::wstring get_window_text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1U, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(copied > 0 ? static_cast<std::size_t>(copied) : 0U);
    return text;
}

void set_editor_text(HWND editor, std::string_view text) {
    const std::wstring normalized = normalize_for_edit(text);
    SetWindowTextW(editor, normalized.c_str());
}

bool paste_normalized_clipboard(HWND editor) {
    if (!OpenClipboard(editor)) {
        return false;
    }

    std::wstring clipboard_text;
    if (HANDLE unicode_data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* locked =
                static_cast<const wchar_t*>(GlobalLock(unicode_data))) {
            clipboard_text = locked;
            GlobalUnlock(unicode_data);
        }
    } else if (HANDLE ansi_data = GetClipboardData(CF_TEXT)) {
        if (const auto* locked =
                static_cast<const char*>(GlobalLock(ansi_data))) {
            clipboard_text = utf8_to_wide(locked);
            GlobalUnlock(ansi_data);
        }
    }

    CloseClipboard();

    if (clipboard_text.empty()) {
        return false;
    }

    const std::wstring normalized =
        normalize_wide_for_edit(clipboard_text);
    SendMessageW(
        editor, EM_REPLACESEL, TRUE,
        reinterpret_cast<LPARAM>(normalized.c_str()));
    return true;
}

LRESULT CALLBACK code_editor_proc(HWND window,
                                  UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam) {
    WNDPROC original = window == g_input_edit
        ? g_input_edit_proc
        : g_output_edit_proc;

    if (message == WM_PASTE && paste_normalized_clipboard(window)) {
        return 0;
    }

    if (message == WM_SETTEXT && lparam != 0) {
        const auto* incoming =
            reinterpret_cast<const wchar_t*>(lparam);
        const std::wstring normalized =
            normalize_wide_for_edit(incoming);
        return CallWindowProcW(
            original, window, message, wparam,
            reinterpret_cast<LPARAM>(normalized.c_str()));
    }

    return CallWindowProcW(
        original, window, message, wparam, lparam);
}

void set_status(std::wstring_view status) {
    SetWindowTextW(g_status, std::wstring(status).c_str());
}

std::wstring executable_directory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return L".";
    }
    path.resize(length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::optional<gba::codebreaker::Seed> parse_seed_edit(HWND edit) {
    return gba::codebreaker::parse_seed_text(
        wide_to_utf8(get_window_text(edit)));
}

std::wstring format_seed_wide(gba::codebreaker::Seed seed) {
    return utf8_to_wide(gba::codebreaker::format_seed(seed));
}

} // namespace gba::gui
