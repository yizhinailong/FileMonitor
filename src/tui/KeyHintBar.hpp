#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

namespace file_monitor::tui {

    struct KeyHint {
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
    class KeyHintBar final {
    public:
        explicit KeyHintBar(std::vector<KeyHint> hints);

        auto Render(int width) -> ftxui::Element;
        // Runs the action of a clicked hint.
        auto OnEvent(ftxui::Event event) -> bool;

    private:
        std::vector<KeyHint> m_hints;
        std::vector<ftxui::Box> m_boxes;
    };

} // namespace file_monitor::tui
