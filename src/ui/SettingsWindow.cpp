#include "SettingsWindow.hpp"

#include <algorithm>
#include <exception>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/FileSystem.hpp"

namespace file_monitor::ui {

    struct DirectoryDialogState {
        std::mutex                         mutex;
        std::vector<std::filesystem::path> selected_directories;
        std::string                        error_message;
        bool                               completed{ false };
    };

    namespace {

        struct DirectoryDialogContext {
            std::shared_ptr<DirectoryDialogState> state;
        };

        auto error_text_color() -> ImVec4 {
            return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT ? ImVec4{ 0.75F, 0.10F, 0.10F, 1.0F } : ImVec4{ 0.85F, 0.25F, 0.25F, 1.0F };
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

        auto utf8_to_path(std::string_view path) -> std::filesystem::path {
            auto const* begin{ reinterpret_cast<char8_t const*>(path.data()) };
            return std::filesystem::path{
                std::u8string{ begin, begin + path.size() }
            };
        }

    } // namespace

    SettingsWindow::SettingsWindow(std::filesystem::path config_path)
        : m_config_path{ std::move(config_path) },
          m_dialog_state{ std::make_shared<DirectoryDialogState>() } {
        auto configuration{ core::load_configuration(m_config_path) };
        if (!configuration) {
            m_configuration_error_message = std::move(configuration.error());
            return;
        }
        m_configuration = std::move(*configuration);
    }

    auto SettingsWindow::SetParentWindow(SDL_Window* window) -> void {
        m_parent_window = window;
    }

    auto SettingsWindow::Open() -> void {
        m_pending_configuration = m_configuration;
        m_monitored_editor      = {};
        m_excluded_editor       = {};
        m_dialog_error_message.clear();
        ImGui::OpenPopup("设置##SettingsWindow");
    }

    auto SettingsWindow::Render(
        std::string_view monitor_error,
        std::string_view log_error
    ) -> bool {
        consumeDirectorySelection();

        auto const* viewport{ ImGui::GetMainViewport() };
        auto const  popup_size{
            ImVec2{
                   std::min(760.0F, viewport->WorkSize.x - 32.0F),
                   std::min(500.0F, viewport->WorkSize.y - 32.0F) }
        };
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, { 0.5F, 0.5F });
        ImGui::SetNextWindowSize(popup_size, ImGuiCond_Appearing);

        if (!ImGui::BeginPopupModal(
                "设置##SettingsWindow",
                nullptr,
                ImGuiWindowFlags_NoSavedSettings
            )) {
            return false;
        }

