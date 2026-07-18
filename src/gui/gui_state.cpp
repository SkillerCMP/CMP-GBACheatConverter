#include "gui/gui_state.hpp"

namespace gba::gui {

HWND g_main = nullptr;
HWND g_input_label = nullptr;
HWND g_output_label = nullptr;
HWND g_input_seed_label = nullptr;
HWND g_input_seed_edit = nullptr;
HWND g_input_manual_key = nullptr;
HWND g_output_seed_label = nullptr;
HWND g_output_seed_edit = nullptr;
HWND g_input_edit = nullptr;
HWND g_output_edit = nullptr;
HWND g_last_code_editor = nullptr;
WNDPROC g_input_edit_proc = nullptr;
WNDPROC g_output_edit_proc = nullptr;
HWND g_swap = nullptr;
HWND g_convert = nullptr;
HWND g_status = nullptr;
HFONT g_ui_font = nullptr;
HFONT g_heading_font = nullptr;
HFONT g_code_font = nullptr;
HMENU g_main_menu = nullptr;
HMENU g_input_menu = nullptr;
HMENU g_input_raw_menu = nullptr;
HMENU g_input_encrypted_menu = nullptr;
HMENU g_output_raw_menu = nullptr;
HMENU g_output_encrypted_menu = nullptr;
HMENU g_ezflash_menu = nullptr;
HMENU g_save_output_as_menu = nullptr;

bool g_auto_convert = false;
bool g_cmp_output = false;
bool g_input_word_wrap = false;
bool g_output_word_wrap = false;
GuiFormat g_input_format = GuiFormat::AutoDetect;
GuiFormat g_output_format = GuiFormat::EzFlash;
gba::ezflash::Mode g_ezflash_mode = gba::ezflash::Mode::Enhanced;
std::optional<GuiFormat> g_last_detected_input;
bool g_in_convert = false;
bool g_dragging_splitter = false;
double g_split_ratio = 0.50;
std::wstring g_ini_path;
InputSeedSource g_input_seed_source = InputSeedSource::None;
bool g_updating_input_seed = false;

} // namespace gba::gui
