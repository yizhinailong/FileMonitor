#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/Configuration.hpp"

struct SDL_Window;

namespace file_monitor::ui {
    struct DirectoryDialogState {
        std::mutex                         mutex;
        std::vector<std::filesystem::path> selected_directories;
        std::string                        error_message;
        bool                               completed{ false };
    };

    class SettingsWindow final {
    public:
        explicit SettingsWindow(std::filesystem::path config_path);

        auto SetParentWindow(SDL_Window* window) -> void;
        auto Open() -> void;
        auto Render(std::string_view monitor_error, std::string_view log_error) -> bool;
        [[nodiscard]]
        auto Directories() const -> std::vector<std::filesystem::path> const&;
        [[nodiscard]]
        auto ExcludedDirectories() const -> std::vector<std::filesystem::path> const&;

    private:
        enum class DirectoryGroup {
            Monitored,
            Excluded
        };

        struct DirectoryEditorState {
            std::string input;
            std::string error_message;
        };

        auto addManualDirectory(DirectoryGroup group) -> void;
        auto addPendingDirectory(
            DirectoryGroup        group,
            std::filesystem::path directory
        ) -> bool;
        auto consumeDirectorySelection() -> void;
        auto directoryEditor(DirectoryGroup group) -> DirectoryEditorState&;
        auto openDirectoryDialog(DirectoryGroup group) -> void;
        auto pendingDirectories(DirectoryGroup group) -> std::vector<std::filesystem::path>&;
        auto renderDirectoryEditor(
            DirectoryGroup   group,
            std::string_view input_hint,
            std::string_view empty_text
        ) -> void;

        std::filesystem::path m_config_path;
        core::Configuration   m_configuration;
        core::Configuration   m_pending_configuration;

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        DirectoryEditorState                  m_monitored_editor;
        DirectoryEditorState                  m_excluded_editor;
        DirectoryGroup                        m_dialog_group{ DirectoryGroup::Monitored };
        std::string                           m_dialog_error_message;
        std::string                           m_configuration_error_message;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui
