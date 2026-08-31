#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct SDL_Window;

namespace file_monitor::ui {
    struct DirectoryDialogState;

    class SettingsWindow final {
    public:
        explicit SettingsWindow(std::filesystem::path config_path);

        auto               SetParentWindow(SDL_Window* window) -> void;
        auto               Open() -> void;
        auto               Render(std::string_view monitor_error, std::string_view log_error) -> bool;
        [[nodiscard]] auto Directories() const
            -> std::vector<std::filesystem::path> const&;

    private:
        auto addManualDirectory() -> void;
        auto addPendingDirectory(std::filesystem::path directory) -> bool;
        auto consumeDirectorySelection() -> void;
        auto openDirectoryDialog() -> void;

        std::filesystem::path              m_config_path;
        std::vector<std::filesystem::path> m_directories;
        std::vector<std::filesystem::path> m_pending_directories;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        std::string                           m_directory_input;
        std::string                           m_directory_input_error_message;
        std::string                           m_dialog_error_message;
        std::string                           m_configuration_error_message;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui
