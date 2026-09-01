#pragma once

#include <string>
#include <vector>

#include "SettingsWindow.hpp"

import core;

struct SDL_Window;

namespace file_monitor::ui {
    class MainWindow final {
    public:
        MainWindow();

        auto SetParentWindow(SDL_Window* window) -> void;
        auto Render() -> void;

    private:
        auto resetFileMonitor() -> void;
        auto renderFileListPanel(float width, float height) -> void;
        auto renderSettingsPanel(float width, float height) -> void;
        auto updateFileMonitor() -> void;

        std::vector<core::FileChange> m_file_changes;

        SettingsWindow         m_settings_window;
        core::DirectoryMonitor m_file_monitor;
        core::ChangeLogger     m_change_logger;
        std::string            m_log_error_message;
        std::string            m_monitor_error_message;
        bool                   m_scroll_to_latest{ false };
    };

} // namespace file_monitor::ui
