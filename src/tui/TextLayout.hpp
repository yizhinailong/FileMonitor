#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/FileSystem.hpp"

namespace file_monitor::tui {

    // Shortens a path to at most `width` columns by replacing its middle with
    // "...". The drive and the first directory stay in front and as many
    // trailing directories as fit stay at the end, for example
    // "D:\proj\...\src\main.cpp".
    auto shorten_path(std::string_view path, int width) -> std::string;
    // Splits text into lines of at most `width` columns, preferring to break
    // after a directory separator.
    auto wrap_text(std::string_view text, int width) -> std::vector<std::string>;
    // The path column of a change in at most `width` columns. A rename within
    // one directory names the directory once: "D:\proj\old.txt -> new.txt".
    auto change_path_text(core::FileChange const& change, int width) -> std::string;
    // "12:34:56.789" for "2026-11-11 12:34:56.789".
    auto time_of_day(std::string_view time) -> std::string_view;

} // namespace file_monitor::tui
