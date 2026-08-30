#pragma once

#include "windows/DirectoryWindow.hpp"
#include "windows/LogWindow.hpp"

namespace file_monitor::ui {

    class MainWindows final {
    public:
        auto Render() -> void;

    private:
        windows::DirectoryWindow m_directory_window;
        windows::LogWindow       m_log_window;
    };

} // namespace file_monitor::ui
