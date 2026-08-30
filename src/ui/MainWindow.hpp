#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/FileSystem.hpp"

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
        std::vector<core::FileState>       m_file_snapshot;
        std::vector<core::FileChange>      m_file_changes;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        std::chrono::steady_clock::time_point m_next_file_scan;
        std::string                           m_dialog_error_message;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui
