#include "MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::ui {

    namespace {

        constexpr auto FILE_SCAN_INTERVAL{ std::chrono::milliseconds{ 500 } };

        struct DirectoryDialogContext {
            std::shared_ptr<DirectoryDialogState> state;
        };

        auto file_change_status_color(core::FileChangeStatus status) -> ImVec4 {
            switch (status) {
                case core::FileChangeStatus::Added:
                    return { 0.30F, 0.85F, 0.40F, 1.0F };
                case core::FileChangeStatus::Removed:
                    return { 1.0F, 0.35F, 0.35F, 1.0F };
                case core::FileChangeStatus::Modified:
                    return { 1.0F, 0.80F, 0.25F, 1.0F };
            }
            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }

        auto SDLCALL directory_dialog_callback(
            void*              userdata,
            char const* const* file_list,
            int /*filter*/
        ) -> void {
            auto context{ std::unique_ptr<DirectoryDialogContext>{
                static_cast<DirectoryDialogContext*>(userdata) } };

            std::vector<std::filesystem::path> selected_directories;
            std::string                        error_message;
            if (file_list == nullptr) {
                error_message = SDL_GetError();
            } else {
                for (auto item{ file_list }; *item != nullptr; ++item) {
                    auto const* utf8_path{ reinterpret_cast<char8_t const*>(*item) };
                    selected_directories.emplace_back(std::u8string{ utf8_path });
                }
            }

            auto const lock{ std::scoped_lock{ context->state->mutex } };
            context->state->selected_directories = std::move(selected_directories);
            context->state->error_message        = std::move(error_message);
            context->state->completed            = true;
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
        updateFileMonitor();

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
        auto const settings_panel_height{
            ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0F
        };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0F, 0.0F });
        renderSettingsPanel(available.x, settings_panel_height);
        renderFileListPanel(available.x, 0.0F);
        ImGui::PopStyleVar();

        renderSettingsWindow();
        ImGui::End();
    }

    auto MainWindow::consumeDirectorySelection() -> void {
        std::vector<std::filesystem::path> selected_directories;
        std::string                        error_message;
        {
            auto const lock{ std::scoped_lock{ m_dialog_state->mutex } };
            if (!m_dialog_state->completed) {
                return;
            }

            m_dialog_state->completed = false;
            selected_directories      = std::move(m_dialog_state->selected_directories);
            error_message             = std::move(m_dialog_state->error_message);
            m_dialog_state->selected_directories.clear();
            m_dialog_state->error_message.clear();
        }

        m_dialog_open          = false;
        m_dialog_error_message = std::move(error_message);

        for (auto& directory : selected_directories) {
            directory = directory.lexically_normal();
            if (std::ranges::find(m_pending_directories, directory) ==
                m_pending_directories.end()) {
                m_pending_directories.emplace_back(std::move(directory));
            }
        }
    }

    auto MainWindow::openDirectoryDialog() -> void {
        m_dialog_open          = true;
        m_dialog_error_message = {};

        auto context{ std::make_unique<DirectoryDialogContext>() };
        context->state = m_dialog_state;

        auto const default_location_text{
            m_pending_directories.empty() ? std::string{} : core::path_to_utf8(m_pending_directories.back())
        };
        auto const* default_location{
            default_location_text.empty() ? nullptr : default_location_text.c_str()
        };
        SDL_ShowOpenFolderDialog(
            directory_dialog_callback,
            context.release(),
            m_parent_window,
            default_location,
            true
        );
    }

    auto MainWindow::resetFileMonitor() -> void {
        m_file_changes.clear();
        m_file_snapshot  = core::scan_files(m_directories);
        m_next_file_scan = std::chrono::steady_clock::now() + FILE_SCAN_INTERVAL;
    }

    auto MainWindow::renderSettingsPanel(float width, float height) -> void {
        auto open_settings{ false };
        if (ImGui::BeginChild("SettingsPanel", { width, height }, true)) {
            open_settings = ImGui::Button("设置", ImGui::GetContentRegionAvail());

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("已选择 %zu 个文件夹", m_directories.size());
            }
        }
        ImGui::EndChild();

        if (open_settings) {
            m_pending_directories = m_directories;
            m_dialog_error_message.clear();
            ImGui::OpenPopup("设置##SettingsWindow");
        }
    }

    auto MainWindow::renderFileListPanel(float width, float height) -> void {
        if (ImGui::BeginChild("FileListPanel", { width, height }, true)) {
            auto const     available{ ImGui::GetContentRegionAvail() };
            constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable |
                                         ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("Files", 5, TABLE_FLAGS, available)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(
                    "序号",
                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
                    56.0F
                );
                ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, 180.0F);
                ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 72.0F);
                ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, 110.0F);
                ImGui::TableSetupColumn("绝对路径", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(m_file_changes.size()));
                while (clipper.Step()) {
                    for (auto item_index{ clipper.DisplayStart };
                         item_index < clipper.DisplayEnd;
                         ++item_index) {
                        auto const  index{ static_cast<std::size_t>(item_index) };
                        auto const& change{ m_file_changes[index] };
                        auto const  status_text{ core::file_change_status_text(change.status) };
                        auto const  status_color{ file_change_status_color(change.status) };

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%zu", index + 1);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(change.time.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextColored(
                            status_color,
                            "%.*s",
                            static_cast<int>(status_text.size()),
                            status_text.data()
                        );
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextUnformatted(change.size.c_str());
                        ImGui::TableSetColumnIndex(4);
                        ImGui::TextUnformatted(change.absolute_path.c_str());
                        ImGui::SetItemTooltip("%s", change.absolute_path.c_str());
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::updateFileMonitor() -> void {
        if (m_directories.empty() || std::chrono::steady_clock::now() < m_next_file_scan) {
            return;
        }

        auto current_snapshot{ core::scan_files(m_directories) };
        auto changes{ core::detect_file_changes(m_file_snapshot, current_snapshot) };
        for (auto& change : changes) {
            m_file_changes.emplace_back(std::move(change));
        }
        m_file_snapshot  = std::move(current_snapshot);
        m_next_file_scan = std::chrono::steady_clock::now() + FILE_SCAN_INTERVAL;
    }

    auto MainWindow::renderSettingsWindow() -> void {
        auto const* viewport{ ImGui::GetMainViewport() };
        auto const  popup_size{
            ImVec2{
                   std::min(720.0F, viewport->WorkSize.x - 32.0F),
                   std::min(420.0F, viewport->WorkSize.y - 32.0F) }
        };
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, { 0.5F, 0.5F });
        ImGui::SetNextWindowSize(popup_size, ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal(
                "设置##SettingsWindow",
                nullptr,
                ImGuiWindowFlags_NoSavedSettings
            )) {
            return;
        }

        ImGui::BeginDisabled(m_dialog_open);
        auto const add_directories{ ImGui::Button("添加文件夹") };
        ImGui::EndDisabled();
        if (add_directories) {
            openDirectoryDialog();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_pending_directories.empty());
        auto const clear_directories{ ImGui::Button("清空") };
        ImGui::EndDisabled();
        if (clear_directories) {
            m_pending_directories.clear();
        }

        if (!m_dialog_error_message.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(
                ImVec4{ 0.85F, 0.25F, 0.25F, 1.0F },
                "选择文件夹失败：%s",
                m_dialog_error_message.c_str()
            );
        }

        constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Resizable |
                                     ImGuiTableFlags_ScrollY |
                                     ImGuiTableFlags_SizingStretchProp;

        std::optional<std::size_t> directory_to_remove;
        if (ImGui::BeginTable(
                "ConfiguredDirectories",
                2,
                TABLE_FLAGS,
                { 0.0F, -ImGui::GetFrameHeightWithSpacing() }
            )) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("文件夹路径", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 72.0F);
            ImGui::TableHeadersRow();

            if (m_pending_directories.empty()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("尚未添加文件夹");
            } else {
                for (auto directory_index{ std::size_t{ 0 } };
                     directory_index < m_pending_directories.size();
                     ++directory_index) {
                    auto const path_text{
                        core::path_to_utf8(m_pending_directories[directory_index])
                    };

                    ImGui::PushID(static_cast<int>(directory_index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(path_text.c_str());
                    ImGui::SetItemTooltip("%s", path_text.c_str());
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("删除", { ImGui::GetContentRegionAvail().x, 0.0F })) {
                        directory_to_remove = directory_index;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        if (directory_to_remove) {
            m_pending_directories.erase(
                m_pending_directories.begin() + *directory_to_remove
            );
        }

        auto const button_width{
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5F
        };
        if (ImGui::Button("取消", { button_width, 0.0F })) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("保存", { ImGui::GetContentRegionAvail().x, 0.0F })) {
            m_directories = m_pending_directories;
            resetFileMonitor();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

} // namespace file_monitor::ui
