#pragma once

#include <string_view>

namespace file_monitor::tui {

    // Puts UTF-8 text on the Windows clipboard of this machine.
    auto copy_to_clipboard(std::string_view text) -> bool;

} // namespace file_monitor::tui
