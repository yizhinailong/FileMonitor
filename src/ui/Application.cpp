#include "Application.hpp"

#include <cstdio>
#include <print>
#include <string_view>

#define FILE_MONITOR_STRINGIFY_IMPL(value) #value
#define FILE_MONITOR_STRINGIFY(value)      FILE_MONITOR_STRINGIFY_IMPL(value)

namespace file_monitor::ui {
    namespace {

        constexpr std::string_view WINDOW_TITLE  = "文件监控 v" FILE_MONITOR_STRINGIFY(FILE_MONITOR_VERSION);
        constexpr int              WINDOW_WIDTH  = 960;
        constexpr int              WINDOW_HEIGHT = 640;

    } // namespace

    auto Application::Run() -> int {
        auto const initialization{
            m_backend.Initialize(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT)
        };
        if (!initialization) {
            std::println(stderr, "{}", initialization.error());
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

#undef FILE_MONITOR_STRINGIFY
#undef FILE_MONITOR_STRINGIFY_IMPL
