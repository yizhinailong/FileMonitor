#include "LogWindow.hpp"

namespace file_monitor::ui::windows {

    LogWindow::LogWindow()
        : Window{ "LogWindow" } {
    }

    auto LogWindow::renderContent() -> void {
        m_text.Render();
    }

} // namespace file_monitor::ui::windows
