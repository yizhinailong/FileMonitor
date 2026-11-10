#include "ConsoleCodePageGuard.hpp"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace file_monitor::tui {

    ConsoleCodePageGuard::ConsoleCodePageGuard()
        : m_input_code_page{ GetConsoleCP() }, m_output_code_page{ GetConsoleOutputCP() } {
    }

    ConsoleCodePageGuard::~ConsoleCodePageGuard() {
        SetConsoleCP(m_input_code_page);
        SetConsoleOutputCP(m_output_code_page);
    }

} // namespace file_monitor::tui
