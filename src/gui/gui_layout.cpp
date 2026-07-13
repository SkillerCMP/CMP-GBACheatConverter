#include "gui/gui_state.hpp"

namespace gba::gui {

void layout_controls(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    constexpr int margin = 10;
    constexpr int splitter_width = 6;
    constexpr int label_y = 5;
    constexpr int label_height = 34;
    constexpr int editor_y = 43;
    constexpr int button_height = 28;
    constexpr int status_height = 22;
    constexpr int bottom_gap = 8;

    const int usable_width = std::max(100, width - margin * 2);
    const int split_center = margin +
        static_cast<int>(usable_width * g_split_ratio);
    const int left_right = split_center - splitter_width / 2;
    const int right_left = split_center + splitter_width / 2;
    const int left_width = std::max(100, left_right - margin);
    const int right_width = std::max(100, width - margin - right_left);

    const int button_y = height - status_height - bottom_gap - button_height;
    const int editor_height = std::max(80, button_y - editor_y - bottom_gap);

    HDWP positions = BeginDeferWindowPos(12);
    auto move = [&](HWND window, int x, int y, int w, int h) {
        positions = DeferWindowPos(positions, window, nullptr, x, y, w, h,
                                   SWP_NOZORDER | SWP_NOACTIVATE);
    };

    move(g_input_label, margin, label_y, left_width, label_height);
    move(g_input_edit, margin, editor_y, left_width, editor_height);

    move(g_output_label, right_left, label_y,
         right_width, label_height);
    move(g_output_edit, right_left, editor_y, right_width, editor_height);

    // Bottom control row:
    //   Swap + In Key + Use checkbox are left-aligned.
    //   Convert is centered.
    //   Out Key is right-aligned.
    constexpr int swap_width = 82;
    constexpr int convert_width = 130;
    constexpr int input_key_label_width = 50;
    constexpr int output_key_label_width = 60;
    constexpr int input_key_edit_width = 135;
    constexpr int output_key_edit_width = 130;
    constexpr int manual_key_width = 46;
    constexpr int key_gap = 4;
    constexpr int group_gap = 5;
    constexpr int key_control_height = 25;

    const int convert_x = (width - convert_width) / 2;
    const int input_key_group_x = margin + swap_width + group_gap;
    const int input_key_edit_x =
        input_key_group_x + input_key_label_width + key_gap;
    const int manual_key_x = input_key_edit_x + input_key_edit_width + key_gap;
    const int output_key_group_width =
        output_key_label_width + key_gap + output_key_edit_width;
    const int output_key_group_x =
        width - margin - output_key_group_width;
    const int key_y =
        button_y + (button_height - key_control_height) / 2;

    move(g_swap, margin, button_y, swap_width, button_height);
    move(g_input_seed_label, input_key_group_x, key_y,
         input_key_label_width, key_control_height);
    move(g_input_seed_edit, input_key_edit_x, key_y,
         input_key_edit_width, key_control_height);
    move(g_input_manual_key, manual_key_x, key_y,
         manual_key_width, key_control_height);
    move(g_convert, convert_x, button_y, convert_width, button_height);
    move(g_output_seed_label, output_key_group_x, key_y,
         output_key_label_width, key_control_height);
    move(g_output_seed_edit,
         output_key_group_x + output_key_label_width + key_gap,
         key_y, output_key_edit_width, key_control_height);

    move(g_status, 0, height - status_height, width, status_height);
    EndDeferWindowPos(positions);

    RedrawWindow(g_main, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

int splitter_center(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    constexpr int margin = 10;
    const int usable_width = std::max(100, static_cast<int>(client.right) - margin * 2);
    return margin + static_cast<int>(usable_width * g_split_ratio);
}

void update_splitter_from_x(int x) {
    RECT client{};
    GetClientRect(g_main, &client);
    constexpr int margin = 10;
    const int usable_width = std::max(100, static_cast<int>(client.right) - margin * 2);
    g_split_ratio = std::clamp(
        static_cast<double>(x - margin) / usable_width, 0.25, 0.75);
    layout_controls(client.right, client.bottom);
}

} // namespace gba::gui
