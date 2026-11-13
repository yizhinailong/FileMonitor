module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

export module file_monitor.tui:key_hint_bar;

import ftxui;

namespace file_monitor::tui {

    export struct KeyHint {
        std::string key;
        std::string label;
        // Runs when the hint is clicked; a hint without an action only
        // describes its key.
        std::function<void()> action;
        // When the line is too narrow, hints with a lower priority are hidden
        // first.
        int priority{ 0 };
        // Hides the hint while it returns false.
        std::function<bool()> available;
    };

    // A line of key hints such as "s 设置  q 退出".
    export class KeyHintBar final {
    public:
        explicit KeyHintBar(std::vector<KeyHint> hints);

        auto Render(int width) -> ftxui::Element;
        // Runs the action of a clicked hint.
        auto OnEvent(ftxui::Event event) -> bool;

    private:
        std::vector<KeyHint> m_hints;
        std::vector<ftxui::Box> m_boxes;
    };

    namespace {

        constexpr auto HINT_GAP{ 2 };
        constexpr ftxui::Box HIDDEN_BOX{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };

        auto hint_width(KeyHint const& hint) -> int {
            return ftxui::string_width(hint.key) + 1 + ftxui::string_width(hint.label);
        }

    } // namespace

    KeyHintBar::KeyHintBar(std::vector<KeyHint> hints)
        : m_hints{ std::move(hints) }, m_boxes(m_hints.size(), HIDDEN_BOX) {}

    auto KeyHintBar::Render(int width) -> ftxui::Element {
        // Fill the line by priority, then show the chosen hints in their
        // original order.
        auto by_priority{
            std::views::iota(std::size_t{ 0 }, m_hints.size()) |
            std::ranges::to<std::vector<std::size_t>>()
        };
        std::ranges::stable_sort(by_priority, std::ranges::greater{}, [this](std::size_t index) {
            return m_hints[index].priority;
        });
        std::vector<bool> shown(m_hints.size(), false);
        auto used_width{ 0 };
        for (auto const index : by_priority) {
            auto const& hint{ m_hints[index] };
            if (hint.available && !hint.available()) {
                continue;
            }
            auto const gap{ used_width == 0 ? 0 : HINT_GAP };
            auto const required_width{ gap + hint_width(hint) };
            if (used_width + required_width <= width) {
                used_width += required_width;
                shown[index] = true;
            }
        }

        ftxui::Elements elements;
        for (auto const index : std::views::iota(std::size_t{ 0 }, m_hints.size())) {
            m_boxes[index] = HIDDEN_BOX;
            if (!shown[index]) {
                continue;
            }
            if (!elements.empty()) {
                elements.push_back(ftxui::text(std::string(HINT_GAP, ' ')));
            }
            auto const& hint{ m_hints[index] };
            elements.push_back(
                ftxui::hbox({
                    ftxui::text(hint.key) | ftxui::bold,
                    ftxui::text(" " + hint.label) | ftxui::dim,
                }) |
                ftxui::reflect(m_boxes[index])
            );
        }
        return ftxui::hbox(std::move(elements));
    }

    auto KeyHintBar::OnEvent(ftxui::Event event) -> bool {
        if (!event.is_mouse()) {
            return false;
        }
        auto const& mouse{ event.mouse() };
        if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Pressed) {
            return false;
        }
        for (auto index{ std::size_t{ 0 } }; index < m_hints.size(); ++index) {
            auto const& hint{ m_hints[index] };
            if (hint.action && m_boxes[index].Contain(mouse.x, mouse.y)) {
                hint.action();
                return true;
            }
        }
        return false;
    }

} // namespace file_monitor::tui
