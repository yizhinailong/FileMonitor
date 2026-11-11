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

    [[nodiscard]]
    auto file_change_status_color(core::FileChangeStatus status) -> ImVec4;
    [[nodiscard]]
    auto success_color() -> ImVec4;
    [[nodiscard]]
    auto warning_color() -> ImVec4;
    [[nodiscard]]
    auto error_color() -> ImVec4;

} // namespace file_monitor::gui
