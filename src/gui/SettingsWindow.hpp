#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/Configuration.hpp"
#include "core/MonitorSession.hpp"

struct SDL_Window;

namespace file_monitor::gui {
    struct DirectoryDialogState {
        std::mutex mutex;
        std::vector<std::filesystem::path> selected_directories;
        std::string error_message;
        bool completed{ false };
    };

    class SettingsWindow final {
    public:
        auto SetParentWindow(SDL_Window* window) -> void;
        // Shows the window from the next Render() on. It can be called from
        // anywhere, unlike ImGui::OpenPopup(), which has to run where the
        // popup is drawn.
        auto Open(core::Configuration const& configuration) -> void;
        // Takes the folders picked in the system dialog, which reports them
        // from another thread; returns whether it had any result.
        [[nodiscard]]
        auto Update() -> bool;
        auto Render(core::MonitorSession& session) -> void;

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
        auto consumeDirectorySelection() -> bool;
        auto directoryEditor(DirectoryGroup group) -> DirectoryEditorState&;
        auto openDirectoryDialog(DirectoryGroup group) -> void;
        auto pendingDirectories(DirectoryGroup group) -> std::vector<std::filesystem::path>&;
        auto renderDirectoryEditor(
            DirectoryGroup group,
            std::string_view input_hint,
            std::string_view empty_text
        ) -> void;

        core::Configuration m_pending_configuration;

        std::shared_ptr<DirectoryDialogState> m_dialog_state{
            std::make_shared<DirectoryDialogState>()
        };
        SDL_Window* m_parent_window{ nullptr };
        DirectoryEditorState m_monitored_editor;
        DirectoryEditorState m_excluded_editor;
        DirectoryGroup m_dialog_group{ DirectoryGroup::Monitored };
        std::string m_dialog_error_message;
        bool m_dialog_open{ false };
        bool m_open_requested{ false };
    };

} // namespace file_monitor::gui
