#pragma once

#include <string_view>

import file_monitor.core;

namespace file_monitor::gui {

    void render_error(std::string_view message);
    void render_error(std::string_view label, std::string_view message);
    void render_session_errors(core::MonitorSession const& session);
    [[nodiscard]] bool has_session_errors(core::MonitorSession const& session);

} // namespace file_monitor::gui
