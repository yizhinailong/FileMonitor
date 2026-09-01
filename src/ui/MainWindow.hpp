#pragma once

#include <string>
#include <vector>

#include "SettingsWindow.hpp"
#include "core/ChangeLogger.hpp"
#include "core/DirectoryMonitor.hpp"

namespace file_monitor::ui {
    class MainWindow final {
    public:
        MainWindow();

        [[nodiscard]]
        auto Settings() -> SettingsWindow&;
        [[nodiscard]]
        auto Settings() const -> SettingsWindow const&;

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
