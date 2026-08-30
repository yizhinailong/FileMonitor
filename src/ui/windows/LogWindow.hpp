#pragma once

#include "Window.hpp"
#include "ui/utils/Text.hpp"

namespace file_monitor::ui::windows {

    class LogWindow final : public Window {
    public:
        LogWindow();

    private:
        auto renderContent() -> void override;

        utils::Text m_text{ "Logs" };
    };

} // namespace file_monitor::ui::windows
