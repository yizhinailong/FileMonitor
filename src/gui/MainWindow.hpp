#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ChangeTable.hpp"
#include "Drop.hpp"
#include "SettingsWindow.hpp"

import file_monitor.core;

namespace file_monitor::gui {

    // The window content from top to bottom: a toolbar, the session errors if
    // there are any, the change table, the details of the selected change and
    // a status bar.
    class MainWindow final {
    public:
        [[nodiscard]] SettingsWindow& Settings();

        // Takes the new changes from the monitor and writes them to the log;
        // returns whether Render() has something new to show. Call this also
        // while the window is minimized and Render() is not.
        [[nodiscard]] bool Update();
        void Render();
        // Opens the settings with the dropped folders added to the monitored
        // ones, or adds them to the list of the open settings they fell on.
        void DropFiles(std::vector<DroppedFile> const& files);

    private:
        void renderToolbar();
        void renderErrors();
        // Takes the place of the change table and the details while the table
        // has no rows.
        void renderEmptyState(ImVec2 size);
        void renderDetails(float height);
        void renderStatusBar();
        void handleShortcuts();
        void applyPathFilter();
        void clearFilters();
        // Does what was asked for with the selected change, if there is one.
        void run(ChangeAction action);
        void showNotice(std::string_view text, bool failed = false);
        void openSettings();

        core::MonitorSession m_session;
        ChangeTable m_change_table{ m_session };
        SettingsWindow m_settings_window;
        // The outcome of the last action, shown in the status bar for a few
        // seconds.
        std::string m_notice;
        bool m_notice_failed{ false };
        std::chrono::steady_clock::time_point m_notice_end;
        // The text of the filter box; the table filters by it as it changes.
        std::string m_filter_text;
        bool m_focus_filter{ false };
    };

} // namespace file_monitor::gui
