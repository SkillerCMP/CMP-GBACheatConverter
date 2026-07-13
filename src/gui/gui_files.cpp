#include "gui/gui_state.hpp"

namespace gba::gui {

bool read_binary_file(const std::wstring& path,
                      std::vector<std::uint8_t>& output) {
    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input) {
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        return false;
    }
    if (static_cast<std::uintmax_t>(size) >
        static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max())) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(size));
    if (output.empty()) {
        return true;
    }
    input.read(reinterpret_cast<char*>(output.data()),
               static_cast<std::streamsize>(output.size()));
    return input.gcount() == static_cast<std::streamsize>(output.size());
}

bool write_file(const std::wstring& path, std::string_view data) {
    std::ofstream output(std::filesystem::path(path),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(output);
}


bool write_binary_file(const std::wstring& path,
                       const std::vector<std::uint8_t>& data) {
    std::ofstream output(std::filesystem::path(path),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    if (!data.empty()) {
        output.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(output);
}

gba::CheatDocument parse_current_output_document() {
    const std::string output = gba::text::cleanup_gamehacking_org_blocks(
        normalize_from_edit(get_window_text(g_output_edit)));
    if (gba::text::trim(output).empty()) {
        throw std::runtime_error(
            "The output window is empty. Convert or paste output first.");
    }

    switch (g_output_format) {
    case GuiFormat::FcdRaw:
        return gba::codebreaker::parse(output, {false});
    case GuiFormat::FcdEncrypted:
        return gba::codebreaker::parse(output, {true});
    case GuiFormat::GameSharkRaw:
        return gba::gameshark::parse(output, {false});
    case GuiFormat::GameSharkEncrypted:
        return gba::gameshark::parse(output, {true});
    case GuiFormat::ActionReplayMaxRaw:
        return gba::armax::parse(output, {false});
    case GuiFormat::ActionReplayMaxEncrypted:
        return gba::armax::parse(output, {true});
    case GuiFormat::EzFlash:
        return gba::ezflash::parse(output);
    case GuiFormat::AutoDetect:
        break;
    }
    throw std::runtime_error("The current output format cannot be parsed.");
}

std::optional<std::string> md5_file(const std::wstring& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<std::uint8_t> object;
    std::array<std::uint8_t, 16U> digest{};

    auto cleanup = [&]() {
        if (hash) {
            BCryptDestroyHash(hash);
            hash = nullptr;
        }
        if (algorithm) {
            BCryptCloseAlgorithmProvider(algorithm, 0U);
            algorithm = nullptr;
        }
    };

    if (BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0U) < 0) {
        cleanup();
        return std::nullopt;
    }

    DWORD object_size = 0U;
    DWORD copied = 0U;
    if (BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
            &copied, 0U) < 0 || object_size == 0U) {
        cleanup();
        return std::nullopt;
    }
    object.resize(object_size);
    if (BCryptCreateHash(
            algorithm, &hash, object.data(),
            static_cast<ULONG>(object.size()), nullptr, 0U, 0U) < 0) {
        cleanup();
        return std::nullopt;
    }

    std::ifstream input(std::filesystem::path(path), std::ios::binary);
    if (!input) {
        cleanup();
        return std::nullopt;
    }
    std::array<char, 65536U> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 && BCryptHashData(
                hash, reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(count), 0U) < 0) {
            cleanup();
            return std::nullopt;
        }
    }
    if (!input.eof()) {
        cleanup();
        return std::nullopt;
    }
    if (BCryptFinishHash(hash, digest.data(),
                         static_cast<ULONG>(digest.size()), 0U) < 0) {
        cleanup();
        return std::nullopt;
    }
    cleanup();

    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0');
    for (std::uint8_t byte : digest) {
        out << std::setw(2) << static_cast<unsigned>(byte);
    }
    return out.str();
}

struct NativeSaveSpec {
    gba::output_modes::Format format;
    const wchar_t* name;
    const wchar_t* title;
    const wchar_t* filter;
    const wchar_t* extension;
    const wchar_t* default_name;
};

