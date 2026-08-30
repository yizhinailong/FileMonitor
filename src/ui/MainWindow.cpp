#include "MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <mutex>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::ui {
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

        auto format_file_size(std::filesystem::directory_entry const& entry) -> std::string {
            std::error_code size_error;
            auto const      size{ entry.file_size(size_error) };
            return size_error ? "Unknown" : std::format("{}", size);
        }

        auto format_modified_time(std::filesystem::directory_entry const& entry) -> std::string {
            std::error_code time_error;
            auto const      file_time{ entry.last_write_time(time_error) };
            if (time_error) {
                return "Unknown";
            }

            auto const system_time{ std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - decltype(file_time)::clock::now() + std::chrono::system_clock::now()
            ) };
            auto const whole_seconds{ std::chrono::floor<std::chrono::seconds>(system_time) };
            auto const milliseconds{
                std::chrono::duration_cast<std::chrono::milliseconds>(system_time - whole_seconds)
                    .count()
            };
            return std::format(
                "{:%Y-%m-%d %H:%M:%S}.{:03}",
                whole_seconds,
                milliseconds
            );
        }

        auto absolute_path_to_utf8(std::filesystem::path const& path) -> std::string {
            std::error_code absolute_error;
            auto const      absolute_path{ std::filesystem::absolute(path, absolute_error) };
            return path_to_utf8(absolute_error ? path : absolute_path);
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

        m_dialog_open = false;
        if (!error_message.empty()) {
            m_directory_status = "Directory selection failed: " + error_message;
            return;
        }

        if (!selected_directory) {
            m_directory_status = m_selected_directory.empty() ? "No directory selected" : m_selected_directory;
            return;
        }

        m_selected_directory = path_to_utf8(*selected_directory);
        m_directory_status   = m_selected_directory;
        loadFiles(*selected_directory);
    }

    auto MainWindow::loadFiles(std::filesystem::path const& directory) -> void {
        std::vector<FileInfo> files;
        std::error_code       iteration_error;
        auto                  iterator{
            std::filesystem::recursive_directory_iterator{
                                                          directory,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          iteration_error }
        };
        auto const end{ std::filesystem::recursive_directory_iterator{} };

        while (!iteration_error && iterator != end) {
            std::error_code type_error;
            if (iterator->is_regular_file(type_error) && !type_error) {
                files.emplace_back(FileInfo{ format_modified_time(*iterator), format_file_size(*iterator), absolute_path_to_utf8(iterator->path()) });
            }
            iterator.increment(iteration_error);
        }

        std::ranges::sort(files, {}, &FileInfo::absolute_path);
        m_files = std::move(files);

        auto const directory_text{ path_to_utf8(directory) };
        if (iteration_error) {
            m_file_summary = std::format(
                "Unable to read all files in {}: {}",
                directory_text,
                iteration_error.message()
            );
        } else {
            m_file_summary = std::format("{} file(s) in {}", m_files.size(), directory_text);
        }
    }

    auto MainWindow::openDirectoryDialog() -> void {
        m_dialog_open      = true;
        m_directory_status = "Selecting directory...";

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
            ImGui::TextUnformatted(m_directory_status.c_str());
            if (!m_selected_directory.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", m_selected_directory.c_str());
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderFileListPanel(float width, float height) -> void {
        if (ImGui::BeginChild("FileListPanel", { width, height }, true)) {
            ImGui::TextUnformatted(m_file_summary.c_str());
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
                ImGui::TableSetupColumn("Size (bytes)", ImGuiTableColumnFlags_WidthFixed, 110.0F);
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
