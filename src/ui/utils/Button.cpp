#include "Button.hpp"

#include <imgui.h>

namespace file_monitor::ui::utils {

    Button::Button(std::string_view label)
        : m_label{ label } {
    }

    auto Button::Render(float width, float height) const -> bool {
        return ImGui::Button(m_label.c_str(), { width, height });
    }

} // namespace file_monitor::ui::utils
