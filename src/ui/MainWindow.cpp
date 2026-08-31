#include "MainWindow.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <ranges>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::ui {

    namespace {

        constexpr auto MAX_FILE_CHANGES{ std::size_t{ 1000 } };
        auto const     DATA_DIRECTORY{ std::filesystem::path{ "data" } / "FileMonitor" };
        auto const     CONFIG_PATH{ DATA_DIRECTORY / "config.json" };

        auto file_change_status_color(core::FileChangeStatus status) -> ImVec4 {
            auto const light_theme{ SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT };
            switch (status) {
                case core::FileChangeStatus::Added:
                    return light_theme ? ImVec4{ 0.05F, 0.45F, 0.15F, 1.0F } : ImVec4{ 0.30F, 0.85F, 0.40F, 1.0F };
                case core::FileChangeStatus::Removed:
                    return light_theme ? ImVec4{ 0.75F, 0.10F, 0.10F, 1.0F } : ImVec4{ 1.0F, 0.35F, 0.35F, 1.0F };
                case core::FileChangeStatus::Modified:
                    return light_theme ? ImVec4{ 0.60F, 0.38F, 0.0F, 1.0F } : ImVec4{ 1.0F, 0.80F, 0.25F, 1.0F };
                case core::FileChangeStatus::Renamed:
                case core::FileChangeStatus::Moved:
                    return light_theme ? ImVec4{ 0.60F, 0.38F, 0.0F, 1.0F } : ImVec4{ 1.0F, 0.80F, 0.25F, 1.0F };
            }
            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }

    } // namespace

    MainWindow::MainWindow()
        : m_settings_window{ CONFIG_PATH },
          m_change_logger{ DATA_DIRECTORY } {
        resetFileMonitor();
    }

    auto MainWindow::SetParentWindow(SDL_Window* window) -> void {
        m_settings_window.SetParentWindow(window);
    }

    auto MainWindow::Render() -> void {
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

        if (m_settings_window.Render(m_monitor_error_message, m_log_error_message)) {
            resetFileMonitor();
        }
        ImGui::End();
    }

    auto MainWindow::resetFileMonitor() -> void {
        m_file_changes.clear();
        m_monitor_error_message.clear();
        m_scroll_to_latest = false;
        m_file_monitor.Start(
            m_settings_window.Directories(),
            m_settings_window.ExcludedDirectories()
        );
    }

    auto MainWindow::renderSettingsPanel(float width, float height) -> void {
        auto open_settings{ false };
        if (ImGui::BeginChild("SettingsPanel", { width, height }, true)) {
            open_settings = ImGui::Button("设置", ImGui::GetContentRegionAvail());

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "监控 %zu 个文件夹，排除 %zu 个文件夹",
                    m_settings_window.Directories().size(),
                    m_settings_window.ExcludedDirectories().size()
                );
            }
        }
        ImGui::EndChild();

        if (open_settings) {
            m_settings_window.Open();
        }
    }

    auto MainWindow::renderFileListPanel(float width, float height) -> void {
        if (ImGui::BeginChild("FileListPanel", { width, height }, true)) {
            auto const     available{ ImGui::GetContentRegionAvail() };
            constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                                         ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable |
                                         ImGuiTableFlags_ScrollX |
                                         ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingFixedFit;
            constexpr auto FIXED_COLUMNS_WIDTH{ 380.0F + 72.0F + 110.0F };
            constexpr auto MIN_PATH_COLUMN_WIDTH{ 640.0F };
            constexpr auto MAX_PATH_COLUMN_WIDTH{ 2400.0F };
            auto           path_column_width{ MIN_PATH_COLUMN_WIDTH };
            auto const     rename_separator_width{ ImGui::CalcTextSize(" -> ").x };
            for (auto const& change : m_file_changes) {
                auto text_width{ ImGui::CalcTextSize(change.absolute_path.c_str()).x };
                if (!change.previous_absolute_path.empty()) {
                    text_width +=
                        ImGui::CalcTextSize(change.previous_absolute_path.c_str()).x +
                        rename_separator_width;
                }
                path_column_width = std::max(path_column_width, text_width);
            }
            path_column_width = std::clamp(
                path_column_width + ImGui::GetStyle().CellPadding.x * 2.0F,
                MIN_PATH_COLUMN_WIDTH,
                MAX_PATH_COLUMN_WIDTH
            );
            path_column_width = std::max(
                path_column_width,
                available.x - FIXED_COLUMNS_WIDTH
            );
            auto const table_content_width{
                FIXED_COLUMNS_WIDTH + path_column_width
            };

            if (ImGui::BeginTable(
                    "Files",
                    4,
                    TABLE_FLAGS,
                    available,
                    table_content_width
                )) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, 380.0F);
                ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 72.0F);
                ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, 110.0F);
                ImGui::TableSetupColumn(
                    "绝对路径",
                    ImGuiTableColumnFlags_WidthFixed,
                    path_column_width
                );
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
                        ImGui::TextUnformatted(change.time.c_str());
                        ImGui::SetItemTooltip("%s", change.time.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(
                            status_color,
                            "%.*s",
                            static_cast<int>(status_text.size()),
                            status_text.data()
                        );
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(change.size.c_str());
                        ImGui::TableSetColumnIndex(3);
                        if (change.previous_absolute_path.empty()) {
                            ImGui::TextUnformatted(change.absolute_path.c_str());
                            ImGui::SetItemTooltip("%s", change.absolute_path.c_str());
                        } else {
                            ImGui::Text(
                                "%s -> %s",
                                change.previous_absolute_path.c_str(),
                                change.absolute_path.c_str()
                            );
                            ImGui::SetItemTooltip(
                                "%s -> %s",
                                change.previous_absolute_path.c_str(),
                                change.absolute_path.c_str()
                            );
                        }
                    }
                }
                if (m_scroll_to_latest) {
                    ImGui::SetScrollHereY(1.0F);
                    m_scroll_to_latest = false;
                }
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::updateFileMonitor() -> void {
        auto changes{ m_file_monitor.TakeChanges() };
        if (!changes.empty()) {
            auto const log_result{ m_change_logger.Write(changes) };
            if (!log_result) {
                m_log_error_message = log_result.error();
            } else {
                m_log_error_message.clear();
            }

            m_file_changes.append_range(changes | std::views::as_rvalue);

            if (m_file_changes.size() > MAX_FILE_CHANGES) {
                auto const excess_count{ m_file_changes.size() - MAX_FILE_CHANGES };
                m_file_changes.erase(
                    m_file_changes.begin(),
                    m_file_changes.begin() + static_cast<std::ptrdiff_t>(excess_count)
                );
            }
            m_scroll_to_latest = true;
        }

        auto monitor_error{ m_file_monitor.TakeError() };
        if (!monitor_error.empty()) {
            m_monitor_error_message = std::move(monitor_error);
        }
    }

} // namespace file_monitor::ui
