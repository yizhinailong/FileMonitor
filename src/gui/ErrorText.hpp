#pragma once

#include <string_view>

import file_monitor.core.monitor_session;

namespace file_monitor::gui {

    auto render_error(std::string_view message) -> void;
    auto render_error(std::string_view label, std::string_view message) -> void;
    auto render_session_errors(core::MonitorSession const& session) -> void;
    [[nodiscard]]
    auto has_session_errors(core::MonitorSession const& session) -> bool;

} // namespace file_monitor::gui
