#include "Widgets.hpp"

#include <format>
#include <ranges>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>

namespace file_monitor::tui {

    auto make_button(
        std::string label,
        std::function<void()> on_click,
        std::function<bool()> enabled
    ) -> ftxui::Component {
        auto is_enabled{ [enabled = std::move(enabled)] {
            return !enabled || enabled();
        } };

        ftxui::ButtonOption option;
        option.label = std::move(label);
        option.on_click = [on_click = std::move(on_click), is_enabled] {
            if (is_enabled()) {
                on_click();
            }
        };
        option.transform = [is_enabled](ftxui::EntryState const& state) {
            auto element{ ftxui::text(" " + state.label + " ") | ftxui::hcenter | ftxui::borderLight };
            if (!is_enabled()) {
                return element | ftxui::dim;
            }
            if (state.focused) {
                element |= ftxui::inverted;
            }
            return element;
        };
        return ftxui::Button(std::move(option));
    }

    auto error_text(std::string_view message) -> ftxui::Element {
        if (message.empty()) {
            return ftxui::emptyElement();
        }

        ftxui::Elements lines;
        for (auto const line : message | std::views::split('\n')) {
            lines.push_back(ftxui::paragraph(std::string_view{ line.data(), line.size() }));
        }
        return ftxui::vbox(std::move(lines)) | ftxui::color(ftxui::Color::Red);
    }

    auto error_text(std::string_view label, std::string_view message) -> ftxui::Element {
        if (message.empty()) {
            return ftxui::emptyElement();
        }
        return error_text(std::format("{}：{}", label, message));
    }

} // namespace file_monitor::tui
