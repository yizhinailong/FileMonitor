#pragma once

namespace file_monitor::tui {

    class ConsoleCodePageGuard final {
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

} // namespace file_monitor::tui
