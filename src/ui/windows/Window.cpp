#include "Window.hpp"

#include <imgui.h>

namespace file_monitor::ui::windows {

    Window::Window(std::string_view identifier)
        : m_identifier{ identifier } {
    }

    auto Window::Render(float width, float height) -> void {
        if (ImGui::BeginChild(m_identifier.c_str(), { width, height }, true)) {
            renderContent();
        }
        ImGui::EndChild();
    }

} // namespace file_monitor::ui::windows
