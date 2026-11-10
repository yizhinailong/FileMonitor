#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

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

} // namespace file_monitor::tui
