#include "MainWindow.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <misc/cpp/imgui_stdlib.h>

#include "ErrorText.hpp"
#include "Style.hpp"
#include "Widgets.hpp"

import file_monitor.core;

namespace file_monitor::gui {
    namespace {

        constexpr auto ADD_DIRECTORY_LABEL{ "添加文件夹…" };
        constexpr auto SETTINGS_LABEL{ "设置" };
        constexpr auto COPY_PATH_LABEL{ "复制路径" };
        constexpr auto SHOW_IN_EXPLORER_LABEL{ "打开位置" };
        constexpr auto CLEAR_FILTERS_LABEL{ "清除过滤" };
        constexpr auto FILTER_HINT{ "过滤路径 (Ctrl+F)" };
        constexpr auto FILTER_WIDTH{ 220.0F };
        constexpr auto NOTICE_DURATION{ std::chrono::seconds{ 3 } };

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

        auto size_description(core::FileChange const& change) -> std::string {
            // Directories have no size.
            if (change.size == "-") {
                return "文件夹";
            }
            return std::format("大小 {}", change.size);
        }

        // A label and a path that can be selected with the mouse but not
        // edited. A path longer than `width` loses its beginning and is shown
        // in full in a tooltip.
        auto draw_path_field(char const* id, std::string_view label, std::string const& path, float width) -> void {
            auto const& style{ ImGui::GetStyle() };
            ImGui::AlignTextToFramePadding();
            draw_disabled_text(label);
            ImGui::SameLine();

            auto const field_width{ width - text_width(label) - style.ItemSpacing.x };
            auto text{ path };
            ImGui::SetNextItemWidth(field_width);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0F, style.FramePadding.y });
            ImGui::InputText(id, &text, ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_ElideLeft);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            if (text_width(path) > field_width) {
                ImGui::SetItemTooltip("%s", path.c_str());
            }
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
        auto const notice_ended{ !m_notice.empty() && std::chrono::steady_clock::now() >= m_notice_end };
        if (notice_ended) {
            m_notice.clear();
        }
        return update.changes_added || update.errors_changed || directories_selected || notice_ended;
    }

    auto MainWindow::DropFiles(std::vector<DroppedFile> const& files) -> void {
        if (files.empty()) {
            return;
        }
        if (m_settings_window.IsOpen()) {
            m_settings_window.DropFiles(files);
            return;
        }

        std::error_code error;
        std::vector<std::filesystem::path> directories;
        for (auto const& file : files) {
            if (std::filesystem::is_directory(file.path, error)) {
                directories.push_back(file.path);
            }
        }
        if (directories.empty()) {
            showNotice("只能拖入文件夹", true);
            return;
        }
        m_settings_window.OpenWithDirectories(m_session.CurrentConfiguration(), std::move(directories));
        showNotice("拖入的文件夹已加入监控列表，保存后生效");
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

        handleShortcuts();
        renderToolbar();
        renderErrors();
        auto const& style{ ImGui::GetStyle() };
        auto const status_bar_height{ ImGui::GetFrameHeight() + style.ItemSpacing.y };
        if (m_change_table.RowCount() == 0) {
            renderEmptyState({ 0.0F, -status_bar_height });
        } else {
            // A row of the height of a button for the summary and one for each
            // path of the selected change.
            auto const* selected_change{ m_change_table.SelectedChange() };
            auto const renamed{ selected_change != nullptr && !selected_change->previous_absolute_path.empty() };
            auto const details_row_count{ renamed ? 3.0F : 2.0F };
            auto const details_height{
                ImGui::GetFrameHeight() * details_row_count +
                style.ItemSpacing.y * (details_row_count - 1.0F) +
                style.WindowPadding.y * 2.0F
            };
            run(m_change_table.Render({ 0.0F, -(details_height + style.ItemSpacing.y + status_bar_height) }));
            renderDetails(details_height);
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
        auto const row_width{ ImGui::GetContentRegionAvail().x };
        auto const settings_width{ button_width(SETTINGS_LABEL) };
        auto const statuses_width{
            row_width - FILTER_WIDTH - settings_width - style.ItemSpacing.x * 4.0F
        };

        ImGui::SetNextItemWidth(FILTER_WIDTH);
        if (m_focus_filter) {
            ImGui::SetKeyboardFocusHere();
            m_focus_filter = false;
        }
        if (ImGui::InputTextWithHint(
                "##Filter",
                FILTER_HINT,
                &m_filter_text,
                ImGuiInputTextFlags_EscapeClearsAll
            )) {
            applyPathFilter();
        }
        ImGui::SameLine();

        // The status counts toggle their status. A hidden status loses its
        // color. The area is clipped in front of the settings button when the
        // window is narrow.
        auto& filter{ m_change_table.Filter() };
        auto const statuses_start{ ImGui::GetCursorScreenPos() };
        ImGui::PushClipRect(
            statuses_start,
            { statuses_start.x + std::max(statuses_width, 0.0F), statuses_start.y + ImGui::GetFrameHeight() },
            true
        );
        ImGui::AlignTextToFramePadding();
        for (auto const status : core::FILE_CHANGE_STATUSES) {
            auto const shows{ filter.ShowsStatus(status) };
            ImGui::PushStyleColor(
                ImGuiCol_TextLink,
                shows ? file_change_status_color(status) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
            );
            auto const label{ std::format(
                "{} {}",
                core::file_change_status_text(status),
                counts[static_cast<std::size_t>(std::to_underlying(status))]
            ) };
            if (ImGui::TextLink(label.c_str())) {
                filter.ToggleStatus(status);
                m_change_table.Refresh();
            }
            ImGui::SetItemTooltip(shows ? "点击隐藏这一类变更" : "点击显示这一类变更");
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
        }
        ImGui::PopClipRect();

        align_right(settings_width);
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
            auto const filtered{ !m_session.Changes().empty() };
            std::string_view message;
            if (filtered) {
                message = "没有符合过滤条件的变更";
            } else if (monitoring) {
                message = "正在监控，还没有文件变更";
            } else {
                message = "尚未添加监控文件夹";
            }
            auto const shows_button{ filtered || !monitoring };
            auto const button_height{ shows_button ? ImGui::GetFrameHeightWithSpacing() : 0.0F };
            auto const free_height{ ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() - button_height };
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(free_height * 0.5F, 0.0F));

            align_center(text_width(message));
            draw_disabled_text(message);
            if (filtered) {
                align_center(button_width(CLEAR_FILTERS_LABEL));
                if (ImGui::Button(CLEAR_FILTERS_LABEL)) {
                    clearFilters();
                }
            } else if (!monitoring) {
                align_center(button_width(ADD_DIRECTORY_LABEL));
                if (ImGui::Button(ADD_DIRECTORY_LABEL)) {
                    openSettings();
                }
            }
        }
        ImGui::EndChild();
    }

    auto MainWindow::renderDetails(float height) -> void {
        auto const& style{ ImGui::GetStyle() };
        auto const* change{ m_change_table.SelectedChange() };
        if (ImGui::BeginChild("Details", { 0.0F, height }, ImGuiChildFlags_Borders) && change == nullptr) {
            std::string_view const hint{ "选择一条变更，查看它完整的时间和路径" };
            auto const free_height{ ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() };
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(free_height * 0.5F, 0.0F));
            align_center(text_width(hint));
            draw_disabled_text(hint);
        } else if (change != nullptr) {
            // The buttons stay at the right edge and the texts are cut off in
            // front of them when the window is narrow.
            auto const row_start{ ImGui::GetCursorPos() };
            auto const row_position{ ImGui::GetCursorScreenPos() };
            auto const row_width{ ImGui::GetContentRegionAvail().x };
            auto const buttons_width{
                button_width(COPY_PATH_LABEL) + style.ItemSpacing.x + button_width(SHOW_IN_EXPLORER_LABEL)
            };
            auto const texts_width{ row_width - buttons_width - style.ItemSpacing.x * 2.0F };
            ImGui::PushClipRect(
                row_position,
                { row_position.x + texts_width, row_position.y + ImGui::GetFrameHeight() },
                true
            );
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, file_change_status_color(change->status));
            draw_text(core::file_change_status_text(change->status));
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
            draw_text(core::file_change_event_time(*change));
            ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
            draw_text(size_description(*change));
            if (auto const creation_time{ core::file_change_creation_time(*change) }; !creation_time.empty()) {
                ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
                draw_text(std::format("创建于 {}", creation_time));
            }
            ImGui::PopClipRect();

            ImGui::SameLine(row_start.x + row_width - buttons_width);
            if (ImGui::Button(COPY_PATH_LABEL)) {
                run(ChangeAction::CopyPath);
            }
            ImGui::SetItemTooltip("Ctrl+C");
            ImGui::SameLine();
            if (ImGui::Button(SHOW_IN_EXPLORER_LABEL)) {
                run(ChangeAction::ShowInExplorer);
            }
            ImGui::SetItemTooltip("在资源管理器中显示，也可以双击这条变更");

            if (change->previous_absolute_path.empty()) {
                draw_path_field("##Path", "路径", change->absolute_path, row_width);
            } else {
                draw_path_field("##PreviousPath", "原路径", change->previous_absolute_path, row_width);
                draw_path_field("##Path", "新路径", change->absolute_path, row_width);
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
        if (!m_notice.empty()) {
            ImGui::SameLine(0.0F, style.ItemSpacing.x * 2.0F);
            ImGui::PushStyleColor(ImGuiCol_Text, m_notice_failed ? error_color() : success_color());
            draw_text(m_notice);
            ImGui::PopStyleColor();
        }

        if (m_change_table.RowCount() == 0) {
            return;
        }
        auto const count_text{
            m_change_table.Filter().IsActive() ?
                std::format("显示 {} / {} 条变更", m_change_table.RowCount(), m_session.Changes().size()) :
                std::format("{} 条变更", m_change_table.RowCount())
        };
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

    auto MainWindow::handleShortcuts() -> void {
        // A letter only counts as a shortcut while no text is being typed;
        // Dear ImGui sees to that.
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) || ImGui::Shortcut(ImGuiKey_Y)) {
            run(ChangeAction::CopyPath);
        }
        if (ImGui::Shortcut(ImGuiKey_O)) {
            run(ChangeAction::ShowInExplorer);
        }
        // The focused table handles End itself by moving to its last row, so
        // the key is only looked at here, not taken.
        auto const focused{ ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) };
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_End, false)) {
            m_change_table.ScrollToLatest();
        }

        // The rest only counts while no text is being typed.
        if (ImGui::GetIO().WantTextInput) {
            return;
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_Slash, false)) {
            m_focus_filter = true;
        }
        for (auto const [index, status] : std::views::enumerate(core::FILE_CHANGE_STATUSES)) {
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + index), false)) {
                m_change_table.Filter().ToggleStatus(status);
                m_change_table.Refresh();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_0, false) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            clearFilters();
        }
    }

    auto MainWindow::applyPathFilter() -> void {
        m_change_table.Filter().SetPathText(m_filter_text);
        m_change_table.Refresh();
    }

    auto MainWindow::clearFilters() -> void {
        m_filter_text.clear();
        m_change_table.Filter().Clear();
        m_change_table.Refresh();
    }

    auto MainWindow::run(ChangeAction action) -> void {
        auto const* change{ m_change_table.SelectedChange() };
        if (change == nullptr) {
            return;
        }
        switch (action) {
            case ChangeAction::None:
                break;
            case ChangeAction::CopyPath:
                ImGui::SetClipboardText(change->absolute_path.c_str());
                showNotice("已复制路径");
                break;
            case ChangeAction::CopyPreviousPath:
                ImGui::SetClipboardText(change->previous_absolute_path.c_str());
                showNotice("已复制原路径");
                break;
            case ChangeAction::ShowInExplorer:
                if (core::show_in_explorer(change->absolute_path)) {
                    showNotice("已在资源管理器中打开");
                } else {
                    showNotice("无法打开资源管理器", true);
                }
                break;
            case ChangeAction::FilterDirectory:
                m_filter_text = std::string{ core::directory_text(change->absolute_path) };
                applyPathFilter();
                showNotice("只看这个文件夹");
                break;
        }
    }

    auto MainWindow::showNotice(std::string_view text, bool failed) -> void {
        m_notice = text;
        m_notice_failed = failed;
        m_notice_end = std::chrono::steady_clock::now() + NOTICE_DURATION;
    }

    auto MainWindow::openSettings() -> void {
        m_settings_window.Open(m_session.CurrentConfiguration());
    }

} // namespace file_monitor::gui
