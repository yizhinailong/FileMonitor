#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/ChangeLogger.hpp"
#include "core/DirectoryMonitor.hpp"

struct SDL_Window;

namespace file_monitor::ui {
    struct DirectoryDialogState {
        std::mutex                         mutex;
        std::vector<std::filesystem::path> selected_directories;
        std::string                        error_message;
        bool                               completed{ false };
    };

    class MainWindow final {
    public:
        MainWindow();

        auto SetParentWindow(SDL_Window* window) -> void;
        auto Render() -> void;

    private:
        auto consumeDirectorySelection() -> void;
        auto openDirectoryDialog() -> void;
        auto resetFileMonitor() -> void;
        auto renderFileListPanel(float width, float height) -> void;
        auto renderSettingsPanel(float width, float height) -> void;
        auto renderSettingsWindow() -> void;
        auto updateFileMonitor() -> void;

        std::vector<std::filesystem::path> m_directories;
        std::vector<std::filesystem::path> m_pending_directories;
        std::vector<core::FileChange>      m_file_changes;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        core::DirectoryMonitor                m_file_monitor;
        core::ChangeLogger                    m_change_logger;
        SDL_Window*                           m_parent_window{ nullptr };
        std::string                           m_dialog_error_message;
        std::string                           m_configuration_error_message;
        std::string                           m_log_error_message;
        std::string                           m_monitor_error_message;
        bool                                  m_dialog_open{ false };
        bool                                  m_scroll_to_latest{ false };
    };

} // namespace file_monitor::ui
