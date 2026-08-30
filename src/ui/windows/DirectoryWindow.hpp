#pragma once

#include "Window.hpp"
#include "ui/utils/Button.hpp"

namespace file_monitor::ui::windows {

    class DirectoryWindow final : public Window {
    public:
        DirectoryWindow();

    private:
        auto renderContent() -> void override;

        utils::Button m_select_directory_button{ "Select directory" };
    };

} // namespace file_monitor::ui::windows
