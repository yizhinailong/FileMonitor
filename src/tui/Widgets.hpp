#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    auto make_button(
        std::string label,
        std::function<void()> on_click,
        std::function<bool()> enabled = {}
    ) -> ftxui::Component;
    auto error_text(std::string_view message) -> ftxui::Element;
    auto error_text(std::string_view label, std::string_view message) -> ftxui::Element;
    auto session_error_text(core::MonitorSession const& session) -> ftxui::Element;
    [[nodiscard]]
    auto has_session_errors(core::MonitorSession const& session) -> bool;
    auto file_change_status_color(core::FileChangeStatus status) -> ftxui::Color;

    // A page fills the terminal and keeps one blank column on either side.
    [[nodiscard]]
    auto page_width() -> int;
    auto page(ftxui::Element content) -> ftxui::Element;

} // namespace file_monitor::tui
