#pragma once

#include "MainWindows.hpp"
#include "SdlImGuiBackend.hpp"

namespace file_monitor::ui {

    class Application final {
    public:
        auto Run() -> int;

    private:
        SdlImGuiBackend m_backend;
        MainWindows     m_main_windows;
    };

} // namespace file_monitor::ui