std::optional<NativeSaveSpec> native_save_spec(int command) {
    switch (command) {
    case ID_FILE_SAVE_EZFLASH_CHT:
        return NativeSaveSpec{
            gba::output_modes::Format::EzFlashCht,
            L"EZ-Flash",
            L"Save EZ-Flash cheat file",
            L"EZ-Flash cheat files\0*.cht\0All files\0*.*\0",
            L"cht", L"converted.cht"};
    case ID_FILE_SAVE_ARMAX_DSC:
        return NativeSaveSpec{
            gba::output_modes::Format::ArmaxDsc,
            L"Action Replay MAX",
            L"Save Action Replay MAX file",
            L"Action Replay MAX files\0*.dsc\0All files\0*.*\0",
            L"dsc", L"converted.dsc"};
    case ID_FILE_SAVE_VBA_CLT:
        return NativeSaveSpec{
            gba::output_modes::Format::VisualBoyAdvanceClt,
            L"VisualBoy Advance",
            L"Save VisualBoy Advance cheat list",
            L"VisualBoy Advance cheat lists\0*.clt\0All files\0*.*\0",
            L"clt", L"converted.clt"};
    case ID_FILE_SAVE_MYBOY_CHT:
        return NativeSaveSpec{
            gba::output_modes::Format::MyBoyCht,
            L"My Boy!",
            L"Save My Boy! cheat file",
            L"My Boy! cheat files\0*.cht\0All files\0*.*\0",
            L"cht", L"converted.cht"};
    case ID_FILE_SAVE_MISTER_ZIP:
        return NativeSaveSpec{
            gba::output_modes::Format::MisterZip,
            L"MiSTer",
            L"Save MiSTer cheat archive",
            L"MiSTer cheat archives\0*.zip\0All files\0*.*\0",
            L"zip", L"converted.zip"};
    case ID_FILE_SAVE_MEDNAFEN_CHT:
        return NativeSaveSpec{
            gba::output_modes::Format::MednafenCht,
            L"Mednafen",
            L"Save Mednafen cheat file",
            L"Mednafen cheat files\0*.cht\0All files\0*.*\0",
            L"cht", L"converted.cht"};
    case ID_FILE_SAVE_MGBA_CHEATS:
        return NativeSaveSpec{
            gba::output_modes::Format::MgbaCheats,
            L"mGBA",
            L"Save mGBA cheat file",
            L"mGBA cheat files\0*.cheats\0All files\0*.*\0",
            L"cheats", L"converted.cheats"};
    case ID_FILE_SAVE_LIBRETRO_CHT:
        return NativeSaveSpec{
            gba::output_modes::Format::LibretroCht,
            L"Libretro / RetroArch",
            L"Save Libretro / RetroArch cheat file",
            L"Libretro cheat files\0*.cht\0All files\0*.*\0",
            L"cht", L"converted.cht"};
    default:
        return std::nullopt;
    }
}

void show_native_save_warnings(
    const gba::output_modes::Result& result,
    std::wstring_view format_name) {
    if (result.warnings.empty()) {
        set_status(L"Saved " + std::wstring(format_name) + L" output.");
        return;
    }

    std::wstring message =
        L"Saved " + std::to_wstring(result.exported_entries) +
        L" compatible cheat entr" +
        (result.exported_entries == 1U ? L"y" : L"ies") + L".\r\n\r\n";
    const std::size_t limit = std::min<std::size_t>(10U, result.warnings.size());
    for (std::size_t index = 0; index < limit; ++index) {
        message += L"- " + utf8_to_wide(result.warnings[index]) + L"\r\n";
    }
    if (result.warnings.size() > limit) {
        message += L"- ...and " +
            std::to_wstring(result.warnings.size() - limit) +
            L" more warning(s).";
    }
    MessageBoxW(g_main, message.c_str(), L"Save completed with warnings",
                MB_OK | MB_ICONWARNING);
    set_status(L"Saved output with compatibility warnings.");
}

