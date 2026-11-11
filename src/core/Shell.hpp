#pragma once

#include <string_view>

namespace file_monitor::core {

    // Opens File Explorer with an existing file or directory selected. For a
    // path that no longer exists, opens its closest existing parent directory.
    auto show_in_explorer(std::string_view path) -> bool;

} // namespace file_monitor::core
