#include "Application.hpp"

namespace file_monitor::ui {
    namespace {

        constexpr std::string_view WINDOW_TITLE  = "FileMonitor";
        constexpr int              WINDOW_WIDTH  = 960;
        constexpr int              WINDOW_HEIGHT = 640;

    } // namespace

    auto Application::Run() -> int {
        if (!m_backend.Initialize(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT)) {
            return 1;
        }
        m_main_window.SetParentWindow(m_backend.NativeWindow());

        while (m_backend.ProcessEvents()) {
            if (!m_backend.BeginFrame()) {
                continue;
            }

            m_main_window.Render();
            if (!m_backend.EndFrame()) {
                return 1;
            }
        }

        return 0;
    }

} // namespace file_monitor::ui
