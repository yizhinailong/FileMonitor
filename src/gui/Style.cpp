#include "Style.hpp"

#include <algorithm>
#include <array>

namespace file_monitor::gui {
    namespace {

        constexpr auto FONT_SIZE{ 18.0F };
        constexpr auto SELECTED_ROW_ALPHA{ 0.45F };
        constexpr auto HOVERED_ROW_ALPHA{ 0.15F };

        // A color for each theme.
        struct ThemeColor {
            ImVec4 light;
            ImVec4 dark;
        };

        constexpr ThemeColor GREEN{
            .light = { 0.05F, 0.45F, 0.15F, 1.0F },
            .dark = { 0.30F, 0.85F, 0.40F, 1.0F }
        };
        constexpr ThemeColor RED{
            .light = { 0.75F, 0.10F, 0.10F, 1.0F },
            .dark = {  1.0F, 0.35F, 0.35F, 1.0F }
        };
        constexpr ThemeColor YELLOW{
            .light = { 0.60F, 0.38F,  0.0F, 1.0F },
            .dark = {  1.0F, 0.80F, 0.25F, 1.0F }
        };
        constexpr ThemeColor CYAN{
            .light = {  0.0F, 0.42F, 0.55F, 1.0F },
            .dark = { 0.35F, 0.80F, 0.92F, 1.0F }
        };
        constexpr ThemeColor MAGENTA{
            .light = { 0.58F, 0.18F, 0.62F, 1.0F },
            .dark = { 0.88F, 0.55F, 0.95F, 1.0F }
        };
        constexpr ThemeColor ERROR_RED{
            .light = { 0.75F, 0.10F, 0.10F, 1.0F },
            .dark = { 0.85F, 0.25F, 0.25F, 1.0F }
        };

        auto current_theme{ Theme::Dark };
        auto current_scale{ 1.0F };

        auto themed(ThemeColor const& color) -> ImVec4 {
            return current_theme == Theme::Light ? color.light : color.dark;
        }

    } // namespace

    auto load_font() -> bool {
        constexpr std::array FONT_PATHS{
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/simhei.ttf"
        };

        auto& io{ ImGui::GetIO() };
        for (auto const* font_path : FONT_PATHS) {
            if (auto* font{ io.Fonts->AddFontFromFileTTF(font_path, FONT_SIZE) }) {
                io.FontDefault = font;
                return true;
            }
        }
        return false;
    }

    auto apply_theme(Theme theme) -> void {
        current_theme = theme;
        if (theme == Theme::Light) {
            ImGui::StyleColorsLight();
        } else {
            ImGui::StyleColorsDark();
        }

        auto& style{ ImGui::GetStyle() };
        // The sizes are set again rather than scaled, so that applying the
        // theme once more does not scale them once more.
        auto const scale{ current_scale };
        // Menus and the settings window cover text, which shows through the
        // default background.
        style.Colors[ImGuiCol_PopupBg].w = 1.0F;
        style.WindowPadding = { 12.0F * scale, 10.0F * scale };
        style.FramePadding = { 10.0F * scale, 5.0F * scale };
        style.CellPadding = { 8.0F * scale, 4.0F * scale };
        style.ItemSpacing = { 8.0F * scale, 8.0F * scale };
        style.WindowRounding = 6.0F * scale;
        style.ChildRounding = 4.0F * scale;
        style.FrameRounding = 4.0F * scale;
        style.PopupRounding = 4.0F * scale;
        style.GrabRounding = 4.0F * scale;
        style.FontScaleDpi = scale;
    }

    auto set_scale(float scale) -> void {
        current_scale = std::max(scale, 0.5F);
        apply_theme(current_theme);
    }

    auto file_change_status_color(core::FileChangeStatus status) -> ImVec4 {
        switch (status) {
            case core::FileChangeStatus::Added:
                return themed(GREEN);
            case core::FileChangeStatus::Removed:
                return themed(RED);
            case core::FileChangeStatus::Modified:
                return themed(YELLOW);
            case core::FileChangeStatus::Renamed:
                return themed(CYAN);
            case core::FileChangeStatus::Moved:
                return themed(MAGENTA);
        }
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }

    auto selected_row_color() -> ImVec4 {
        auto color{ ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive) };
        color.w = SELECTED_ROW_ALPHA;
        return color;
    }

    auto hovered_row_color() -> ImVec4 {
        auto color{ ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive) };
        color.w = HOVERED_ROW_ALPHA;
        return color;
    }

    auto success_color() -> ImVec4 {
        return themed(GREEN);
    }

    auto warning_color() -> ImVec4 {
        return themed(YELLOW);
    }

    auto error_color() -> ImVec4 {
        return themed(ERROR_RED);
    }

} // namespace file_monitor::gui
