#include "DirectoryWindow.hpp"

#include <mutex>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::ui::windows {
    struct DirectoryDialogState {
        std::mutex                           mutex;
        std::optional<std::filesystem::path> selected_directory;
        std::string                          error_message;
        bool                                 completed{ false };
    };

    namespace {

        struct DirectoryDialogContext {
            std::shared_ptr<DirectoryDialogState> state;
        };

        auto path_to_utf8(std::filesystem::path const& path) -> std::string {
            auto const utf8_path{ path.u8string() };
            return { reinterpret_cast<char const*>(utf8_path.data()), utf8_path.size() };
        }

        auto SDLCALL directory_dialog_callback(
            void*              userdata,
            char const* const* file_list,
            int /*filter*/
        ) -> void {
            auto context{ std::unique_ptr<DirectoryDialogContext>{
                static_cast<DirectoryDialogContext*>(userdata) } };

            std::optional<std::filesystem::path> selected_directory;
            std::string                          error_message;
            if (file_list == nullptr) {
                error_message = SDL_GetError();
            } else if (*file_list != nullptr) {
                auto const* utf8_path{ reinterpret_cast<char8_t const*>(*file_list) };
                selected_directory = std::filesystem::path{ std::u8string{ utf8_path } };
            }

            auto const lock{ std::scoped_lock{ context->state->mutex } };
            context->state->selected_directory = std::move(selected_directory);
            context->state->error_message      = std::move(error_message);
            context->state->completed          = true;
        }

    } // namespace

    DirectoryWindow::DirectoryWindow()
        : Window{ "DirectoryWindow" }, m_dialog_state{ std::make_shared<DirectoryDialogState>() } {
    }

    auto DirectoryWindow::SetParentWindow(SDL_Window* window) -> void {
        m_parent_window = window;
    }

    auto DirectoryWindow::TakeSelectedDirectory() -> std::optional<std::filesystem::path> {
        auto const lock{ std::scoped_lock{ m_dialog_state->mutex } };
        if (!m_dialog_state->completed) {
            return std::nullopt;
        }

        m_dialog_state->completed = false;
        m_dialog_open             = false;

        if (!m_dialog_state->error_message.empty()) {
            m_status_text.SetContent("Directory selection failed: " + m_dialog_state->error_message);
            m_dialog_state->error_message.clear();
            return std::nullopt;
        }

        if (!m_dialog_state->selected_directory) {
            if (m_selected_directory.empty()) {
                m_status_text.SetContent("No directory selected");
            } else {
                m_status_text.SetContent(m_selected_directory);
            }
            return std::nullopt;
        }

        auto selected_directory{ std::move(m_dialog_state->selected_directory) };
        m_dialog_state->selected_directory.reset();
        m_selected_directory = path_to_utf8(*selected_directory);
        m_status_text.SetContent(m_selected_directory);
        return selected_directory;
    }

    auto DirectoryWindow::renderContent() -> void {
        ImGui::BeginDisabled(m_dialog_open);
        auto const select_directory{ m_select_directory_button.Render() };
        ImGui::EndDisabled();

        if (select_directory) {
            openDirectoryDialog();
        }

        ImGui::SameLine();
        m_status_text.Render();
        if (!m_selected_directory.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", m_selected_directory.c_str());
        }
    }

    auto DirectoryWindow::openDirectoryDialog() -> void {
        m_dialog_open = true;
        m_status_text.SetContent("Selecting directory...");

        auto context{ std::make_unique<DirectoryDialogContext>() };
        context->state = m_dialog_state;

        auto const* default_location{
            m_selected_directory.empty() ? nullptr : m_selected_directory.c_str()
        };
        SDL_ShowOpenFolderDialog(
            directory_dialog_callback,
            context.release(),
            m_parent_window,
            default_location,
            false
        );
    }

} // namespace file_monitor::ui::windows
