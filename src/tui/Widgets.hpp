#pragma once

#include <string_view>

#include <ftxui/dom/elements.hpp>

#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    auto error_text(std::string_view message) -> ftxui::Element;
    auto error_text(std::string_view label, std::string_view message) -> ftxui::Element;
    auto session_error_text(core::MonitorSession const& session) -> ftxui::Element;
    [[nodiscard]]
    auto has_session_errors(core::MonitorSession const& session) -> bool;
    // The status text in its color.
    auto file_change_status_element(core::FileChangeStatus status) -> ftxui::Element;

    // A page fills the terminal and keeps one blank column on either side.
    [[nodiscard]]
    auto page_width() -> int;
    auto page(ftxui::Element content) -> ftxui::Element;

} // namespace file_monitor::tui
