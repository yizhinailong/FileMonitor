#pragma once

#include <imgui.h>

import file_monitor.core;

namespace file_monitor::gui {

    enum class Theme {
        Light,
        Dark
    };

    // Loads the font of the interface, which has the glyphs for Chinese text.
    // Call once after creating the Dear ImGui context.
    [[nodiscard]] bool load_font();
    // Sets the sizes and colors of Dear ImGui and the colors of the functions
    // below.
    void apply_theme(Theme theme);
    // Scales the sizes and the text of the interface, for a display that
    // reports this scale through Windows; 1 is 100%. The current theme is
    // applied again.
    void set_scale(float scale);

    [[nodiscard]] ImVec4 file_change_status_color(core::FileChangeStatus status);
    // The backgrounds of the selected row of a list and of the row under the
    // pointer. The pointer does not hide which row is selected, as it does
    // with the colors Dear ImGui uses for both by default.
    [[nodiscard]] ImVec4 selected_row_color();
    [[nodiscard]] ImVec4 hovered_row_color();
    [[nodiscard]] ImVec4 success_color();
    [[nodiscard]] ImVec4 warning_color();
    [[nodiscard]] ImVec4 error_color();

} // namespace file_monitor::gui
