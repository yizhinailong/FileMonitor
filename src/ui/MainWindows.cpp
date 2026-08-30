#include "MainWindows.hpp"

#include <imgui.h>

namespace file_monitor::ui {

    auto MainWindows::SetParentWindow(SDL_Window* window) -> void {
        m_directory_window.SetParentWindow(window);
    }

    auto MainWindows::Render() -> void {
        if (auto directory{ m_directory_window.TakeSelectedDirectory() }) {
            m_log_window.SetDirectory(*directory);
        }

        auto const* viewport{ ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0F, 0.0F });
        ImGui::Begin("FileMonitorMainWindows", nullptr, WINDOW_FLAGS);
        ImGui::PopStyleVar();

        auto const available{ ImGui::GetContentRegionAvail() };
        auto const directory_window_height{ available.y * 0.15F };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0F, 0.0F });
        m_directory_window.Render(available.x, directory_window_height);
        m_log_window.Render(available.x, 0.0F);
        ImGui::PopStyleVar();

        ImGui::End();
    }

} // namespace file_monitor::ui
