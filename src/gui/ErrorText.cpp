#include "ErrorText.hpp"

#include <format>

#include <imgui.h>

#include "Style.hpp"

namespace file_monitor::gui {

    void render_error(std::string_view message)
    {
        if (message.empty()) {
            return;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, error_color());
        ImGui::TextWrapped("%.*s", static_cast<int>(message.size()), message.data());
        ImGui::PopStyleColor();
    }

    void render_error(std::string_view label, std::string_view message)
    {
        if (message.empty()) {
            return;
        }
        render_error(std::format("{}：{}", label, message));
    }

    void render_session_errors(core::MonitorSession const& session)
    {
        render_error("监控失败", session.MonitorError());
        render_error("配置失败", session.ConfigurationError());
        render_error("日志失败", session.LogError());
    }

    bool has_session_errors(core::MonitorSession const& session)
    {
        return !session.MonitorError().empty() ||
            !session.ConfigurationError().empty() ||
            !session.LogError().empty();
    }

} // namespace file_monitor::gui
