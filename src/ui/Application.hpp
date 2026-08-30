#pragma once

#include "MainWindow.hpp"
#include "SdlImGuiBackend.hpp"

namespace file_monitor::ui {

    class Application final {
    public:
        auto Run() -> int;

    private:
        SdlImGuiBackend m_backend;
        MainWindow      m_main_window;
    };

} // namespace file_monitor::ui