void save_native_output(int command) {
    const auto spec = native_save_spec(command);
    if (!spec) {
        return;
    }

    try {
        gba::CheatDocument document = parse_current_output_document();
        if (document.entries.empty()) {
            throw std::runtime_error(
                "No compatible cheat entries were found in the output window.");
        }

        std::vector<wchar_t> path(32768U, L'\0');
        std::copy(spec->default_name,
                  spec->default_name + std::wcslen(spec->default_name),
                  path.begin());
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_main;
        dialog.lpstrTitle = spec->title;
        dialog.lpstrFilter = spec->filter;
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.lpstrDefExt = spec->extension;
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST |
                       OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&dialog)) {
            return;
        }

        gba::output_modes::Options options;
        options.game_name = wide_to_utf8(
            std::filesystem::path(path.data()).stem().wstring());
        options.ezflash_mode =
            g_ezflash_mode == gba::ezflash::Mode::Original
                ? gba::output_modes::EzFlashMode::Original
                : gba::output_modes::EzFlashMode::Enhanced;

        if (spec->format == gba::output_modes::Format::MednafenCht) {
            std::vector<wchar_t> rom_path(32768U, L'\0');
            OPENFILENAMEW rom_dialog{};
            rom_dialog.lStructSize = sizeof(rom_dialog);
            rom_dialog.hwndOwner = g_main;
            rom_dialog.lpstrTitle =
                L"Select the matching GBA ROM for the Mednafen MD5";
            rom_dialog.lpstrFilter =
                L"Game Boy Advance ROMs\0*.gba\0All files\0*.*\0";
            rom_dialog.lpstrFile = rom_path.data();
            rom_dialog.nMaxFile = static_cast<DWORD>(rom_path.size());
            rom_dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                               OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
            if (!GetOpenFileNameW(&rom_dialog)) {
                set_status(L"Mednafen save cancelled: no ROM selected.");
                return;
            }
            const auto digest = md5_file(rom_path.data());
            if (!digest) {
                throw std::runtime_error(
                    "The selected ROM could not be read for its MD5.");
            }
            options.rom_md5 = *digest;
            options.game_name = wide_to_utf8(
                std::filesystem::path(rom_path.data()).stem().wstring());
        }

        gba::output_modes::Result result =
            gba::output_modes::export_document(document, spec->format, options);
        if (spec->format != gba::output_modes::Format::EzFlashCht) {
            result.warnings.insert(result.warnings.begin(),
                                   document.warnings.begin(),
                                   document.warnings.end());
        }
        if (!result.success || result.data.empty()) {
            std::string error = "No compatible cheats could be saved.";
            if (!result.warnings.empty()) {
                error += "\n\n" + result.warnings.front();
            }
            throw std::runtime_error(error);
        }
        if (!write_binary_file(path.data(), result.data)) {
            throw std::runtime_error("The output file could not be saved.");
        }
        std::wstring saved_format = spec->name;
        if (spec->format == gba::output_modes::Format::EzFlashCht) {
            saved_format += g_ezflash_mode == gba::ezflash::Mode::Original
                ? L" Original"
                : L" Enhanced";
        }
        show_native_save_warnings(result, saved_format);
    } catch (const std::exception& error) {
        const std::wstring message = utf8_to_wide(error.what());
        MessageBoxW(g_main, message.c_str(), L"Save failed",
                    MB_OK | MB_ICONERROR);
        set_status(L"Save failed: " + message);
    }
}


std::optional<GuiFormat> gui_format_from_native_input(
    gba::native_input::InputFormat format) {
    switch (format) {
    case gba::native_input::InputFormat::FcdRaw:
        return GuiFormat::FcdRaw;
    case gba::native_input::InputFormat::FcdEncrypted:
        return GuiFormat::FcdEncrypted;
    case gba::native_input::InputFormat::GameSharkRaw:
        return GuiFormat::GameSharkRaw;
    case gba::native_input::InputFormat::GameSharkEncrypted:
        return GuiFormat::GameSharkEncrypted;
    case gba::native_input::InputFormat::ActionReplayMaxRaw:
        return GuiFormat::ActionReplayMaxRaw;
    case gba::native_input::InputFormat::ActionReplayMaxEncrypted:
        return GuiFormat::ActionReplayMaxEncrypted;
    case gba::native_input::InputFormat::EzFlash:
        return GuiFormat::EzFlash;
    }
    return std::nullopt;
}

