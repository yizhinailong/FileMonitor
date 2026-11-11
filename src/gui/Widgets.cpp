#include "Widgets.hpp"

#include <algorithm>

#include <imgui.h>

namespace file_monitor::gui {

    auto text_width(std::string_view text) -> float {
        return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    }

    auto draw_text(std::string_view text) -> void {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
    }

    auto draw_disabled_text(std::string_view text) -> void {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        draw_text(text);
        ImGui::PopStyleColor();
    }

    auto button_width(char const* label) -> float {
        return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0F;
    }

    auto align_right(float width) -> void {
        auto const free_width{ ImGui::GetContentRegionAvail().x - width };
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(free_width, 0.0F));
    }

    auto align_center(float width) -> void {
        auto const free_width{ ImGui::GetContentRegionAvail().x - width };
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(free_width * 0.5F, 0.0F));
    }

} // namespace file_monitor::gui
