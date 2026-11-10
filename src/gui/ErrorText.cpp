#include "ErrorText.hpp"

#include <format>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace file_monitor::gui {
    namespace {

        auto error_text_color() -> ImVec4 {
            return SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT ?
                ImVec4{ 0.75F, 0.10F, 0.10F, 1.0F } :
                ImVec4{ 0.85F, 0.25F, 0.25F, 1.0F };
        }

    } // namespace

    auto render_error(std::string_view message) -> void {
        if (message.empty()) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, error_text_color());
        ImGui::TextWrapped("%.*s", static_cast<int>(message.size()), message.data());
        ImGui::PopStyleColor();
    }

    auto render_error(std::string_view label, std::string_view message) -> void {
        if (message.empty()) {
            return;
        }
        render_error(std::format("{}：{}", label, message));
    }

    auto render_session_errors(core::MonitorSession const& session) -> void {
        render_error("监控失败", session.MonitorError());
        render_error("配置失败", session.ConfigurationError());
        render_error("日志失败", session.LogError());
    }

    auto has_session_errors(core::MonitorSession const& session) -> bool {
        return !session.MonitorError().empty() ||
            !session.ConfigurationError().empty() ||
            !session.LogError().empty();
    }

} // namespace file_monitor::gui
