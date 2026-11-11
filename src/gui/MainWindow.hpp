#pragma once

#include <imgui.h>

#include "ChangeTable.hpp"
#include "SettingsWindow.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::gui {

    // The window content from top to bottom: a toolbar, the session errors if
    // there are any, the change table and a status bar.
    class MainWindow final {
    public:
        [[nodiscard]]
        auto Settings() -> SettingsWindow&;

        // Takes the new changes from the monitor and writes them to the log;
        // returns whether Render() has something new to show. Call this also
        // while the window is minimized and Render() is not.
        [[nodiscard]]
        auto Update() -> bool;
        auto Render() -> void;

    private:
        auto renderToolbar() -> void;
        auto renderErrors() -> void;
        // Takes the place of the change table while it has no rows.
        auto renderEmptyState(ImVec2 size) -> void;
        auto renderStatusBar() -> void;
        auto openSettings() -> void;

        core::MonitorSession m_session;
        ChangeTable m_change_table{ m_session };
        SettingsWindow m_settings_window;
    };

} // namespace file_monitor::gui
