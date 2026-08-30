#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace file_monitor::ui {

    struct DirectoryDialogState;

    class MainWindow final {
    public:
        MainWindow();

        auto SetParentWindow(SDL_Window* window) -> void;
        auto Render() -> void;

    private:
        struct FileInfo {
            std::string modified_time;
            std::string size;
            std::string absolute_path;
        };

        auto consumeDirectorySelection() -> void;
        auto loadFiles(std::filesystem::path const& directory) -> void;
        auto openDirectoryDialog() -> void;
        auto renderDirectoryPanel(float width, float height) -> void;
        auto renderFileListPanel(float width, float height) -> void;

        std::string           m_directory_status{ "No directory selected" };
        std::string           m_file_summary{ "Select a directory to list its files" };
        std::vector<FileInfo> m_files;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        std::string                           m_selected_directory;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui
