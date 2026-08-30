#pragma once

#include <string>
#include <string_view>

namespace file_monitor::ui::windows {

    class Window {
    public:
        explicit Window(std::string_view identifier);
        virtual ~Window()                        = default;

        Window(Window const&)                    = delete;
        auto operator=(Window const&) -> Window& = delete;
        Window(Window&&)                         = delete;
        auto operator=(Window&&) -> Window&      = delete;

        auto Render(float width, float height) -> void;

    protected:
        virtual auto renderContent() -> void = 0;

    private:
        std::string m_identifier;
    };

} // namespace file_monitor::ui::windows