        if (!m_dialog_error_message.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "选择文件夹失败：%s",
                m_dialog_error_message.c_str()
            );
        }
        if (!monitor_error.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "监控失败：%.*s",
                static_cast<int>(monitor_error.size()),
                monitor_error.data()
            );
        }
        if (!m_configuration_error_message.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "配置失败：%s",
                m_configuration_error_message.c_str()
            );
        }
        if (!log_error.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "日志失败：%.*s",
                static_cast<int>(log_error.size()),
                log_error.data()
            );
        }

        if (ImGui::BeginChild(
                "DirectorySettings",
                { 0.0F, -ImGui::GetFrameHeightWithSpacing() }
            )) {
            if (ImGui::BeginTabBar("DirectoryGroups")) {
                if (ImGui::BeginTabItem("监控文件夹")) {
                    renderDirectoryEditor(
                        DirectoryGroup::Monitored,
                        "输入监控文件夹路径",
                        "尚未添加监控文件夹"
                    );
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("不监控文件夹")) {
                    renderDirectoryEditor(
                        DirectoryGroup::Excluded,
                        "输入不监控的文件夹路径",
                        "尚未添加不监控文件夹"
                    );
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        auto const button_width{
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5F
        };
        if (ImGui::Button("取消", { button_width, 0.0F })) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        auto configuration_changed{ false };
        if (ImGui::Button("保存", { ImGui::GetContentRegionAvail().x, 0.0F })) {
            auto const save_result{
                core::save_configuration(m_config_path, m_pending_configuration)
            };
            if (!save_result) {
                m_configuration_error_message = save_result.error();
            } else {
                m_configuration_error_message.clear();
                m_configuration       = m_pending_configuration;
                configuration_changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
        return configuration_changed;
    }

    auto SettingsWindow::Directories() const
        -> std::vector<std::filesystem::path> const& {
        return m_configuration.directories;
    }

    auto SettingsWindow::ExcludedDirectories() const
        -> std::vector<std::filesystem::path> const& {
        return m_configuration.excluded_directories;
    }

    auto SettingsWindow::addManualDirectory(DirectoryGroup group) -> void {
        auto& editor{ directoryEditor(group) };
        if (editor.input.empty()) {
            editor.error_message = "请输入文件夹路径";
            return;
        }

        std::filesystem::path directory;
        try {
            directory = utf8_to_path(editor.input).lexically_normal();
        } catch (std::exception const& error) {
            editor.error_message = "文件夹路径无效：" + std::string{ error.what() };
            return;
        }

        std::error_code check_error;
        auto const      is_directory{ std::filesystem::is_directory(directory, check_error) };
        if (check_error) {
            editor.error_message = "检查文件夹失败：" + check_error.message();
            return;
        }
        if (!is_directory) {
            editor.error_message = "路径不是存在的文件夹";
            return;
        }
        if (!addPendingDirectory(group, directory)) {
            editor.error_message = "该文件夹已添加";
            return;
        }

        editor.input.clear();
        editor.error_message.clear();
    }

    auto SettingsWindow::addPendingDirectory(
        DirectoryGroup        group,
        std::filesystem::path directory
    ) -> bool {
        directory = directory.lexically_normal();
        auto& directories{ pendingDirectories(group) };
        if (std::ranges::contains(directories, directory)) {
            return false;
        }

        directories.emplace_back(std::move(directory));
        return true;
    }

    auto SettingsWindow::consumeDirectorySelection() -> void {
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
            addPendingDirectory(m_dialog_group, std::move(directory));
        }
    }

    auto SettingsWindow::directoryEditor(DirectoryGroup group)
        -> DirectoryEditorState& {
        return group == DirectoryGroup::Monitored ? m_monitored_editor : m_excluded_editor;
    }

    auto SettingsWindow::openDirectoryDialog(DirectoryGroup group) -> void {
        m_dialog_open          = true;
        m_dialog_group         = group;
        m_dialog_error_message = {};

        auto context{ std::make_unique<DirectoryDialogContext>() };
        context->state = m_dialog_state;

        auto const& directories{ pendingDirectories(group) };
        auto const  default_location_text{
            directories.empty() ? std::string{} : core::path_to_utf8(directories.back())
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
        return group == DirectoryGroup::Monitored ? m_pending_configuration.directories : m_pending_configuration.excluded_directories;
    }

    auto SettingsWindow::renderDirectoryEditor(
        DirectoryGroup   group,
        std::string_view input_hint,
        std::string_view empty_text
    ) -> void {
        auto& editor{ directoryEditor(group) };
        auto& directories{ pendingDirectories(group) };

        ImGui::PushID(std::to_underlying(group));
        constexpr auto ADD_BUTTON_WIDTH{ 72.0F };
        constexpr auto SELECT_BUTTON_WIDTH{ 104.0F };
        constexpr auto CLEAR_BUTTON_WIDTH{ 72.0F };
        auto const     button_spacing{ ImGui::GetStyle().ItemSpacing.x * 3.0F };
        ImGui::SetNextItemWidth(
            std::max(
                1.0F,
                ImGui::GetContentRegionAvail().x - ADD_BUTTON_WIDTH - SELECT_BUTTON_WIDTH -
                    CLEAR_BUTTON_WIDTH - button_spacing
            )
        );
        auto const submit_directory{
            ImGui::InputTextWithHint(
                "##DirectoryPath",
                input_hint.data(),
                &editor.input,
                ImGuiInputTextFlags_EnterReturnsTrue
            )
        };
        if (ImGui::IsItemEdited()) {
            editor.error_message.clear();
        }
        ImGui::SameLine();
        auto const add_directory{
            ImGui::Button("添加", { ADD_BUTTON_WIDTH, 0.0F })
        };
        if (submit_directory || add_directory) {
            addManualDirectory(group);
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_dialog_open);
        auto const select_directories{
            ImGui::Button("选择文件夹", { SELECT_BUTTON_WIDTH, 0.0F })
        };
        ImGui::EndDisabled();
        if (select_directories) {
            openDirectoryDialog(group);
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(directories.empty());
        auto const clear_directories{
            ImGui::Button("清空", { CLEAR_BUTTON_WIDTH, 0.0F })
        };
        ImGui::EndDisabled();
        if (clear_directories) {
            directories.clear();
        }

        if (!editor.error_message.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "%s",
                editor.error_message.c_str()
            );
        }

        constexpr auto TABLE_FLAGS = ImGuiTableFlags_Borders |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Resizable |
                                     ImGuiTableFlags_ScrollX |
                                     ImGuiTableFlags_ScrollY |
                                     ImGuiTableFlags_SizingFixedFit;
        constexpr auto ACTION_COLUMN_WIDTH{ 72.0F };
        constexpr auto MIN_PATH_COLUMN_WIDTH{ 520.0F };
        constexpr auto MAX_PATH_COLUMN_WIDTH{ 2000.0F };
        auto           path_column_width{ MIN_PATH_COLUMN_WIDTH };
        for (auto const& directory : directories) {
            auto const path_text{ core::path_to_utf8(directory) };
            path_column_width = std::max(
                path_column_width,
                ImGui::CalcTextSize(path_text.c_str()).x
            );
        }
        path_column_width = std::clamp(
            path_column_width + ImGui::GetStyle().CellPadding.x * 2.0F,
            MIN_PATH_COLUMN_WIDTH,
            MAX_PATH_COLUMN_WIDTH
        );
        path_column_width = std::max(
            path_column_width,
            ImGui::GetContentRegionAvail().x - ACTION_COLUMN_WIDTH
        );
        auto const table_content_width{ path_column_width + ACTION_COLUMN_WIDTH };

        std::optional<std::size_t> directory_to_remove;
        if (ImGui::BeginTable(
                "ConfiguredDirectories",
                2,
                TABLE_FLAGS,
                { 0.0F, 0.0F },
                table_content_width
            )) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(
                "文件夹路径",
                ImGuiTableColumnFlags_WidthFixed,
                path_column_width
            );
            ImGui::TableSetupColumn(
                "操作",
                ImGuiTableColumnFlags_WidthFixed,
                ACTION_COLUMN_WIDTH
            );
            ImGui::TableHeadersRow();

            if (directories.empty()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled(
                    "%.*s",
                    static_cast<int>(empty_text.size()),
                    empty_text.data()
                );
            } else {
                for (auto [directory_index, directory] :
                     std::views::enumerate(directories)) {
                    auto const path_text{ core::path_to_utf8(directory) };

                    ImGui::PushID(static_cast<int>(directory_index));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(path_text.c_str());
                    ImGui::SetItemTooltip("%s", path_text.c_str());
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::Button("删除", { ImGui::GetContentRegionAvail().x, 0.0F })) {
                        directory_to_remove = static_cast<std::size_t>(directory_index);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        if (directory_to_remove) {
            directories.erase(directories.begin() + *directory_to_remove);
        }
        ImGui::PopID();
    }

} // namespace file_monitor::ui
