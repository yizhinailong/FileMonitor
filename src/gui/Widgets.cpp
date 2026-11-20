#include "Widgets.hpp"

#include <algorithm>

#include <imgui.h>

namespace file_monitor::gui {

    float text_width(std::string_view text)
    {
        return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    }

    void draw_text(std::string_view text)
    {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    void draw_disabled_text(std::string_view text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        draw_text(text);
        ImGui::PopStyleColor();
    }

    float button_width(char const* label)
    {
        return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0F;
    }

    void align_right(float width)
    {
        auto const free_width{ ImGui::GetContentRegionAvail().x - width };
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(free_width, 0.0F));
    }

    void align_center(float width)
    {
        auto const free_width{ ImGui::GetContentRegionAvail().x - width };
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(free_width * 0.5F, 0.0F));
    }

} // namespace file_monitor::gui
