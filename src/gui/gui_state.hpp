#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>

#include "cli/cli_app.hpp"
#include "core/detect.hpp"
#include "core/inline_notes.hpp"
#include "core/text.hpp"
#include "core/types.hpp"
#include "core/version.hpp"
#include "formats/armax.hpp"
#include "formats/codebreaker.hpp"
#include "formats/gameshark.hpp"
#include "formats/xploder.hpp"
#include "formats/ezflash.hpp"
#include "export/output_modes.hpp"
#include "import/native_input.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gba::gui {

inline constexpr wchar_t kWindowClass[] = L"GbaCheatConverterWindow";
inline constexpr wchar_t kWindowTitle[] =
    L"GBA Cheat Converter v" GBA_CHEAT_VERSION_W;

inline constexpr int ID_INPUT_EDIT = 1003;
inline constexpr int ID_OUTPUT_EDIT = 1004;
inline constexpr int ID_OUTPUT_SEED_EDIT = 1005;
inline constexpr int ID_INPUT_SEED_EDIT = 1006;
inline constexpr int ID_INPUT_MANUAL_KEY = 1007;
inline constexpr int ID_SWAP = 1011;
inline constexpr int ID_CONVERT = 1012;

inline constexpr int ID_FILE_OPEN = 2001;
inline constexpr int ID_FILE_SAVE = 2002;
inline constexpr int ID_FILE_EXIT = 2003;
inline constexpr int ID_FILE_SAVE_ARMAX_DSC = 2040;
inline constexpr int ID_FILE_SAVE_VBA_CLT = 2041;
inline constexpr int ID_FILE_SAVE_MYBOY_CHT = 2042;
inline constexpr int ID_FILE_SAVE_MISTER_ZIP = 2043;
inline constexpr int ID_FILE_SAVE_MEDNAFEN_CHT = 2044;
inline constexpr int ID_FILE_SAVE_MGBA_CHEATS = 2045;
inline constexpr int ID_FILE_SAVE_LIBRETRO_CHT = 2046;
inline constexpr int ID_FILE_SAVE_EZFLASH_CHT = 2047;
inline constexpr int ID_EDIT_CUT = 2010;
inline constexpr int ID_EDIT_COPY = 2011;
inline constexpr int ID_EDIT_PASTE = 2012;
inline constexpr int ID_EDIT_SELECT_ALL = 2013;
inline constexpr int ID_EDIT_CLEAR = 2014;
inline constexpr int ID_OPTIONS_AUTO_CONVERT = 2020;
inline constexpr int ID_OPTIONS_EZ_ORIGINAL = 2021;
inline constexpr int ID_OPTIONS_EZ_ENHANCED = 2022;
inline constexpr int ID_HELP_ABOUT = 2030;
inline constexpr int IDI_APP_ICON = 101;

inline constexpr int ID_INPUT_AUTO_DETECT = 2090;
inline constexpr int ID_INPUT_RAW_FCD = 2100;
inline constexpr int ID_INPUT_RAW_GSA = 2101;
inline constexpr int ID_INPUT_RAW_ARMAX = 2102;
inline constexpr int ID_INPUT_RAW_EZ = 2103;
inline constexpr int ID_INPUT_ENCRYPTED_FCD = 2110;
inline constexpr int ID_INPUT_ENCRYPTED_GSA = 2111;
inline constexpr int ID_INPUT_ENCRYPTED_ARMAX = 2112;

inline constexpr int ID_OUTPUT_RAW_FCD = 2200;
inline constexpr int ID_OUTPUT_RAW_GSA = 2201;
inline constexpr int ID_OUTPUT_RAW_ARMAX = 2202;
inline constexpr int ID_OUTPUT_RAW_EZ = 2203;
inline constexpr int ID_OUTPUT_ENCRYPTED_FCD = 2210;
inline constexpr int ID_OUTPUT_ENCRYPTED_GSA = 2211;
inline constexpr int ID_OUTPUT_ENCRYPTED_ARMAX = 2212;

enum class GuiFormat : int {
    FcdRaw = 0,
    GameSharkRaw = 1,
    ActionReplayMaxRaw = 2,
    EzFlash = 3,
    FcdEncrypted = 4,
    GameSharkEncrypted = 5,
    ActionReplayMaxEncrypted = 6,
    AutoDetect = 7
};

enum class InputSeedSource {
    None,
    Manual,
    Detected
};

