#include "MainWindow.hpp"

#include <filesystem>
#include <optional>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::ui {

    namespace {

        struct DirectoryDialogContext {
            std::shared_ptr<DirectoryDialogState> state;
        };

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

    MainWindow::MainWindow()
        : m_dialog_state{ std::make_shared<DirectoryDialogState>() } {
    }

    auto MainWindow::SetParentWindow(SDL_Window* window) -> void {
        m_parent_window = window;
    }

    auto MainWindow::Render() -> void {
        consumeDirectorySelection();

        auto const* viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0F, 0.0F });
        ImGui::Begin("FileMonitorMainWindow", nullptr, WINDOW_FLAGS);
        ImGui::PopStyleVar();

        auto const available{ ImGui::GetContentRegionAvail() };
        auto const directory_panel_height{ available.y * 0.15F };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0F, 0.0F });
        renderDirectoryPanel(available.x, directory_panel_height);
        renderFileListPanel(available.x, 0.0F);
        ImGui::PopStyleVar();

        ImGui::End();
    }

    auto MainWindow::consumeDirectorySelection() -> void {
        std::optional<std::filesystem::path> selected_directory;
        std::string                          error_message;
        {
            auto const lock{ std::scoped_lock{ m_dialog_state->mutex } };
            if (!m_dialog_state->completed) {
                return;
            }

            m_dialog_state->completed = false;
            selected_directory        = std::move(m_dialog_state->selected_directory);
            error_message             = std::move(m_dialog_state->error_message);
            m_dialog_state->selected_directory.reset();
            m_dialog_state->error_message.clear();
        }

        m_selected_directory = core::path_to_utf8(*selected_directory);
        m_files              = core::load_files(*selected_directory);
    }

    auto MainWindow::openDirectoryDialog() -> void {
        m_dialog_open = true;

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

    auto MainWindow::renderDirectoryPanel(float width, float height) -> void {
        if (ImGui::BeginChild("DirectoryPanel", { width, height }, true)) {
            ImGui::BeginDisabled(m_dialog_open);
            auto const select_directory{ ImGui::Button("Select directory") };
            ImGui::EndDisabled();

            if (select_directory) {
                openDirectoryDialog();
            }

            ImGui::SameLine();
            if (!m_selected_directory.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_selected_directory.c_str());
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderFileListPanel(float width, float height) -> void {
        if (ImGui::BeginChild("FileListPanel", { width, height }, true)) {
            auto const     available{ ImGui::GetContentRegionAvail() };
            constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable |
                                         ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("Files", 4, TABLE_FLAGS, available)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(
                    "No.",
                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
                    56.0F
                );
                ImGui::TableSetupColumn("Modified time", ImGuiTableColumnFlags_WidthFixed, 160.0F);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 110.0F);
                ImGui::TableSetupColumn("Absolute path", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(m_files.size()));
                while (clipper.Step()) {
                    for (auto item_index{ clipper.DisplayStart };
                         item_index < clipper.DisplayEnd;
                         ++item_index) {
                        auto const  index{ static_cast<std::size_t>(item_index) };
                        auto const& file{ m_files[index] };

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%zu", index + 1);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(file.modified_time.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(file.size.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(file.absolute_path.c_str());
                        ImGui::SetItemTooltip("%s", file.absolute_path.c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

} // namespace file_monitor::ui
