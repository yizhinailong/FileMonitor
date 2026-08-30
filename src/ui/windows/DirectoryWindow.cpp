#include "DirectoryWindow.hpp"

namespace file_monitor::ui::windows {

    DirectoryWindow::DirectoryWindow()
        : Window{ "DirectoryWindow" } {
    }

    auto DirectoryWindow::renderContent() -> void {
        m_text.Render();
    }

} // namespace file_monitor::ui::windows