extern HWND g_main;
extern HWND g_input_label;
extern HWND g_output_label;
extern HWND g_input_seed_label;
extern HWND g_input_seed_edit;
extern HWND g_input_manual_key;
extern HWND g_output_seed_label;
extern HWND g_output_seed_edit;
extern HWND g_input_edit;
extern HWND g_output_edit;
extern HWND g_last_code_editor;
extern WNDPROC g_input_edit_proc;
extern WNDPROC g_output_edit_proc;
extern HWND g_swap;
extern HWND g_convert;
extern HWND g_status;
extern HFONT g_ui_font;
extern HFONT g_heading_font;
extern HFONT g_code_font;
extern HMENU g_main_menu;
extern HMENU g_input_menu;
extern HMENU g_input_raw_menu;
extern HMENU g_input_encrypted_menu;
extern HMENU g_output_raw_menu;
extern HMENU g_output_encrypted_menu;
extern HMENU g_ezflash_menu;
extern HMENU g_save_output_as_menu;

extern bool g_auto_convert;
extern GuiFormat g_input_format;
extern GuiFormat g_output_format;
extern gba::ezflash::Mode g_ezflash_mode;
extern std::optional<GuiFormat> g_last_detected_input;
extern bool g_in_convert;
extern bool g_dragging_splitter;
extern double g_split_ratio;
extern std::wstring g_ini_path;
extern InputSeedSource g_input_seed_source;
extern bool g_updating_input_seed;

std::wstring utf8_to_wide(std::string_view input);
std::string wide_to_utf8(std::wstring_view input);
std::wstring normalize_for_edit(std::string_view input);
std::wstring normalize_wide_for_edit(std::wstring_view input);
std::string normalize_from_edit(std::wstring_view input);
std::wstring get_window_text(HWND control);
void set_editor_text(HWND editor, std::string_view text);
LRESULT CALLBACK code_editor_proc(HWND window, UINT message,
                                  WPARAM wparam, LPARAM lparam);
void set_status(std::wstring_view status);
std::wstring executable_directory();
std::optional<gba::codebreaker::Seed> parse_seed_edit(HWND edit);
std::wstring format_seed_wide(gba::codebreaker::Seed seed);

int input_format_index();
int output_format_index();
bool is_fcd_encrypted(GuiFormat format);
bool is_armax_family(GuiFormat format);
bool is_armax_encrypted(GuiFormat format);
std::optional<GuiFormat> gui_format_from_detection(gba::detect::Format format);
std::wstring format_name(GuiFormat format, bool output);
std::wstring format_mode_name(GuiFormat format);
std::wstring format_family_name(GuiFormat format, bool output);
void update_format_labels();
void update_format_menus();
bool input_seed_controls_relevant();
bool manual_input_key_enabled();
void set_manual_input_key_enabled(bool enabled);
void update_input_seed_label();
void set_input_seed_text(std::wstring_view value);
std::string current_cleaned_input();
void sync_input_seed_from_text(bool announce_detection = false);
void update_seed_controls();
void select_input_format(GuiFormat format);
void select_output_format(GuiFormat format);
void update_auto_convert_menu();
void update_ezflash_mode_menu();

void show_warnings(const std::vector<std::string>& warnings,
                   std::wstring_view prefix = {});
bool perform_conversion(bool modal_errors);
void maybe_auto_convert();

bool read_binary_file(const std::wstring& path,
                      std::vector<std::uint8_t>& output);
bool load_input_path(const std::wstring& path, bool dropped);
bool write_file(const std::wstring& path, std::string_view data);
bool write_binary_file(const std::wstring& path,
                       const std::vector<std::uint8_t>& data);
gba::CheatDocument parse_current_output_document();
std::optional<std::string> md5_file(const std::wstring& path);
void save_native_output(int command);
void open_input_file();
void save_output_file();
void swap_input_output();
HWND focused_edit_control();
HWND focused_code_editor();
void clear_focused_code_editor();

void load_settings();
void save_settings();
void layout_controls(int width, int height);
int splitter_center(HWND window);
void update_splitter_from_x(int x);
HMENU create_main_menu();
void create_controls();
LRESULT handle_command(WPARAM wparam, LPARAM lparam);
LRESULT CALLBACK window_proc(HWND window, UINT message,
                             WPARAM wparam, LPARAM lparam);

} // namespace gba::gui
