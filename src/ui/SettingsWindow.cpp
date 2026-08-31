#include "SettingsWindow.hpp"

#include <algorithm>
#include <exception>
#include <mutex>
#include <optional>
#include <utility>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/Configuration.hpp"
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
        m_directories = std::move(configuration->directories);
    }

    auto SettingsWindow::SetParentWindow(SDL_Window* window) -> void {
        m_parent_window = window;
    }

    auto SettingsWindow::Open() -> void {
        m_pending_directories = m_directories;
        m_directory_input.clear();
        m_directory_input_error_message.clear();
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
            return false;
        }

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
                "输入文件夹路径",
                &m_directory_input,
                ImGuiInputTextFlags_EnterReturnsTrue
            )
        };
        if (ImGui::IsItemEdited()) {
            m_directory_input_error_message.clear();
        }
        ImGui::SameLine();
        auto const add_directory{
            ImGui::Button("添加", { ADD_BUTTON_WIDTH, 0.0F })
        };
        if (submit_directory || add_directory) {
            addManualDirectory();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_dialog_open);
        auto const select_directories{
            ImGui::Button("选择文件夹", { SELECT_BUTTON_WIDTH, 0.0F })
        };
        ImGui::EndDisabled();
        if (select_directories) {
            openDirectoryDialog();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_pending_directories.empty());
        auto const clear_directories{
            ImGui::Button("清空", { CLEAR_BUTTON_WIDTH, 0.0F })
        };
        ImGui::EndDisabled();
        if (clear_directories) {
            m_pending_directories.clear();
        }

        if (!m_directory_input_error_message.empty()) {
            ImGui::TextColored(
                error_text_color(),
                "%s",
                m_directory_input_error_message.c_str()
            );
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

        auto directories_changed{ false };
        if (ImGui::Button("保存", { ImGui::GetContentRegionAvail().x, 0.0F })) {
            auto const save_result{
                core::save_configuration(
                    m_config_path,
                    core::Configuration{ .directories = m_pending_directories }
                )
            };
            if (!save_result) {
                m_configuration_error_message = save_result.error();
            } else {
                m_configuration_error_message.clear();
                m_directories       = m_pending_directories;
                directories_changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
        return directories_changed;
    }

    auto SettingsWindow::Directories() const
        -> std::vector<std::filesystem::path> const& {
        return m_directories;
    }

    auto SettingsWindow::addManualDirectory() -> void {
        if (m_directory_input.empty()) {
            m_directory_input_error_message = "请输入文件夹路径";
            return;
        }

        std::filesystem::path directory;
        try {
            directory = utf8_to_path(m_directory_input).lexically_normal();
        } catch (std::exception const& error) {
            m_directory_input_error_message = "文件夹路径无效：" + std::string{ error.what() };
            return;
        }

        std::error_code check_error;
        auto const      is_directory{ std::filesystem::is_directory(directory, check_error) };
        if (check_error) {
            m_directory_input_error_message =
                "检查文件夹失败：" + check_error.message();
            return;
        }
        if (!is_directory) {
            m_directory_input_error_message = "路径不是存在的文件夹";
            return;
        }
        if (!addPendingDirectory(directory)) {
            m_directory_input_error_message = "该文件夹已添加";
            return;
        }

        m_directory_input.clear();
        m_directory_input_error_message.clear();
    }

    auto SettingsWindow::addPendingDirectory(std::filesystem::path directory) -> bool {
        directory = directory.lexically_normal();
        if (std::ranges::find(m_pending_directories, directory) !=
            m_pending_directories.end()) {
            return false;
        }

        m_pending_directories.emplace_back(std::move(directory));
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
            addPendingDirectory(std::move(directory));
        }
    }

    auto SettingsWindow::openDirectoryDialog() -> void {
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

} // namespace file_monitor::ui
