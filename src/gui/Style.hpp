#pragma once

#include <imgui.h>

#include "core/FileSystem.hpp"

namespace file_monitor::gui {

    enum class Theme {
        Light,
        Dark
    };

    // Loads the font of the interface, which has the glyphs for Chinese text.
    // Call once after creating the Dear ImGui context.
    [[nodiscard]]
    auto load_font() -> bool;
    // Sets the sizes and colors of Dear ImGui and the colors of the functions
    // below.
    auto apply_theme(Theme theme) -> void;
    // Scales the sizes and the text of the interface, for a display that
    // reports this scale through Windows; 1 is 100%. The current theme is
    // applied again.
    auto set_scale(float scale) -> void;

    [[nodiscard]]
    auto file_change_status_color(core::FileChangeStatus status) -> ImVec4;
    // The backgrounds of the selected row of a list and of the row under the
    // pointer. The pointer does not hide which row is selected, as it does
    // with the colors Dear ImGui uses for both by default.
    [[nodiscard]]
    auto selected_row_color() -> ImVec4;
    [[nodiscard]]
    auto hovered_row_color() -> ImVec4;
    [[nodiscard]]
    auto success_color() -> ImVec4;
    [[nodiscard]]
    auto warning_color() -> ImVec4;
    [[nodiscard]]
    auto error_color() -> ImVec4;

} // namespace file_monitor::gui
