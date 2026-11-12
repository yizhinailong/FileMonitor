#pragma once

#include <string>
#include <string_view>
#include <vector>

import file_monitor.core;

namespace file_monitor::tui {

    // core::shorten_path() for a width in terminal columns.
    auto shorten_path(std::string_view path, int width) -> std::string;
    // core::change_path_text() for a width in terminal columns.
    auto change_path_text(core::FileChange const& change, int width) -> std::string;
    // Splits text into lines of at most `width` columns, preferring to break
    // after a directory separator.
    auto wrap_text(std::string_view text, int width) -> std::vector<std::string>;

} // namespace file_monitor::tui