void show_native_import_warnings(
    const gba::native_input::Result& imported) {
    if (imported.warnings.empty()) return;
    std::wstring message =
        L"The file was imported with " +
        std::to_wstring(imported.warnings.size()) + L" warning(s):\r\n\r\n";
    const std::size_t limit =
        std::min<std::size_t>(10U, imported.warnings.size());
    for (std::size_t index = 0U; index < limit; ++index) {
        message += L"- " + utf8_to_wide(imported.warnings[index]) + L"\r\n";
    }
    if (imported.warnings.size() > limit) {
        message += L"- ...and " +
            std::to_wstring(imported.warnings.size() - limit) +
            L" more warning(s).";
    }
    MessageBoxW(g_main, message.c_str(), L"Import completed with warnings",
                MB_OK | MB_ICONWARNING);
}

bool load_input_path(const std::wstring& path, bool dropped) {
    std::vector<std::uint8_t> bytes;
    if (!read_binary_file(path, bytes)) {
        MessageBoxW(g_main,
                    dropped ? L"The dropped file could not be opened."
                            : L"The selected file could not be opened.",
                    L"Open failed", MB_OK | MB_ICONERROR);
        return false;
    }

    const std::string filename = wide_to_utf8(
        std::filesystem::path(path).filename().wstring());
    const gba::native_input::Result imported =
        gba::native_input::import_file(filename, bytes);
    if (imported.recognized) {
        if (!imported.success) {
            const std::wstring message = imported.warnings.empty()
                ? L"The native cheat file could not be imported safely."
                : utf8_to_wide(imported.warnings.front());
            MessageBoxW(g_main, message.c_str(), L"Import failed",
                        MB_OK | MB_ICONERROR);
            set_status(L"Import failed: " + message);
            return false;
        }
        const auto format = gui_format_from_native_input(imported.input_format);
        if (!format) {
            MessageBoxW(g_main,
                        L"The imported file resolved to an unsupported input format.",
                        L"Import failed", MB_OK | MB_ICONERROR);
            return false;
        }

        g_in_convert = true;
        g_input_format = *format;
        g_last_detected_input.reset();
        set_manual_input_key_enabled(false);
        g_input_seed_source = InputSeedSource::None;
        set_editor_text(g_input_edit, imported.text);
        update_format_menus();
        update_format_labels();
        update_seed_controls();
        sync_input_seed_from_text(false);
        g_in_convert = false;

        set_status(L"Imported " + utf8_to_wide(imported.source_name) +
                   L" as " + format_name(*format, false) + L".");
        show_native_import_warnings(imported);
        maybe_auto_convert();
        return true;
    }

    if (std::find(bytes.begin(), bytes.end(), std::uint8_t{0}) != bytes.end()) {
        MessageBoxW(g_main,
                    L"The file is binary but is not a supported native cheat format.",
                    L"Unsupported input file", MB_OK | MB_ICONERROR);
        set_status(L"Unsupported binary input file.");
        return false;
    }

    const std::string data = gba::text::strip_utf8_bom(
        std::string(bytes.begin(), bytes.end()));
    set_editor_text(g_input_edit, data);
    set_status(dropped ? L"Dropped input file loaded."
                       : L"Loaded input file.");
    maybe_auto_convert();
    return true;
}

