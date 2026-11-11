#include "ErrorText.hpp"

#include <format>

#include <imgui.h>

#include "Style.hpp"

namespace file_monitor::gui {

    auto render_error(std::string_view message) -> void {
        if (message.empty()) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, error_color());
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
