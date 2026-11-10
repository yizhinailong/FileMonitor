#include "MainWindow.hpp"

#include <algorithm>
#include <cstddef>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "ErrorText.hpp"

namespace file_monitor::gui {

    namespace {

        constexpr auto FIXED_COLUMNS_WIDTH{ 380.0F + 72.0F + 110.0F };

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

    auto MainWindow::Settings() -> SettingsWindow& {
        return m_settings_window;
    }

    auto MainWindow::Render() -> void {
        if (m_session.Update().changes_added) {
            m_scroll_to_latest = true;
        }

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
        renderErrorPanel(available.x);
        renderFileListPanel(available.x, 0.0F);
        ImGui::PopStyleVar();

        m_settings_window.Render(m_session);
        ImGui::End();
    }

    auto MainWindow::renderSettingsPanel(float width, float height) -> void {
        auto const& configuration{ m_session.CurrentConfiguration() };
        auto open_settings{ false };
        if (ImGui::BeginChild("SettingsPanel", { width, height }, true)) {
            open_settings = ImGui::Button("设置", ImGui::GetContentRegionAvail());

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "监控 %zu 个文件夹，排除 %zu 个文件夹",
                    configuration.directories.size(),
                    configuration.excluded_directories.size()
                );
            }
        }
        ImGui::EndChild();

        if (open_settings) {
            m_settings_window.Open(configuration);
        }
    }

    auto MainWindow::renderErrorPanel(float width) -> void {
        if (!has_session_errors(m_session)) {
            return;
        }
        if (ImGui::BeginChild(
                "ErrorPanel",
                { width, 0.0F },
                ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY
            )) {
            render_session_errors(m_session);
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderFileListPanel(float width, float height) -> void {
        if (ImGui::BeginChild("FileListPanel", { width, height }, true)) {
            auto const available_width{ ImGui::GetContentRegionAvail().x };
            constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY;
            auto const preferred_path_width{ calculatePathColumnWidth() };
            auto const table_content_width{
                std::max(
                    available_width,
                    FIXED_COLUMNS_WIDTH + preferred_path_width
                )
            };

            if (ImGui::BeginTable("Files", 4, TABLE_FLAGS, { available_width, 0.0F }, table_content_width)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, 380.0F);
                ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 72.0F);
                ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, 110.0F);
                ImGui::TableSetupColumn("绝对路径", ImGuiTableColumnFlags_WidthStretch, 1.0F);
                ImGui::TableHeadersRow();

                auto const changes{ m_session.Changes() };
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(changes.size()));
                while (clipper.Step()) {
                    for (auto item_index{ clipper.DisplayStart };
                         item_index < clipper.DisplayEnd;
                         ++item_index) {
                        renderFileChangeRow(changes[static_cast<std::size_t>(item_index)]);
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

    auto MainWindow::renderFileChangeRow(core::FileChange const& change) -> void {
        auto const status_text{ core::file_change_status_text(change.status) };
        auto const status_color{ file_change_status_color(change.status) };

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

    auto MainWindow::calculatePathColumnWidth() const -> float {
        auto path_width{ ImGui::CalcTextSize("绝对路径").x };
        auto const rename_separator_width{ ImGui::CalcTextSize(" -> ").x };
        for (auto const& change : m_session.Changes()) {
            auto text_width{ ImGui::CalcTextSize(change.absolute_path.c_str()).x };
            if (!change.previous_absolute_path.empty()) {
                text_width +=
                    ImGui::CalcTextSize(change.previous_absolute_path.c_str()).x +
                    rename_separator_width;
            }
            path_width = std::max(path_width, text_width);
        }
        path_width += ImGui::GetStyle().CellPadding.x * 2.0F;
        return path_width;
    }

} // namespace file_monitor::gui
