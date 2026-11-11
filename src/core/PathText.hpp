#pragma once

#include <string>
#include <string_view>

#include "FileSystem.hpp"

namespace file_monitor::core {

    // Returns the width of a text in the unit of the interface, such as
    // terminal columns or pixels.
    using TextWidth = float (*)(std::string_view text);

    // Shortens a path to at most `width` by replacing its middle with "...".
    // The drive and the first directory stay in front and as many trailing
    // directories as fit stay at the end, for example
    // "D:\proj\...\src\main.cpp".
    auto shorten_path(std::string_view path, float width, TextWidth text_width) -> std::string;
    // The path of a change in at most `width`. A rename within one directory
    // names the directory once: "D:\proj\old.txt -> new.txt".
    auto change_path_text(FileChange const& change, float width, TextWidth text_width) -> std::string;
    // The directory a path lies in, with its last separator: "D:\a\" for
    // "D:\a\b.txt". A path that ends with a separator, as the path of a
    // changed directory does, is returned as it is.
    auto directory_text(std::string_view path) -> std::string_view;

} // namespace file_monitor::core
