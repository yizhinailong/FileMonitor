#pragma once

#include <string_view>

namespace file_monitor::gui {

    // The width of a text in pixels in the current font.
    [[nodiscard]]
    auto text_width(std::string_view text) -> float;
    auto draw_text(std::string_view text) -> void;
    auto draw_disabled_text(std::string_view text) -> void;
    // The width of a button with this label; the part of the label after "##"
    // is not shown and does not count.
    [[nodiscard]]
    auto button_width(char const* label) -> float;
    // Move the cursor so that the next item of this width ends at the right
    // edge of the available region, or sits in the middle of it.
    auto align_right(float width) -> void;
    auto align_center(float width) -> void;

} // namespace file_monitor::gui
