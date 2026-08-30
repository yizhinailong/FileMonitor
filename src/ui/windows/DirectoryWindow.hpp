#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "Window.hpp"
#include "ui/utils/Button.hpp"
#include "ui/utils/Text.hpp"

struct SDL_Window;

namespace file_monitor::ui::windows {

    struct DirectoryDialogState;

    class DirectoryWindow final : public Window {
    public:
        DirectoryWindow();

        auto               SetParentWindow(SDL_Window* window) -> void;
        [[nodiscard]] auto TakeSelectedDirectory() -> std::optional<std::filesystem::path>;

    private:
        auto renderContent() -> void override;
        auto openDirectoryDialog() -> void;

        utils::Button m_select_directory_button{ "Select directory" };
        utils::Text   m_status_text{ "No directory selected" };

        std::shared_ptr<DirectoryDialogState> m_dialog_state;
        SDL_Window*                           m_parent_window{ nullptr };
        std::string                           m_selected_directory;
        bool                                  m_dialog_open{ false };
    };

} // namespace file_monitor::ui::windows
