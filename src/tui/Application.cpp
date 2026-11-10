#include "Application.hpp"

#include <chrono>
#include <csignal>
#include <stop_token>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

namespace file_monitor::tui {
    namespace {

        constexpr auto POLL_INTERVAL{ std::chrono::milliseconds{ 100 } };

    } // namespace

    Application::Application()
        : m_screen{ ftxui::App::Fullscreen() },
          m_main_window{ ftxui::Make<MainWindow>(m_session, m_screen.ExitLoopClosure()) } {
    }

    auto Application::Run() -> int {
        // FTXUI leaves the loop on Ctrl+C and then re-raises SIGINT with the
        // handler installed before it. Ignoring SIGINT here lets Run() return
        // normally so that the monitor stops and the console is restored.
        auto const previous_interrupt_handler{ std::signal(SIGINT, SIG_IGN) };

        std::jthread poller{ [this](std::stop_token stop_token) {
            while (!stop_token.stop_requested()) {
                std::this_thread::sleep_for(POLL_INTERVAL);
                m_screen.Post([this] {
                    if (m_main_window->Update()) {
                        m_screen.PostEvent(ftxui::Event::Custom);
                    }
                });
            }
        } };

        m_screen.Loop(m_main_window);
        poller.request_stop();
        poller.join();

        std::signal(SIGINT, previous_interrupt_handler);
        return 0;
    }

} // namespace file_monitor::tui
