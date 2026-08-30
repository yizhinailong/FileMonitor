#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/FileSystem.hpp"

struct SDL_Window;

namespace file_monitor::ui {
    struct DirectoryDialogState {
        std::mutex                           mutex;
        std::optional<std::filesystem::path> selected_directory;
        std::string                          error_message;
        bool                                 completed{ false };
    };

    class MainWindow final {
    public:
        MainWindow();

        auto SetParentWindow(SDL_Window* window) -> void;
        auto Render() -> void;

    private:
        auto consumeDirectorySelection() -> void;
        auto openDirectoryDialog() -> void;
        auto renderDirectoryPanel(float width, float height) -> void;
        auto renderFileListPanel(float width, float height) -> void;

        std::vector<core::FileInfo> m_files;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        std::string                           m_selected_directory;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui
