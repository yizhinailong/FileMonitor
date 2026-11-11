#include "SettingsWindow.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <system_error>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "ErrorText.hpp"
#include "Style.hpp"
#include "Widgets.hpp"
#include "core/Utils.hpp"

namespace file_monitor::gui {
    namespace {

        constexpr auto POPUP_NAME{ "设置##SettingsWindow" };
        constexpr auto ACTION_COLUMN_WIDTH{ 72.0F };
        constexpr auto WARNING_COLUMN_WIDTH{ 220.0F };
        constexpr auto MIN_LIST_HEIGHT{ 120.0F };

        struct DirectoryDialogContext {
            std::shared_ptr<DirectoryDialogState> state;
        };

        auto SDLCALL directory_dialog_callback(
            void* userdata,
            char const* const* file_list,
            int /*filter*/
        ) -> void {
            auto context{ std::unique_ptr<DirectoryDialogContext>{
                static_cast<DirectoryDialogContext*>(userdata) } };

            std::vector<std::filesystem::path> selected_directories;
            std::string error_message;
            if (file_list == nullptr) {
                error_message = SDL_GetError();
            } else {
                for (auto item{ file_list }; *item != nullptr; ++item) {
                    selected_directories.emplace_back(core::utils::utf8_to_path(*item));
                }
            }

            auto const lock{ std::scoped_lock{ context->state->mutex } };
            context->state->selected_directories = std::move(selected_directories);
            context->state->error_message = std::move(error_message);
            context->state->completed = true;
        }

        auto contains(ImVec2 minimum, ImVec2 maximum, float x, float y) -> bool {
            return x >= minimum.x && x < maximum.x && y >= minimum.y && y < maximum.y;
        }

    } // namespace

    auto SettingsWindow::SetParentWindow(SDL_Window* window) -> void {
        m_parent_window = window;
    }

    auto SettingsWindow::Open(core::Configuration const& configuration) -> void {
        m_pending_configuration = configuration;
        m_saved_configuration = configuration;
        m_monitored_editor = {};
        m_excluded_editor = {};
        m_dialog_error_message.clear();
        m_drop_error.clear();
        m_confirm_discard = false;
        m_open_requested = true;
        refreshWarnings();
    }

    auto SettingsWindow::OpenWithDirectories(
        core::Configuration const& configuration,
        std::vector<std::filesystem::path> directories
    ) -> void {
        Open(configuration);
        for (auto& directory : directories) {
            core::add_directory(m_pending_configuration.directories, std::move(directory));
        }
        refreshWarnings();
    }

    auto SettingsWindow::IsOpen() const -> bool {
        return m_open || m_open_requested;
    }

    auto SettingsWindow::DropFiles(std::vector<DroppedFile> const& files) -> void {
        if (!IsOpen()) {
            return;
        }
        std::error_code error;
        for (auto const& file : files) {
            if (!contains(m_window_min, m_window_max, file.x, file.y)) {
                continue;
            }
            auto const group{
                contains(m_excluded_min, m_excluded_max, file.x, file.y) ?
                    DirectoryGroup::Excluded :
                    DirectoryGroup::Monitored
            };
            if (!std::filesystem::is_directory(file.path, error)) {
                m_drop_error = "只能拖入文件夹";
                continue;
            }
            m_drop_error.clear();
            if (core::add_directory(pendingDirectories(group), file.path)) {
                m_confirm_discard = false;
                refreshWarnings();
            }
        }
    }

    auto SettingsWindow::Update() -> bool {
        return consumeDirectorySelection();
    }

    auto SettingsWindow::Render(core::MonitorSession& session) -> void {
        if (m_open_requested) {
            m_open_requested = false;
            m_open = true;
            ImGui::OpenPopup(POPUP_NAME);
        }

        auto const* viewport{ ImGui::GetMainViewport() };
        auto const popup_size{
            ImVec2{
                   std::min(820.0F, viewport->WorkSize.x - 32.0F),
                   std::min(640.0F, viewport->WorkSize.y - 32.0F) }
        };
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, { 0.5F, 0.5F });
        ImGui::SetNextWindowSize(popup_size, ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal(POPUP_NAME, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            if (m_open) {
                m_open = false;
            }
            return;
        }
        m_window_min = ImGui::GetWindowPos();
        auto const window_size{ ImGui::GetWindowSize() };
        m_window_max = { m_window_min.x + window_size.x, m_window_min.y + window_size.y };

        // Escape and Ctrl+S, as long as no text is being typed.
        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
                save(session);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                cancel();
            }
        }

        render_error("选择文件夹失败", m_dialog_error_message);
        render_session_errors(session);
        if (hasUnsavedChanges()) {
            auto const note{ "有未保存的修改" };
            align_right(text_width(note));
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, warning_color());
            draw_text(note);
            ImGui::PopStyleColor();
        }

        // The two lists share the height above the buttons.
        auto const& style{ ImGui::GetStyle() };
        auto const available{ ImGui::GetContentRegionAvail() };
        auto const footer_height{ ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0F };
        auto const group_height{
            std::max((available.y - footer_height) * 0.5F - style.ItemSpacing.y, MIN_LIST_HEIGHT)
        };
        renderDirectoryGroup(DirectoryGroup::Monitored, group_height);
        renderDirectoryGroup(DirectoryGroup::Excluded, group_height);
        renderFooter(session);
        ImGui::EndPopup();
    }

    auto SettingsWindow::addManualDirectory(DirectoryGroup group) -> void {
        auto& editor{ directoryEditor(group) };
        auto const result{
            core::add_directory_input(pendingDirectories(group), editor.input)
        };
        if (!result) {
            editor.error_message = result.error();
            return;
        }

        editor.input.clear();
        editor.error_message.clear();
        markChanged();
    }

    auto SettingsWindow::consumeDirectorySelection() -> bool {
        std::vector<std::filesystem::path> selected_directories;
        std::string error_message;
        {
            auto const lock{ std::scoped_lock{ m_dialog_state->mutex } };
            if (!m_dialog_state->completed) {
                return false;
            }

            m_dialog_state->completed = false;
            selected_directories = std::move(m_dialog_state->selected_directories);
            error_message = std::move(m_dialog_state->error_message);
            m_dialog_state->selected_directories.clear();
            m_dialog_state->error_message.clear();
        }

        m_dialog_open = false;
        m_dialog_error_message = std::move(error_message);

        for (auto& directory : selected_directories) {
            core::add_directory(pendingDirectories(m_dialog_group), std::move(directory));
        }
        markChanged();
        return true;
    }

    auto SettingsWindow::directoryEditor(DirectoryGroup group)
        -> DirectoryEditorState& {
        return group == DirectoryGroup::Monitored ? m_monitored_editor : m_excluded_editor;
    }

    auto SettingsWindow::openDirectoryDialog(DirectoryGroup group) -> void {
        m_dialog_open = true;
        m_dialog_group = group;
        m_dialog_error_message = {};

        auto context{ std::make_unique<DirectoryDialogContext>() };
        context->state = m_dialog_state;

        auto const& directories{ pendingDirectories(group) };
        auto const default_location_text{
            directories.empty() ? std::string{} : core::utils::path_to_utf8(directories.back())
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

    auto SettingsWindow::pendingDirectories(DirectoryGroup group)
        -> std::vector<std::filesystem::path>& {
        return group == DirectoryGroup::Monitored ?
            m_pending_configuration.directories :
            m_pending_configuration.excluded_directories;
    }

    auto SettingsWindow::renderDirectoryGroup(DirectoryGroup group, float height) -> void {
        auto& editor{ directoryEditor(group) };
        auto& directories{ pendingDirectories(group) };
        auto const& warnings{
            group == DirectoryGroup::Monitored ? m_monitored_warnings : m_excluded_warnings
        };
        auto const title{ std::format(
            "{}（{}）",
            group == DirectoryGroup::Monitored ? "监控文件夹" : "不监控文件夹",
            directories.size()
        ) };

        ImGui::PushID(std::to_underlying(group));
        if (ImGui::BeginChild("Group", { 0.0F, height })) {
            ImGui::SeparatorText(title.c_str());

            auto const& style{ ImGui::GetStyle() };
            auto const add_width{ button_width("添加") };
            auto const choose_width{ button_width("选择文件夹…") };
            ImGui::SetNextItemWidth(
                std::max(
                    ImGui::GetContentRegionAvail().x - add_width - choose_width - style.ItemSpacing.x * 2.0F,
                    120.0F
                )
            );
            auto const submit{
                ImGui::InputTextWithHint(
                    "##DirectoryPath",
                    group == DirectoryGroup::Monitored ?
                        "输入或粘贴文件夹路径，按 Enter 添加" :
                        "输入或粘贴不监控的文件夹路径，按 Enter 添加",
                    &editor.input,
                    ImGuiInputTextFlags_EnterReturnsTrue
                )
            };
            if (ImGui::IsItemEdited()) {
                editor.error_message.clear();
            }
            ImGui::SameLine();
            auto const add{ ImGui::Button("添加") };
            if (submit || add) {
                addManualDirectory(group);
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(m_dialog_open);
            auto const choose{ ImGui::Button("选择文件夹…") };
            ImGui::EndDisabled();
            if (choose) {
                openDirectoryDialog(group);
            }

            render_error(editor.error_message);
            if (group == DirectoryGroup::Monitored) {
                render_error("拖放失败", m_drop_error);
            }

            // One line for the warnings and the errors that appear below the
            // list is always kept, so the list does not move when one appears.
            auto const table_height{ std::max(
                ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(),
                1.0F
            ) };
            auto const table_min{ ImGui::GetCursorScreenPos() };
            auto const table_width{ ImGui::GetContentRegionAvail().x };
            constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY;

            std::optional<std::size_t> directory_to_remove;
            if (ImGui::BeginTable("Directories", 3, TABLE_FLAGS, { 0.0F, table_height })) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("文件夹路径", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("提示", ImGuiTableColumnFlags_WidthFixed, WARNING_COLUMN_WIDTH);
                ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, ACTION_COLUMN_WIDTH);
                ImGui::TableHeadersRow();

                if (directories.empty()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    draw_disabled_text(
                        group == DirectoryGroup::Monitored ?
                            "尚未添加监控文件夹" :
                            "尚未添加不监控文件夹"
                    );
                } else {
                    for (auto [directory_index, directory] : std::views::enumerate(directories)) {
                        auto const path_text{ core::utils::path_to_utf8(directory) };

                        ImGui::PushID(static_cast<int>(directory_index));
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        draw_text(path_text);
                        ImGui::SetItemTooltip("%s", path_text.c_str());

                        ImGui::TableSetColumnIndex(1);
                        if (directory_index < static_cast<std::ptrdiff_t>(warnings.size()) &&
                            !warnings[static_cast<std::size_t>(directory_index)].empty()) {
                            ImGui::PushStyleColor(ImGuiCol_Text, warning_color());
                            draw_text(warnings[static_cast<std::size_t>(directory_index)]);
                            ImGui::PopStyleColor();
                        }

                        ImGui::TableSetColumnIndex(2);
                        if (ImGui::Button("删除", { -1.0F, 0.0F })) {
                            directory_to_remove = static_cast<std::size_t>(directory_index);
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
            if (directory_to_remove) {
                directories.erase(directories.begin() + static_cast<std::ptrdiff_t>(*directory_to_remove));
                markChanged();
            }

            if (group == DirectoryGroup::Monitored) {
                m_monitored_min = table_min;
                m_monitored_max = { table_min.x + table_width, table_min.y + table_height };
            } else {
                m_excluded_min = table_min;
                m_excluded_max = { table_min.x + table_width, table_min.y + table_height };
            }
        }
        ImGui::EndChild();
        ImGui::PopID();
    }

    auto SettingsWindow::renderFooter(core::MonitorSession& session) -> void {
        ImGui::Separator();
        auto const& style{ ImGui::GetStyle() };
        auto const button_width{ (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5F };

        std::string const close_label{
            hasUnsavedChanges() ? (m_confirm_discard ? "放弃修改" : "取消") : "关闭"
        };
        if (ImGui::Button(close_label.c_str(), { button_width, 0.0F })) {
            cancel();
        }
        if (hasUnsavedChanges()) {
            ImGui::SetItemTooltip("有未保存的修改，再点一次放弃");
        }
        ImGui::SameLine();
        if (ImGui::Button("保存", { ImGui::GetContentRegionAvail().x, 0.0F })) {
            save(session);
        }
        ImGui::SetItemTooltip("Ctrl+S");
    }

    auto SettingsWindow::refreshWarnings() -> void {
        m_monitored_warnings = core::monitored_directory_warnings(m_pending_configuration);
        m_excluded_warnings = core::excluded_directory_warnings(m_pending_configuration);
    }

    auto SettingsWindow::markChanged() -> void {
        m_confirm_discard = false;
        refreshWarnings();
    }

    auto SettingsWindow::hasUnsavedChanges() const -> bool {
        return m_pending_configuration.directories != m_saved_configuration.directories ||
            m_pending_configuration.excluded_directories != m_saved_configuration.excluded_directories;
    }

    auto SettingsWindow::save(core::MonitorSession& session) -> void {
        if (session.SaveConfiguration(m_pending_configuration)) {
            m_saved_configuration = m_pending_configuration;
            m_confirm_discard = false;
            m_open = false;
            m_open_requested = false;
            ImGui::CloseCurrentPopup();
        }
    }

    auto SettingsWindow::cancel() -> void {
        if (hasUnsavedChanges() && !m_confirm_discard) {
            m_confirm_discard = true;
            return;
        }
        m_open = false;
        m_open_requested = false;
        m_confirm_discard = false;
        ImGui::CloseCurrentPopup();
    }

} // namespace file_monitor::gui
