#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>

#include "Drop.hpp"
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

    // The settings as a modal window with the monitored and the excluded
    // directories one below the other, each with an input for a new directory
    // and a warning next to a directory that has no effect.
    class SettingsWindow final {
    public:
        auto SetParentWindow(SDL_Window* window) -> void;
        // Shows the window from the next Render() on. It can be called from
        // anywhere, unlike ImGui::OpenPopup(), which has to run where the
        // popup is drawn.
        auto Open(core::Configuration const& configuration) -> void;
        // Opens the window with these directories added to the monitored ones,
        // waiting to be saved.
        auto OpenWithDirectories(
            core::Configuration const& configuration,
            std::vector<std::filesystem::path> directories
        ) -> void;
        [[nodiscard]]
        auto IsOpen() const -> bool;
        // Adds the dropped folders, which were dropped on the window, to the
        // list they were dropped on.
        auto DropFiles(std::vector<DroppedFile> const& files) -> void;
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
        auto renderDirectoryGroup(DirectoryGroup group, float height) -> void;
        auto renderFooter(core::MonitorSession& session) -> void;
        auto refreshWarnings() -> void;
        // Anything but the first click on the cancel button withdraws the
        // request to discard the changes.
        auto markChanged() -> void;
        [[nodiscard]]
        auto hasUnsavedChanges() const -> bool;
        auto save(core::MonitorSession& session) -> void;
        // Closes the window, or with unsaved changes first asks for a second
        // click.
        auto cancel() -> void;

        core::Configuration m_pending_configuration;
        // The configuration when the window opened, which unsaved changes are
        // compared with.
        core::Configuration m_saved_configuration;
        std::vector<std::string> m_monitored_warnings;
        std::vector<std::string> m_excluded_warnings;
        std::string m_drop_error;

        std::shared_ptr<DirectoryDialogState> m_dialog_state{
            std::make_shared<DirectoryDialogState>()
        };
        SDL_Window* m_parent_window{ nullptr };
        DirectoryEditorState m_monitored_editor;
        DirectoryEditorState m_excluded_editor;
        DirectoryGroup m_dialog_group{ DirectoryGroup::Monitored };
        std::string m_dialog_error_message;
        bool m_dialog_open{ false };
        bool m_open{ false };
        bool m_open_requested{ false };
        bool m_confirm_discard{ false };

        // Where the window and the two lists ended up in the last frame, for
        // the drops.
        ImVec2 m_window_min{};
        ImVec2 m_window_max{};
        ImVec2 m_monitored_min{};
        ImVec2 m_monitored_max{};
        ImVec2 m_excluded_min{};
        ImVec2 m_excluded_max{};
    };

} // namespace file_monitor::gui
