module;

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

export module file_monitor.tui:console_code_page_guard;

namespace file_monitor::tui {

    export class ConsoleCodePageGuard final {
    public:
        ConsoleCodePageGuard();
        ~ConsoleCodePageGuard();

        ConsoleCodePageGuard(ConsoleCodePageGuard const&) = delete;
        ConsoleCodePageGuard(ConsoleCodePageGuard&&) = delete;
        auto operator=(ConsoleCodePageGuard const&) -> ConsoleCodePageGuard& = delete;
        auto operator=(ConsoleCodePageGuard&&) -> ConsoleCodePageGuard& = delete;

    private:
        unsigned int m_input_code_page;
        unsigned int m_output_code_page;
    };

    ConsoleCodePageGuard::ConsoleCodePageGuard()
        : m_input_code_page{ GetConsoleCP() }, m_output_code_page{ GetConsoleOutputCP() } {
    }

    ConsoleCodePageGuard::~ConsoleCodePageGuard() {
        SetConsoleCP(m_input_code_page);
        SetConsoleOutputCP(m_output_code_page);
    }

} // namespace file_monitor::tui