void open_input_file() {
    std::vector<wchar_t> path(32768U, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter =
        L"Supported cheat files\0*.txt;*.cht;*.ini;*.dsc;*.clt;*.cheats;*.zip\0"
        L"Native Save Output As files\0*.cht;*.dsc;*.clt;*.cheats;*.zip\0"
        L"All files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    load_input_path(path.data(), false);
}

void save_output_file() {
    std::vector<wchar_t> path(32768U, L'\0');
    const std::wstring default_name = g_output_format == GuiFormat::EzFlash
        ? L"converted.cht" : L"converted.txt";
    std::copy(default_name.begin(), default_name.end(), path.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"Text files\0*.txt\0EZ cheat files\0*.cht\0All files\0*.*\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = g_output_format == GuiFormat::EzFlash
        ? L"cht" : L"txt";
    dialog.nFilterIndex = g_output_format == GuiFormat::EzFlash ? 2U : 1U;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog)) {
        return;
    }

    const std::string data = gba::text::normalize_newlines_crlf(
        normalize_from_edit(get_window_text(g_output_edit)));
    if (!write_file(path.data(), data)) {
        MessageBoxW(g_main, L"The output file could not be saved.",
                    L"Save failed", MB_OK | MB_ICONERROR);
        return;
    }
    set_status(L"Output saved.");
}

void swap_input_output() {
    const std::wstring old_input = get_window_text(g_input_edit);
    const std::wstring old_output = get_window_text(g_output_edit);
    const auto old_input_seed = parse_seed_edit(g_input_seed_edit);
    const auto old_output_seed = parse_seed_edit(g_output_seed_edit);

    GuiFormat resolved_input = g_input_format;
    if (resolved_input == GuiFormat::AutoDetect) {
        if (g_last_detected_input) {
            resolved_input = *g_last_detected_input;
        } else {
            const std::string input =
                gba::text::cleanup_gamehacking_org_blocks(
                    normalize_from_edit(old_input));
            const auto detected = gui_format_from_detection(
                gba::detect::format(input).format);
            if (!detected) {
                MessageBoxW(
                    g_main,
                    L"Auto Detect must identify the current input before the "
                    L"formats can be swapped.",
                    L"Swap unavailable", MB_OK | MB_ICONWARNING);
                return;
            }
            resolved_input = *detected;
        }
    }

    g_in_convert = true;
    SetWindowTextW(g_input_edit, old_output.c_str());
    SetWindowTextW(g_output_edit, old_input.c_str());

    const GuiFormat old_output_format = g_output_format;
    g_input_format = old_output_format;
    g_output_format = resolved_input;
    g_last_detected_input.reset();

    if (is_fcd_encrypted(g_output_format) && old_input_seed) {
        SetWindowTextW(
            g_output_seed_edit,
            format_seed_wide(*old_input_seed).c_str());
    }
    set_manual_input_key_enabled(false);
    g_input_seed_source = InputSeedSource::None;

    update_format_menus();
    update_format_labels();
    update_seed_controls();
    sync_input_seed_from_text(false);

    // Encrypted FCD output normally contains its own plaintext key row, so it
    // will be restored as Auto after the swap. If the swapped text is keyless,
    // carry the old output key into explicit Use/manual-key mode instead.
    if (is_fcd_encrypted(g_input_format) &&
        g_input_seed_source != InputSeedSource::Detected && old_output_seed) {
        set_manual_input_key_enabled(true);
        set_input_seed_text(format_seed_wide(*old_output_seed));
        g_input_seed_source = InputSeedSource::Manual;
        update_input_seed_label();
        update_seed_controls();
    }
    g_in_convert = false;
    set_status(L"Input, output, selected formats, and FCD keys swapped.");
    maybe_auto_convert();
}

HWND focused_edit_control() {
    const HWND focus = GetFocus();
    if (focus == g_input_edit ||
        focus == g_output_edit ||
        focus == g_input_seed_edit ||
        focus == g_output_seed_edit) {
        return focus;
    }

    return g_last_code_editor ? g_last_code_editor : g_input_edit;
}

HWND focused_code_editor() {
    const HWND focus = GetFocus();
    if (focus == g_input_edit || focus == g_output_edit) {
        return focus;
    }

    return g_last_code_editor ? g_last_code_editor : g_input_edit;
}

void clear_focused_code_editor() {
    HWND editor = focused_code_editor();
    SetWindowTextW(editor, L"");
    set_status(editor == g_output_edit
        ? L"Output cleared."
        : L"Input cleared.");
}

} // namespace gba::gui
