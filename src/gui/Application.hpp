#pragma once

#include "MainWindow.hpp"
#include "SdlImGuiBackend.hpp"

namespace file_monitor::gui {

    class Application final {
    public:
        int Run();

    private:
        SdlImGuiBackend m_backend;
        MainWindow m_main_window;
    };

} // namespace file_monitor::gui
