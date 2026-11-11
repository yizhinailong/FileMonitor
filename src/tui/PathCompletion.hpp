#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace file_monitor::tui {

    struct PathCompletion {
        // The input with its last segment completed as far as the matching
        // directories agree; it ends with a separator when one directory
        // matches.
        std::string text;
        // The names of the matching directories, sorted, when more than one
        // matches.
        std::vector<std::string> candidates;
        bool matched{ false };
    };

    // Completes the last segment of a directory path with the names of the
    // subdirectories that start with it, ignoring case like Windows does. A
    // relative path starts from the current working directory.
    auto complete_directory_path(std::string_view input) -> PathCompletion;

} // namespace file_monitor::tui
