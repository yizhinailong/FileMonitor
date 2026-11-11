#include "MainWindow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "ErrorText.hpp"
#include "Style.hpp"
#include "Widgets.hpp"
#include "core/FileSystem.hpp"

namespace file_monitor::gui {
    namespace {

        constexpr auto ADD_DIRECTORY_LABEL{ "添加文件夹…" };
        constexpr auto SETTINGS_LABEL{ "设置" };

        struct MonitorState {
            std::string_view text;
            ImVec4 color;
        };

        auto monitor_state(core::MonitorSession const& session) -> MonitorState {
            if (session.CurrentConfiguration().directories.empty()) {
                return { "未监控", warning_color() };
            }
            if (!session.MonitorError().empty()) {
                return { "出错", error_color() };
            }
            return { "监控中", success_color() };
        }

        auto directory_summary(core::Configuration const& configuration) -> std::string {
            if (configuration.excluded_directories.empty()) {
                return std::format("{} 个文件夹", configuration.directories.size());
            }
            return std::format(
                "{} 个文件夹，排除 {} 个",
                configuration.directories.size(),
                configuration.excluded_directories.size()
            );
        }

        // A filled circle as high as a line of text, in place of an icon.
        auto draw_dot(ImVec4 const& color) -> void {
            auto const height{ ImGui::GetFrameHeight() };
            auto const radius{ ImGui::GetFontSize() * 0.25F };
            auto const position{ ImGui::GetCursorScreenPos() };
            ImGui::GetWindowDrawList()->AddCircleFilled(
                { position.x + radius, position.y + height * 0.5F },
                radius,
                ImGui::GetColorU32(color)
            );
            ImGui::Dummy({ radius * 2.0F, height });
        }

    } // namespace

    auto MainWindow::Settings() -> SettingsWindow& {
        return m_settings_window;
    }

    auto MainWindow::Update() -> bool {
        auto const update{ m_session.Update() };
        auto const directories_selected{ m_settings_window.Update() };
        return update.changes_added || update.errors_changed || directories_selected;
    }

    auto MainWindow::Render() -> void {
        m_change_table.Refresh();

        auto const* viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::Begin("FileMonitorMainWindow", nullptr, WINDOW_FLAGS);
        ImGui::PopStyleVar(2);

        renderToolbar();
        renderErrors();
        ImVec2 const table_size{ 0.0F, -(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y) };
        if (m_change_table.RowCount() == 0) {
            renderEmptyState(table_size);
        } else {
            m_change_table.Render(table_size);
        }
        renderStatusBar();

        m_settings_window.Render(m_session);
        ImGui::End();
    }

    auto MainWindow::renderToolbar() -> void {
        std::array<std::size_t, core::FILE_CHANGE_STATUS_COUNT> counts{};
        for (auto const& change : m_session.Changes()) {
            ++counts[static_cast<std::size_t>(std::to_underlying(change.status))];
        }

        auto const& style{ ImGui::GetStyle() };
        ImGui::AlignTextToFramePadding();
        for (auto const status : core::FILE_CHANGE_STATUSES) {
            ImGui::PushStyleColor(ImGuiCol_Text, file_change_status_color(status));
            draw_text(core::file_change_status_text(status));
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0F, style.ItemInnerSpacing.x);
            draw_text(std::to_string(counts[static_cast<std::size_t>(std::to_underlying(status))]));
            ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
        }

        align_right(button_width(SETTINGS_LABEL));
        if (ImGui::Button(SETTINGS_LABEL)) {
            openSettings();
        }
    }

    auto MainWindow::renderErrors() -> void {
        if (!has_session_errors(m_session)) {
            return;
        }
        if (ImGui::BeginChild("Errors", { 0.0F, 0.0F }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
            render_session_errors(m_session);
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderEmptyState(ImVec2 size) -> void {
        if (ImGui::BeginChild("NoChanges", size, ImGuiChildFlags_Borders)) {
            auto const monitoring{ !m_session.CurrentConfiguration().directories.empty() };
            std::string_view const message{ monitoring ? "正在监控，还没有文件变更" : "尚未添加监控文件夹" };
            auto const button_height{ monitoring ? 0.0F : ImGui::GetFrameHeightWithSpacing() };
            auto const free_height{ ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() - button_height };
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(free_height * 0.5F, 0.0F));

            align_center(text_width(message));
            draw_disabled_text(message);
            if (!monitoring) {
                align_center(button_width(ADD_DIRECTORY_LABEL));
                if (ImGui::Button(ADD_DIRECTORY_LABEL)) {
                    openSettings();
                }
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderStatusBar() -> void {
        auto const& style{ ImGui::GetStyle() };
        auto const state{ monitor_state(m_session) };
        ImGui::AlignTextToFramePadding();
        draw_dot(state.color);
        ImGui::SameLine(0.0F, style.ItemInnerSpacing.x * 1.5F);
        draw_text(state.text);
        ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
        draw_disabled_text(directory_summary(m_session.CurrentConfiguration()));

        if (m_change_table.RowCount() == 0) {
            return;
        }
        auto const count_text{ std::format("{} 条变更", m_change_table.RowCount()) };
        ImGui::SameLine();
        if (m_change_table.FollowsLatest()) {
            auto const follow_text{ count_text + "    跟随最新" };
            align_right(text_width(follow_text));
            draw_disabled_text(follow_text);
            return;
        }

        // The part after "###" identifies the button while its label changes.
        auto const new_change_count{ m_change_table.NewChangeCount() };
        auto const latest_label{
            new_change_count > 0 ?
                std::format("↓ {} 条新变更###Latest", new_change_count) :
                std::string{ "↓ 回到最新###Latest" }
        };
        align_right(text_width(count_text) + style.ItemSpacing.x * 2.0F + button_width(latest_label.c_str()));
        draw_disabled_text(count_text);
        ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
        if (ImGui::Button(latest_label.c_str())) {
            m_change_table.ScrollToLatest();
        }
    }

    auto MainWindow::openSettings() -> void {
        m_settings_window.Open(m_session.CurrentConfiguration());
    }

} // namespace file_monitor::gui
