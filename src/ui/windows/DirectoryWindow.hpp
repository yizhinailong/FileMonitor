#pragma once

#include "Window.hpp"
#include "ui/utils/Text.hpp"

namespace file_monitor::ui::windows {

    class DirectoryWindow final : public Window {
    public:
        DirectoryWindow();

    private:
        auto renderContent() -> void override;

        utils::Text m_text{ "Select directory" };
    };

} // namespace file_monitor::ui::windows
