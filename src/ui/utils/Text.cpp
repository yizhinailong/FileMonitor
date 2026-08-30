#include "Text.hpp"

#include <imgui.h>

namespace file_monitor::ui::utils {

    Text::Text(std::string_view content)
        : m_content{ content } {
    }

    auto Text::SetContent(std::string_view content) -> void {
        m_content = content;
    }

    auto Text::Render() const -> void {
        auto const* content_end{ m_content.data() + m_content.size() };
        ImGui::TextUnformatted(m_content.data(), content_end);
    }

} // namespace file_monitor::ui::utils
