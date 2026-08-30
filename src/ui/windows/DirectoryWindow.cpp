#include "DirectoryWindow.hpp"

namespace file_monitor::ui::windows {

    DirectoryWindow::DirectoryWindow()
        : Window{ "DirectoryWindow" } {
    }

    auto DirectoryWindow::renderContent() -> void {
        m_select_directory_button.Render();
    }

} // namespace file_monitor::ui::windows
