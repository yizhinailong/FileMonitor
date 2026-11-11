#include "Style.hpp"

#include <array>

namespace file_monitor::gui {
    namespace {

        constexpr auto FONT_SIZE{ 18.0F };

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

    auto error_color() -> ImVec4 {
        return themed(ERROR_RED);
    }

} // namespace file_monitor::gui
