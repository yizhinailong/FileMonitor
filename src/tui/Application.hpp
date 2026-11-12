#pragma once

#include <memory>

#include <ftxui/component/app.hpp>

#include "ConsoleCodePageGuard.hpp"
#include "MainWindow.hpp"

import file_monitor.core;

namespace file_monitor::tui {

    class Application final {
    public:
        Application();

        auto Run() -> int;

    private:
        ConsoleCodePageGuard m_code_page_guard;
        ftxui::App m_screen;
        core::MonitorSession m_session;
        std::shared_ptr<MainWindow> m_main_window;
    };

} // namespace file_monitor::tui
